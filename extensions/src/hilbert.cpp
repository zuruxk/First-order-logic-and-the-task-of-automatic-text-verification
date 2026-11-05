// Вывод запроса из базы знаний в исчислении с аксиомами A1–A3, A∀ и правилом MP,
// затем снятие посылок по теореме о дедукции.
#include "hilbert.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>

namespace hil {
    // ------------------------------------------------------------------ формулы
    // Формулы хранятся без повторов: одинаковые формулы — один и тот же узел.
    // Тогда сравнение — это сравнение номеров, а память не растёт от длинных формул.
    static std::unordered_map<std::string, F>& table() {
        static std::unordered_map<std::string, F> t;
        return t;
    }
    static F intern(Node n) {
        auto& t = table();
        auto it = t.find(n.key);
        if (it != t.end()) return it->second;
        n.id = (int)t.size() + 1;
        auto p = std::make_shared<const Node>(n);
        t.emplace(p->key, p);
        return p;
    }

    F atom(const fol::Atom& a) {
        Node n; n.kind = Node::AtomK; n.atom = a; n.key = "a" + fol::atomToString(a);
        return intern(n);
    }
    F neg(const F& x) {
        Node n; n.kind = Node::NotK; n.a = x; n.key = "n" + std::to_string(x->id);
        return intern(n);
    }
    F imp(const F& x, const F& y) {
        Node n; n.kind = Node::ImpK; n.a = x; n.b = y; n.key = "i" + std::to_string(x->id) + "," + std::to_string(y->id);
        return intern(n);
    }
    F all(const std::string& var, const F& body) {
        Node n; n.kind = Node::AllK; n.var = var; n.a = body; n.key = "f" + var + "," + std::to_string(body->id);
        return intern(n);
    }
    bool same(const F& x, const F& y) { return x.get() == y.get(); }

    static std::string showRec(const F& f, bool top) {
        switch (f->kind) {
            case Node::AtomK: return ext::atomText(f->atom);
            case Node::NotK: return "¬" + showRec(f->a, false);
            case Node::AllK: return "∀" + ext::termText(f->var) + "(" + showRec(f->a, true) + ")";
            case Node::ImpK: {
                std::string s = showRec(f->a, false) + "→" + showRec(f->b, false);
                return top ? s : "(" + s + ")";
            }
        }
        return "?";
    }
    std::string show(const F& f) { return showRec(f, true); }

    F substitute(const F& f, const std::string& var, const std::string& term) {
        switch (f->kind) {
            case Node::AtomK: {
                fol::Atom a = f->atom;
                for (auto& x : a.args) if (x == var) x = term;
                return atom(a);
            }
            case Node::NotK: return neg(substitute(f->a, var, term));
            case Node::ImpK: return imp(substitute(f->a, var, term), substitute(f->b, var, term));
            case Node::AllK: return f->var == var ? f : all(f->var, substitute(f->a, var, term));
        }
        return f;
    }

    static F literalF(const fol::Literal& l) { return l.negated ? neg(atom(l.atom)) : atom(l.atom); }
    static F oppositeF(const fol::Literal& l) { return l.negated ? atom(l.atom) : neg(atom(l.atom)); }

    F clauseToFormula(const fol::Clause& c) {
        F body = literalF(c.back());
        for (int i = (int)c.size() - 2; i >= 0; --i) body = imp(oppositeF(c[i]), body);
        std::vector<std::string> vars;
        for (const auto& l : c)
            for (const auto& x : l.atom.args)
                if (fol::isVariable(x) && std::find(vars.begin(), vars.end(), x) == vars.end()) vars.push_back(x);
        for (int i = (int)vars.size() - 1; i >= 0; --i) body = all(vars[i], body);
        return body;
    }

    // ------------------------------------------------------------------ построитель вывода
    namespace {
        struct Builder {
            Proof lines;
            std::unordered_map<int, int> index;
            std::size_t limit = 0;          // 0 — без лимита
            bool overflow = false;

            int add(Line::Kind k, const F& f, int i = -1, int j = -1, const std::string& note = "") {
                auto it = index.find(f->id);
                if (it != index.end()) return it->second;
                if (limit && lines.size() >= limit) { overflow = true; return -1; }
                lines.push_back({k, f, i, j, note});
                index[f->id] = (int)lines.size() - 1;
                return (int)lines.size() - 1;
            }
            // MP: из строки x (X) и строки xy (X→Y) получаем Y
            int mp(int x, int xy) {
                if (x < 0 || xy < 0) return -1;
                return add(Line::MP, lines[xy].f->b, x, xy);
            }
        };

        // оставить только строки, нужные для строки last, и перенумеровать
        Proof prune(const Proof& p, int last) {
            std::vector<char> need(p.size(), 0);
            need[last] = 1;
            for (int k = last; k >= 0; --k)
                if (need[k] && p[k].kind == Line::MP) need[p[k].i] = need[p[k].j] = 1;
            std::vector<int> num(p.size(), -1);
            Proof out;
            for (int k = 0; k <= last; ++k)
                if (need[k]) {
                    Line l = p[k];
                    if (l.kind == Line::MP) { l.i = num[l.i]; l.j = num[l.j]; }
                    num[k] = (int)out.size();
                    out.push_back(l);
                }
            return out;
        }

