#include "logic.hpp"
#include <algorithm>
#include <set>

namespace fol {
    bool isVariable (const std::string& s) {
        return !s.empty() && s[0] == '?';
    }
    
    static std::string deref (std::string x, const Substitution& s) {
        std::set <std::string> seen;
        while (isVariable(x) && s.count(x) && !seen.count(x)) {
            seen.insert(x);
            x = s.at(x);
            }
        return x;
    }
    
    static bool unifyTerm (const std::string& a0, const std::string& b0, Substitution& s) {
        auto a = deref(a0, s),b=deref(b0, s);
        if (a == b) return true;
        if(isVariable(a)) {
            s[a] = b;
            return true;
        }
        if(isVariable(b)) {
            s[b] = a;
            return true;
        }
        return false;
    }
    
    bool unifyAtoms (const Atom& a, const Atom& b, Substitution& s) {
        if (a.predicate != b.predicate || a.args.size() != b.args.size()) return false;
        for(std::size_t i = 0; i < a.args.size(); ++i) {
            if (!unifyTerm (a.args[i], b.args[i],s)) return false;
        }
        return true;
    }
    
    Atom applySubstitution (const Atom& a, const Substitution& s) {
        Atom r = a;
        for(auto& x: r.args) x = deref(x, s);
        return r;
    }
    
    std::string atomToString (const Atom&a) {
        std::string s = a.predicate + "(";
        for (std::size_t i = 0; i < a.args.size(); ++i) {
            if(i) s += ",";
            s += a.args[i];
        }
        return s + ")";
    
    }
    std::string clauseToString (const Clause& c) {
        if (c.empty()) return "{}";
        std::string s;
        for (std::size_t i = 0; i < c.size(); ++i) {
            if(i) s += " | ";
            s += (c[i].negated ? "~" : "") + atomToString (c[i].atom);
        }
        return s;
    }

    bool simplifyClause (Clause& c) {
        Clause out;
        for (auto& l: c) {
            bool dup = false;
            for (auto& o: out) {
                if (o.negated != l.negated && o.atom.predicate == l.atom.predicate && o.atom.args == l.atom.args) return false;
                if (o.negated == l.negated && o.atom.predicate == l.atom.predicate && o.atom.args == l.atom.args) dup=true;
            }
            if (!dup) out.push_back(l);
        }
        c = std::move(out);
        return true;
    }

    std::string canonicalKey (const Clause& c) {
        Clause v = c;
        std::sort (v.begin(), v.end(), [](auto& a, auto& b) {
            if (a.atom.predicate != b.atom.predicate) return a.atom.predicate < b.atom.predicate;
            if (a.negated != b.negated) return a.negated < b.negated;
            return a.atom.args < b.atom.args;
        });
        std::map <std::string, std::string> m;
        int n = 0;
        for (auto& l: v) {
            for (auto& x: l.atom.args) {
                if (isVariable(x)) {
                    if (!m.count(x)) m[x] = "?v" + std::to_string(n++);
                    x = m[x];
                }
            }
        }
        return clauseToString(v);
    }
}