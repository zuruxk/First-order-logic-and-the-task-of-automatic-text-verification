// Метод аналитических таблиц для клауз.
//
// Исходный tableaux() после подстановки констант ветвится по значению переменной
// (true / false), то есть фактически это DPLL. Здесь ветвление устроено как в таблицах:
// берётся ещё не выполненная клауза L1 ∨ … ∨ Lk и ветвь делится на k ветвей,
// в каждой из которых добавлен один из литералов (β-правило для «или»).
// Ветвь закрыта, если в ней есть X и ¬X. Все ветви закрыты — запрос следует;
// открытая ветвь — набор фактов, при котором база верна, а запрос ложен (контрпример).
#include "ext_methods.hpp"
#include <cstdlib>
#include <map>

namespace ext {
    namespace {
        // Литерал кодируется числом: +k — атом номер k истинен, -k — ложен.
        struct Search {
            const std::vector<Clause>& clauses;
            const std::vector<std::vector<int>>& coded;
            std::size_t maxNodes;
            bool trace;
            Result& res;
            std::vector<signed char> value;   // 0 — неизвестно, 1 — истина, -1 — ложь

            // 1 — ветвь закрыта, 0 — открыта, -1 — лимит
            int expand(int depth) {
                if (++res.steps > maxNodes) return -1;
                int best = -1;
                std::vector<int> bestOpen;
                std::vector<int> open;
                for (std::size_t ci = 0; ci < coded.size(); ++ci) {
                    bool sat = false;
                    open.clear();
                    for (std::size_t li = 0; li < coded[ci].size(); ++li) {
                        int lit = coded[ci][li];
                        signed char v = value[std::abs(lit)];
                        if (v == 0) open.push_back((int)li);
                        else if ((v > 0) == (lit > 0)) { sat = true; break; }
                    }
                    if (sat) continue;
                    if (open.empty()) {
                        if (trace && res.trace.size() < 300)
                            res.trace.push_back(std::string(2 * depth, ' ') + "✕ закрыта: противоречит клаузе " + clauseText(clauses[ci]));
                        return 1;
                    }
                    if (best < 0 || open.size() < bestOpen.size()) {
                        best = (int)ci;
                        bestOpen = open;
                        if (bestOpen.size() == 1) break;    // единичная клауза — без ветвления
                    }
                }
                if (best < 0) {
                    if (trace && res.trace.size() < 300) res.trace.push_back(std::string(2 * depth, ' ') + "○ открытая ветвь — контрпример");
                    return 0;
                }
                bool split = bestOpen.size() > 1;
                for (int li : bestOpen) {
                    int lit = coded[best][li];
                    value[std::abs(lit)] = lit > 0 ? 1 : -1;
                    if (trace && res.trace.size() < 300)
                        res.trace.push_back(std::string(2 * depth, ' ') + (split ? "├ " : "│ ") + literalText(clauses[best][li])
                                            + (split ? "   (ветвь по " + clauseText(clauses[best]) + ")" : ""));
                    int r = expand(depth + (split ? 1 : 0));
                    value[std::abs(lit)] = 0;
                    if (r != 1) return r;
                }
                return 1;
            }
        };
    }

    Result tableauClausal(const std::vector<Clause>& kb, const Atom& query, std::size_t maxNodes, bool trace) {
        Result res;
        auto clauses = groundAll(kb, constantsOf(kb, query));
        clauses.push_back({Literal{true, query}});
        if (trace) res.trace.push_back("корень: база + " + literalText(Literal{true, query}));
        std::map<std::string, int> ids;
        std::vector<std::vector<int>> coded;
        for (const auto& c : clauses) {
            std::vector<int> row;
            for (const auto& l : c) {
                auto it = ids.emplace(fol::atomToString(l.atom), (int)ids.size() + 1).first;
                row.push_back(l.negated ? -it->second : it->second);
            }
            coded.push_back(row);
        }
        Search s{clauses, coded, maxNodes, trace, res, std::vector<signed char>(ids.size() + 1, 0)};
        int r = s.expand(0);
        res.status = r == 1 ? Status::Proved : r == 0 ? Status::NotProved : Status::LimitReached;
        return res;
    }
}
