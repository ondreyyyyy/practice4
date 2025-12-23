#include "exchangeAPI.h"
#include "randomBot.h"
#include "structures.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

using namespace std;

RandomBot::RandomBot(const string& basename): running(false) {
    username = generateUsername(basename);
    try {
        userKey = api.createUser(username);
        cout << "[RANDOM] Пользователь создан: " << username << endl;
    }
    catch (const exception& e) {
        cout << "[RANDOM] Ошибка при создании пользователя: " << e.what() << endl; 
        throw;
    }
}

RandomBot::~RandomBot() {
    stop();
}

string RandomBot::generateUsername(const string& basename) {
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    stringstream ss;
    ss << basename << time;
    return ss.str();
}

void RandomBot::run() {
    if (running.load()) {
        return;
    }

    running.store(true);
    workerThread = thread(&RandomBot::workerFunc, this);
    cout << "[RANDOM] Робот " << username << " запущен.\n";
}

void RandomBot::stop() {
    running.store(false);
    {
        lock_guard<mutex> lock(mtx);
    }
    cv.notify_all();

    if (workerThread.joinable()) {
        workerThread.join();
    }
    cout << "[RANDOM] Робот " << username << " остановлен.\n";
}

void RandomBot::workerFunc() {
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> sleepDist(900, 1100);
    uniform_int_distribution<size_t> orderIndexDist;
    uniform_real_distribution<double> deleteChanceDist(0.0, 1.0);

    auto pairsJson = api.getPairs();
    if (pairsJson.empty()) {
        cerr << "[RANDOM] Нет доступных торговых пар. Завершение.\n";
        return;
    }

    Vector<PairInfo> pairs;
    for (const auto& p: pairsJson) {
        PairInfo info;
        info.pairId = to_string(p["pair_id"]);
        info.firstLotId = to_string(p["sale_lot_id"]);
        info.secondLotId = to_string(p["buy_lot_id"]);
        pairs.push_back(info);
    }

    while (true) {
        if (!running) {
            break;
        }

        try {
            auto balanceJson = api.getBalance();
            if (balanceJson.empty()) {
                this_thread::sleep_for(chrono::milliseconds(50));
                continue;
            }

            auto ordersJson = api.getAllOrders();

            if (deleteChanceDist(rng) <= 0.1 && createdOrderId.get_size() > 0) {
                try {
                    size_t index;
                    if (createdOrderId.get_size() == 1) {
                        index = 0;
                    }
                    else {
                        index = orderIndexDist(rng) % createdOrderId.get_size();
                    }

                    int orderId = createdOrderId[index];

                    bool isOpen = false;
                    for (const auto& order: ordersJson) {
                        if (order["order_id"] == orderId) {
                            string closed = order["closed"];
                            bool isClosed = false;
                            for (char c: closed) {
                                if (!isspace(static_cast<unsigned char>(c))) {
                                    isClosed = true;
                                    break;
                                }
                            }
                            isOpen = !isClosed;
                            break;
                        }
                    }

                    if (!isOpen) {
                        int* ptr = &createdOrderId[index];
                        {
                            lock_guard<mutex> ordersLock(orderMtx);
                            createdOrderId.erase(ptr);
                        }
                        cout << "[RANDOM] Попытка удалить закрытый ордер. Пользователь " << username << ", ордер " << orderId << endl; 
                    }
                    else {
                        api.deleteOrder(orderId);
                        int* ptr = &createdOrderId[index];
                        {
                            lock_guard<mutex> ordersLock(orderMtx);
                            createdOrderId.erase(ptr);
                        }
                        cout << "[RANDOM] Удален ордер " << orderId << ", пользователь " << username << endl;
                    }
                }
                catch (const exception& e) {
                    cerr << "[RANDOM] Ошибка при удалении ордера пользователем " << username << ", ошибка: " << e.what() << endl;
                }
            }

            uniform_int_distribution<size_t> pairDist(0, pairs.get_size() - 1);
            size_t pairIdx = pairDist(rng);

            int pairId = stoi(pairs[pairIdx].pairId);
            int baseLotId = stoi(pairs[pairIdx].firstLotId);
            int quoteLotId = stoi(pairs[pairIdx].secondLotId);

            bool hasBuy = false;
            bool hasSell = false;
            double bestBuy = 0.0;
            double bestSell = 0.0;

            for (const auto& order: ordersJson) {
                string closed = order["closed"];
                bool isClosed = false;
                for (char c: closed) {
                    if (!isspace(static_cast<unsigned char>(c))) {
                        isClosed = true;
                        break;
                    }
                }
                if (isClosed || order["pair_id"] != pairId) {
                    continue;
                }

                double price = order["price"];
                string type = order["type"];

                if (type == "buy") {
                    if (!hasBuy || price > bestBuy) {
                        bestBuy = price;
                        hasBuy = true;
                    }
                }
                if (type == "sell") {
                    if (!hasSell || price < bestSell) {
                        bestSell = price;
                        hasSell = true;
                    }
                }
            }

            double marketPrice = 1.0;
            if (hasBuy && hasSell) {
                marketPrice = (bestBuy + bestSell) / 2.0;
            }
            else if (hasBuy) {
                marketPrice = bestBuy;
            }
            else if (hasSell) {
                marketPrice = bestSell;
            }

            if (marketPrice <= 0.0) {
                marketPrice = 1.0;
            }

            double baseBalance = 0.0;
            double quoteBalance = 0.0;

            for (const auto& item: balanceJson) {
                int lotId = item["lot_id"];
                double quantity = item["quantity"];
                if (lotId == baseLotId) {
                    baseBalance = quantity;
                }
                if (lotId == quoteLotId) {
                    quoteBalance = quantity;
                }
            }

            uniform_real_distribution<double> typeDist(0.0, 1.0);
            string orderType;

            if (quoteBalance <= 0.001 && baseBalance <= 0.001) {
                orderType = (typeDist(rng) < 0.5) ? "buy" : "sell";
            }
            else if (quoteBalance < baseBalance / marketPrice * 0.2) {
                orderType = "buy";
            }
            else if (quoteBalance > baseBalance / marketPrice * 5.0) {
                orderType = "sell";
            }
            else {
                orderType = (typeDist(rng) < 0.5) ? "buy" : "sell";
            }

            uniform_real_distribution<double> priceDist(0.95, 1.05);
            double price = marketPrice * priceDist(rng);
            if (price < 0.0001) {
                price = 0.0001;
            }

            double quantity;
            uniform_real_distribution<double> qtyDist(0.01, 0.1);

            if (orderType == "buy") {
                double maxQty = quoteBalance / price * qtyDist(rng);
                quantity = 0.001;
                if (maxQty > 0.001) {
                    quantity = maxQty;
                    if (quantity > 100.0) {
                        quantity = 100.0;
                    }
                }
            }
            else {
                double maxQty = baseBalance * qtyDist(rng);
                quantity = 0.001;
                if (maxQty > 0.001) {
                    quantity = maxQty;
                    if (quantity > 100.0) {
                        quantity = 100.0;
                    }
                }
            }

            int orderId = api.createOrder(pairId, quantity, price, orderType);
            {
                lock_guard<mutex> ordersLock(orderMtx);
                createdOrderId.push_back(orderId);
            }
            cout << "[RANDOM] Создан ордер " << orderId << ", " << orderType << ". Пара: " << pairId << ", цена: " << price << ", объем: " << quantity << endl; 
        }
        catch (const exception& e) {
            cerr << "[RANDOM] Ошибка при создании ордера, пользователь: " << username << ". Ошибка: " << e.what() << endl;
        }
        {
            unique_lock<mutex> lock(mtx);
            if (running.load()) {
                cv.wait_for(lock, chrono::milliseconds(1000), [this]() { return !running.load(); });
            }
        }
    }
}

double RandomBot::getRUBBalance() {
    try {
        return api.getBalanceInRUB();
    }
    catch (const exception& e) {
        cerr << "[RANDOM] Ошибка при получении баланса в рублях. Пользователь: " << username << ". Ошибка: " << e.what() << endl;
        return 0.0;
    }
}

void RandomBot::printStats() {
    try {
        double rubBalance = getRUBBalance();
        cout << "[RANDOM] У пользователя " << username << " " << rubBalance << " RUB.\n";
    }
    catch (const exception& e) {
        cerr << "[RANDOM] Ошибка вывода баланса пользователя " << username << ". Ошибка: " << e.what() << endl;
    }
}