        // Теорема о дедукции: вывод с посылкой h превращается в вывод без неё,
        // каждая строка C заменяется строками, которые заканчиваются на h→C.
        bool discharge(const Proof& p, const F& h, std::size_t limit, Proof& out) {
            Builder b;
            b.limit = limit;
            std::vector<int> hc(p.size(), -1);       // номер строки h→C_k в новом выводе
            for (std::size_t k = 0; k < p.size(); ++k) {
                const Line& l = p[k];
                const F& c = l.f;
                if (same(c, h)) {
                    // h→h: 5 строк из A1, A2 и MP
                    F hh = imp(h, h);
                    int a = b.add(Line::Ax1, imp(h, imp(hh, h)));
                    int bb = b.add(Line::Ax2, imp(imp(h, imp(hh, h)), imp(imp(h, hh), hh)));
                    int c3 = b.mp(a, bb);
                    int d = b.add(Line::Ax1, imp(h, hh));
                    hc[k] = b.mp(d, c3);
                } else if (l.kind == Line::MP) {
                    // C получена из X и X→C; уже есть h→X и h→(X→C)
                    const F& x = p[l.i].f;
                    int ax = b.add(Line::Ax2, imp(imp(h, imp(x, c)), imp(imp(h, x), imp(h, c))));
                    int m1 = b.mp(hc[l.j], ax);
                    hc[k] = b.mp(hc[l.i], m1);
                } else {
                    // аксиома или другая посылка: C, C→(h→C) по A1, h→C по MP
                    int c1 = b.add(l.kind, c, -1, -1, l.note);
                    int a1 = b.add(Line::Ax1, imp(c, imp(h, c)));
                    hc[k] = b.mp(c1, a1);
                }
                if (b.overflow) return false;
            }
            out = prune(b.lines, hc[p.size() - 1]);
            return true;
        }

        // разбор посылки после снятия ∀: антецеденты и заключение
        void spine(const F& f, std::vector<F>& ante, F& concl) {
            F cur = f;
            while (cur->kind == Node::ImpK) { ante.push_back(cur->a); cur = cur->b; }
            concl = cur;
        }

        // ---- проверка экземпляров аксиом ----
        bool isImp(const F& f) { return f->kind == Node::ImpK; }
        bool isAx1(const F& f) {               // X→(Y→X)
            return isImp(f) && isImp(f->b) && same(f->a, f->b->b);
        }
        bool isAx2(const F& f) {               // (X→(Y→Z))→((X→Y)→(X→Z))
            if (!isImp(f) || !isImp(f->a) || !isImp(f->a->b) || !isImp(f->b) || !isImp(f->b->a) || !isImp(f->b->b)) return false;
            const F &x = f->a->a, &y = f->a->b->a, &z = f->a->b->b;
            return same(f->b->a->a, x) && same(f->b->a->b, y) && same(f->b->b->a, x) && same(f->b->b->b, z);
        }
        bool isAx3(const F& f) {               // (¬Y→¬X)→((¬Y→X)→Y)
            if (!isImp(f) || !isImp(f->a) || !isImp(f->b) || !isImp(f->b->a)) return false;
            const F &ny = f->a->a, &nx = f->a->b;
            if (ny->kind != Node::NotK || nx->kind != Node::NotK) return false;
            const F &y = ny->a, &x = nx->a;
            return same(f->b->a->a, ny) && same(f->b->a->b, x) && same(f->b->b, y);
        }
        // ∀v B → B[v:=t]: ищем t, сравнивая формулы по структуре
        bool instanceOf(const F& pattern, const std::string& v, const F& f, std::string& t) {
            if (pattern->kind != f->kind) return false;
            switch (pattern->kind) {
                case Node::AtomK: {
                    const auto &a = pattern->atom, &b = f->atom;
                    if (a.predicate != b.predicate || a.args.size() != b.args.size()) return false;
                    for (std::size_t k = 0; k < a.args.size(); ++k) {
                        if (a.args[k] == v) {
                            if (fol::isVariable(b.args[k])) return false;
                            if (t.empty()) t = b.args[k];
                            else if (t != b.args[k]) return false;
                        } else if (a.args[k] != b.args[k]) return false;
                    }
                    return true;
                }
                case Node::NotK: return instanceOf(pattern->a, v, f->a, t);
                case Node::ImpK: return instanceOf(pattern->a, v, f->a, t) && instanceOf(pattern->b, v, f->b, t);
                case Node::AllK: return pattern->var == f->var && instanceOf(pattern->a, v, f->a, t);
            }
            return false;
        }
        bool isAxAll(const F& f) {
            if (!isImp(f) || f->a->kind != Node::AllK) return false;
            std::string t;
            return instanceOf(f->a->a, f->a->var, f->b, t);
        }
    }

