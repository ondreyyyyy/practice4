#include "auxiliary.h"
#include "structures.h"
#include "Vector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

Vector<string> splitCSV(const string& line) {
    Vector<string> res;
    string cell;
    for (char c : line) {
        if (c == ',') {
            stringstream ss(cell);
            string token;
            ss >> token;
            res.push_back(token);
            cell.clear();
        } else {
            cell += c;
        }
    }

    stringstream ss(cell);
    string token;
    ss >> token;
    res.push_back(token);

    return res;
}


Vector<string> parseValues(const string& query) {
    Vector<string> values;
    string cur;
    bool inside = false;

    for (char c : query) {
        if (c == '\'') {
            inside = !inside;
            if (!inside && !cur.empty()) {
                values.push_back(cur);
                cur.clear();
            }
        } else if (inside) {
            cur += c;
        }
    }
    return values;
}

void writeTitle(const DatabaseManager& DBmanager, const string& tableName, const string& csvPath) {
    const DBtable& tbl = DBmanager.getTable(tableName);  

    const Vector<string>& cols = tbl.getColumns();
    if (cols.get_size() == 0) {
        cerr << "У таблицы нет колонок.\n";
        return;
    }

    ofstream out(csvPath);
    if (!out.is_open()) {
        cerr << "Ошибка создания CSV файла.\n";
        return;
    }

    for (int i = 0; i < cols.get_size(); i++) {
        if (i > 0) out << ",";
        out << cols[i];
    }
    out << "\n";
}

string stripTable(const string& full) {
    stringstream ss(full);
    string table, column;

    if (getline(ss, table, '.') && getline(ss, column)) {
        return column;
    }
    return full;
}

int columnIndex(const Vector<string>& cols, const string& colName) {
    for (size_t i = 0; i < cols.get_size(); i++) {
        if (cols[i] == colName) {
            return (int)i;
        }
    }
    return -1;
}

int precedence(const string& op) {
    if (op == "AND") return 2;
    if (op == "OR")  return 1;
    return 0;
}

bool isColumnRef(const string& s) {
    stringstream ss(s);
    string table, column;

    return getline(ss, table, '.') && getline(ss, column) && !column.empty();
}

string escape(const string& s) {
    string out;
    out.reserve(s.size());
    for (char c: s) {
        if (c == '\'' || c == ',') {
            continue;
        }
        out += c;
    }
    return out;
}

json parseJsonBody(const string& body) {
    if (body.empty()) {
        throw runtime_error("Empty json");
    }
    return json::parse(body);
}

bool hasField(const json& j, const string& field) {
    return j.contains(field) && !j[field].is_null();
}