#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "structures.h"
#include "insert.h"
#include "auxiliary.h"
#include "file.h"
#include "select.h"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

void loadSchema(DatabaseManager& DBmanager) {
    const string pathToConfig = "schema.json";
    ifstream file(pathToConfig);
    if (!file.is_open()) {
        cerr << "Ошибка открытия .json файла " << pathToConfig << endl;
        return;
    }

    json schema;
    file >> schema;
    for (const auto& table: schema["structure"].items()) {
        DBtable tempTable;
        tempTable.setName(table.key());

        for (const string& col: table.value()) {
            tempTable.addColumn(col);
        }

        DBmanager.addTable(tempTable);
    }

    DBmanager.setSchemaName(schema["name"]);
    DBmanager.setTuplesLimit(schema["tuples_limit"]);
}

void loadLotsFromConfig(DatabaseManager& DBmanager) {
    const string pathToConfig = "config.json";
    ifstream file(pathToConfig);
    if (!file.is_open()) {
        cerr << "При обработке файла config.json произошла ошибка.\n";
        return;
    }

    json cfg;
    file >> cfg;

    string pkPath = DBmanager.getSchemaName() + "/lot/lot_pk_sequence";
    int pk = 1;
    ifstream pkFile(pkPath);
    if (pkFile.is_open()) {
        pkFile >> pk;
        pkFile.close();
    }

    for (auto& lotName: cfg["lots"]) {
        string safe = escape((string)lotName);
        string query = "VALUES('" + safe + "')";
        insertData(DBmanager, "lot", query, pk);
    }
}

void generatePairs(DatabaseManager& DBmanager) {
    Vector<string> selectCol = {"lot.lot_id"};
    Vector<string> tables = {"lot"};
    Vector<Condition> cond;
    Vector<string> ids;

    selectDataCapture(DBmanager, selectCol, tables, cond, ids);

    string pkPath = DBmanager.getSchemaName() + "/pair/pair_pk_sequence";
    int pk = 1;
    ifstream pkFile(pkPath);
    if (pkFile.is_open()) pkFile >> pk;

    for (int i = 0; i < ids.get_size(); i++) {
        for (int j = i + 1; j < ids.get_size(); j++) {

            string id1 = ids[i]; // теперь это настоящий lot_id
            string id2 = ids[j];

            string query = "VALUES('" + id1 + "','" + id2 + "')";
            insertData(DBmanager, "pair", query, pk);
        }
    }
}

void createCSV(const string& tableDirectory, const DBtable& table) {
    string CSVdirectory = tableDirectory + "/1.csv";
    ofstream csvFile(CSVdirectory);
        
    if (!csvFile.is_open()) {
        cerr << "При создании CSV файла произошла ошибка!\n";
        return;
    } 

    csvFile << table.getName() << "_id";

    for (const string& col: table.getColumns()) {
        csvFile << "," << col;
    }

    csvFile << "\n";
    csvFile.close();
}

void createPkFile(const string& tableDirectory, const string& tableName) {
    string pkFilePath = tableDirectory + "/" + tableName + "_pk_sequence";
    ofstream pkFile(pkFilePath);

    if (!pkFile.is_open()) {
        cerr << "При создании файла с первичным ключом произошла ошибка!\n" << endl;
        return;
    }

    pkFile << 1;
    pkFile.close();
}

void createLockFile(const string& tableDirectory, const string& tableName) {
    string lockFilePath = tableDirectory + "/" + tableName + "_lock";
    ofstream lockFile(lockFilePath);

    if (!lockFile.is_open()) {
        cerr << "При создании файла блокировки таблиц произошла ошибка!\n" << endl;
        return;       
    }

    lockFile << "unlocked";
    lockFile.close();
}

void createFileStruct(const DatabaseManager& DBmanager) {
    if (!fs::exists(DBmanager.getSchemaName())) {
        fs::create_directories(DBmanager.getSchemaName());
    }

    const auto& tableHash = DBmanager.getTables();

    for (size_t i = 0; i < tableHash.getCapacity(); i++) {
        Node<string, DBtable>* node = tableHash.getChain(i);

        while (node != nullptr) {
            const DBtable& table = node->getValue();

            string tableDirectory = DBmanager.getSchemaName() + "/" + table.getName();

            if (!fs::exists(tableDirectory)) {
                fs::create_directories(tableDirectory);
            }

            createCSV(tableDirectory, table);
            createPkFile(tableDirectory, table.getName());
            createLockFile(tableDirectory, table.getName());

            node = node->getNext();
        }
    }
}

void initCryptoDatabase(DatabaseManager& DBmanager) {
    loadSchema(DBmanager);
    if (!fs::exists(DBmanager.getSchemaName())) {
        createFileStruct(DBmanager);  
        loadLotsFromConfig(DBmanager);  
        generatePairs(DBmanager);   
    }     
    cout << "Структура биржи инициализирована.\n";
}