    // ------------------------------------------------------------------ поиск вывода
    Result prove(const std::vector<fol::Clause>& kb, const fol::Atom& query, std::size_t maxTheoremLines) {
        Result r;
        for (const auto& c : kb) if (!c.empty()) r.premises.push_back(clauseToFormula(c));
        r.goal = atom(query);
        const auto constants = ext::constantsOf(kb, query);

        Builder b;
        std::unordered_map<int, int> known;      // выведенные литералы -> строка
        int goalLine = -1;

        // Прямой вывод, где каждый шаг записан строками вывода:
        // посылка, (A∀ + MP для каждой переменной), затем MP по каждому антецеденту.
        bool changed = true;
        while (changed && goalLine < 0) {
            changed = false;
            for (const F& p : r.premises) {
                std::vector<std::string> vars;
                F body = p;
                while (body->kind == Node::AllK) { vars.push_back(body->var); body = body->a; }
                std::vector<std::string> pick(vars.size());
                std::function<void(std::size_t)> each = [&](std::size_t vi) {
                    if (goalLine >= 0) return;
                    if (vi < vars.size()) {
                        for (const auto& k : constants) { pick[vi] = k; each(vi + 1); }
                        return;
                    }
                    F g = body;
                    for (std::size_t v = 0; v < vars.size(); ++v) g = substitute(g, vars[v], pick[v]);
                    std::vector<F> ante; F concl;
                    spine(g, ante, concl);
                    if (known.count(concl->id)) return;
                    for (const F& a : ante) if (!known.count(a->id)) return;
                    int cur = b.add(Line::Premise, p);
                    F f = p;
                    for (std::size_t v = 0; v < vars.size(); ++v) {
                        F inst = substitute(f->a, vars[v], pick[v]);
                        int ax = b.add(Line::AxAll, imp(f, inst), -1, -1, ext::termText(vars[v]) + " := " + pick[v]);
                        cur = b.mp(cur, ax);
                        f = inst;
                    }
                    for (const F& a : ante) cur = b.mp(known[a->id], cur);
                    known[concl->id] = cur;
                    changed = true;
                    if (same(concl, r.goal)) goalLine = cur;
                };
                each(0);
                if (goalLine >= 0) break;
            }
        }
        if (goalLine < 0) { r.status = ext::Status::NotProved; return r; }

        r.status = ext::Status::Proved;
        r.direct = prune(b.lines, goalLine);
        r.directChecked = check(r.direct, r.premises, r.goal);

        // Посылки, которые реально использованы, в порядке появления
        std::vector<F> used;
        for (const auto& l : r.direct) if (l.kind == Line::Premise) used.push_back(l.f);
        r.usedPremises = used.size();
        if (maxTheoremLines == 0) return r;
        r.theoremLimit = maxTheoremLines;

        // Теорема о дедукции: снимаем посылки с последней к первой
        Proof cur = r.direct;
        F formula = r.goal;
        for (int k = (int)used.size() - 1; k >= 0; --k) {
            Proof next;
            if (!discharge(cur, used[k], maxTheoremLines, next)) return r;   // превышен лимит
            cur = std::move(next);
            formula = imp(used[k], formula);
        }
        r.theoremBuilt = true;
        r.theoremFormula = formula;
        r.theorem = std::move(cur);
        r.theoremChecked = check(r.theorem, {}, r.theoremFormula);
        return r;
    }

    bool check(const Proof& p, const std::vector<F>& premises, const F& goal) {
        if (p.empty() || !same(p.back().f, goal)) return false;
        std::set<int> allowed;
        for (const auto& f : premises) allowed.insert(f->id);
        for (std::size_t k = 0; k < p.size(); ++k) {
            const Line& l = p[k];
            bool ok = false;
            switch (l.kind) {
                case Line::Premise: ok = allowed.count(l.f->id) > 0; break;
                case Line::Ax1: ok = isAx1(l.f); break;
                case Line::Ax2: ok = isAx2(l.f); break;
                case Line::Ax3: ok = isAx3(l.f); break;
                case Line::AxAll: ok = isAxAll(l.f); break;
                case Line::MP:
                    ok = l.i >= 0 && l.j >= 0 && l.i < (int)k && l.j < (int)k
                         && isImp(p[l.j].f) && same(p[l.j].f->a, p[l.i].f) && same(p[l.j].f->b, l.f);
                    break;
            }
            if (!ok) return false;
        }
        return true;
    }

    std::vector<std::string> format(const Proof& p) {
        std::vector<std::string> out;
        for (std::size_t k = 0; k < p.size(); ++k) {
            const Line& l = p[k];
            std::string why;
            switch (l.kind) {
                case Line::Premise: why = "посылка"; break;
                case Line::Ax1: why = "A1"; break;
                case Line::Ax2: why = "A2"; break;
                case Line::Ax3: why = "A3"; break;
                case Line::AxAll: why = "A∀, " + l.note; break;
                case Line::MP: why = "MP " + std::to_string(l.i + 1) + ", " + std::to_string(l.j + 1); break;
            }
            out.push_back(std::to_string(k + 1) + ". " + show(l.f) + "   [" + why + "]");
        }
        return out;
    }
}
