// Бенчмарк: все методы (исходные, исправленные, вывод из аксиом) на одних и тех же базах.
//
// Сборка и запуск — через CMake из папки extensions (см. extensions/README.md):
//   cmake -S extensions -B build-ext
//   cmake --build build-ext
//   ./build-ext/bench
//
// Каждый метод запускается много раз (пока не наберётся 0,3 с или 2000 запусков),
// печатается среднее время и отклонение. Результаты пишутся ещё и в bench_results.csv.
#include "ext_methods.hpp"
#include "hilbert.hpp"
#include "nlp_parser.hpp"
#include "other_methods.hpp"
#include "resolution.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace fol;

namespace {
    std::ofstream csv;

    std::vector<Clause> load(const std::vector<std::string>& s) {
        std::vector<Clause> kb;
        for (auto& x : s) {
            auto p = nlp::parseKnowledgeSentence(x);
            if (!p.success) std::cerr << "Не разобрано: " << x << "\n";
            for (auto& c : p.clauses) kb.push_back(c);
        }
        return kb;
    }

    // f() возвращает текст результата; замеряется только f()
    void bench(const std::string& base, const std::string& name, const std::function<std::string()>& f,
               const std::string& extra = "") {
        using Clock = std::chrono::steady_clock;
        std::vector<double> t;
        std::string res;
        double total = 0;
        while ((total < 3e5 && t.size() < 2000) || t.size() < 3) {
            auto a = Clock::now();
            res = f();
            auto b = Clock::now();
            double us = std::chrono::duration<double, std::micro>(b - a).count();
            t.push_back(us);
            total += us;
        }
        double m = total / t.size(), sd = 0;
        for (double x : t) sd += (x - m) * (x - m);
        sd = std::sqrt(sd / t.size());
        char num[64];
        snprintf(num, sizeof num, "%11.1f %9.1f %6zu", m, sd, t.size());
        std::cout << "  " << ext::padRight(name, 26) << ext::padRight(res, 16) << num << "  " << extra << "\n";
        csv << '"' << base << "\",\"" << name << "\",\"" << res << "\"," << m << "," << sd << "," << t.size() << ",\"" << extra << "\"\n";
    }

    void run(const std::string& title, const std::vector<std::string>& sentences, const std::string& query) {
        auto kb = load(sentences);
        Atom q;
        std::string why;
        nlp::parseQuery(query, q, why);
        std::cout << "\n" << title << " (" << kb.size() << " клауз), запрос " << ext::atomText(q) << "\n";
        std::cout << "  " << ext::padRight("метод", 26) << ext::padRight("результат", 16)
                  << "  среднее,мкс  откл.  запусков\n";

        auto st = [](ext::Status s) { return std::string(ext::statusText(s)); };
        bench(title, "резолюции (исходные)", [&] { auto r = prove(kb, q, 1000, 20000, false);
            return std::string(r.status == ProofStatus::Proved ? "доказано" : r.status == ProofStatus::LimitReached ? "лимит" : "не доказано"); });
        bench(title, "прямой (исходный)", [&] { auto r = forwardChaining(kb, q);
            return std::string(r.status == SimpleStatus::Proved ? "доказано" : r.status == SimpleStatus::Unsupported ? "не поддерживает" : "не доказано"); });
        bench(title, "таблицы (исходные)", [&] { auto r = tableaux(kb, q);
            return std::string(r.status == SimpleStatus::Proved ? "доказано" : r.status == SimpleStatus::LimitReached ? "лимит" : "не доказано"); });
        bench(title, "резолюции v2 (SOS)", [&] { return st(ext::resolutionSOS(kb, q).status); });
        bench(title, "прямой v2 (с «не»)", [&] { return st(ext::forwardUnit(kb, q).status); });
        bench(title, "таблицы v2 (по клаузам)", [&] { return st(ext::tableauClausal(kb, q).status); });

        // Вывод из аксиом: время поиска вывода из посылок; длина вывода до и после теоремы о дедукции
        auto full = hil::prove(kb, q, 300000);
        std::string extra;
        if (full.status == ext::Status::Proved) {
            extra = "из посылок: " + std::to_string(full.direct.size()) + " строк; без посылок (ТД): "
                  + (full.theoremBuilt ? std::to_string(full.theorem.size()) + " строк" : "> 300000 строк");
        }
        bench(title, "аксиомы + MP", [&] { return st(hil::prove(kb, q, 0).status); }, extra);
    }
}

int main() {
    csv.open("bench_results.csv");
    csv << "база,метод,результат,среднее_мкс,отклонение_мкс,запусков,примечание\n";

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

    // База с «или»: следует только через разбор случаев
    run("База с «или»", {"Все студенты сдавшие экзамен получают зачет.", "Иван является студентом.",
        "Иван сдал экзамен или Анна сдала экзамен.", "Если Анна сдала экзамен, то Иван сдал экзамен."}, "Иван получает зачет?");

    // Рост базы: N студентов и цепочка «если i сдал, то i+1 сдал»
    for (int n : {5, 10, 20, 40, 80}) {
        std::vector<std::string> s = {"Все студенты сдавшие экзамен получают зачет."};
        for (int i = 0; i < n; i++) s.push_back("Студ" + std::to_string(i) + " является студентом.");
        s.push_back("Студ0 сдал экзамен.");
        for (int i = 0; i + 1 < n; i++)
            s.push_back("Если Студ" + std::to_string(i) + " сдал экзамен, то Студ" + std::to_string(i + 1) + " сдал экзамен.");
        run("Цепочка N=" + std::to_string(n), s, "Студ" + std::to_string(n - 1) + " получает зачет?");
    }
    std::cout << "\nРезультаты записаны в bench_results.csv\n";
}
