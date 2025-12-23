#ifndef FILE_H
#define FILE_H

#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "structures.h"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

void loadSchema(DatabaseManager& DBmanager);
void createCSV(const string& tableDirectory, const DBtable& table);
void createPkFile(const string& tableDirectory, const string& tableName);
void createLockFile(const string& tableDirectory, const string& tableName);
void createFileStruct(const DatabaseManager& DBmanager);
void initCryptoDatabase(DatabaseManager& DBmanager);

#endif