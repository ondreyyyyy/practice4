#include "api.h"
#include "auxiliary.h"
#include "file.h"
#include "select.h"
#include "structures.h"
#include "insert.h"
#include "nlohmann/json.hpp"
#include <random>
#include <shared_mutex>
#include <mutex>
#include <sstream>
#include <cmath>

using json = nlohmann::json;
using namespace std;

const double EPSILON = 0.000001;

string makeHttpResponse(int statusCode, const string& body) {
    string statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 201: statusText = "Created"; break;
        case 400: statusText = "Bad Request"; break;
        case 401: statusText = "Unauthorized"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown";
    }
    
    return "HTTP/1.1 " + to_string(statusCode) + " " + statusText + "\r\n" + "Content-Type: application/json\r\n" + "Content-Length: " + to_string(body.size()) + "\r\n" + "\r\n" + body;
}

string getUserIdByKey(DatabaseManager& dbManager, const string& userKey) {
    try {
        Vector<string> selectCol = {"user.user_id"};
        Vector<string> tables = {"user"};
        Vector<Condition> cond;

        string safeKey = escape(userKey);
        cond.push_back(Condition{"user.key", safeKey, "="});

        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);
        if (results.get_size() > 0) {
            return results[0];
        }
        return "";
    }
    catch (const exception& e) {
        return "";
    }
}

string generateUserKey() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int keyLength = 32;
    string key;
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    for (int i = 0; i < keyLength; ++i) {
        key += alphanum[dis(gen)];
    }
    
    return key;
}

string handleGetLots(DatabaseManager& dbManager) {
    try {
        cout << "[INFO] Запрос списка лотов" << endl;
        Vector<string> selectCol = {"lot.lot_id", "lot.name"};
        Vector<string> tables = {"lot"};
        Vector<Condition> conditions;

        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, conditions, results);

        json response = json::array();

        for (size_t i = 0; i < results.get_size(); i++) {
            stringstream ss(results[i]);
            string lotId, name;
            
            getline(ss, lotId, ',');
            getline(ss, name, ',');

            json lot;
            lot["lot_id"] = stoi(lotId);
            lot["name"] = name;
            response.push_back(lot);
        }
        return makeHttpResponse(200, response.dump(4));
    }
    catch (const exception& e) {
        json error;
        error["error"] = "Internal server error33";
        error["message"] = e.what();
        return makeHttpResponse(500, error.dump(4));
    }
}

string handleGetPairs(DatabaseManager& dbManager) {
    try {
        cout << "[INFO] Запрос списка пар" << endl;
        Vector<string> selectCol = {"pair.pair_id", "pair.first_lot_id", "pair.second_lot_id"};
        Vector<string> tables = {"pair"};
        Vector<Condition> cond;
        
        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);

        json response = json::array();

        for (size_t i = 0; i < results.get_size(); i++) {
            stringstream ss(results[i]);
            string pairId, firstLotId, secondLotId;

            getline(ss, pairId, ',');
            getline(ss, firstLotId, ',');
            getline(ss, secondLotId, ',');

            json pair;
            pair["pair_id"] = stoi(pairId);
            pair["sale_lot_id"] = stoi(firstLotId);
            pair["buy_lot_id"] = stoi(secondLotId);
            response.push_back(pair);
        }

        return makeHttpResponse(200, response.dump(4));
    }
    catch (const exception& e) {
        json error;
        error["error"] = "Internal server error44";
        error["message"] = e.what();
        return makeHttpResponse(500, error.dump(4));
    }
}

