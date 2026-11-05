// Метод резолюций со стратегией «множество поддержки» и предпочтением коротких клауз.
//
// Чем отличается от исходного src/resolution.cpp:
//  * исходный перебирает все пары клауз подряд, поэтому число пар растёт квадратично
//    и на цепочке из 20 студентов упирается в лимит 20 000 пар;
//  * здесь резольвируются только пары, где хотя бы одна клауза «произошла» от отрицания
//    запроса (set of support), и первыми берутся самые короткие клаузы (unit preference);
//  * запоминается, из каких клауз получена каждая резольвента, поэтому можно напечатать
//    сам вывод пустой клаузы с подстановками (унификацией).
#include "ext_methods.hpp"
#include <map>
#include <set>

namespace ext {
    namespace {
        struct Node {
            Clause clause;
            int p1 = -1, p2 = -1;     // родители (-1 — клауза базы или ¬запрос)
            std::string how;          // "база", "¬запрос" или подстановка
        };

        unsigned long renameCounter = 0;

        Clause renameVars(const Clause& c) {
            Clause r = c;
            std::map<std::string, std::string> m;
            std::string suffix = "_" + std::to_string(++renameCounter);
            for (auto& l : r)
                for (auto& x : l.atom.args)
                    if (fol::isVariable(x)) {
                        if (!m.count(x)) m[x] = x.substr(0, x.find('_')) + suffix;
                        x = m[x];
                    }
            return r;
        }

        std::string substText(const Substitution& s) {
            std::string out;
            for (const auto& kv : s) {
                Atom probe{"", {kv.first}};
                std::string v = fol::applySubstitution(probe, s).args[0];
                if (fol::isVariable(v)) continue;
                if (!out.empty()) out += ", ";
                out += termText(kv.first) + " := " + termText(v);
            }
            return out;
        }

        // Сопоставление: можно ли подставить что-то вместо переменных c, чтобы литерал стал равен l
        bool matchAtom(const Atom& pattern, const Atom& target, Substitution& s) {
            if (pattern.predicate != target.predicate || pattern.args.size() != target.args.size()) return false;
            for (std::size_t k = 0; k < pattern.args.size(); ++k) {
                const std::string& p = pattern.args[k];
                if (fol::isVariable(p)) {
                    auto it = s.find(p);
                    if (it == s.end()) s[p] = target.args[k];
                    else if (it->second != target.args[k]) return false;
                } else if (p != target.args[k]) return false;
            }
            return true;
        }

        // c поглощает d, если после подстановки все литералы c есть в d (тогда d ничего не добавляет)
        bool subsumes(const Clause& c, const Clause& d, std::size_t i, Substitution s) {
            if (i == c.size()) return true;
            for (const auto& l : d) {
                if (l.negated != c[i].negated) continue;
                Substitution t = s;
                if (matchAtom(c[i].atom, l.atom, t) && subsumes(c, d, i + 1, t)) return true;
            }
            return false;
        }

        struct Resolvent { Clause clause; std::string subst; };

        std::vector<Resolvent> resolve(const Clause& a0, const Clause& b0) {
            Clause a = renameVars(a0), b = renameVars(b0);
            std::vector<Resolvent> out;
            for (std::size_t i = 0; i < a.size(); ++i)
                for (std::size_t j = 0; j < b.size(); ++j) {
                    if (a[i].negated == b[j].negated) continue;
                    Substitution s;
                    if (!fol::unifyAtoms(a[i].atom, b[j].atom, s)) continue;
                    Clause r;
                    for (std::size_t k = 0; k < a.size(); ++k)
                        if (k != i) { Literal l = a[k]; l.atom = fol::applySubstitution(l.atom, s); r.push_back(l); }
                    for (std::size_t k = 0; k < b.size(); ++k)
                        if (k != j) { Literal l = b[k]; l.atom = fol::applySubstitution(l.atom, s); r.push_back(l); }
                    out.push_back({r, substText(s)});
                }
            return out;
        }
    }

    Result resolutionSOS(const std::vector<Clause>& kb, const Atom& query, std::size_t maxClauses, bool trace) {
        Result res;
        std::vector<Node> nodes;
        std::set<std::string> seen;
        std::vector<int> usable;                         // обработанные клаузы
        std::set<std::pair<std::size_t, int>> sos;       // (длина, номер) — короткие первыми

        for (auto c : kb) {
            if (!fol::simplifyClause(c)) continue;       // тавтология
            if (!seen.insert(fol::canonicalKey(c)).second) continue;
            nodes.push_back({c, -1, -1, "база"});
            usable.push_back((int)nodes.size() - 1);
        }
        Clause negQ{Literal{true, query}};
        nodes.push_back({negQ, -1, -1, "¬запрос"});
        seen.insert(fol::canonicalKey(negQ));
        sos.insert({1, (int)nodes.size() - 1});

        auto buildTrace = [&](int last) {
            std::set<int> need;
            std::vector<int> stack{last};
            while (!stack.empty()) {
                int v = stack.back(); stack.pop_back();
                if (v < 0 || need.count(v)) continue;
                need.insert(v);
                stack.push_back(nodes[v].p1);
                stack.push_back(nodes[v].p2);
            }
            std::map<int, int> num;
            int n = 0;
            for (int v : need) num[v] = ++n;
            for (int v : need) {
                const Node& nd = nodes[v];
                std::string why = nd.p1 < 0 ? nd.how
                    : "из " + std::to_string(num[nd.p1]) + " и " + std::to_string(num[nd.p2]) + (nd.how.empty() ? "" : "; " + nd.how);
                res.trace.push_back(std::to_string(num[v]) + ". " + clauseText(nd.clause) + "   [" + why + "]");
            }
        };

        while (!sos.empty()) {
            int given = sos.begin()->second;
            sos.erase(sos.begin());
            for (int u : usable) {
                for (auto& r : resolve(nodes[given].clause, nodes[u].clause)) {
                    ++res.steps;
                    if (r.clause.empty()) {
                        nodes.push_back({r.clause, u, given, r.subst});
                        if (trace) buildTrace((int)nodes.size() - 1);
                        res.status = Status::Proved;
                        return res;
                    }
                    if (!fol::simplifyClause(r.clause)) continue;
                    if (!seen.insert(fol::canonicalKey(r.clause)).second) continue;
                    bool redundant = false;                  // поглощается уже имеющейся клаузой
                    for (const auto& nd : nodes)
                        if (nd.clause.size() <= r.clause.size() && subsumes(nd.clause, r.clause, 0, {})) { redundant = true; break; }
                    if (redundant) continue;
                    nodes.push_back({r.clause, u, given, r.subst});
                    sos.insert({r.clause.size(), (int)nodes.size() - 1});
                    if (nodes.size() > maxClauses) { res.status = Status::LimitReached; return res; }
                }
            }
            usable.push_back(given);
        }
        res.status = Status::NotProved;
        if (trace) res.trace.push_back("Новых клауз нет: из отрицания запроса противоречие не выводится.");
        return res;
    }
}
