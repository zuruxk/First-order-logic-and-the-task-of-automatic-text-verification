#pragma once
#include <map>
#include <string>
#include <vector>

namespace fol {
    struct Atom {
        std::string predicate;
        std::vector <std::string> args;
    };
    struct Literal {
        bool negated = false;
        Atom atom;
    };
    using Clause = std::vector <Literal>;
    using Substitution = std::map <std::string, std::string>;
    bool isVariable (const std::string&);
    bool unifyAtoms (const Atom&, const Atom&, Substitution&);
    Atom applySubstitution (const Atom&, const Substitution&);
    std::string atomToString (const Atom&);
    std::string clauseToString (const Clause&);
    bool simplifyClause (Clause&);
    std::string canonicalKey (const Clause&);
}
