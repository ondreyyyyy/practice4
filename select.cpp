#include "select.h"
#include "structures.h"
#include "auxiliary.h"
#include "filter.h"
#include "Vector.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

void processLevel(
    int level,
    int tableCount,
    Vector<ifstream*>& files,
    Vector<int>& fileIndex,
    Vector<bool>& fileEnded,
    Vector<Vector<string>>& rowBuffers,
    const string& schema,
    const Vector<string>& tableNames,
    const Vector<Vector<string>>& tableColumns,
    const Vector<string>& selectColumns,
    const Vector<Condition>& conditions
) {
    if (level == tableCount) {
        Vector<string> fullColumns;
        Vector<string> fullValues;

        for (int t = 0; t < tableCount; ++t) {
            string pkColumnName = tableNames[t] + "." + tableNames[t] + "_id";
            fullColumns.push_back(pkColumnName);

            if (rowBuffers[t].get_size() > 0) {
                fullValues.push_back(rowBuffers[t][0]);
            } else {
                fullValues.push_back(string());
            }

            size_t colsCount = tableColumns[t].get_size();
            for (size_t ci = 0; ci < colsCount; ++ci) {
                fullColumns.push_back(tableNames[t] + "." + tableColumns[t][ci]);

                if ((ci + 1) < rowBuffers[t].get_size()) {
                    fullValues.push_back(rowBuffers[t][ci + 1]); 
                }
                else {
                    fullValues.push_back(string());
                }
            }
        }


        if (!filterMatch(fullColumns, fullValues, conditions)) {
            return;
        }

        for (size_t si = 0; si < selectColumns.get_size(); ++si) {
            int idx = columnIndex(fullColumns, selectColumns[si]);
            if (idx >= 0 && idx < (int)fullValues.get_size()) {
                cout << fullValues[idx];
            }
            if (si + 1 < selectColumns.get_size()) {
                cout << ",";
            }
        }
        cout << "\n";
        return;
    }

    while (true) {
        if (fileEnded[level]) return;

        string line;
        if (!getline(*files[level], line)) {
            files[level]->close();
            fileIndex[level]++;

            string next = schema + "/" + tableNames[level] + "/" + to_string(fileIndex[level]) + ".csv";
            files[level] = new ifstream(next);
            if (!files[level]->is_open()) {
                fileEnded[level] = true;
                return;
            }
            string hdr;
            getline(*files[level], hdr);
            if (!getline(*files[level], line)) {
                fileEnded[level] = true;
                return;
            }
        }

        rowBuffers[level] = splitCSV(line);

        for (int lvl = level + 1; lvl < tableCount; ++lvl) {
            if (files[lvl]) { 
                files[lvl]->close(); 
                delete files[lvl]; 
                files[lvl] = nullptr; 
            }
            fileIndex[lvl] = 1;
            fileEnded[lvl] = false;

            string path = schema + "/" + tableNames[lvl] + "/1.csv";
            files[lvl] = new ifstream(path);
            if (!files[lvl]->is_open()) {
                fileEnded[lvl] = true;
            } else {
                string hdr; 
                getline(*files[lvl], hdr);
            }
        }

        processLevel(level + 1, tableCount, files, fileIndex, fileEnded, rowBuffers, schema, tableNames, tableColumns, selectColumns, conditions);
    }
}


void selectData(
    DatabaseManager& DBmanager,
    const Vector<string>& selectColumns,
    const Vector<string>& tableNames,
    const Vector<Condition>& conditions
) {
    const string schema = DBmanager.getSchemaName();
    int tableCount = tableNames.get_size();
    if (tableCount == 0) {
        cerr << "Не указаны таблицы.\n";
        return;
    }

    Vector<Vector<string>> tableColumns;
    for (size_t ti = 0; ti < tableNames.get_size(); ++ti) {
        DBtable& T = DBmanager.getTable(tableNames[ti]);
        tableColumns.push_back(T.getColumns());
    }

    Vector<int> fileIndex(tableCount, 1);
    Vector<ifstream*> files(tableCount);
    Vector<bool> fileEnded(tableCount);

    for (int i = 0; i < tableCount; ++i) {
        fileEnded[i] = false;
        string path = schema + "/" + tableNames[i] + "/1.csv";
        files[i] = new ifstream(path);
        if (!files[i]->is_open()) {
            cerr << "Ошибка открытия " << tableNames[i] << "\n";
            for (int j = 0; j < i; ++j) { 
                if (files[j]) { 
                    files[j]->close(); 
                    delete files[j]; 
                    files[j] = nullptr; 
                } 
            }
            return;
        }
        string hdr; 
        getline(*files[i], hdr);
    }

    Vector<Vector<string>> rowBuffers(tableCount);

    processLevel(0, tableCount, files, fileIndex, fileEnded, rowBuffers, schema, tableNames, tableColumns, selectColumns, conditions);

    for (int i = 0; i < tableCount; ++i) {
        if (files[i]) { 
            files[i]->close(); 
            delete files[i]; 
            files[i] = nullptr; 
        }
    }
}

void selectDataCapture(DatabaseManager& DBmanager, const Vector<string>& selectColumns, const Vector<string>& tableNames, const Vector<Condition>& conditions, Vector<string>& output) {
    stringstream buff;
    auto oldBuff = cout.rdbuf(buff.rdbuf());

    selectData(DBmanager, selectColumns, tableNames, conditions);

    cout.rdbuf(oldBuff);
    string line;
    while (getline(buff, line)) {
        if (!line.empty()) {
            output.push_back(line);
        }
    }
}