string handleCreateUser(DatabaseManager& dbManager, const string& body) {
    static mutex orderCreationMutex;
    lock_guard<mutex> mainLock(orderCreationMutex);
    try {
        cout << "[INFO] Создание пользователя: " << body << endl;
        json request = parseJsonBody(body);

        if (!hasField(request, "username")) {
            json error = {{"error", "Не заполнено: username"}};
            return makeHttpResponse(400, error.dump(4));
        }

        string username = request["username"].get<string>();
        if (username.empty()) {
            json error = {{"error", "Username не может быть пустым"}};
            return makeHttpResponse(400, error.dump(4));
        }

        string pkPath = dbManager.getSchemaName() + "/user/user_pk_sequence";
        int userId = 1;
        ifstream pkFile(pkPath);
        if (pkFile.is_open()) {
            pkFile >> userId;
            pkFile.close();
        }
        
        string userKey = generateUserKey();
        int oldUserId = userId;

        string safeUsername = escape(username);
        string safeKey = escape(userKey);
        string query = "VALUES('" + safeUsername + "','" + safeKey + "')";
        insertData(dbManager, "user", query, userId);

        Vector<string> lotCol = {"lot.lot_id"};
        Vector<string> lotTables = {"lot"};
        Vector<Condition> lotCond;
        Vector<string> lotResult;
        selectDataCapture(dbManager, lotCol, lotTables, lotCond, lotResult);

        for (size_t i = 0; i < lotResult.get_size(); i++) {
            string lotId = lotResult[i];
            string userLotPkPath = dbManager.getSchemaName() + "/user_lot/user_lot_pk_sequence";
            int userLotPk = 1;
            ifstream userLotPkFile(userLotPkPath);
            if (userLotPkFile.is_open()) {
                userLotPkFile >> userLotPk;
                userLotPkFile.close();
            }

            string userLotQuery = "VALUES('" + to_string(oldUserId) + "','" + lotId + "','1000.000000')";
            insertData(dbManager, "user_lot", userLotQuery, userLotPk);
        }

        json response;
        response["key"] = userKey;
        return makeHttpResponse(201, response.dump(4));
    }
    catch (const exception& e) {
        json error = {{"error", "Internal server error55"}, {"message", e.what()}};
        return makeHttpResponse(500, error.dump(4));
    }
}

string handleGetBalance(DatabaseManager& dbManager, const string& userKey) {
    try {
        cout << "[INFO] Пользователь " << userKey << " запрашивает баланс" << endl;
        if (userKey.empty()) {
            json error = {{"error", "Нет заголовка X-USER-KEY"}};
            return makeHttpResponse(401, error.dump(4));
        }

        string userId = getUserIdByKey(dbManager, userKey);
        if (userId.empty()) {
            json error = {{"error", "Некорректный ключ пользователя"}};
            return makeHttpResponse(403, error.dump(4));
        }

        Vector<string> selectCol = {"user_lot.lot_id", "user_lot.quantity"};
        Vector<string> tables = {"user_lot"};
        Vector<Condition> cond;
        cond.push_back(Condition{"user_lot.user_id", userId, "="});

        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);

        json response = json::array();
        for (size_t i = 0; i < results.get_size(); i++) {
            stringstream ss(results[i]);
            string lotId, quantity;
            getline(ss, lotId, ',');
            getline(ss, quantity, ',');
            json balance;
            balance["lot_id"] = stoi(lotId);
            balance["quantity"] = stod(quantity);
            response.push_back(balance);
        }
        return makeHttpResponse(200, response.dump(4));
    }
    catch (const exception& e) {
        json error = {{"error", "Internal server error66"}, {"message", e.what()}};
        return makeHttpResponse(500, error.dump(4));
    }
}

string handleGetOrders(DatabaseManager& dbManager) {
    try {
        cout << "[INFO] Запрос списка ордеров" << endl;
        Vector<string> selectCol = {
            "order.order_id", 
            "order.user_id", 
            "order.pair_id", 
            "order.quantity", 
            "order.price", 
            "order.type", 
            "order.closed"
        };
        Vector<string> tables = {"order"};
        Vector<Condition> cond;
        
        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);
        json response = json::array();
        
        for (size_t i = 0; i < results.get_size(); i++) {
            stringstream ss(results[i]);
            string orderId, userId, pairId, quantity, price, type, closed;
            
            getline(ss, orderId, ',');
            getline(ss, userId, ',');
            getline(ss, pairId, ',');
            getline(ss, quantity, ',');
            getline(ss, price, ',');
            getline(ss, type, ',');
            getline(ss, closed, ',');
            
            json order;
            order["order_id"] = stoi(orderId);
            order["user_id"] = stoi(userId);
            order["pair_id"] = stoi(pairId);
            order["quantity"] = stod(quantity);
            order["price"] = stod(price);
            order["type"] = type;
            order["closed"] = closed;
            response.push_back(order);
        }
        
        return makeHttpResponse(200, response.dump(4));
        
    } catch (const exception& e) {
        json error = {{"error", "Internal server error77"}, {"message", e.what()}};
        return makeHttpResponse(500, error.dump(4));
    }
}

