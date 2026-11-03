#include "nlp_parser.hpp"
#include "other_methods.hpp"
#include "resolution.hpp"
#include "tokenizer.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace fol;

int main() {
    cout<<"СРАВНЕНИЕ МЕТОДОВ ЛОГИЧЕСКОГО ВЫВОДА (учебная NLP-демонстрация)\n";
    cout<<"Введите предложения базы знаний по одному. Пустая строка завершает ввод.\n";
    vector <Clause> kb;
    string line;
    int n = 1;
    while (true) {
        cout<<"Предложение "<<n<<": ";
        if (!getline(cin, line)) return 0;
        if (nlp::tokenize(line).empty()) break;
        auto p = nlp::parseKnowledgeSentence(line);
        cout<<"Токены: ";
        for (auto& t: p.tokens) cout<<"["<<t<<"] ";
        cout<<"\n";
        if (!p.success) {
            cout<<"Не разобрано: "<<p.explanation<<"\n";
            continue;
        }
        cout<<"Анализ: "<<p.explanation<<"\nЛогическая форма (КНФ):\n";
        for (std::size_t i = 0; i < p.clauses.size(); ++i) cout<<"  "<<clauseToString(p.clauses[i])<<"\n";
        for (const auto& c: p.clauses) kb.push_back(c);
        ++n;
    }
    if (kb.empty()) {
        cout<<"База знаний пуста.\n";
        return 0;
    }
    cout<<"Введите проверяемое предложение: ";
    string qtext;
    if (!getline(cin, qtext)) return 0;
    Atom q;
    string why;
    if (!nlp::parseQuery(qtext, q, why)) {
        cout<<"Запрос не распознан: "<<why<<"\n";
        return 1;
    }
    cout<<"Запрос преобразован: "<<atomToString(q)<<"\n\n";
    cout<<"Выберите метод:\n1 - Метод резолюций\n2 - Прямой логический вывод\n3 - Метод аналитических таблиц\nВаш выбор: ";
    string choice;
    getline(cin, choice);
    auto start = chrono::steady_clock::now();
    bool proved = false, limited = false, unsupported = false;
    size_t steps = 0;
    if (choice == "1") {
        cout<<"\n   Метод резолюций\n";
        cout<<"Лимит клауз [1000], Enter - по умолчанию: ";
        string v;
        getline (cin, v);
        size_t mc = v.empty() ? 1000 : stoull(v);
        cout<<"Лимит пар [20000], Enter - по умолчанию: ";
        getline (cin, v);
        size_t mp = v.empty() ? 20000 : stoull(v);
        start = chrono::steady_clock::now();
        auto r = prove (kb, q, mc, mp, false);
        proved = r.status == ProofStatus::Proved;
        limited = r.status == ProofStatus::LimitReached;
        steps = r.pairsChecked;
        if (r.status == ProofStatus::Saturated) cout<<"Доказательство не найдено при насыщении множества клауз.\n";
        cout<<"Создано клауз: "<<r.clausesGenerated<<"; проверено пар: "<<r.pairsChecked<<"\n";
    }
    else if (choice == "2") {
        cout<<"\n   Прямой логический вывод\n";
        auto r = forwardChaining (kb, q);
        proved = r.status == SimpleStatus::Proved;
        limited = r.status == SimpleStatus::LimitReached;
        steps = r.steps;
        cout<<"Проверено применений правил: "<<r.steps<<"\n";
        if (r.status == SimpleStatus::Unsupported) {
            unsupported = true;
            cout<<"ВНИМАНИЕ: прямой вывод поддерживает только положительные факты и правила Хорна; для дизъюнкций/отрицательных фактов выберите резолюции или таблицы.\n";
        }
    }
    else if (choice == "3") {
        cout<<"\n   Метод аналитических таблиц\n";
        auto r = tableaux(kb, q);
        proved = r.status == SimpleStatus::Proved;
        limited = r.status == SimpleStatus::LimitReached;
        steps = r.steps;
        cout<<"Обработано узлов таблицы: "<<r.steps<<"\n";
    }
    else {
        cout<<"Неизвестный выбор. Запустите программу ещё раз и выберите 1, 2 или 3.\n";
        return 1;
    }
    auto finish = chrono::steady_clock::now();
    auto micros = chrono::duration_cast <chrono::microseconds>(finish-start).count();
    cout<<"\nРЕЗУЛЬТАТ: ";
    if (proved) cout<<"ДОКАЗАНО - запрос логически следует из введённой базы знаний.\n";
    else if (unsupported) cout<<"МЕТОД НЕ ПОДДЕРЖИВАЕТ ТАКОЙ ВИД ФОРМУЛ. Используйте резолюции или аналитические таблицы.\n";
    else if (limited) cout<<"Достигнут лимит поиска; результат не определён.\n";
    else cout<<"Доказательство не найдено данным методом в рамках его реализованного поиска. Это не означает, что утверждение ложно в реальном мире.\n";
    cout<<"Время работы: "<<micros<<" мкс ("<<(micros/1000.0)<<" мс).\n";
    cout<<"Счётчик шагов/узлов (зависит от метода): "<<steps<<"\n";
    cout<<"Время включает работу выбранного алгоритма, но не ввод и NLP-разбор предложений.\n";
}
