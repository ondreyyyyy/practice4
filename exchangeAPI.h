#ifndef EXCHANGEAPI_H
#define EXCHANGEAPI_H

#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include "Vector.h"

using json = nlohmann::json;
using namespace std;

class ExchangeAPI {
private:
    string serverUrl;
    string userKey;
    mutable mutex userkeyMtx;
    string sendRequest(const string& method, const string& endpoint, const json& body = json(), const Vector<string>& headers = {});

public:
    ExchangeAPI();
    string createUser(const string& username);
    void setUserKey(const string& key);
    json getLots();
    json getPairs();
    json getBalance();
    json getAllOrders();
    int createOrder(int pairId, double quantity, double price, const string& type);
    bool deleteOrder(int orderId);
    double getBalanceInRUB();
    json getActiveOrder(int pairId = -1);
};

#endif