PairInfo getPairInfo(DatabaseManager& dbManager, const string& pairId) {
    PairInfo info;
    Vector<string> selectCol = {"pair.first_lot_id", "pair.second_lot_id"};
    Vector<string> tables = {"pair"};
    Vector<Condition> cond;
    cond.push_back(Condition{"pair.pair_id", pairId, "="});
    
    Vector<string> results;
    selectDataCapture(dbManager, selectCol, tables, cond, results);
    
    if (!results.empty()) {
        stringstream ss(results[0]);
        getline(ss, info.firstLotId, ',');
        getline(ss, info.secondLotId, ',');
        info.pairId = pairId;
    }
    return info;
}

double getUserBalance(DatabaseManager& dbManager, const string& userId, const string& lotId) {
    Vector<string> selectCol = {"user_lot.quantity"};
    Vector<string> tables = {"user_lot"};
    Vector<Condition> cond;
    cond.push_back(Condition{"user_lot.user_id", userId, "="});
    cond.push_back(Condition{"user_lot.lot_id", lotId, "="});
    
    Vector<string> results;
    selectDataCapture(dbManager, selectCol, tables, cond, results);
    
    if (!results.empty()) {
        return stod(results[0]);
    }
    return 0.0;
}

void updateUserBalance(DatabaseManager& dbManager, const string& userId, const string& lotId, double delta) {
    try {
        if (abs(delta) < EPSILON) {
            return;
        }

        const string schema = dbManager.getSchemaName();
        string tableName = "user_lot";
        
        int fileIndex = 1;
        bool found = false;
        bool recordUpdated = false;
        
        while (true) {
            string csvPath = schema + "/" + tableName + "/" + to_string(fileIndex) + ".csv";
            ifstream in(csvPath);
            
            if (!in.is_open()) {
                break;
            }
            
            string tmpPath = csvPath + ".tmp";
            ofstream out(tmpPath);
            
            if (!out.is_open()) {
                in.close();
                return;
            }
            
            string header;
            getline(in, header);
            out << header << "\n";
            
            Vector<string> headerCols = splitCSV(header);
            size_t colCount = headerCols.get_size();
            
            int userIdIdx = -1, lotIdIdx = -1, quantityIdx = -1;
            for (size_t i = 0; i < colCount; i++) {
                if (headerCols[i] == "user_id") userIdIdx = i;
                if (headerCols[i] == "lot_id") lotIdIdx = i;
                if (headerCols[i] == "quantity") quantityIdx = i;
            }
            
            if (userIdIdx == -1 || lotIdIdx == -1 || quantityIdx == -1) {
                in.close();
                out.close();
                remove(tmpPath.c_str());
                return;
            }
            
            string line;
            
            while (getline(in, line)) {
                if (line.empty()) continue;
                
                Vector<string> values = splitCSV(line);
                if (values.get_size() >= colCount) {
                    string currentUserId = values[userIdIdx];
                    string currentLotId = values[lotIdIdx];
                    
                    if (currentUserId == userId && currentLotId == lotId) {
                        found = true;
                        double currentBalance = stod(values[quantityIdx]);
                        double newBalance = currentBalance + delta;
                        
                        if (newBalance < -EPSILON) {
                            in.close();
                            out.close();
                            remove(tmpPath.c_str());
                            throw runtime_error("Отрицательный баланс у пользователя");
                        }
                        
                        if (newBalance < EPSILON) {
                            recordUpdated = true;
                            continue;
                        }
                        
                        values[quantityIdx] = to_string(newBalance);
                        
                        string newLine;
                        for (size_t i = 0; i < values.get_size(); i++) {
                            if (i > 0) newLine += ",";
                            newLine += values[i];
                        }
                        
                        out << newLine << "\n";
                        recordUpdated = true;
                        continue;
                    }
                }
                
                out << line << "\n";
            }
            
            in.close();
            out.close();
            
            if (recordUpdated) {
                remove(csvPath.c_str());
                rename(tmpPath.c_str(), csvPath.c_str());
                return;
            } else {
                remove(tmpPath.c_str());
            }
            
            fileIndex++;
        }
        
        if (!found && delta > EPSILON) {
            fileIndex = 1;
            string lastFilePath;
            
            while (true) {
                string csvPath = schema + "/" + tableName + "/" + to_string(fileIndex) + ".csv";
                ifstream test(csvPath);
                if (!test.is_open()) {
                    break;
                }
                test.close();
                lastFilePath = csvPath;
                fileIndex++;
            }
            
            if (lastFilePath.empty()) {
                lastFilePath = schema + "/" + tableName + "/1.csv";
            }
            
            ofstream out(lastFilePath, ios::app);
            if (out.is_open()) {
                string pkPath = schema + "/" + tableName + "/" + tableName + "_pk_sequence";
                int newPk = 1;
                ifstream pkFile(pkPath);
                if (pkFile.is_open()) {
                    pkFile >> newPk;
                    pkFile.close();
                }
                
                out << newPk << "," << userId << "," << lotId << "," << to_string(delta) << "\n";
                out.close();
                
                ofstream pkOut(pkPath);
                if (pkOut.is_open()) {
                    pkOut << (newPk + 1);
                    pkOut.close();
                }
            }
        } else if (!found && delta < 0) {
            throw runtime_error("Ошибка поиска баланса");
        }
            
    } catch (const exception& e) {
        throw;
    }
}

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << now_time_t;
    return ss.str();
}

