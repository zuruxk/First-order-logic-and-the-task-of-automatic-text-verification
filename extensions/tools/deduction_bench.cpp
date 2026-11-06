// Замер: вывод с посылками и вывод только из аксиом (после теоремы о дедукции).
// Для каждой базы печатает число строк и среднее время построения.
//   ./build-ext/deduction_bench
#include "hilbert.hpp"
#include "nlp_parser.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace fol;
using Clock = std::chrono::steady_clock;

static std::vector<Clause> load(const std::vector<std::string>& text) {
    std::vector<Clause> kb;
    for (const auto& s : text)
        for (const auto& c : nlp::parseKnowledgeSentence(s).clauses) kb.push_back(c);
    return kb;
}

// Повторяем, пока не наберётся 0,3 с (но не меньше 5 и не больше 2000 раз).
template <class F> static double measure(F f) {
    for (int i = 0; i < 2; ++i) f();
    int runs = 0; double total = 0;
    while ((total < 300000 || runs < 5) && runs < 2000) {
        auto a = Clock::now(); f(); auto b = Clock::now();
        total += std::chrono::duration<double, std::micro>(b - a).count(); ++runs;
    }
    return total / runs;
}

static void run(const char* name, const std::vector<std::string>& text, const char* query) {
    auto kb = load(text);
    Atom q; std::string why;
    nlp::parseQuery(query, q, why);
    const std::size_t limit = 300000;

    auto direct = hil::prove(kb, q, 0);
    auto full = hil::prove(kb, q, limit);
    double tDirect = measure([&] { hil::prove(kb, q, 0); });

    std::printf("%-24s | %6zu | %10.1f | ", name, direct.direct.size(), tDirect);
    if (full.theoremBuilt) {
        double tFull = measure([&] { hil::prove(kb, q, limit); });
        std::printf("%10zu | %10.1f\n", full.theorem.size(), tFull);
    } else {
        std::printf("%10s | %10s\n", "> 300000", "не строим");
    }
}

static std::vector<std::string> chain(int n) {
    std::vector<std::string> s = {"Все студенты сдавшие экзамен получают зачет."};
    for (int i = 0; i < n; ++i) s.push_back("Студ" + std::to_string(i) + " является студентом.");
    s.push_back("Студ0 сдал экзамен.");
    for (int i = 0; i + 1 < n; ++i)
        s.push_back("Если Студ" + std::to_string(i) + " сдал экзамен, то Студ" + std::to_string(i + 1) + " сдал экзамен.");
    return s;
}

int main() {
    std::printf("%-24s | %6s | %10s | %10s | %10s\n", "База", "строк", "мкс", "строк", "мкс");
    std::printf("%-24s | %17s | %23s\n", "", "с посылками", "только из аксиом");
    run("База 1", {"Все студенты, сдавшие экзамен, получают зачёт.", "Иван является студентом и сдал экзамен."},
        "Иван получает зачёт?");
    run("База 2", {"Все студенты сдавшие экзамен получают зачет.", "Иван является студентом.", "Иван сдал экзамен.",
                   "Анна является студентом.", "Пётр является студентом.", "Пётр сдал экзамен.",
                   "Мария является студентом.", "Мария не сдала экзамен.", "Олег является студентом.",
                   "Елена является студентом.", "Если Иван сдал экзамен, то Анна сдала экзамен.",
                   "Если Пётр сдал экзамен, то Олег не сдал экзамен.",
                   "Если Олег не сдал экзамен, то Елена сдала экзамен."},
        "Елена получает зачет?");
    run("Цепочка, 5 студентов", chain(5), "Студ4 получает зачет?");
    run("Цепочка, 10 студентов", chain(10), "Студ9 получает зачет?");
}
