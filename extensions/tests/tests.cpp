// Проверка всех методов на случайных базах знаний.
//  * Исходные резолюции, исходные таблицы, резолюции v2 и таблицы v2 должны давать
//    одинаковый ответ (если база непротиворечива и не упёрлись в лимит).
//  * Прямой вывод v2 и вывод из аксиом корректны: если они доказали, то запрос
//    действительно следует (таблицы тоже доказывают).
//  * Каждый вывод из аксиом проверяется функцией hil::check: только посылки,
//    экземпляры аксиом и MP; вывод без посылок — только аксиомы и MP.
//  * На базах Хорна прямой вывод v2 совпадает с исходным прямым выводом.
#include "ext_methods.hpp"
#include "hilbert.hpp"
#include "nlp_parser.hpp"
#include "other_methods.hpp"
#include "resolution.hpp"
#include <iostream>
#include <random>

using namespace fol;

namespace {
    int failures = 0;
    void fail(const std::string& what, const std::vector<Clause>& kb, const Atom& q) {
        if (++failures > 5) return;
        std::cout << "ОШИБКА: " << what << ", запрос " << ext::atomText(q) << "\n";
        for (auto& c : kb) std::cout << "    " << ext::clauseText(c) << "\n";
    }

    bool consistent(const std::vector<Clause>& kb) {
        // база непротиворечива, если «запрос», которого в ней нет, из неё не следует
        Atom fresh{"Fresh__", {"_c"}};
        return ext::tableauClausal(kb, fresh).status == ext::Status::NotProved;
    }
}

int main() {
    // 1. Примеры из презентации
    {
        std::vector<Clause> kb;
        for (auto s : {"Все студенты, сдавшие экзамен, получают зачёт.", "Иван является студентом и сдал экзамен."})
            for (auto& c : nlp::parseKnowledgeSentence(s).clauses) kb.push_back(c);
        Atom q; std::string why;
        nlp::parseQuery("Иван получает зачёт?", q, why);
        bool ok = ext::resolutionSOS(kb, q).status == ext::Status::Proved
               && ext::forwardUnit(kb, q).status == ext::Status::Proved
               && ext::tableauClausal(kb, q).status == ext::Status::Proved;
        auto h = hil::prove(kb, q);
        ok = ok && h.status == ext::Status::Proved && h.directChecked && h.theoremBuilt && h.theoremChecked;
        std::cout << "Пример про Ивана: " << (ok ? "все методы доказали, выводы из аксиом проверены" : "ОШИБКА") << "\n";
        if (!ok) ++failures;
        // проверка сама должна ловить ошибки: портим одну строку вывода
        auto broken = h.direct;
        broken[broken.size() / 2].f = hil::atom(Atom{"Fake", {"иван"}});
        bool caught = !hil::check(broken, h.premises, h.goal);
        std::cout << "Испорченный вывод проверка " << (caught ? "отклонила" : "НЕ ЗАМЕТИЛА") << "\n";
        if (!caught) ++failures;
    }

    // 2. Случайные базы
    std::mt19937 rng(2026);
    const char* preds[] = {"Student", "Passed", "Credit", "D"};
    const char* consts[] = {"a", "b", "c"};
    int total = 0, compared = 0, hilbertProved = 0, theorems = 0, forwardProved = 0, horn = 0;
    for (int it = 0; it < 4000; ++it) {
        std::vector<Clause> kb;
        int n = 3 + (int)(rng() % 9);
        for (int k = 0; k < n; ++k) {
            Clause c;
            int len = 1 + (int)(rng() % 3);
            bool rule = rng() % 3 == 0;
            for (int l = 0; l < len; ++l) {
                Literal L;
                L.negated = rng() % 2;
                L.atom.predicate = preds[rng() % 4];
                L.atom.args = {rule ? std::string("?x") : std::string(consts[rng() % 3])};
                c.push_back(L);
            }
            if (simplifyClause(c)) kb.push_back(c);
        }
        if (kb.empty()) continue;
        Atom q{preds[rng() % 4], {consts[rng() % 3]}};
        ++total;

        auto r1 = prove(kb, q, 1000, 20000, false);
        auto t1 = tableaux(kb, q);
        auto r2 = ext::resolutionSOS(kb, q);
        auto t2 = ext::tableauClausal(kb, q);
        auto f2 = ext::forwardUnit(kb, q);
        bool truth = t1.status == SimpleStatus::Proved;     // эталон — исходные таблицы (полный перебор)

        if (t2.status != ext::Status::LimitReached && (t2.status == ext::Status::Proved) != truth)
            fail("таблицы v2 расходятся с исходными", kb, q);
        if (r1.status != ProofStatus::LimitReached && (r1.status == ProofStatus::Proved) != truth)
            fail("исходные резолюции расходятся с таблицами", kb, q);
        if (consistent(kb) && r2.status != ext::Status::LimitReached) {
            ++compared;
            if ((r2.status == ext::Status::Proved) != truth) fail("резолюции v2 расходятся с таблицами", kb, q);
        }
        if (f2.status == ext::Status::Proved) {
            ++forwardProved;
            if (!truth) fail("прямой вывод v2 доказал то, что не следует", kb, q);
        }
        // Сравнение с исходным прямым выводом только на базах Хорна без фактов с переменной
        // (исходный forwardChaining не выводит ничего из фактов вида ∀x P(x); парсер таких не создаёт).
        bool isHorn = true;
        for (auto& c : kb) {
            int pos = 0; bool vars = false;
            for (auto& l : c) { pos += !l.negated; for (auto& x : l.atom.args) vars |= isVariable(x); }
            if (pos != 1 || (c.size() == 1 && vars)) isHorn = false;
        }
        if (isHorn) {
            ++horn;
            auto fw = forwardChaining(kb, q);
            if ((fw.status == SimpleStatus::Proved) != (f2.status == ext::Status::Proved))
                fail("прямой вывод v2 расходится с исходным на базе Хорна", kb, q);
        }

        auto h = hil::prove(kb, q, 200000);
        if (h.status == ext::Status::Proved) {
            ++hilbertProved;
            if (!truth) fail("вывод из аксиом доказал то, что не следует", kb, q);
            if (!h.directChecked) fail("вывод из посылок не прошёл проверку", kb, q);
            if (h.theoremBuilt) {
                ++theorems;
                if (!h.theoremChecked) fail("вывод без посылок не прошёл проверку", kb, q);
            }
        }
    }
    std::cout << "Случайных баз: " << total << "\n"
              << "  резолюции v2 сравнены с таблицами на " << compared << " непротиворечивых базах\n"
              << "  прямой вывод v2 доказал " << forwardProved << " запросов, базы Хорна: " << horn << "\n"
              << "  вывод из аксиом найден " << hilbertProved << " раз, из них без посылок (ТД) построен и проверен "
              << theorems << " раз\n";
    std::cout << (failures ? "ЕСТЬ ОШИБКИ: " + std::to_string(failures) : std::string("Все проверки пройдены")) << "\n";
    return failures ? 1 : 0;
}