bool updateOrderQuantity(DatabaseManager& dbManager, const string& orderId, double newQuantity) {
    const string schema = dbManager.getSchemaName();
    string tableName = "order";
    
    int fileIndex = 1;
    
    while (true) {
        string csvPath = schema + "/" + tableName + "/" + to_string(fileIndex) + ".csv";
        ifstream in(csvPath);
        
        if (!in.is_open()) {
            break;
        }
        
        string tmpPath = csvPath + ".tmp";
        ofstream out(tmpPath);
        
        if (!out.is_open()) {
            in.close();
            return false;
        }
        
        string header;
        getline(in, header);
        out << header << "\n";
        
        Vector<string> headerCols = splitCSV(header);
        size_t colCount = headerCols.get_size();
        
        int orderIdIdx = -1, quantityIdx = -1;
        for (size_t i = 0; i < colCount; i++) {
            if (headerCols[i] == "order_id") orderIdIdx = i;
            if (headerCols[i] == "quantity") quantityIdx = i;
        }
        
        if (orderIdIdx == -1 || quantityIdx == -1) {
            in.close();
            out.close();
            remove(tmpPath.c_str());
            return false;
        }
        
        string line;
        bool updated = false;
        
        while (getline(in, line)) {
            if (line.empty()) continue;
            
            Vector<string> values = splitCSV(line);
            if (values.get_size() >= colCount) {
                string currentOrderId = values[orderIdIdx];
                
                if (currentOrderId == orderId) {
                    updated = true;
                    values[quantityIdx] = to_string(newQuantity);
                    
                    string newLine;
                    for (size_t i = 0; i < values.get_size(); i++) {
                        if (i > 0) newLine += ",";
                        newLine += values[i];
                    }
                    
                    out << newLine << "\n";
                    continue;
                }
            }
            
            out << line << "\n";
        }
        
        in.close();
        out.close();
        
        if (updated) {
            remove(csvPath.c_str());
            rename(tmpPath.c_str(), csvPath.c_str());
            return true;
        }
        
        remove(tmpPath.c_str());
        fileIndex++;
    }
    
    return false;
}

bool closeOrderWithTimestamp(DatabaseManager& dbManager, const string& orderId, const string& timestamp) {
    const string schema = dbManager.getSchemaName();
    string tableName = "order";
    
    int fileIndex = 1;
    string closeTime = timestamp.empty() ? getCurrentTimestamp() : timestamp;
    
    while (true) {
        string csvPath = schema + "/" + tableName + "/" + to_string(fileIndex) + ".csv";
        ifstream in(csvPath);
        
        if (!in.is_open()) {
            break;
        }
        
        string tmpPath = csvPath + ".tmp";
        ofstream out(tmpPath);
        
        if (!out.is_open()) {
            in.close();
            return false;
        }
        
        string header;
        getline(in, header);
        out << header << "\n";
        
        Vector<string> headerCols = splitCSV(header);
        size_t colCount = headerCols.get_size();
        
        int orderIdIdx = -1, closedIdx = -1;
        for (size_t i = 0; i < colCount; i++) {
            if (headerCols[i] == "order_id") orderIdIdx = i;
            if (headerCols[i] == "closed") closedIdx = i;
        }
        
        if (orderIdIdx == -1 || closedIdx == -1) {
            in.close();
            out.close();
            remove(tmpPath.c_str());
            return false;
        }
        
        string line;
        bool foundAndUpdated = false;
        
        while (getline(in, line)) {
            if (line.empty()) continue;
            
            Vector<string> values = splitCSV(line);
            if (values.get_size() >= colCount) {
                string currentOrderId = values[orderIdIdx];
                
                if (currentOrderId == orderId) {
                    foundAndUpdated = true;
                    values[closedIdx] = closeTime;
                    
                    string newLine;
                    for (size_t i = 0; i < values.get_size(); i++) {
                        if (i > 0) newLine += ",";
                        newLine += values[i];
                    }
                    
                    out << newLine << "\n";
                    continue;
                }
            }
            
            out << line << "\n";
        }
        
        in.close();
        out.close();
        
        if (foundAndUpdated) {
            remove(csvPath.c_str());
            rename(tmpPath.c_str(), csvPath.c_str());
            return true;
        }
        
        remove(tmpPath.c_str());
        fileIndex++;
    }
    
    return false;
}

