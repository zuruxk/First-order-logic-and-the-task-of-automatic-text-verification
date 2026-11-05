#include "ext_methods.hpp"
#include <algorithm>
#include <functional>
#include <set>

namespace ext {
    const char* statusText(Status s) {
        switch (s) {
            case Status::Proved: return "доказано";
            case Status::NotProved: return "не доказано";
            case Status::LimitReached: return "лимит";
            case Status::Unsupported: return "не поддерживает";
        }
        return "?";
    }

    std::string termText(const std::string& t) {
        if (!fol::isVariable(t)) return t;
        std::string s = t.substr(1);
        auto p = s.find('_');
        return p == std::string::npos ? s : s.substr(0, p);
    }

    std::string atomText(const Atom& a) {
        std::string s = a.predicate + "(";
        for (std::size_t i = 0; i < a.args.size(); ++i) {
            if (i) s += ",";
            s += termText(a.args[i]);
        }
        return s + ")";
    }

    std::string literalText(const Literal& l) {
        return (l.negated ? "¬" : "") + atomText(l.atom);
    }

    std::string clauseText(const Clause& c) {
        if (c.empty()) return "□";
        std::string s;
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (i) s += " ∨ ";
            s += literalText(c[i]);
        }
        return s;
    }

    std::string padRight(const std::string& s, std::size_t width) {
        std::size_t chars = 0;
        for (unsigned char ch : s) if ((ch & 0xC0) != 0x80) ++chars;
        return chars >= width ? s + " " : s + std::string(width - chars, ' ');
    }

    std::vector<std::string> constantsOf(const std::vector<Clause>& kb, const Atom& query) {
        std::set<std::string> seen;
        std::vector<std::string> out;
        auto add = [&](const Atom& a) {
            for (const auto& x : a.args)
                if (!fol::isVariable(x) && seen.insert(x).second) out.push_back(x);
        };
        for (const auto& c : kb) for (const auto& l : c) add(l.atom);
        add(query);
        if (out.empty()) out.push_back("_c");
        return out;
    }

    std::vector<Clause> groundAll(const std::vector<Clause>& kb, const std::vector<std::string>& constants) {
        std::vector<Clause> out;
        std::set<std::string> seen;
        for (const auto& c : kb) {
            std::vector<std::string> vars;
            for (const auto& l : c)
                for (const auto& x : l.atom.args)
                    if (fol::isVariable(x) && std::find(vars.begin(), vars.end(), x) == vars.end()) vars.push_back(x);
            std::function<void(std::size_t, Substitution&)> rec = [&](std::size_t i, Substitution& s) {
                if (i == vars.size()) {
                    Clause g;
                    for (const auto& l : c) {
                        Literal z = l;
                        z.atom = fol::applySubstitution(l.atom, s);
                        g.push_back(z);
                    }
                    if (fol::simplifyClause(g) && seen.insert(fol::canonicalKey(g)).second) out.push_back(g);
                    return;
                }
                for (const auto& k : constants) {
                    s[vars[i]] = k;
                    rec(i + 1, s);
                }
                s.erase(vars[i]);
            };
            Substitution s;
            rec(0, s);
        }
        return out;
    }
}
