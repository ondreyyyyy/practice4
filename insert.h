#ifndef INSERT_H
#define INSERT_H

#include <string>
#include "structures.h"

void insertData(DatabaseManager& DBmanager, const std::string& tableName, const std::string& rawValues, int& pk);

#endif