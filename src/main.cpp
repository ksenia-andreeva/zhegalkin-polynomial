#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <set>
#include <windows.h>
using namespace std;

// ДАЛЬШЕ - узлы дерева, задают логику вычислений
struct ExprNode { // values — текущ знач переменных, varOrder — упорядоч список имён переменных
    virtual bool eval(const vector<bool>& values, const vector<char>& varOrder) const = 0;
    virtual ~ExprNode() = default; 
}; // базовый узел: вирт функц, которую переопред наследники + авто деструктор для очистки

struct VarNode : ExprNode { 
    char name;
    VarNode(char c) : name(c) {} // конструктор, создание объекта 

    bool eval(const vector<bool>& values, const vector<char>& varOrder) const override {
        // поиск индекса переменной в varOrder
        auto it = find(varOrder.begin(), varOrder.end(), name);
        int idx = distance(varOrder.begin(), it);
        return values[idx];
    } 
}; // узел переменной 

struct NotNode : ExprNode {
    unique_ptr<ExprNode> child;
    NotNode(unique_ptr<ExprNode> c) : child(move(c)) {}
    bool eval(const vector<bool>& values, const vector<char>& varOrder) const override {
        return !child->eval(values, varOrder);
    } // полиморфизм eval - единый, работает с разными типами узлов (остальные eval)
}; // узел не

struct AndNode : ExprNode {
    unique_ptr<ExprNode> left, right;
    AndNode(unique_ptr<ExprNode> l, unique_ptr<ExprNode> r)
        : left(move(l)), right(move(r)) {}
    bool eval(const vector<bool>& values, const vector<char>& varOrder) const override {
        return left->eval(values, varOrder) && right->eval(values, varOrder);
    }
}; // узел и

struct OrNode : ExprNode {
    unique_ptr<ExprNode> left, right;
    OrNode(unique_ptr<ExprNode> l, unique_ptr<ExprNode> r)
        : left(move(l)), right(move(r)) {}
    bool eval(const vector<bool>& values, const vector<char>& varOrder) const override {
        return left->eval(values, varOrder) || right->eval(values, varOrder);
    }
}; // узел или

// ДАЛЬШЕ - таблица истиности, объединяет узлы и парсер
class TruthTable {
public:
    explicit TruthTable(const string& expr) {
        set<char> varSet; // динамический массив для сбора переменных
        int pos = 0;
        root = parse_expression(expr, pos, varSet);
        skip_spaces(expr, pos);
        if (pos != expr.size()) {
            throw runtime_error("Лишние символы после выражения");}

        vars.assign(varSet.begin(), varSet.end()); // упорядоченный список переменных
        int n = vars.size();

        table.resize(1 << n); 
        for (int i = 0; i < (1 << n); ++i) {
            vector<bool> values(n);
            for (int j = 0; j < n; ++j) {
                values[j] = (i >> (n - 1 - j)) & 1;
            }
            table[i] = root->eval(values, vars);
        } // заполнение таблицы истинности
    } // запуск парсера, сбор имен переменных, генер комбинаций, заполн табл

    const vector<bool>& get_table() const { return table; }
    const vector<char>& get_variables() const { return vars; } 
    // доступ к приватным таблице и именам переменных

private:
    vector<char> vars; // имена переменных 
    vector<bool> table; // таблица истинности
    unique_ptr<ExprNode> root; // AST (дерево)
    
    // ДАЛЬШЕ - парсер, строит дерево из строки
    void skip_spaces(const string& s, int& i) {
        while (i < s.size() && isspace((unsigned char)s[i])) ++i;
    } // скип пробелов в строке

    unique_ptr<ExprNode> parse_expression(const string& s, int& i, set<char>& varSet) {
        auto left = parse_term(s, i, varSet);
        skip_spaces(s, i);
        while (i < s.size() && s[i] == '+') {
            ++i;
            auto right = parse_term(s, i, varSet);
            left = make_unique<OrNode>(move(left), move(right));
            skip_spaces(s, i);
        } // кидает в след функцию - или
        return left;
    } // основа дерева, получает данные, закидывает в след функцию

