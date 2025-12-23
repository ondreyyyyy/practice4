#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <string>
#include "Vector.h"
#include "hashtable.h"

class DBtable {
private:
    string name;
    Vector<string> columns;

public:
    DBtable();
    DBtable(const string& name, const Vector<string>& columns);

    const string& getName() const;
    const Vector<string>& getColumns() const;
    Vector<string>& accessColumns();

    void setName(const string& n);

    void addColumn(const string& col);
};

class DatabaseManager {
private:
    std::string schemaName;
    int tuplesLimit;
    HashTable<std::string, DBtable> tables;
    HashTable<std::string, int> lockFDs;

public:
    DatabaseManager();
    DatabaseManager(const std::string& name, int tplsLimit);

    const std::string& getSchemaName() const;
    int getTuplesLimit() const;

    HashTable<std::string, DBtable>& getTables();
    const HashTable<std::string, DBtable>& getTables() const;

    void setSchemaName(const std::string& s);
    void setTuplesLimit(int limit);

    void addTable(const DBtable& table);
    DBtable& getTable(const std::string& name);
    const DBtable& getTable(const std::string& name) const;
};

struct Condition {
    string column;
    string value;       
    string logicalOperator;
};

struct PairInfo {
    string pairId;
    string firstLotId;
    string secondLotId;
};

#endif