void unlockFundsForOrder(DatabaseManager& dbManager, const string& userId, const string& orderType, const string& assetLot, const string& currencyLot, double quantity, double price) {
    string lotToUnlock;
    double amountToUnlock;
    
    if (orderType == "buy") {
        lotToUnlock = currencyLot;
        amountToUnlock = quantity * price;
    }
    else {
        lotToUnlock = assetLot;
        amountToUnlock = quantity;
    }
    
    updateUserBalance(dbManager, userId, lotToUnlock, +amountToUnlock);
}

string handleCreateOrder(DatabaseManager& dbManager, const string& body, const string& userKey) {
    static mutex createOrderMutex;
    lock_guard<mutex> lock(createOrderMutex);
    try {
        cout << "[INFO] Пользователь " << userKey << " создаёт ордер: " << body << endl;
        
        if (userKey.empty()) {
            return makeHttpResponse(401, R"({"error": "Отсутствует заголовок X-USER-KEY"})");
        }
        
        string userId = getUserIdByKey(dbManager, userKey);
        if (userId.empty()) {
            return makeHttpResponse(403, R"({"error": "Неверный ключ пользователя"})");
        }
        
        json request = parseJsonBody(body);
        if (!hasField(request, "pair_id") || !hasField(request, "quantity") || 
            !hasField(request, "price") || !hasField(request, "type")) {
            return makeHttpResponse(400, R"({"error": "Отсутствуют обязательные поля"})");
        }
        
        int pairIdInt = request["pair_id"].get<int>();
        string pairId = to_string(pairIdInt);
        double originalQuantity = request["quantity"].get<double>();
        double ourPrice = request["price"].get<double>();
        string orderType = request["type"].get<string>();
        
        if (orderType != "buy" && orderType != "sell") {
            return makeHttpResponse(400, R"({"error": "тип ордера только 'buy' или 'sell'"})");
        }
        
        if (originalQuantity <= 0 || ourPrice <= 0) {
            return makeHttpResponse(400, R"({"error": "запрос и цена должны быть положительными"})");
        }
        
        Vector<string> pairSelectCol = {"pair.first_lot_id", "pair.second_lot_id"};
        Vector<string> pairTables = {"pair"};
        Vector<Condition> pairCond;
        pairCond.push_back(Condition{"pair.pair_id", pairId, "="});
        
        Vector<string> pairResults;
        selectDataCapture(dbManager, pairSelectCol, pairTables, pairCond, pairResults);
        
        if (pairResults.empty()) {
            return makeHttpResponse(404, R"({"error": "Пара не найдена"})");
        }
        
        stringstream pairSS(pairResults[0]);
        string assetLot, currencyLot;
        getline(pairSS, assetLot, ',');
        getline(pairSS, currencyLot, ',');
        
        if (orderType == "buy") {
            double lockedAmount = originalQuantity * ourPrice;
            double currentBalance = getUserBalance(dbManager, userId, currencyLot);
            if (currentBalance < lockedAmount) {
                json error = {{"error", "Недостаточно средств"}, {"запрошено", lockedAmount}, {"доступно", currentBalance}};
                return makeHttpResponse(400, error.dump(4));
            }
            
            updateUserBalance(dbManager, userId, currencyLot, -lockedAmount);
        } else if (orderType == "sell") {
            double currentBalance = getUserBalance(dbManager, userId, assetLot);
            if (currentBalance < originalQuantity) {
                json error = {{"error", "Недостаточно средств"}, {"запрошено", originalQuantity}, {"доступно", currentBalance}};
                return makeHttpResponse(400, error.dump(4));
            }
            
            updateUserBalance(dbManager, userId, assetLot, -originalQuantity);
        }
        
        string oppositeType = (orderType == "buy") ? "sell" : "buy";
        
        Vector<string> selectCol = {
            "order.order_id", 
            "order.user_id", 
            "order.quantity", 
            "order.price", 
            "order.type",
            "order.closed"
        };
        Vector<string> tables = {"order"};
        Vector<Condition> cond;
        cond.push_back(Condition{"order.pair_id", pairId, "="});
        cond.push_back(Condition{"order.type", oppositeType, "="});
        cond.push_back(Condition{"order.closed", "", "="});
        
        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);
        
        Vector<string> matchingOrderIds;
        Vector<string> matchingUserIds;
        Vector<double> matchingQuantities;
        Vector<double> matchingPrices;
        
        for (size_t i = 0; i < results.get_size(); i++) {
            stringstream ss(results[i]);
            string orderId, matchUserId, qtyStr, priceStr, type, closed;
            
            getline(ss, orderId, ',');
            getline(ss, matchUserId, ',');
            getline(ss, qtyStr, ',');
            getline(ss, priceStr, ',');
            getline(ss, type, ',');
            getline(ss, closed, ',');

            auto isReallyEmpty = [](const string& s) -> bool {
                if (s.empty()) return true;
                for (char c : s) {
                    if (!isspace(static_cast<unsigned char>(c))) {
                        return false;
                    }
                }
                return true;
            };

            if (!isReallyEmpty(closed)) {
                continue;
            }

            if (matchUserId == userId) {
                continue;
            }
            
            double matchQuantity = stod(qtyStr);
            double matchPrice = stod(priceStr);
            
            bool priceMatch = false;
            if (orderType == "buy") {
                priceMatch = (matchPrice <= ourPrice + EPSILON);
            } else {
                priceMatch = (matchPrice >= ourPrice - EPSILON);
            }

            if (orderType == type) { 
                continue;  
            }
            
            if (priceMatch && matchQuantity > EPSILON) {
                matchingOrderIds.push_back(orderId);
                matchingUserIds.push_back(matchUserId);
                matchingQuantities.push_back(matchQuantity);
                matchingPrices.push_back(matchPrice);
            }
        }
        
        if (orderType == "buy") {
            for (size_t i = 0; i < matchingOrderIds.get_size(); i++) {
                for (size_t j = i + 1; j < matchingOrderIds.get_size(); j++) {
                    if (matchingPrices[j] < matchingPrices[i]) {
                        swap(matchingOrderIds[i], matchingOrderIds[j]);
                        swap(matchingUserIds[i], matchingUserIds[j]);
                        swap(matchingQuantities[i], matchingQuantities[j]);
                        swap(matchingPrices[i], matchingPrices[j]);
                    }
                }
            }
        }
        
        double remainingQuantity = originalQuantity;
        double executedQuantity = 0;
        double totalExecutedValue = 0;
        bool anyTradeExecuted = false;
        
        for (size_t i = 0; i < matchingOrderIds.get_size() && remainingQuantity > EPSILON; i++) {
            string matchOrderId = matchingOrderIds[i];
            string matchUserId = matchingUserIds[i];
            double matchQuantity = matchingQuantities[i];
            double executionPrice = (orderType == "buy") ? matchingPrices[i] : ourPrice;
            
            double tradeQuantity = min(remainingQuantity, matchQuantity);
            double tradeValue = tradeQuantity * executionPrice;
            
            totalExecutedValue += tradeValue;
            anyTradeExecuted = true;
            
            if (orderType == "buy") {
                updateUserBalance(dbManager, userId, assetLot, +tradeQuantity);
                updateUserBalance(dbManager, matchUserId, currencyLot, +tradeValue);
                
                Vector<string> sellOrderSelectCol = {"order.price"};
                Vector<string> sellOrderTables = {"order"};
                Vector<Condition> sellOrderCond;
                sellOrderCond.push_back(Condition{"order.order_id", matchOrderId, "="});
                
                Vector<string> sellOrderResults;
                selectDataCapture(dbManager, sellOrderSelectCol, sellOrderTables, sellOrderCond, sellOrderResults);
                
                if (!sellOrderResults.empty()) {
                    stringstream sellOrderSS(sellOrderResults[0]);
                    string sellOrderPriceStr;
                    getline(sellOrderSS, sellOrderPriceStr, ',');
                    
                    double sellOrderPrice = stod(sellOrderPriceStr);
                    if (sellOrderPrice < executionPrice + EPSILON) {
                        double excessPerUnit = executionPrice - sellOrderPrice;
                        double totalExcess = tradeQuantity * excessPerUnit;
                        updateUserBalance(dbManager, userId, currencyLot, +totalExcess);
                    }
                }
            }
            else if (orderType == "sell") {
                updateUserBalance(dbManager, userId, currencyLot, +tradeValue);
                updateUserBalance(dbManager, matchUserId, assetLot, +tradeQuantity);
                
                Vector<string> buyOrderSelectCol = {"order.price"};
                Vector<string> buyOrderTables = {"order"};
                Vector<Condition> buyOrderCond;
                buyOrderCond.push_back(Condition{"order.order_id", matchOrderId, "="});
                
                Vector<string> buyOrderResults;
                selectDataCapture(dbManager, buyOrderSelectCol, buyOrderTables, buyOrderCond, buyOrderResults);
                
                if (!buyOrderResults.empty()) {
                    stringstream buyOrderSS(buyOrderResults[0]);
                    string buyOrderPriceStr;
                    getline(buyOrderSS, buyOrderPriceStr, ',');
                    
                    double buyOrderPrice = stod(buyOrderPriceStr);
                    if (buyOrderPrice > executionPrice + EPSILON) {
                        double excessPerUnit = buyOrderPrice - executionPrice;
                        double totalExcess = tradeQuantity * excessPerUnit;
                        updateUserBalance(dbManager, matchUserId, currencyLot, +totalExcess);
                    }
                }
            }
            
            double newMatchQuantity = matchQuantity - tradeQuantity;
            
            if (newMatchQuantity <= EPSILON) {
                closeOrderWithTimestamp(dbManager, matchOrderId);
            } 
            else {
                updateOrderQuantity(dbManager, matchOrderId, newMatchQuantity);
                    
                string pkPath = dbManager.getSchemaName() + "/order/order_pk_sequence";
                int newPk = 1;
                ifstream pkFile(pkPath);
                if (pkFile.is_open()) {
                    pkFile >> newPk;
                    pkFile.close();
                }
                
                string closedField = getCurrentTimestamp();
                string executionPriceStr = to_string(executionPrice);
                string orderQuery = "VALUES('" + matchUserId + "','" + pairId + "','" + to_string(tradeQuantity) + "','" + executionPriceStr + "','" + oppositeType + "','" + closedField + "')";
                    
                insertData(dbManager, "order", orderQuery, newPk);
            }
            
            remainingQuantity -= tradeQuantity;
            executedQuantity += tradeQuantity;
        }
        
        if (anyTradeExecuted) {
            if (orderType == "buy") {
                double initiallyLocked = originalQuantity * ourPrice;
                double actuallySpent = totalExecutedValue;
                double amountToReturn = initiallyLocked - actuallySpent - (remainingQuantity * ourPrice);
                
                if (amountToReturn > EPSILON) {
                    updateUserBalance(dbManager, userId, currencyLot, +amountToReturn);
                }
            }
        }
        
        int responseId;
        
        if (executedQuantity > EPSILON && remainingQuantity > EPSILON) {
            double avgExecutionPrice = totalExecutedValue / executedQuantity;
            
            string pkPath = dbManager.getSchemaName() + "/order/order_pk_sequence";
            int orderPk = 1;
            ifstream pkFile(pkPath);
            if (pkFile.is_open()) {
                pkFile >> orderPk;
                pkFile.close();
            }
            responseId = orderPk;
            
            string closedField = getCurrentTimestamp();
            string orderQuery = "VALUES('" + userId + "','" + pairId + "','" + to_string(executedQuantity) + "','" + to_string(avgExecutionPrice) + "','" + orderType + "','" + closedField + "')";
            
            insertData(dbManager, "order", orderQuery, orderPk);
            
            closedField = "";
            orderQuery = "VALUES('" + userId + "','" + pairId + "','" + to_string(remainingQuantity) + "','" + to_string(ourPrice) + "','" + orderType + "','" + closedField + "')";
            insertData(dbManager, "order", orderQuery, orderPk);
                 
        } else if (executedQuantity > EPSILON) {
            string pkPath = dbManager.getSchemaName() + "/order/order_pk_sequence";
            int orderPk = 1;
            ifstream pkFile(pkPath);
            if (pkFile.is_open()) {
                pkFile >> orderPk;
                pkFile.close();
            }
            responseId = orderPk;
            
            double avgExecutionPrice = totalExecutedValue / executedQuantity;
            string closedField = getCurrentTimestamp();
            string orderQuery = "VALUES('" + userId + "','" + pairId + "','" + to_string(executedQuantity) + "','" + to_string(avgExecutionPrice) + "','" + orderType + "','" + closedField + "')";
            insertData(dbManager, "order", orderQuery, orderPk);
                 
        } else {
            string pkPath = dbManager.getSchemaName() + "/order/order_pk_sequence";
            int orderPk = 1;
            ifstream pkFile(pkPath);
            if (pkFile.is_open()) {
                pkFile >> orderPk;
                pkFile.close();
            }
            responseId = orderPk;
            
            string closedField = "";
            string orderQuery = "VALUES('" + userId + "','" + pairId + "','" + to_string(originalQuantity) + "','" + to_string(ourPrice) + "','" + orderType + "','" + closedField + "')";
            insertData(dbManager, "order", orderQuery, orderPk);
        }
        
        json response;
        response["order_id"] = responseId;

        return makeHttpResponse(201, response.dump(4));
        
    } catch (const exception& e) {
        json error = {{"error", "Internal server error22"}, {"message", e.what()}};
        return makeHttpResponse(500, error.dump(4));
    }
}

