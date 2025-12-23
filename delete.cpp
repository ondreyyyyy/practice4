#include "delete.h"
#include "structures.h"
#include "auxiliary.h"
#include "Vector.h"
#include "filter.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

void deleteData(DatabaseManager& DBmanager, const string& tableName, const Vector<Condition>& conditions) {
    const string schema = DBmanager.getSchemaName();
    DBtable& table = DBmanager.getTable(tableName);

    Vector<Condition> localCond = conditions;

    for (size_t ci = 0; ci < localCond.get_size(); ci++) {
        Condition& cond = localCond[ci];

        if (cond.column == "(" || cond.column == ")" || cond.logicalOperator == "AND" || cond.logicalOperator == "OR") {
            continue;
        }

        cond.column = stripTable(cond.column);

        if (!cond.value.empty() && cond.value.front() == '\'' && cond.value.back() == '\'') {
            cond.value = cond.value.substr(1, cond.value.size() - 2);
        }

        stringstream ss(cond.value);
        string token;
        ss >> token;
        cond.value = token;
    }

    int fileIndex = 1;
    bool deletedAny = false;

    while (true) {
        string csvPath = schema + "/" + tableName + "/" + to_string(fileIndex) + ".csv";

        ifstream in(csvPath);
        if (!in.is_open()) break;

        string tmpPath = csvPath + ".tmp";
        ofstream out(tmpPath);
        if (!out.is_open()) {
            cerr << "Ошибка создания временного файла\n";
            return;
        }

        string header;
        getline(in, header);
        out << header << "\n";

        Vector<string> headerCols = splitCSV(header);
        size_t colCount = headerCols.get_size();

        string line;
        while (getline(in, line)) {
            if (line.empty()) continue;

            Vector<string> values = splitCSV(line);

            Vector<string> filteredCols;
            Vector<string> filteredValues;

            for (size_t ci = 1; ci < colCount; ci++) {
                filteredCols.push_back(headerCols[ci]);

                if (ci < values.get_size()) {
                    filteredValues.push_back(values[ci]);
                }
                else {
                    filteredValues.push_back("");
                } 
            }

            bool match = filterMatch(filteredCols, filteredValues, localCond);

            if (!match) {
                out << line << "\n";
            }
            else {
                deletedAny = true;
            }
        }

        in.close();
        out.close();

        remove(csvPath.c_str());
        rename(tmpPath.c_str(), csvPath.c_str());

        fileIndex++;
    }

    if (!deletedAny) {
        cout << "Нет строк для удаления в таблице: " << tableName << "\n";
    }
    else {
        cout << "Удаление выполнено успешно в таблице: " << tableName << "\n";
    }
}