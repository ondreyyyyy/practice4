#include "interface.h"
#include "structures.h"
#include "file.h"
#include "lockingtable.h"
#include "insert.h"
#include "select.h"
#include "delete.h"
#include "auxiliary.h"
#include "Vector.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

Vector<Condition> parseConditions(const string& where) {
    Vector<Condition> result;
    if (where.empty()) {
        return result;
    }

    string cur;
    bool inStr = false;
    Vector<string> tokens;

    for (size_t i = 0; i < where.size(); i++) {
        char c = where[i];

        if (c == '\'') {
            cur += c;
            inStr = !inStr;
            continue;
        }

        if (inStr) {
            cur += c;
            continue;
        }

        if (c == '(' || c == ')') {
            if (!cur.empty()) { 
                tokens.push_back(cur); 
                cur.clear(); 
            }
            tokens.push_back(string(1, c));
            continue;
        }

        if (isspace((unsigned char)c)) {
            if (!cur.empty()) { 
                tokens.push_back(cur);
                cur.clear(); 
            }
            continue;
        }

        cur += c;
    }

    if (!cur.empty()) {
        tokens.push_back(cur);
    }

    for (size_t i = 0; i < tokens.get_size(); i++) {
        string tk = tokens[i];

        if (tk == "(" || tk == ")") {
            Condition c; 
            c.column = tk; 
            result.push_back(c);
            continue;
        }

        if (tk == "AND" || tk == "OR") {
            Condition c; 
            c.logicalOperator = tk; 
            result.push_back(c);
            continue;
        }

        if (i + 2 < tokens.get_size() && tokens[i + 1] == "=") {
            Condition c;
            c.column = tokens[i];
            string right = tokens[i + 2];

            if (right.size() >= 2 && right.front() == '\'' && right.back() == '\'') {
                right = right.substr(1, right.size() - 2);
            }

            c.value = right;
            result.push_back(c);
            i += 2;
            continue;
        }
    }

    return result;
}

void executeInsert(DatabaseManager& dbManager, const string& command) {
    stringstream ss(command);

    string kw1, kw2, tableName, kw3;

    if (!(ss >> kw1) || kw1 != "INSERT") {
        cout << "Ошибка: ожидается INSERT\n";
        return;
    }

    if (!(ss >> kw2) || kw2 != "INTO") {
        cout << "Ошибка: ожидается INTO\n";
        return;
    }

    if (!(ss >> tableName)) {
        cout << "Ошибка: отсутствует имя таблицы\n";
        return;
    }

    if (!(ss >> kw3) || kw3 != "VALUES") {
        cout << "Ошибка: ожидается VALUES\n";
        return;
    }

    string valuesRaw;
    getline(ss, valuesRaw);

    while (!valuesRaw.empty() && isspace(valuesRaw.front())) {
        valuesRaw.erase(valuesRaw.begin());
    }
    while (!valuesRaw.empty() && isspace(valuesRaw.back())) {
        valuesRaw.pop_back();
    }

    if (valuesRaw.empty()) {
        cout << "Ошибка: отсутствует список значений\n";
        return;
    }

    if (valuesRaw.front() != '(' || valuesRaw.back() != ')') {
        cout << "Ошибка: VALUES должны быть в формате ( ... )\n";
        return;
    }

    valuesRaw = valuesRaw.substr(1, valuesRaw.size() - 2);

    if (!lockTable(dbManager, tableName)) {
        cout << "Ошибка: Таблица '" << tableName << "' заблокирована\n";
        return;
    }

    try {
        string pkPath = dbManager.getSchemaName() + "/" + tableName + "/" + tableName + "_pk_sequence";
        ifstream pkFile(pkPath);
        int pk = 1;
        if (pkFile.is_open()) {
            pkFile >> pk;
            pkFile.close();
        }

        insertData(dbManager, tableName, valuesRaw, pk);
    }
    catch (const exception& e) {
        cout << "Ошибка при вставке: " << e.what() << endl;
    }

    unlockTable(dbManager, tableName);
}

void trimLeft(string& s) {
    while (!s.empty() && isspace(s.front())) {
        s.erase(s.begin());
    }
}

void trimRight(string& s) {
    while (!s.empty() && isspace(s.back())) {
        s.pop_back();
    }
}

void trim(string& s) {
    trimLeft(s);
    trimRight(s);
}

