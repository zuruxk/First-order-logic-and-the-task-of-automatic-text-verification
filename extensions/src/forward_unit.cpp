// Прямой вывод, который понимает отрицания.
//
// Исходный forwardChaining работает только с правилами Хорна (ровно один положительный
// литерал в клаузе) и на базе с «не» отвечает «не поддерживается».
// Здесь запоминаются и положительные, и отрицательные факты. Клауза, в которой все
// литералы кроме одного уже ложны, даёт оставшийся литерал (единичное распространение,
// как unit propagation в DPLL). Для правил Хорна это ровно обычный прямой вывод.
// Метод корректен, но без разбора случаев не выводит всё, что следует из баз с «или».
#include "ext_methods.hpp"
#include <map>

namespace ext {
    Result forwardUnit(const std::vector<Clause>& kb, const Atom& query, bool trace) {
        Result res;
        auto ground = groundAll(kb, constantsOf(kb, query));
        std::map<std::string, bool> value;                // известные литералы
        const std::string goal = fol::atomToString(query);

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& c : ground) {
                ++res.steps;
                bool satisfied = false;
                int unknown = 0;
                const Literal* last = nullptr;
                for (const auto& l : c) {
                    auto it = value.find(fol::atomToString(l.atom));
                    if (it == value.end()) { ++unknown; last = &l; continue; }
                    if (it->second != l.negated) { satisfied = true; break; }
                }
                if (satisfied) continue;
                if (unknown == 0) {
                    // все литералы ложны: база противоречива, из неё следует что угодно
                    if (trace) res.trace.push_back("Противоречие: ложна клауза " + clauseText(c) + " — база противоречива.");
                    res.status = Status::Proved;
                    return res;
                }
                if (unknown == 1) {
                    value[fol::atomToString(last->atom)] = !last->negated;
                    changed = true;
                    if (trace) res.trace.push_back(literalText(*last) + (c.size() == 1 ? "   [факт]" : "   [из " + clauseText(c) + "]"));
                    if (fol::atomToString(last->atom) == goal) {
                        res.status = last->negated ? Status::NotProved : Status::Proved;
                        if (trace && last->negated) res.trace.push_back("Выведено отрицание запроса.");
                        return res;
                    }
                }
            }
        }
        res.status = Status::NotProved;
        if (trace) res.trace.push_back("Новых фактов нет, запрос не выведен.");
        return res;
    }
}
