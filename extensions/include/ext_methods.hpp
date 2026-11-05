#pragma once
// Дополнения к проекту: исправленные версии методов вывода.
// Исходные файлы проекта (include/, src/) не меняются — здесь только новый код.
#include "logic.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace ext {
    using fol::Atom;
    using fol::Clause;
    using fol::Literal;
    using fol::Substitution;

    enum class Status { Proved, NotProved, LimitReached, Unsupported };
    const char* statusText(Status s);

    struct Result {
        Status status = Status::NotProved;
        std::size_t steps = 0;               // счётчик работы (свой у каждого метода)
        std::vector<std::string> trace;      // ход вывода (заполняется, если trace = true)
    };

    // ---- печать ----
    std::string termText(const std::string& t);     // "?x_12" -> "x"
    std::string atomText(const Atom& a);            // Student(иван)
    std::string literalText(const Literal& l);      // ¬Student(иван)
    std::string clauseText(const Clause& c);        // ¬A ∨ B, пустая клауза -> □
    std::string padRight(const std::string& s, std::size_t width);   // выравнивание с учётом UTF-8

    // ---- подстановка констант вместо переменных (эрбрановские примеры) ----
    std::vector<std::string> constantsOf(const std::vector<Clause>& kb, const Atom& query);
    std::vector<Clause> groundAll(const std::vector<Clause>& kb, const std::vector<std::string>& constants);

    // ---- исправленные методы ----

    // Резолюции со стратегией «множество поддержки» (set of support):
    // резольвируются только пары, в которых хотя бы одна клауза произошла от отрицания запроса,
    // первыми берутся самые короткие клаузы (unit preference). Печатает ход вывода.
    // Полна, если сама база знаний непротиворечива.
    Result resolutionSOS(const std::vector<Clause>& kb, const Atom& query,
                         std::size_t maxClauses = 5000, bool trace = false);

    // Прямой вывод с отрицаниями: известны и положительные, и отрицательные литералы;
    // клауза, в которой все литералы кроме одного ложны, даёт оставшийся литерал.
    // Корректен, но без разбора случаев неполон для баз с «или».
    Result forwardUnit(const std::vector<Clause>& kb, const Atom& query, bool trace = false);

    // Аналитические таблицы для клауз: к базе добавляется ¬запрос,
    // ветвь раскрывается по литералам ещё не выполненной клаузы (β-правило для «или»),
    // ветвь закрыта, если в ней есть X и ¬X.
    Result tableauClausal(const std::vector<Clause>& kb, const Atom& query,
                          std::size_t maxNodes = 200000, bool trace = false);
}