    unique_ptr<ExprNode> parse_term(const string& s, int& i, set<char>& varSet) {
        auto left = parse_factor(s, i, varSet);
        skip_spaces(s, i);
        while (i < s.size() &&
               (s[i] == '*' || isalpha(s[i]) || s[i] == '(' || s[i] == '!')) {
            if (s[i] == '*') ++i;
            auto right = parse_factor(s, i, varSet);
            left = make_unique<AndNode>(move(left), move(right));
            skip_spaces(s, i);
        } // кидает в след функцию - и
        return left;
    } // проверяет продолжается ли произведение, закид в след функцию

    unique_ptr<ExprNode> parse_factor(const string& s, int& i, set<char>& varSet) {
        skip_spaces(s, i);
        if (i >= s.size()) throw runtime_error("Неожиданный конец выражения");

        if (s[i] == '!') {
            ++i;
            return make_unique<NotNode>(parse_factor(s, i, varSet));
        } // кидает в след функцию - не
        else if (isalpha(s[i])) {
            char var = s[i];
            ++i;
            varSet.insert(var);   
            return make_unique<VarNode>(var);
        } // запоминает новую переменную
        else if (s[i] == '(') {
            ++i;
            auto inside = parse_expression(s, i, varSet);
            skip_spaces(s, i);
            if (i >= s.size() || s[i] != ')')
                throw runtime_error("Ожидалась ')'");
            ++i;
            return inside;
        } // ошибка не закрыта скобка
        else {
            throw runtime_error(string("Неожиданный символ: ") + s[i]);
        }
    } // добавляет буквы в словарь, подсчитывает слагаемые
};


string tabl(const TruthTable& table)
{
    const auto& vars = table.get_variables();
    const auto& data = table.get_table();
    int n = vars.size();
    int rows = 1<<n;

    vector<int> current;
    for (bool b: data) current.push_back(b ? 1:0);

    vector<string> conjName(rows); // строковые имена переменных
    for (int i=0; i<rows; ++i) {
        if (i==0) { conjName[i]="1";
        } else {
            string conj;
            for (int j=0; j<n; ++j) {
                if (i & (1<<(n-1-j))) {
                    conj+=vars[j];
                }
            }
            conjName[i]=conj;
        }
    }

    cout << "\nТреугольная таблица\n";
    vector<int> coefficients;
    int step=0;
    while (current.size()>1) {
        cout << conjName[step]<< " : ";
        for (int val : current) { 
            cout << val << ' ';}
        cout << "\n";
        coefficients.push_back(current[0]);
        vector<int> next;
        for (size_t i=0; i+1 < current.size(); ++i) {
            next.push_back(current[i] ^ current[i+1]);}
        current = move(next);
        ++step;
    }
    cout << conjName[step] << " : " << current[0] << '\n';
    coefficients.push_back(current[0]);

    string polynom;
    for (int i=0; i<rows; ++i) {
        if (coefficients[i]) {
            if (!polynom.empty()) polynom += " ^ ";
            polynom += (i==0) ? "1" : conjName[i];
        }
    }
    if (polynom.empty()) polynom="0";
    return polynom;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    unique_ptr<TruthTable> tablePtr; // таблица знач функции 
    string expr;
    string polynom = "";
    char c;

    while (true) {
        cout << "\n1. Ввести функцию по которой нужно построить полином.";
        cout << "\n2. Вывести таблицу построения полинома.";
        cout << "\n3. Вывести построенный полином Жегалкина.";
        cout << "\n4. Завершить работу.";
        cout << "\n> ";
        cin >> c;
        cin.ignore();

        switch(c) {
            case '1': {
                cout << "\nВведите функцию (переменные - большие латинские буквы, знаки - '+', '*', '!'): \n";
                getline(cin, expr); 
                try { 
                    tablePtr=make_unique<TruthTable>(expr);
                } catch (const exception& e) {
                    cerr << "\nОшибка: " << e.what() << endl;
                    tablePtr.reset();}
                break;
            }
            case '2': {
                if (tablePtr) {
                    polynom=tabl(*tablePtr);
                } else {
                    cout << "\nСначала введите функцию (пункт 1).\n";
                }
                break;
            }
            case '3': {
                if (polynom!="") {
                    cout << "\nПолином Жегалкина от функции " << expr << ":\n";
                    cout << polynom << "\n";
                } else {
                    cout << "\nСначала постройте таблицу полинома (пункт 2).\n";
                }
                break;
            }
            case '4': return 0;
            default: cout << "\nВводите цифры от 1 до 4!!\n";
        }
    }
}