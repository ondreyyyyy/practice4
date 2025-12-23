#ifndef DELETE_H
#define DELETE_H

#include "structures.h"

void deleteData(DatabaseManager& DBmanager, const string& tableName, const Vector<Condition>& conditions);

#endif