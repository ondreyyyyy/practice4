#ifndef SMARTBOT_H
#define SMARTBOT_H

#include "exchangeAPI.h"
#include "Vector.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

using namespace std;
using json = nlohmann::json;
const double MIN_PROFIT = 0.002;

class SmartBot {
private:
    ExchangeAPI api;
    string username;
    string userKey;
    int rubLotId;
    atomic<bool> running;
    thread workerThread;
    Vector<int> activeOrderIds;
    mutex mtx;

    void workerFunc();
    void executeAlgorythm();
    void analyzeMarket(int pairId, const json& orders, double& bestBuy, double& bestSell, int& buyCount, int& sellCount) const;
    
    double calculateSpread(int buyCount, int sellCount) const;
    double calculateSafeQuantity(int pairId, const string& type, double bestBuy, double bestSell, const json& balance);
    double getRUBBalance();

    void cleanupOrders();
    void cancelOldOrders();
    string generateUsername(const string& basename);

public:
    SmartBot(const string& basename = "neverov");
    ~SmartBot();

    void run();
    void stop();
    void printStats();
};

#endif // SMARTBOT_H