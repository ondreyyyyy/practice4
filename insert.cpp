#include "structures.h"
#include "Vector.h"
#include "insert.h"
#include "auxiliary.h"
#include <iostream>
#include <fstream>

using namespace std;


void insertData(DatabaseManager& DBmanager, const string& table, const string& query, int& pk) {
    DBtable& tbl = DBmanager.getTable(table);

    string schemaDir = DBmanager.getSchemaName();
    const int limit = DBmanager.getTuplesLimit();

    int num = 1;
    string csvPath;
    bool needNewCSV = false;

    while (true) {
        csvPath = schemaDir + "/" + table + "/" + to_string(num) + ".csv";
        
        ifstream in(csvPath);
        if (!in.is_open()) {
            needNewCSV = true;
            break;
        }

        string header;
        getline(in, header);
        if (header.empty()) {
            needNewCSV = true;
            break;
        }

        int rows = 0;
        string line;
        while (getline(in, line)) {
            rows++;
        }
        in.close();

        if (rows < limit) {
            break;
        }

        num++;
    }

    if (needNewCSV) {
        writeTitle(DBmanager, table, csvPath);
    }
    else {
        ifstream in(csvPath);
        if (in.peek() == ifstream::traits_type::eof()) {
            writeTitle(DBmanager, table, csvPath);
        }
        in.close();
    }

    Vector<string> values = parseValues(query);
    Vector<string> columns = tbl.accessColumns();

    int colCount = columns.get_size();
    int valCount = values.get_size();

    if (valCount > colCount) {
        cerr << "Слишком много значений для вставки.\n";
        return;
    }

    ofstream out(csvPath, ios::app);
    if (!out.is_open()) {
        cerr << "Ошибка записи в CSV файл.\n";
        return;
    }

    out << pk;

    int totalDataCols = colCount - 1;

    for (int i = 0; i < colCount; i++) {
        out << ",";
        if (i < valCount) {
            out << values[i];
        }
    }

    out << "\n";
    out.close();

    pk++;

    string pkPath = schemaDir + "/" + table + "/" + table + "_pk_sequence";
    ofstream pkFile(pkPath);

    if (!pkFile.is_open()) {
        cerr << "Ошибка записи pk_sequence\n";
        return;
    }

    pkFile << pk;
    pkFile.close();
}   