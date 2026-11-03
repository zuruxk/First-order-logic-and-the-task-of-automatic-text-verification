#include "other_methods.hpp"
#include <algorithm>
#include <functional>
#include <set>
#include <string>

namespace fol {
    namespace {
        bool matchBody (const std::vector <Atom>& body, std::size_t i, const std::vector <Atom>& facts, Substitution subst, const std::function <void(const Substitution&)>& done) {
            if (i == body.size()) {
                done(subst);
                return true;
            }
            bool any = false;
            for (const auto& fact : facts) {
                auto next = subst;
                if (unifyAtoms (body[i], fact, next)) {
                    any = true;
                    matchBody (body, i+1, facts, next, done);
                }
            }
            return any;
        }
        std::string groundKey (const Atom& a) {
            return atomToString(a);
        }
        using Assignment = std::map <std::string, bool>;
        bool literalValue (const Literal& l, const Assignment& a, bool& known) {
            auto it = a.find (groundKey(l.atom));
            known = it != a.end();
            if (!known) return false;
            return l.negated ? !it->second : it->second;
        }
        bool dpll (const std::vector <Clause>& clauses, Assignment a, std::size_t& nodes, std::size_t limit, bool& hitLimit) {
            if (++nodes > limit) {
                hitLimit = true;
                return false;
            }
            bool changed = true;
            while (changed) {
                changed = false;
                for (const auto& c: clauses) {
                    bool satisfied = false; 
                    int open=0;
                    const Literal* last = nullptr;
                    for (const auto& l: c) {
                        bool known = false;
                        bool val = literalValue (l, a, known);
                        if (known&&val) {
                            satisfied = true;
                            break;
                        }
                        if (!known) {
                            ++open;
                            last=&l;
                        }
                    }
                    if (satisfied) continue;
                    if (open == 0) return false;
                    if (open == 1) {
                        a[groundKey(last->atom)] =! last->negated;
                        changed = true;
                    }
                }
            }
            const Literal* choice = nullptr;
            for (const auto& c: clauses) {
                bool sat = false;
                for (const auto& l: c) {
                    bool known = false;
                    if (literalValue(l, a, known) && known) {
                        sat = true;
                        break;
                    }
                }
                if (sat) continue;
                for (const auto& l: c) if (!a.count(groundKey(l.atom))) {
                    choice = &l;
                    break;
                }
                if (choice) break;
            }
            if (!choice) return true;
            auto key = groundKey(choice->atom);
            Assignment left = a;
            left[key] =! choice->negated;
            if (dpll(clauses, left, nodes, limit, hitLimit)) return true;
            Assignment right = a;
            right[key] = choice->negated;
            return dpll (clauses, right, nodes, limit, hitLimit);
        }
    }

    SimpleResult forwardChaining (const std::vector <Clause>& kb, const Atom& query, std::size_t maxSteps) {
        std::vector <Atom> facts;
        std::set <std::string> known;
        std::vector <std::pair <std::vector <Atom>,Atom>> rules;
        for (const auto& c: kb) {
            int positives = 0;
            Atom head;
            std::vector <Atom> body;
            for (const auto& l: c) {
                if (l.negated) body.push_back (l.atom);
                else {
                    ++positives;
                    head = l.atom;
                }
            }
            if (positives != 1) return {SimpleStatus::Unsupported, 0};
            if (body.empty() && !std::any_of (head.args.begin(), head.args.end(), isVariable)) {
                if (known.insert(groundKey(head)).second)facts.push_back(head);
            }
            else rules.push_back({body, head});
        }
        std::size_t steps = 0;
        bool changed = true;
        while (changed) {
            for (const auto& f: facts) {
                Substitution s;
                if (unifyAtoms(f, query, s) && applySubstitution(query, s).args == f.args) return{SimpleStatus::Proved, steps};
            }
            changed = false;
            for (const auto& rule: rules) {
                if (++steps > maxSteps) return {SimpleStatus::LimitReached, steps};
                std::vector <Atom> derived;
                matchBody (rule.first, 0,facts, {}, [&](const Substitution& s) {
                    Atom h = applySubstitution(rule.second, s);
                    if (std::none_of(h.args.begin(), h.args.end(), isVariable)) derived.push_back(h);
                });
                for (const auto& h: derived) if (known.insert(groundKey(h)).second) {
                    facts.push_back(h);
                    changed = true;
                }
            }
        }
        for(const auto& f: facts) {
            Substitution s;
            if (unifyAtoms(f, query, s) && applySubstitution(query, s).args == f.args) return {SimpleStatus::Proved, steps};}
        return {SimpleStatus::NotProved, steps};
    }
    SimpleResult tableaux (const std::vector <Clause>& kb, const Atom& query, std::size_t maxNodes) {
        std::set <std::string> constants;
        auto collect = [&](const Atom& a) {
            for (const auto& x: a.args) if (!isVariable(x)) constants.insert(x);
        };
        for (const auto& c: kb) for (const auto& l: c) collect (l.atom);
        collect(query);
        if (constants.empty()) constants.insert("_domain");
        std::vector <Clause> grounded;
        for (const auto& c: kb) {
            std::vector <std::string> vars;
            for (const auto& l: c) for (const auto& x: l.atom.args) if (isVariable(x) && std::find (vars.begin(), vars.end(), x) == vars.end()) vars.push_back(x);
            std::function <void(std::size_t,Substitution)> expand = [&](std::size_t i, Substitution s) {
                if (i == vars.size()) {
                    Clause g;
                    for (const auto& l: c) {
                        Literal z = l;
                        z.atom = applySubstitution(l.atom,s);
                        g.push_back(z);
                    }
                    grounded.push_back(g);
                    return;
                }
                for(const auto& k: constants) {
                    auto t = s;
                    t[vars[i]] = k;expand(i + 1,t);
                }
            };
            expand(0, {});
        }
        grounded.push_back ({Literal{true, query}});
        std::size_t nodes = 0;
        bool limit = false;
        bool satisfiable = dpll (grounded, {}, nodes, maxNodes, limit);
        if (limit) return {SimpleStatus::LimitReached, nodes};
        return {satisfiable ? SimpleStatus::NotProved : SimpleStatus::Proved , nodes};
    }
}
