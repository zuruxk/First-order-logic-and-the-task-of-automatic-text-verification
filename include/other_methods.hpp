#pragma once
#include "logic.hpp"
#include <cstddef>
#include <vector>

namespace fol {
    enum class SimpleStatus {
        Proved,
        NotProved,
        LimitReached,
        Unsupported
    };
    struct SimpleResult {
        SimpleStatus status;
        std::size_t steps = 0;
    };
    SimpleResult forwardChaining (const std::vector <Clause>& kb, const Atom& query, std::size_t maxSteps = 10000);
    SimpleResult tableaux (const std::vector <Clause>& kb, const Atom& query, std::size_t maxNodes = 20000);
}
