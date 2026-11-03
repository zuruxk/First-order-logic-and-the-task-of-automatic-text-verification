#include "resolution.hpp"
#include <iostream>
#include <map>
#include <set>

namespace fol {
    namespace {
        unsigned long fresh = 0;
        Clause standardize (const Clause& c) {
            Clause r = c;
            std::map <std::string, std::string> m;
            auto suffix = "_" + std::to_string (++fresh);
            for (auto& l: r) for (auto& x: l.atom.args) if (isVariable(x)) {
                if (!m.count(x)) m[x] = x + suffix;
                x = m[x];
            }
            return r;
        }
        std::vector <Clause> resolve (const Clause& a0, const Clause& b0) {
            auto a = standardize (a0), b = standardize (b0);
            std::vector <Clause> out;
            for (std::size_t i=0; i < a.size(); ++i) for (std::size_t j =0; j < b.size(); ++j) {
                if (a[i].negated == b[j].negated) continue;
                Substitution sub;
                if (!unifyAtoms (a[i].atom, b[j].atom,sub)) continue;
                Clause r;
                for (std::size_t k = 0; k < a.size(); ++k) if (k!=i) {
                    auto l = a[k];
                    l.atom = applySubstitution (l.atom, sub);
                    r.push_back(l);
                }
                for (std::size_t k = 0; k < b.size(); ++k) if (k != j) {
                    auto l = b[k];
                    l.atom = applySubstitution (l.atom, sub);
                    r.push_back(l);
                }
                if (simplifyClause(r)) out.push_back (std::move(r));
            }
            return out;
        }
    }
    ProofResult prove (const std::vector <Clause>& kb, const Atom& q, std::size_t maxC, std::size_t maxP, bool verbose) {
        std::vector <Clause> cs = kb;
        cs.push_back ({{true, q}});
        std::set <std::string> seen;
        std::vector <Clause> unique;
        for (auto c: cs) if (simplifyClause(c) && seen.insert (canonicalKey(c)).second) unique.push_back(c);
        cs = std::move(unique);
        if (verbose) {
            std::cout<<"Клаузальная форма + отрицание запроса:\n";
            for (std::size_t i = 0; i < cs.size(); ++i) std::cout<<"C"<<i+1<<": "<<clauseToString (cs[i])<<"\n";
        }
        std::size_t pairs = 0;
        for (std::size_t i = 0; i < cs.size(); ++i) for (std::size_t j = i + 1; j < cs.size(); ++j) {
            if (++pairs > maxP) return {ProofStatus::LimitReached, cs.size(),pairs};
            for (auto& r: resolve(cs[i], cs[j])) {
                if (verbose) std::cout<<"Резольвента: "<<clauseToString(r)<<"\n";
                if (r.empty()) return {ProofStatus::Proved, cs.size(), pairs};
                if (seen.insert(canonicalKey(r)).second) {
                    cs.push_back(r);
                    if (cs.size() > maxC) return {ProofStatus::LimitReached, cs.size(), pairs};
                }
            }
        }
        return {ProofStatus::Saturated, cs.size(), pairs};
    }
}
