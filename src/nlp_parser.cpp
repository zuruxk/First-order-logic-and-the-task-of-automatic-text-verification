#include "nlp_parser.hpp"
#include "tokenizer.hpp"
#include <algorithm>

namespace nlp {
    namespace {
        using Tokens = std::vector <std::string>;
        bool has (const Tokens& t, const std::string& x) {
            return std::find (t.begin(), t.end(), x) != t.end();
        }
        fol::Atom A (const std::string& p, const std::string& x) {
            return {p, {x}};
        }
        std::string entityName (std::string x) {
            if (!x.empty() && x[0] >= 'a' && x[0] <= 'z')x[0] -= 32;
            return x;
        }
        // Tokens slice (const Tokens& t, std::size_t a, std::size_t b) {
        //     return Tokens (t.begin() + static_cast <long>(a), t.begin() + static_cast <long>(b));
        // }
        std::vector <Tokens> splitOn (const Tokens& t, const std::string& op) {
            std::vector <Tokens> out;
            Tokens part;
            for (const auto& x: t) {
                if(x == op) {
                    if(!part.empty()) out.push_back(part);
                    part.clear();
                }
                else part.push_back(x);
            }
            if (!part.empty()) out.push_back(part);
            return out;
        }
        bool parseAtomTokens (Tokens t, fol::Literal& lit, std::string& why) {
            if (t.empty()) {
                why = "Пустая часть логического выражения.";
                return false;
            }
            bool neg = false;
            auto ni = std::find (t.begin(), t.end(), "не");
            if (ni != t.end()) {
                neg = true;
                t.erase(ni);
            }
            if (t.empty()) {
                why = "После отрицания не найдено утверждение.";
                return false;
            }
            std::string e = entityName (t.front());
            if ((t.size() >= 2 && ((t[1] == "является" && (has(t, "студент") || has(t, "студентом"))) || t[1] == "студент"))) {
                lit = {neg, A("Student", e)};
                return true;
            }
            if ((has(t, "сдал") || has(t, "сдала") || has(t, "сдали")) && has(t, "экзамен")) {
                lit = {neg, A("Passed", e)};
                return true;
            }
            if ((has(t, "получает") || has(t,"получают")) && has(t, "зачет")) {
                lit = {neg, A("Credit", e)};
                return true;
            }
            why = "Не распознано утверждение. Поддерживаются конструкции про студента, сдачу экзамена и получение зачёта.";
            return false;
        }
        void addImplicitSubject (Tokens& part,const std::string& subject) {
            if (part.empty()) return;
            const auto& w = part.front();
            if (w == "не" || w == "сдал" || w == "сдала" || w == "сдали" || w == "получает" || w == "получают") part.insert(part.begin(), subject);
        }
        bool parseDisjunction (Tokens t, const std::string& subject, fol::Clause& c, std::string& why) {
            auto parts = splitOn(t, "или");
            if (parts.size() < 2) {
                addImplicitSubject (t, subject);
                fol::Literal l;
                if (!parseAtomTokens(t, l, why)) return false;
                c.push_back(l);
                return true;
            }
            for (auto& p: parts) {
                addImplicitSubject (p, subject);
                fol::Literal l;
                if (!parseAtomTokens(p, l, why)) return false;
                c.push_back(l);
            }
            return true;
        }
    }
    ParseResult parseKnowledgeSentence (const std::string& s) {
        ParseResult r;
        r.normalized = normalizeRussian(s);
        r.tokens = tokenize(s);
        const auto& t = r.tokens;
        if (t.empty()) {
            r.explanation = "Пустое предложение.";
            return r;
        }
        if ((t[0] == "все" || t[0] == "каждый") && (has(t, "студенты") || has(t, "студент") || has(t, "студентом")) && (has(t, "сдавшие") || has(t, "который") || has(t, "сдал")) &&
            has(t, "экзамен") && (has(t, "получают") || has(t, "получает")) && has(t, "зачет")) {
            r.success = true;
            r.isRule = true;
            r.clause = {{true, A("Student", "?x")}, {true, A("Passed", "?x")}, {false, A("Credit", "?x")}};
            r.clauses = {r.clause};
            r.explanation = "Распознано универсальное правило: студент, сдавший экзамен, получает зачёт.";
            return r;
        }
        if (t[0] == "если") {
            auto it = std::find(t.begin(), t.end(), "то");
            if (it == t.end()) {
                r.explanation = "В условном предложении не найдено слово 'то'.";
                return r;
            }
            Tokens left (t.begin() + 1,it), right(it + 1, t.end());
            auto antecedents = splitOn(left, "и");
            if (antecedents.empty() || right.empty() || has(left, "или") || has(right, "и") || has(right, "или")) {
                r.explanation = "Поддерживается форма: Если A и B, то C. В заключении C должна быть одна простая часть.";
                return r;
            }
            fol::Clause c;
            for (auto& part: antecedents) {
                addImplicitSubject (part, left.empty() ? std::string() : left.front());
                fol::Literal l;
                if (!parseAtomTokens(part, l, r.explanation)) return r;
                l.negated =! l.negated;
                c.push_back(l);
            }
            fol::Literal conclusion;
            addImplicitSubject (right, right.empty() ? std::string() : right.front());
            if (!parseAtomTokens (right, conclusion, r.explanation)) return r;
            c.push_back(conclusion);
            r.success = true;
            r.isRule = true;
            r.clause = c;
            r.clauses = {c};
            r.explanation = "Распознано условное правило и преобразовано в клаузу: отрицания условий ИЛИ заключение.";
            return r;
        }
        auto conjuncts = splitOn(t, "и");
        for (auto& part: conjuncts) {
            fol::Clause c;
            if (!parseDisjunction(part, t.front(), c, r.explanation)) return r;
            r.clauses.push_back(c);
        }
        if (r.clauses.empty()) {
            r.explanation = "Не удалось выделить логические части предложения.";
            return r;
        }
        r.success = true;
        r.clause = r.clauses.front();
        bool hasAnd = conjuncts.size() > 1, hasOr = std::any_of (r.clauses.begin(), r.clauses.end(), [](const auto& c) {return c.size()>1;});
        r.explanation = "Распознано логическое выражение";
        if (hasAnd) r.explanation += " с конъюнкцией (И)";
        if (hasOr) r.explanation += " и дизъюнкцией (ИЛИ)";
        if (std::any_of (r.clauses.begin(), r.clauses.end(), [](const auto& c) {
            return std::any_of (c.begin(), c.end(), [](const auto& l) {
                return l.negated;
            });
        })) r.explanation += " и отрицанием (НЕ)";
        r.explanation += "; выражение преобразовано в клаузальную форму.";
        return r;
    }
    bool parseQuery (const std::string& s, fol::Atom& q, std::string&why) {
        auto r = parseKnowledgeSentence(s);
        if (!r.success) {
            why = r.explanation;
            return false;
        }
        if (r.isRule || r.clauses.size() !=1 || r.clauses[0].size() !=1 || r.clauses[0][0].negated) {
            why = "Пока запрос должен быть одним простым положительным утверждением. Составные выражения поддерживаются в базе знаний.";
            return false;
        }
        q = r.clauses[0][0].atom;
        why = r.explanation;
        return true;
    }
}
