#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

// Тема 2: Адресная книга (консольная программа)
// Возможности: добавление, удаление, поиск контактов, просмотр всех контактов.
// Данные храним в простом CSV-файле рядом с .exe, чтобы студент мог запускать на Windows без настроек.

struct Contact {
    int id;                 // Уникальный идентификатор контакта
    std::string name;       // Имя
    std::string phone;      // Телефон
    std::string email;      // Email
};

// Имя файла, где храним контакты
static const char* kFileName = "contacts.csv";

// Удаляем ведущие/замыкающие пробелы (простая функция)
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Приводим строку к нижнему регистру (для удобного поиска без учета регистра)
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

// Безопасное чтение строки (включая пробелы)
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return trim(line);
}

// Пытаемся преобразовать строку в int
bool tryParseInt(const std::string& s, int& out) {
    try {
        size_t pos = 0;
        int value = std::stoi(s, &pos);
        if (pos != s.size()) return false; // лишние символы
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

// Экранируем запятые в CSV самым простым способом: если есть запятая или кавычка — берем в кавычки
std::string csvEscape(const std::string& s) {
    bool needQuotes = (s.find(',') != std::string::npos) || (s.find('"') != std::string::npos);
    if (!needQuotes) return s;

    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\""; // двойная кавычка
        else out += c;
    }
    out += "\"";
    return out;
}

// Разбираем одну CSV-строку на поля (поддержка кавычек)
std::vector<std::string> csvSplit(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (inQuotes) {
            if (c == '"') {
                // Если двойные кавычки "" — это символ "
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    i++;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
    }

    fields.push_back(cur);
    return fields;
}

// Загружаем контакты из файла
std::vector<Contact> loadContacts() {
    std::vector<Contact> contacts;

    std::ifstream fin(kFileName);
    if (!fin.is_open()) {
        // Файла может не быть — это нормально при первом запуске
        return contacts;
    }

    std::string line;
    while (std::getline(fin, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::vector<std::string> parts = csvSplit(line);
        if (parts.size() < 4) continue;

        int id = 0;
        if (!tryParseInt(trim(parts[0]), id)) continue;

        Contact c;
        c.id = id;
        c.name = parts[1];
        c.phone = parts[2];
        c.email = parts[3];
        contacts.push_back(c);
    }

    return contacts;
}

// Сохраняем контакты в файл
bool saveContacts(const std::vector<Contact>& contacts) {
    std::ofstream fout(kFileName, std::ios::trunc);
    if (!fout.is_open()) return false;

    for (const auto& c : contacts) {
        // Формат: id,name,phone,email
        fout << c.id << ","
             << csvEscape(c.name) << ","
             << csvEscape(c.phone) << ","
             << csvEscape(c.email) << "\n";
    }
    return true;
}

// Получаем следующий ID (максимальный + 1)
int nextId(const std::vector<Contact>& contacts) {
    int mx = 0;
    for (const auto& c : contacts) mx = std::max(mx, c.id);
    return mx + 1;
}

// Печать одного контакта
void printContact(const Contact& c) {
    std::cout << "ID: " << c.id
              << " | Имя: " << c.name
              << " | Телефон: " << c.phone
              << " | Email: " << c.email << "\n";
}

// Показать все контакты
void showAll(const std::vector<Contact>& contacts) {
    if (contacts.empty()) {
        std::cout << "Список контактов пуст.\n";
        return;
    }
    for (const auto& c : contacts) {
        printContact(c);
    }
}

// Добавить контакт
void addContact(std::vector<Contact>& contacts) {
    Contact c;
    c.id = nextId(contacts);

    c.name = readLine("Введите имя: ");
    c.phone = readLine("Введите телефон: ");
    c.email = readLine("Введите email: ");

    // Мини-проверка: имя не должно быть пустым
    if (c.name.empty()) {
        std::cout << "Имя не может быть пустым. Добавление отменено.\n";
        return;
    }

    contacts.push_back(c);
    std::cout << "Контакт добавлен. ID = " << c.id << "\n";
}

// Удалить контакт по ID
void deleteById(std::vector<Contact>& contacts) {
    std::string s = readLine("Введите ID для удаления: ");
    int id = 0;
    if (!tryParseInt(s, id)) {
        std::cout << "Ошибка: ID должно быть числом.\n";
        return;
    }

    auto it = std::remove_if(contacts.begin(), contacts.end(), [id](const Contact& c) {
        return c.id == id;
    });

    if (it == contacts.end()) {
        std::cout << "Контакт с таким ID не найден.\n";
        return;
    }

    contacts.erase(it, contacts.end());
    std::cout << "Контакт удален.\n";
}

// Поиск по подстроке (имя/телефон/email), без учета регистра для имени/email
void searchContacts(const std::vector<Contact>& contacts) {
    std::string q = readLine("Введите строку для поиска: ");
    if (q.empty()) {
        std::cout << "Пустой запрос.\n";
        return;
    }

    std::string qLower = toLower(q);
    bool found = false;

    for (const auto& c : contacts) {
        // Для телефона ищем как есть (цифры), для имени/email — без регистра
        std::string nameLower = toLower(c.name);
        std::string emailLower = toLower(c.email);

        bool ok =
            (nameLower.find(qLower) != std::string::npos) ||
            (c.phone.find(q) != std::string::npos) ||
            (emailLower.find(qLower) != std::string::npos);

        if (ok) {
            printContact(c);
            found = true;
        }
    }

    if (!found) {
        std::cout << "Ничего не найдено.\n";
    }
}

// Меню
void printMenu() {
    std::cout << "\n=== Адресная книга ===\n";
    std::cout << "1) Показать все контакты\n";
    std::cout << "2) Добавить контакт\n";
    std::cout << "3) Удалить контакт по ID\n";
    std::cout << "4) Поиск контактов\n";
    std::cout << "0) Выход\n";
}

int main() {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

    // Загружаем список контактов из файла при старте
    std::vector<Contact> contacts = loadContacts();

    while (true) {
        printMenu();
        std::string choice = readLine("Выберите пункт: ");

        if (choice == "1") {
            showAll(contacts);
        } else if (choice == "2") {
            addContact(contacts);
            if (!saveContacts(contacts)) {
                std::cout << "Ошибка сохранения в файл.\n";
            }
        } else if (choice == "3") {
            deleteById(contacts);
            if (!saveContacts(contacts)) {
                std::cout << "Ошибка сохранения в файл.\n";
            }
        } else if (choice == "4") {
            searchContacts(contacts);
        } else if (choice == "0") {
            std::cout << "Выход.\n";
            break;
        } else {
            std::cout << "Неизвестный пункт меню.\n";
        }
    }

    return 0;
}
