#include "smartBot.h"
#include "exchangeAPI.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <cctype>
#include <cmath>

using namespace std;

SmartBot::SmartBot(const string& basename): running(false), rubLotId(-1) {
    username = generateUsername(basename);
    try {
        userKey = api.createUser(username);
        auto lots = api.getLots();
        for (const auto& lot: lots) {
            if (lot["name"] == "RUB") {
                rubLotId = lot["lot_id"];
                break;
            }
        }

        if (rubLotId == -1) {
            throw runtime_error("[SMART] Лот с названием RUB не найден.\n");
        }

        cout << "[SMART] Пользователь создан: " << username << " с балансом (в RUB): " << getRUBBalance() << endl;
    }
    catch (const exception& e) {
        cout << "[SMART] Ошибка при создании пользователя: " << e.what() << endl; 
        throw;
    }
}

SmartBot::~SmartBot() {
    stop();
}

string SmartBot::generateUsername(const string& basename) {
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    stringstream ss;
    ss << basename << time;
    return ss.str();
}

void SmartBot::run() {
    if (running.load()) {
        return;
    }
    running.store(true);
    workerThread = thread(&SmartBot::workerFunc, this);
    cout << "[SMART] Робот " << username << " запущен.\n";
}

void SmartBot::stop() {
    running.store(false);
    if (workerThread.joinable()) {
        workerThread.join();
    }
    cout << "[SMART] Робот " << username << " остановлен.\n";
}

void SmartBot::workerFunc() {
    random_device rd;
    mt19937 rng(rd());
    int iteration = 0;

    while (running.load()) {
        try {
            iteration++;
            auto allOrders = api.getAllOrders();
            cleanupOrders();

            if (iteration % 7 == 0) {
                cancelOldOrders();
            }

            executeAlgorythm();
            if (iteration % 15 == 0) {
                printStats();
            }
        }
        catch (const exception& e) {
            cerr << "[SMART] Ошибка при инициализации алгоритма работы робота " << username << ". Ошибка: " << e.what() << endl;
        }
        this_thread::sleep_for(chrono::milliseconds(1000));
    }
}

void SmartBot::analyzeMarket(int pairId, const json& orders, double& bestBuy, double& bestSell, int& buyCount, int& sellCount) const {
    bestBuy = 0;
    bestSell = 0;
    buyCount = 0;
    sellCount = 0;

    for (const auto& order: orders) {
        if (order["pair_id"] != pairId) {
            continue;
        }

        string closed = order["closed"] ;
        bool isClosed = false;
        for (char c : closed) {
            if (!isspace(static_cast<unsigned char>(c))) {
                isClosed = true;
                break;
            }
        }
        if (isClosed) {
            continue;
        }

        string type = order["type"];
        double price = order["price"];

        if (type == "buy") {
            buyCount++;
            if (price > bestBuy) {
                bestBuy = price;
            }
        }
        else if (type == "sell") {
            sellCount++;
            if (bestSell == 0 || price < bestSell) {
                bestSell = price;
            }
        }
    }
}

double SmartBot::calculateSpread(int buyCount, int sellCount) const {
    int totalOrders = buyCount + sellCount;
    if (totalOrders == 0) {
        return 0.05;
    }  
    if (totalOrders < 3) {
        return 0.03;
    }   
    if (totalOrders < 8) {
        return 0.02;
    }  
    if (totalOrders < 15) {
        return 0.015;
    }  
    if (totalOrders < 25) {
        return 0.012;
    }    
    return 0.01;  
}

double SmartBot::calculateSafeQuantity(int pairId, const string& type, double bestBuy, double bestSell, const json& balance) {
    try {
        auto pairs = api.getPairs();
        int saleLot = -1;
        int buyLot = -1;

        for (const auto& p: pairs) {
            if (p["pair_id"] == pairId) {
                saleLot = p["sale_lot_id"];
                buyLot = p["buy_lot_id"]; 
                break;
            }
        }

        if (saleLot == -1) {
            cerr << "[SMART] Ошибка при получении пар: " << username << endl;
            return 0;
        }

        int targetLot = (type == "sell") ? saleLot : buyLot;

        double amount = 0;
        for (const auto& b: balance) {
            if (b["lot_id"] == targetLot) {
                amount = b["quantity"].get<double>();
                break;
            }
        }

        if (amount <= 0) {
            cerr << "[SMART] Баланс пустой или неверный: " << username << endl;
            return 0;
        }

        double marketPrice = 0;
        if (bestBuy > 0 && bestSell > 0) {
            marketPrice = (bestBuy + bestSell) / 2;
        } 
        else if (bestBuy > 0) {
            marketPrice = bestBuy;
        } 
        else if (bestSell > 0) {
            marketPrice = bestSell;
        } 
        else {
            marketPrice = 1.0;
        }

        if (type == "buy") {
            return min(amount / marketPrice * 0.25, 5.0);
        } 
        else {
            return min(amount * 0.25, 5.0);
        }
    }
    catch (const exception& e) {
        cerr << "[SMART] Ошибка при расчете количества актива: " << username << endl;
        return 0;
    }
}

