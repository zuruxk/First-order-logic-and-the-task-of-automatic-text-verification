// Расширенная версия программы: исходные три метода + исправленные версии + вывод из аксиом.
//
// Запуск:
//   ./text_verification_ext                          — ввод с клавиатуры, как в исходной программе
//   ./text_verification_ext examples/kb2.txt         — база и запрос из файла, все методы сразу
//   ./text_verification_ext examples/kb1.txt 7       — из файла, только метод 7
//
// Файл: предложения базы по одному в строке, пустая строка, затем запрос.
#include "ext_methods.hpp"
#include "hilbert.hpp"
#include "nlp_parser.hpp"
#include "other_methods.hpp"
#include "resolution.hpp"
#include "tokenizer.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace {
    const char* methodNames[] = {
        "",
        "Метод резолюций (исходный)",
        "Прямой вывод (исходный)",
        "Аналитические таблицы (исходные)",
        "Резолюции v2: множество поддержки + ход вывода",
        "Прямой вывод v2: с отрицаниями",
        "Таблицы v2: ветвление по клаузам",
        "Вывод из аксиом: A1-A3, A∀, MP, теорема о дедукции",
    };

    using Clock = chrono::steady_clock;
    double micros(Clock::time_point a, Clock::time_point b) {
        return chrono::duration<double, micro>(b - a).count();
    }

    void printTrace(const vector<string>& t, size_t maxLines = 60) {
        for (size_t i = 0; i < t.size() && i < maxLines; ++i) cout << "  " << t[i] << "\n";
        if (t.size() > maxLines) cout << "  … ещё " << t.size() - maxLines << " строк\n";
    }

    // Запускает один метод. verbose — печатать ход вывода. Возвращает строку для итоговой таблицы.
    string run(int m, const vector<fol::Clause>& kb, const fol::Atom& q, bool verbose) {
        cout << "\n=== " << m << ". " << methodNames[m] << " ===\n";
        string status;
        size_t steps = 0;
        double us = 0;
        if (m == 1) {
            auto t0 = Clock::now();
            auto r = fol::prove(kb, q, 1000, 20000, false);
            auto t1 = Clock::now();
            us = micros(t0, t1);
            steps = r.pairsChecked;
            status = r.status == fol::ProofStatus::Proved ? "доказано"
                   : r.status == fol::ProofStatus::LimitReached ? "лимит" : "не доказано";
            cout << "Проверено пар: " << steps << "\n";
        } else if (m == 2 || m == 3) {
            auto t0 = Clock::now();
            auto r = m == 2 ? fol::forwardChaining(kb, q) : fol::tableaux(kb, q);
            auto t1 = Clock::now();
            us = micros(t0, t1);
            steps = r.steps;
            status = r.status == fol::SimpleStatus::Proved ? "доказано"
                   : r.status == fol::SimpleStatus::LimitReached ? "лимит"
                   : r.status == fol::SimpleStatus::Unsupported ? "не поддерживает" : "не доказано";
            cout << (m == 2 ? "Применений правил: " : "Узлов: ") << steps << "\n";
        } else if (m >= 4 && m <= 6) {
            auto t0 = Clock::now();
            ext::Result r = m == 4 ? ext::resolutionSOS(kb, q, 5000, false)
                          : m == 5 ? ext::forwardUnit(kb, q, false)
                                   : ext::tableauClausal(kb, q, 200000, false);
            auto t1 = Clock::now();
            us = micros(t0, t1);
            steps = r.steps;
            status = ext::statusText(r.status);
            if (verbose) {
                // ход вывода — отдельным запуском, чтобы печать не попала в замер времени
                ext::Result t = m == 4 ? ext::resolutionSOS(kb, q, 5000, true)
                              : m == 5 ? ext::forwardUnit(kb, q, true)
                                       : ext::tableauClausal(kb, q, 200000, true);
                cout << (m == 4 ? "Вывод пустой клаузы:\n" : m == 5 ? "Выведенные факты:\n" : "Дерево:\n");
                printTrace(t.trace);
            }
            cout << (m == 4 ? "Создано резольвент: " : m == 5 ? "Просмотров клауз: " : "Узлов: ") << steps << "\n";
        } else if (m == 7) {
            auto t0 = Clock::now();
            auto r = hil::prove(kb, q, 0);                // только вывод из посылок
            auto t1 = Clock::now();
            us = micros(t0, t1);
            status = ext::statusText(r.status);
            if (r.status != ext::Status::Proved) {
                cout << "Цепочкой MP из посылок запрос не выводится.\n"
                        "(Здесь используются только A∀ и MP, без разбора случаев — см. README.)\n";
            } else {
                steps = r.direct.size();
                cout << "Посылки — предложения базы, клауза L1 ∨ … ∨ Ln записана как ¬L1→(…→Ln):\n";
                if (verbose) printTrace(hil::format(r.direct), 80);
                cout << "Вывод из посылок: " << r.direct.size() << " строк, проверка: "
                     << (r.directChecked ? "верно" : "ОШИБКА") << "\n";
                if (verbose) {
                    auto full = hil::prove(kb, q, 1000000);
                    cout << "\nТеорема о дедукции: снимаем использованные посылки (" << full.usedPremises << " шт.).\n";
                    if (!full.theoremBuilt) {
                        cout << "Вывод без посылок превысил " << full.theoremLimit << " строк — не строим.\n";
                    } else {
                        cout << "Получена теорема (выводится только из аксиом):\n  ⊢ " << hil::show(full.theoremFormula) << "\n";
                        cout << "Вывод: " << full.theorem.size() << " строк, проверка: "
                             << (full.theoremChecked ? "верно" : "ОШИБКА") << "\n";
                        ofstream f("hilbert_proof.txt");
                        f << "Вывод из посылок:\n";
                        for (auto& s : hil::format(full.direct)) f << s << "\n";
                        f << "\nТеорема: ⊢ " << hil::show(full.theoremFormula) << "\nВывод только из аксиом:\n";
                        for (auto& s : hil::format(full.theorem)) f << s << "\n";
                        cout << "Полный вывод записан в hilbert_proof.txt\n";
                    }
                }
            }
        }
        cout << "Результат: " << status << "; время алгоритма: " << us << " мкс\n";
        char num[32];
        snprintf(num, sizeof num, "%12.1f", us);
        return ext::padRight(methodNames[m], 54) + ext::padRight(status, 16) + num;
    }

    bool addSentence(const string& line, vector<fol::Clause>& kb, bool echo) {
        auto p = nlp::parseKnowledgeSentence(line);
        if (!p.success) {
            cout << "Не разобрано: " << line << "\n  " << p.explanation << "\n";
            return false;
        }
        if (echo) {
            cout << "  " << line << "\n";
            for (auto& c : p.clauses) cout << "      " << ext::clauseText(c) << "\n";
        }
        for (auto& c : p.clauses) kb.push_back(c);
        return true;
    }
}

