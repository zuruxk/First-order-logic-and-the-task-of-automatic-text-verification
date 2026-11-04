// Замер времени трёх методов: 2000 запусков на каждый пример, среднее и отклонение.
// Сборка из корня проекта:
//   g++ -std=c++17 -O2 -Iinclude bench.cpp src/logic.cpp src/resolution.cpp src/other_methods.cpp src/nlp_parser.cpp src/tokenizer.cpp -o bench
//   ./bench

#include "nlp_parser.hpp"
#include "resolution.hpp"
#include "other_methods.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
using namespace fol;

std::vector<Clause> load(const std::vector<std::string>& s) {
    std::vector<Clause> kb;
    for (auto& x : s) {
        auto p = nlp::parseKnowledgeSentence(x);
        if (!p.success) std::cerr << "Не разобрано: " << x << "\n";
        for (auto& c : p.clauses) kb.push_back(c);
    }
    return kb;
}

template <class F> void bench(const char* name, F f) {
    const int R = 2000;
    std::vector<double> t;
    std::string res;
    for (int r = 0; r < R; r++) {
        auto a = std::chrono::steady_clock::now();
        res = f();
        auto b = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::micro>(b - a).count());
    }
    double m = 0, sd = 0;
    for (double x : t) m += x;
    m /= R;
    for (double x : t) sd += (x - m) * (x - m);
    sd = std::sqrt(sd / R);
    printf("  %-11s %-16s среднее %9.2f мкс  откл. %8.2f\n", name, res.c_str(), m, sd);
}

void run(const char* title, const std::vector<std::string>& s, const std::string& q) {
    auto kb = load(s);
    Atom qa;
    std::string why;
    nlp::parseQuery(q, qa, why);
    printf("%s (%zu клауз), запрос %s\n", title, kb.size(), atomToString(qa).c_str());
    bench("резолюции", [&] { auto r = prove(kb, qa, 1000, 20000, false);
        return std::string(r.status == ProofStatus::Proved ? "доказано" : r.status == ProofStatus::LimitReached ? "лимит" : "нет"); });
    bench("прямой", [&] { auto r = forwardChaining(kb, qa);
        return std::string(r.status == SimpleStatus::Proved ? "доказано" : r.status == SimpleStatus::Unsupported ? "не поддерж." : "нет"); });
    bench("таблицы", [&] { auto r = tableaux(kb, qa);
        return std::string(r.status == SimpleStatus::Proved ? "доказано" : r.status == SimpleStatus::LimitReached ? "лимит" : "нет"); });
}

int main() {
    std::vector<std::string> base = {"Все студенты сдавшие экзамен получают зачет.", "Иван является студентом.", "Иван сдал экзамен.",
        "Анна является студентом.", "Пётр является студентом.", "Пётр сдал экзамен.", "Олег является студентом.",
        "Елена является студентом.", "Если Иван сдал экзамен, то Анна сдала экзамен."};

    run("База 1", {"Все студенты, сдавшие экзамен, получают зачёт.", "Иван является студентом и сдал экзамен."}, "Иван получает зачёт?");

    auto kb2 = base;
    kb2.insert(kb2.begin() + 7, "Мария является студентом.");
    kb2.insert(kb2.begin() + 8, "Мария не сдала экзамен.");
    kb2.push_back("Если Пётр сдал экзамен, то Олег не сдал экзамен.");
    kb2.push_back("Если Олег не сдал экзамен, то Елена сдала экзамен.");
    run("База 2 (с «не»)", kb2, "Елена получает зачет?");

    auto kb3 = base;
    kb3.push_back("Если Пётр сдал экзамен, то Олег сдал экзамен.");
    kb3.push_back("Если Олег сдал экзамен, то Елена сдала экзамен.");
    run("База 2 без «не»", kb3, "Елена получает зачет?");
    run("База 2 без «не», не следует", kb3, "Мария получает зачет?");

    // Рост базы: N студентов и цепочка «если i сдал, то i+1 сдал»
    for (int N : {5, 10, 20, 40}) {
        std::vector<std::string> s = {"Все студенты сдавшие экзамен получают зачет."};
        for (int i = 0; i < N; i++) s.push_back("Студ" + std::to_string(i) + " является студентом.");
        s.push_back("Студ0 сдал экзамен.");
        for (int i = 0; i + 1 < N; i++)
            s.push_back("Если Студ" + std::to_string(i) + " сдал экзамен, то Студ" + std::to_string(i + 1) + " сдал экзамен.");
        char t[64];
        snprintf(t, sizeof t, "Цепочка N=%d", N);
        run(t, s, "Студ" + std::to_string(N - 1) + " получает зачет?");
    }
}
