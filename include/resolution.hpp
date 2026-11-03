#pragma once
#include "logic.hpp"
#include <cstddef>
#include <vector>

namespace fol {
    enum class ProofStatus {
        Proved,
        Saturated,
        LimitReached
    };
    struct ProofResult {
        ProofStatus status;
        std::size_t clausesGenerated = 0, pairsChecked = 0;
    };
    ProofResult prove (const std::vector <Clause>&, const Atom&, std::size_t = 1000, std::size_t = 20000, bool = true);
}
