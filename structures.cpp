#include "structures.h"
#include <fstream>

using namespace std;

DBtable::DBtable() : name(""), columns() {}

DBtable::DBtable(const string& name, const Vector<string>& columns)
    : name(name), columns(columns) {}

const string& DBtable::getName() const {
    return name;
}

const Vector<string>& DBtable::getColumns() const {
    return columns;
}

Vector<string>& DBtable::accessColumns() {
    return columns;
}

void DBtable::setName(const string& n) {
    name = n;
}

void DBtable::addColumn(const string& col) {
    columns.push_back(col);
}

DatabaseManager::DatabaseManager()
    : schemaName(""), tuplesLimit(0) {}

DatabaseManager::DatabaseManager(const string& schemaName, int tuplesLimit)
    : schemaName(schemaName), tuplesLimit(tuplesLimit) {}

const string& DatabaseManager::getSchemaName() const {
    return schemaName;
}

int DatabaseManager::getTuplesLimit() const {
    return tuplesLimit;
}

HashTable<string, DBtable>& DatabaseManager::getTables() {
    return tables;
}

const HashTable<string, DBtable>& DatabaseManager::getTables() const {
    return tables;
}

void DatabaseManager::setSchemaName(const string& s) {
    schemaName = s;
}

void DatabaseManager::setTuplesLimit(int limit) {
    tuplesLimit = limit;
}

void DatabaseManager::addTable(const DBtable& table) {
    tables.insert(table.getName(), table);
}

DBtable& DatabaseManager::getTable(const string& name) {
    return tables.at(name);
}

const DBtable& DatabaseManager::getTable(const string& name) const {
    return tables.at(name);
}