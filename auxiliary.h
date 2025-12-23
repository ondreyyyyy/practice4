#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <string>
#include "Vector.h"
#include "structures.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Vector<std::string> splitCSV(const std::string& line);
Vector<std::string> parseValues(const std::string& query);
void writeTitle(const DatabaseManager& DBmanager, const std::string& tableName, const std::string& csvPath); // insert
int CSVcount(const string& schemaName, const string& table);
string stripTable(const string& full); // delete
int columnIndex(const Vector<string>& cols, const string& colName); // filter
int precedence(const string& op);
bool isColumnRef(const string& s);
string escape(const string& s);
json parseJsonBody(const string& body);
bool hasField(const json& j, const string& field);

#endif
