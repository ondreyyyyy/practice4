#ifndef SELECT_H
#define SELECT_H

#include "Vector.h"
#include "structures.h"

void selectData(DatabaseManager& DBmanager, const Vector<string>& selectColumns, const Vector<string>& tableNames, const Vector<Condition>& conditions);
void selectDataCapture(DatabaseManager& DBmanager, const Vector<string>& selectColumns, const Vector<string>& tableNames, const Vector<Condition>& conditions, Vector<string>& output);

#endif