#include <iostream>
#include <string>
#include <algorithm>

// Под Windows подключаем windows.h, чтобы работали SetConsoleOutputCP/SetConsoleCP
#ifdef _WIN32
#include <windows.h>
#endif

// Тема 16: Калькулятор дробей (консольная программа)
// Поддержка операций: +, -, *, /
// Ввод чисел: целое "5" или дробь "3/4" (также можно "-2/7").
// Результат всегда сокращается.

long long gcd_ll(long long a, long long b) {
    // НОД (алгоритм Евклида)
    a = std::llabs(a);
    b = std::llabs(b);
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

struct Fraction {
    long long num; // числитель
    long long den; // знаменатель (всегда > 0)

    Fraction(long long n = 0, long long d = 1) : num(n), den(d) {
        normalize();
    }

    void normalize() {
        // Проверка на нулевой знаменатель
        if (den == 0) {
            den = 1; // простая защита, чтобы программа не падала
        }

        // Знаменатель храним положительным
        if (den < 0) {
            den = -den;
            num = -num;
        }

        // Сокращение дроби
        long long g = gcd_ll(num, den);
        if (g != 0) {
            num /= g;
            den /= g;
        }
    }
};

Fraction add(const Fraction& a, const Fraction& b) {
    // a/b + c/d = (ad + cb) / bd
    return Fraction(a.num * b.den + b.num * a.den, a.den * b.den);
}

Fraction sub(const Fraction& a, const Fraction& b) {
    // a/b - c/d = (ad - cb) / bd
    return Fraction(a.num * b.den - b.num * a.den, a.den * b.den);
}

Fraction mul(const Fraction& a, const Fraction& b) {
    // (a/b) * (c/d) = (ac) / (bd)
    return Fraction(a.num * b.num, a.den * b.den);
}

bool isZero(const Fraction& a) {
    return a.num == 0;
}

Fraction divf(const Fraction& a, const Fraction& b) {
    // (a/b) / (c/d) = (a/b) * (d/c)
    return Fraction(a.num * b.den, a.den * b.num);
}

std::string trim(const std::string& s) {
    // Удаление пробелов по краям
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool tryParseInt(const std::string& s, long long& out) {
    // Пробуем преобразовать строку в целое число
    try {
        size_t pos = 0;
        long long val = std::stoll(s, &pos);
        if (pos != s.size()) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseFraction(const std::string& input, Fraction& out) {
    // Ввод может быть:
    // 1) "5"    -> 5/1
    // 2) "3/4"  -> 3/4
    // 3) "-2/7" -> -2/7

    std::string s = trim(input);
    if (s.empty()) return false;

    size_t slashPos = s.find('/');
    if (slashPos == std::string::npos) {
        // Это целое число
        long long n = 0;
        if (!tryParseInt(s, n)) return false;
        out = Fraction(n, 1);
        return true;
    }

    // Это дробь
    std::string left = trim(s.substr(0, slashPos));
    std::string right = trim(s.substr(slashPos + 1));

    long long n = 0, d = 0;
    if (!tryParseInt(left, n)) return false;
    if (!tryParseInt(right, d)) return false;
    if (d == 0) return false; // запрет нулевого знаменателя

    out = Fraction(n, d);
    return true;
}

void printFraction(const Fraction& f) {
    // Если знаменатель 1 — печатаем как целое
    if (f.den == 1) {
        std::cout << f.num;
    } else {
        std::cout << f.num << "/" << f.den;
    }
}

int main() {
#ifdef _WIN32
    // Просим Windows-консоль работать в UTF-8, чтобы русский текст отображался нормально
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "=== Калькулятор дробей (Тема 16) ===\n";
    std::cout << "Вводите числа как 5 или 3/4. Операции: + - * /\n";
    std::cout << "Для выхода введите 0 в меню.\n\n";

    while (true) {
        std::cout << "1) Посчитать выражение\n";
        std::cout << "0) Выход\n";
        std::cout << "Выберите пункт: ";

        std::string menu;
        std::getline(std::cin, menu);
        menu = trim(menu);

        if (menu == "0") {
            std::cout << "Выход.\n";
            break;
        }
        if (menu != "1") {
            std::cout << "Неизвестный пункт меню.\n\n";
            continue;
        }

        std::string aStr, bStr, opStr;

        std::cout << "Введите первое число (a): ";
        std::getline(std::cin, aStr);

        std::cout << "Введите операцию (+ - * /): ";
        std::getline(std::cin, opStr);
        opStr = trim(opStr);

        std::cout << "Введите второе число (b): ";
        std::getline(std::cin, bStr);

        Fraction a, b;
        if (!parseFraction(aStr, a)) {
            std::cout << "Ошибка: неверный формат первого числа.\n\n";
            continue;
        }
        if (!parseFraction(bStr, b)) {
            std::cout << "Ошибка: неверный формат второго числа.\n\n";
            continue;
        }

        Fraction res;

        if (opStr == "+") {
            res = add(a, b);
        } else if (opStr == "-") {
            res = sub(a, b);
        } else if (opStr == "*") {
            res = mul(a, b);
        } else if (opStr == "/") {
            if (isZero(b)) {
                std::cout << "Ошибка: деление на ноль запрещено.\n\n";
                continue;
            }
            res = divf(a, b);
        } else {
            std::cout << "Ошибка: неизвестная операция.\n\n";
            continue;
        }

        std::cout << "Результат: ";
        printFraction(res);
        std::cout << "\n\n";
    }

    return 0;
}
