#ifndef API_H
#define API_H

#include <string>
#include "structures.h"

using namespace std;

string handleGetBalance(DatabaseManager& dbManager, const string& userKey);
string getUserIdByKey(DatabaseManager& dbManager, const string& userKey);
string generateUserKey();
string handleCreateUser(DatabaseManager& dbManager, const string& body);
string handleGetLots(DatabaseManager& dbManager);
string handleGetPairs(DatabaseManager& dbManager);
string handleCreateOrder(DatabaseManager&, const string& body, const string& userKey);
string handleGetOrders(DatabaseManager& dbManager);
string handleDeleteOrder(DatabaseManager&, const string& body, const string& userKey);
string makeHttpResponse(int statusCode, const string& body);
PairInfo getPairInfo(DatabaseManager& dbManager, const string& pairId);
double getUserBalance(DatabaseManager& dbManager, const string& userId, const string& lotId);
bool updateOrderQuantity(DatabaseManager& dbManager, const string& orderId, double newQuantity);
string getCurrentTimestamp();
bool canDeleteOrder(DatabaseManager& dbManager, const string& orderId, const string& userId);
bool closeOrderWithTimestamp(DatabaseManager& dbManager, const string& orderId, const string& timestamp = "");

#endif