void SmartBot::executeAlgorythm() {
    auto balance = api.getBalance();
    auto pairs = api.getPairs();
    auto orders = api.getActiveOrder(-1);

    struct Opportunity {
        int pairId;
        string type;
        double price;
        double quantity;
        double expectedRUBProfit;
    };

    Vector<Opportunity> opportunities;
    double currentRUB = getRUBBalance();

    for (const auto& p : pairs) {
        int pairId = p["pair_id"];
        double bestBuy = 0, bestSell = 0;
        int buyCount = 0, sellCount = 0;

        analyzeMarket(pairId, orders, bestBuy, bestSell, buyCount, sellCount);

        if (bestBuy <= 0 && bestSell <= 0) {
            continue;
        }

        double midPrice = (bestBuy > 0 && bestSell > 0) ? (bestBuy + bestSell) / 2 : (bestBuy > 0 ? bestBuy : (bestSell > 0 ? bestSell : 1.0));
        double spread = calculateSpread(buyCount, sellCount);

        double buyPrice = midPrice * (1 - spread);
        double buyQty = calculateSafeQuantity(pairId, "buy", bestBuy, bestSell, balance);
        if (buyQty > 0.001) {
            double expectedProfit = (bestSell > 0 ? bestSell - buyPrice : 0);
            double expectedRUB = expectedProfit * buyQty;

            if (expectedRUB / buyPrice >= MIN_PROFIT) {
                opportunities.push_back({pairId, "buy", buyPrice, buyQty, expectedRUB});
            }
        }

        double sellPrice = midPrice * (1 + spread);
        double sellQty = calculateSafeQuantity(pairId, "sell", bestBuy, bestSell, balance);
        if (sellQty > 0.001) {
            double expectedProfit = (sellPrice - bestBuy);
            double expectedRUB = expectedProfit * sellQty;

            if (bestBuy > 0 && expectedRUB / bestBuy >= MIN_PROFIT) {
                opportunities.push_back({pairId, "sell", sellPrice, sellQty, expectedRUB});
            }
        }
    }

    if (opportunities.get_size() > 0) {
        Opportunity best = opportunities[0];
        for (size_t i = 1; i < opportunities.get_size(); i++) {
            if (opportunities[i].expectedRUBProfit > best.expectedRUBProfit) {
                best = opportunities[i];
            }
        }

        int id = api.createOrder(best.pairId, best.quantity, best.price, best.type);
        lock_guard<mutex> lock(mtx);
        activeOrderIds.push_back(id);
    }
}

void SmartBot::cleanupOrders() {
    auto orders = api.getAllOrders();
    Vector<int> stillActive;

    {
        lock_guard<mutex> lock(mtx);
        for (int id : activeOrderIds) {
            bool open = false;
            for (const auto& o : orders) {
                if (o["order_id"] == id) {
                    string closed = o["closed"];
                    open = closed.empty();
                    break;
                }
            }
            if (open) {
                stillActive.push_back(id);
            }
        }
    }
    {
        lock_guard<mutex> lock(mtx);
        activeOrderIds = stillActive;
    }
}

void SmartBot::cancelOldOrders() {
    int id = -1;
    {
        lock_guard<mutex> lock(mtx);
        if (activeOrderIds.get_size() == 0) return;
        id = activeOrderIds[0];
    }

    if (api.deleteOrder(id)) {
        lock_guard<mutex> lock(mtx);
        if (activeOrderIds.get_size() > 0 && activeOrderIds[0] == id) {
            activeOrderIds.erase(&activeOrderIds[0]);
        }
    }
}

double SmartBot::getRUBBalance() {
    auto bal = api.getBalance();
    for (const auto& b : bal) {
        if (b["lot_id"] == rubLotId) {
            return b["quantity"];
        }
    }
    return 0.0;
}

void SmartBot::printStats() {
    size_t count;
    {
        lock_guard<mutex> lock(mtx);
        count = activeOrderIds.get_size();
    }

    cout << "[SMART] USER: " << username << " RUB: " << getRUBBalance() << " ORDERS: " << count << endl;
}