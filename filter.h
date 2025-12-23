#ifndef FILTER_H
#define FILTER_H

#include "Vector.h"
#include "structures.h"

bool filterMatch(const Vector<string>& columns, const Vector<string>& values, const Vector<Condition>& conditions);

#endif