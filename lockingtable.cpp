#include <iostream>
#include <fstream>
#include <string>
#include "structures.h"
#include "lockingtable.h"

using namespace std;

bool lockTable(const DatabaseManager& DBmanager, const string& tableName) {
    string path = DBmanager.getSchemaName() + "/" + tableName + "/" + tableName + "_lock";

    ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    string state;
    in >> state;
    in.close();

    if (state == "locked") {
        return false;
    }

    ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    out << "locked";
    out.close();
    return true;
}

void unlockTable(const DatabaseManager& DBmanager, const string& tableName) {
    string path = DBmanager.getSchemaName() + "/" + tableName + "/" + tableName + "_lock";

    ofstream out(path);
    if (!out.is_open()) {
        cout << "Ошибка при разблокировке.\n";
        return;
    }

    out << "unlocked";
    out.close();
}