bool canDeleteOrder(DatabaseManager& dbManager, const string& orderId, const string& userId) {
    Vector<string> selectCol = {"order.user_id", "order.closed"};
    Vector<string> tables = {"order"};
    Vector<Condition> cond;
    cond.push_back(Condition{"order.order_id", orderId, "="});
    
    Vector<string> results;
    selectDataCapture(dbManager, selectCol, tables, cond, results);
    
    if (results.empty()) {
        return false;
    }
    
    stringstream ss(results[0]);
    string orderUserId, closed;
    getline(ss, orderUserId, ',');
    getline(ss, closed, ',');
    return (orderUserId == userId && closed.empty());
}

string handleDeleteOrder(DatabaseManager& dbManager, const string& body, const string& userKey) {
    try {
        cout << "[INFO] Пользователь " << userKey << " удаляет ордер: " << body << endl;
        
        if (userKey.empty()) {
            return makeHttpResponse(401, R"({"error": "Нет заголовка X-USER-KEY"})");
        }
        
        string userId = getUserIdByKey(dbManager, userKey);
        if (userId.empty()) {
            return makeHttpResponse(403, R"({"error": "Неверный ключ пользователя"})");
        }
        
        json request = parseJsonBody(body);
        if (!hasField(request, "order_id")) {
            return makeHttpResponse(400, R"({"error": "Отсутствует order_id"})");
        }
        
        string orderId = to_string(request["order_id"].get<int>());
        
        if (!canDeleteOrder(dbManager, orderId, userId)) {
            return makeHttpResponse(403, R"({"error": "Удаление данного ордера невозможно"})");
        }
        
        Vector<string> selectCol = {
            "order.user_id", 
            "order.pair_id", 
            "order.quantity", 
            "order.price", 
            "order.type"
        };
        Vector<string> tables = {"order"};
        Vector<Condition> cond;
        cond.push_back(Condition{"order.order_id", orderId, "="});
        
        Vector<string> results;
        selectDataCapture(dbManager, selectCol, tables, cond, results);
        
        if (results.empty()) {
            return makeHttpResponse(404, R"({"error": "Ордер не найден"})");
        }
        
        stringstream ss(results[0]);
        string orderUserId, pairId, orderQuantity, orderPrice, orderType;
        getline(ss, orderUserId, ',');
        getline(ss, pairId, ',');
        getline(ss, orderQuantity, ',');
        getline(ss, orderPrice, ',');
        getline(ss, orderType, ',');
        
        double quantity = stod(orderQuantity);
        double price = stod(orderPrice);
        
        PairInfo pair = getPairInfo(dbManager, pairId);
        if (!pair.firstLotId.empty() && !pair.secondLotId.empty()) {
            string assetLot = pair.firstLotId;
            string currencyLot = pair.secondLotId;
            
            unlockFundsForOrder(dbManager, userId, orderType, assetLot, currencyLot, quantity, price);
        }
        
        closeOrderWithTimestamp(dbManager, orderId);
        
        json response;
        response["order_id"] = stoi(orderId);
        
        return makeHttpResponse(200, response.dump(4));
        
    } catch (const exception& e) {
        json error = {{"error", "Internal server error11"}, {"message", e.what()}};
        return makeHttpResponse(500, error.dump(4));
    }
}