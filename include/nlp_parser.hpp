#pragma once
#include "logic.hpp"
#include <string>
#include <vector>

namespace nlp {
    struct ParseResult {
        bool success = false, isRule = false;
        fol::Clause clause;
        std::vector <fol::Clause> clauses;
        std::string explanation, normalized;
        std::vector <std::string> tokens;
    };
    ParseResult parseKnowledgeSentence (const std::string&);
    bool parseQuery (const std::string&, fol::Atom&, std::string&);
}
