#pragma once
// Вывод в исчислении с аксиомами (как в ЛЗ2), перенесённый на базу знаний проекта.
//
// Аксиомы:  A1. X→(Y→X)
//           A2. (X→(Y→Z))→((X→Y)→(X→Z))
//           A3. (¬Y→¬X)→((¬Y→X)→Y)
//           A∀. ∀x F(x) → F(t)        (аксиома исчисления предикатов, 4° в лекции)
// Правило:  MP: X, X→Y ⊢ Y
//
// 1) Строится вывод запроса из посылок (предложений базы знаний) только по A∀ и MP.
// 2) По теореме о дедукции посылки снимаются одна за другой, и получается вывод
//    ⊢ γ1→(γ2→(…→запрос)) только из аксиом — как в задании 2 ЛЗ2.
#include "ext_methods.hpp"
#include <memory>
#include <string>
#include <vector>

namespace hil {
    struct Node;
    using F = std::shared_ptr<const Node>;

    struct Node {
        enum Kind { AtomK, NotK, ImpK, AllK } kind;
        fol::Atom atom;          // для AtomK
        std::string var;         // для AllK (имя переменной вида "?x")
        F a, b;                  // NotK: a;  ImpK: a→b;  AllK: ∀var a
        int id = 0;              // одинаковые формулы — один и тот же узел с одним номером
        std::string key;         // короткая запись через номера подформул
    };

    F atom(const fol::Atom& a);
    F neg(const F& x);
    F imp(const F& x, const F& y);
    F all(const std::string& var, const F& body);
    bool same(const F& x, const F& y);
    std::string show(const F& f);                               // ¬, →, ∀x
    F substitute(const F& f, const std::string& var, const std::string& term);

    // Клауза l1 ∨ … ∨ ln  ->  ¬l1 → (¬l2 → … → ln), с ∀ по переменным (как A∨B = ¬A→B в ЛЗ2)
    F clauseToFormula(const fol::Clause& c);

    struct Line {
        enum Kind { Premise, Ax1, Ax2, Ax3, AxAll, MP } kind;
        F f;
        int i = -1, j = -1;      // для MP: строка X и строка X→Y
        std::string note;        // для A∀: "x := иван"
    };
    using Proof = std::vector<Line>;

    struct Result {
        ext::Status status = ext::Status::NotProved;
        std::vector<F> premises;     // все посылки (база знаний)
        F goal;
        Proof direct;                // вывод запроса из посылок (A∀ + MP)
        bool directChecked = false;
        std::size_t usedPremises = 0;

        bool theoremBuilt = false;   // удалось ли снять все посылки (не превышен лимит)
        F theoremFormula;            // γ1→(γ2→…→запрос)
        Proof theorem;               // вывод только из аксиом
        bool theoremChecked = false;
        std::size_t theoremLimit = 0;
    };

    // maxTheoremLines = 0 — не строить вывод без посылок (только вывод из посылок).
    Result prove(const std::vector<fol::Clause>& kb, const fol::Atom& query,
                 std::size_t maxTheoremLines = 1000000);

    // Независимая проверка: каждая строка — посылка, экземпляр аксиомы или MP из предыдущих.
    bool check(const Proof& p, const std::vector<F>& premises, const F& goal);

    std::vector<std::string> format(const Proof& p);
}