void executeSelect(DatabaseManager& dbManager, const string& command) {
    stringstream ss(command);

    string kw1;
    if (!(ss >> kw1) || kw1 != "SELECT") {
        cout << "Ошибка: ожидается SELECT\n";
        return;
    }

    Vector<string> selectColumns;
    {
        string token;
        string buffer;

        while (ss >> token) {
            if (token == "FROM")
                break;

            if (!buffer.empty())
                buffer += " ";
            buffer += token;
        }

        if (token != "FROM") {
            cout << "Ошибка: отсутствует FROM\n";
            return;
        }

        string col;
        stringstream colss(buffer);
        while (getline(colss, col, ',')) {
            trim(col);
            if (!col.empty())
                selectColumns.push_back(col);
        }

        if (selectColumns.get_size() == 0) {
            cout << "Ошибка: отсутствуют столбцы после SELECT\n";
            return;
        }
    }

    Vector<string> tableNames;
    bool hasWhere = false;
    string whereClause = "";

    {
        string token;
        string buffer;

        while (ss >> token) {
            if (token == "WHERE") {
                hasWhere = true;
                break;
            }

            if (!buffer.empty()) {
                buffer += " ";
            }
            buffer += token;
        }

        string t;
        stringstream tss(buffer);
        while (getline(tss, t, ',')) {
            trim(t);
            if (!t.empty()) {
                tableNames.push_back(t);
            }
        }

        if (tableNames.get_size() == 0) {
            cout << "Ошибка: отсутствуют таблицы после FROM\n";
            return;
        }

        if (hasWhere) {
            getline(ss, whereClause);
            trim(whereClause);
        }
    }

    Vector<string> lockedTables;

    for (size_t i = 0; i < tableNames.get_size(); i++) {
        if (!lockTable(dbManager, tableNames[i])) {
            cout << "Ошибка: таблица заблокирована: " << tableNames[i] << "\n";

            for (size_t j = 0; j < lockedTables.get_size(); j++) {
                unlockTable(dbManager, lockedTables[j]);
            }

            return;
        }
        lockedTables.push_back(tableNames[i]);
    }

    try {
        Vector<Condition> conditions = parseConditions(whereClause);

        for (size_t i = 0; i < selectColumns.get_size(); i++) {
            cout << selectColumns[i];
            if (i + 1 < selectColumns.get_size()) {
                cout << " | ";
            }
        }
        cout << "\n";

        selectData(dbManager, selectColumns, tableNames, conditions);
    }
    catch (const exception& e) {
        cout << "Ошибка при выборке: " << e.what() << endl;
    }

    for (size_t i = 0; i < tableNames.get_size(); i++) {
        unlockTable(dbManager, tableNames[i]);
    }
}

void executeDelete(DatabaseManager& dbManager, const string& command) {
    stringstream ss(command);

    string kw1, kw2, tableName, kw3;

    if (!(ss >> kw1) || kw1 != "DELETE") {
        cout << "Ошибка: ожидается DELETE\n";
        return;
    }

    if (!(ss >> kw2) || kw2 != "FROM") {
        cout << "Ошибка: ожидается FROM\n";
        return;
    }

    if (!(ss >> tableName)) {
        cout << "Ошибка: отсутствует имя таблицы\n";
        return;
    }

    if (!(ss >> kw3) || kw3 != "WHERE") {
        cout << "Ошибка: отсутствует WHERE\n";
        return;
    }

    string whereClause;
    getline(ss, whereClause);

    while (!whereClause.empty() && isspace(whereClause.front())) {
        whereClause.erase(whereClause.begin());
    }

    if (whereClause.empty()) {
        cout << "Ошибка: отсутствует условие после WHERE\n";
        return;
    }

    if (!lockTable(dbManager, tableName)) {
        cout << "Ошибка: таблица заблокирована: " << tableName << "\n";
        return;
    }

    try {
        Vector<Condition> conditions = parseConditions(whereClause);
        deleteData(dbManager, tableName, conditions);
    }
    catch (const exception& e) {
        cout << "Ошибка при удалении: " << e.what() << endl;
    }

    unlockTable(dbManager, tableName);
}

string processCommand(DatabaseManager& dbManager, const string& command) {
    stringstream output;

    streambuf* oldCoutBuffer = cout.rdbuf();
    cout.rdbuf(output.rdbuf());

    try {
        stringstream ss(command);

        string cmd;
        ss >> cmd;

        if (cmd.empty()) {
            cout << "Пустая команда.\n";
            cout.rdbuf(oldCoutBuffer);
            return output.str();
        }

        string upperCmd = cmd;
        for (char &c : upperCmd) c = toupper(c);

        if (upperCmd == "INSERT") {
            executeInsert(dbManager, command);
        }
        else if (upperCmd == "SELECT") {
            executeSelect(dbManager, command);
        }
        else if (upperCmd == "DELETE") {
            executeDelete(dbManager, command);
        }
        else if (upperCmd == "EXIT" || upperCmd == "QUIT") {
            cout.rdbuf(oldCoutBuffer);
            return "EXIT";
        }
        else {
            cout << "Неизвестная команда.\n";
        }

    } catch (const exception& e) {
        cout << "Ошибка: " << e.what() << endl;
    }

    cout.rdbuf(oldCoutBuffer);
    return output.str();
}