int main(int argc, char** argv) {
    vector<fol::Clause> kb;
    string qtext;
    int method = 0;

    if (argc >= 2) {
        ifstream in(argv[1]);
        if (!in) { cout << "Не удалось открыть файл " << argv[1] << "\n"; return 1; }
        if (argc >= 3) method = atoi(argv[2]);
        string line;
        cout << "База знаний из " << argv[1] << ":\n";
        while (getline(in, line) && !nlp::tokenize(line).empty()) addSentence(line, kb, true);
        while (getline(in, line) && nlp::tokenize(line).empty()) {}
        qtext = line;
    } else {
        cout << "ПРОВЕРКА ТЕКСТА: исходные методы + исправленные + вывод из аксиом\n"
                "Введите предложения базы знаний по одному. Пустая строка завершает ввод.\n";
        string line;
        int n = 1;
        while (true) {
            cout << "Предложение " << n << ": ";
            if (!getline(cin, line)) return 0;
            if (nlp::tokenize(line).empty()) break;
            if (addSentence(line, kb, true)) ++n;
        }
        cout << "Введите проверяемое предложение: ";
        if (!getline(cin, qtext)) return 0;
    }
    if (kb.empty()) { cout << "База знаний пуста.\n"; return 0; }

    fol::Atom q;
    string why;
    if (!nlp::parseQuery(qtext, q, why)) { cout << "Запрос не распознан: " << why << "\n"; return 1; }
    cout << "Запрос: " << qtext << "  ->  " << ext::atomText(q) << "\n";

    if (method == 0 && argc < 2) {
        cout << "\nВыберите метод:\n";
        for (int m = 1; m <= 7; ++m) cout << "  " << m << " - " << methodNames[m] << "\n";
        cout << "  8 - все методы и сравнительная таблица\nВаш выбор: ";
        string c;
        getline(cin, c);
        method = atoi(c.c_str());
    }
    if (method == 0) method = 8;
    if (method < 1 || method > 8) { cout << "Нужно число от 1 до 8.\n"; return 1; }

    if (method != 8) { run(method, kb, q, true); return 0; }

    vector<string> rows;
    for (int m = 1; m <= 7; ++m) rows.push_back(run(m, kb, q, m >= 4));
    cout << "\n=== Сравнение (время только самого алгоритма, один запуск) ===\n";
    cout << ext::padRight("Метод", 54) << ext::padRight("Результат", 16) << "         мкс\n";
    for (auto& r : rows) cout << r << "\n";
    cout << "Для точных замеров используйте bench (много запусков).\n";
    return 0;
}
