#include "filter.h"
#include "auxiliary.h"
#include "Vector.h"
#include <iostream>

using namespace std;

bool filterMatch(const Vector<string>& columns, const Vector<string>& values, const Vector<Condition>& conditions) {
    Vector<Condition> output;     
    Vector<Condition> operations;  


    for (size_t i = 0; i < conditions.get_size(); i++) { // формирование постфиксной записи
        const Condition& c = conditions[i];

        if (c.column == "(") { // условие (A AND/OR B) .......
            operations.push_back(c);
        }
        else if (c.column == ")") { // все, что до ( идет в вектор с постфиксной записью

            while (!operations.empty() && operations[operations.get_size() - 1].column != "(") {
                output.push_back(operations[operations.get_size() - 1]);
                operations.pop_back();
            } // ((A AND B) OR C) = A B AND C OR 

            if (!operations.empty()) {
                operations.pop_back(); 
            }
        }
        else if (c.logicalOperator == "AND" || c.logicalOperator == "OR") { // условие A AND/OR B

            while (!operations.empty()) {
                Condition& top = operations[operations.get_size() - 1];

                if (precedence(top.logicalOperator) >= precedence(c.logicalOperator)) { // сравнение с последней операцией в массиве
                    output.push_back(top);
                    operations.pop_back();
                } else {
                    break;
                }
            }

            operations.push_back(c);
        }
        else { // условие A = 'value'
            output.push_back(c);
        }
    }

    Vector<bool> evaluate;

    for (size_t i = 0; i < output.get_size(); i++) {

        const Condition& c = output[i];

        if (c.logicalOperator == "AND" || c.logicalOperator == "OR") {

            if (evaluate.get_size() < 2) {
                cerr << "Недостаточно операндов.\n";
                return false;
            }

            bool b = evaluate[evaluate.get_size() - 1];
            evaluate.pop_back();
            bool a = evaluate[evaluate.get_size() - 1];
            evaluate.pop_back();

            if (c.logicalOperator == "AND") {
                evaluate.push_back(a && b);
            }
            else {
                evaluate.push_back(a || b);
            }

            continue;
        }

        int idxL = columnIndex(columns, c.column);

        if (idxL < 0 || idxL >= (int)values.get_size()) {
            evaluate.push_back(false);
            continue;
        }

        bool rightIsColumn = isColumnRef(c.value);

        if (rightIsColumn) {
            int idxR = columnIndex(columns, c.value);

            if (idxR < 0 || idxR >= (int)values.get_size()) {
                evaluate.push_back(false);
                continue;
            }

            evaluate.push_back(values[idxL] == values[idxR]);
        }
        else {
            evaluate.push_back(values[idxL] == c.value);
        }
    }

    if (evaluate.empty()) { // если нет условий -это всегда истина
        return true;
    }

    return evaluate[0];
}