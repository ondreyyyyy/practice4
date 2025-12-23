#include "exchangeAPI.h"
#include "Vector.h"
#include <iostream>
#include <mutex>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>

using namespace std;

ExchangeAPI::ExchangeAPI() {
    int port = 7432;
    string host = "127.0.0.1";
    ifstream cfgFile("config.json");
    if (cfgFile.is_open()) {
        try {
            json cfg;
            cfgFile >> cfg;
            port = cfg.at("database_port").get<int>();
            host = cfg.at("database_ip").get<string>();
        }
        catch (const exception& e) {
            cout << "[CLIENT] Ошибка json. Порт и адрес по умолчанию.\n";
        }
    }

    if (host == "localhost") {
        host = "127.0.0.1";
    }

    serverUrl = host + ":" + to_string(port);
}

string ExchangeAPI::sendRequest(
    const string& method,
    const string& endpoint,
    const json& body,
    const Vector<string>& headers
) {
    size_t pos = serverUrl.find(':');
    string host = serverUrl.substr(0, pos);
    int port = stoi(serverUrl.substr(pos + 1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) throw runtime_error("socket error");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        throw runtime_error("connect error");
    }

    string request =
        method + " " + endpoint + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n";

    for (size_t i = 0; i < headers.get_size(); ++i)
        request += headers[i] + "\r\n";

    string bodyStr = body.is_null() ? "" : body.dump();
    if (!bodyStr.empty())
        request += "Content-Length: " + to_string(bodyStr.size()) + "\r\n";

    request += "\r\n" + bodyStr;

    send(sock, request.c_str(), request.size(), 0);

    string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
        response.append(buf, n);

    close(sock);

    stringstream ss(response);
    string statusLine;
    getline(ss, statusLine);

    int status;
    string http;
    stringstream(statusLine) >> http >> status;

    string line, bodyResp;
    while (getline(ss, line) && line != "\r") {}
    while (getline(ss, line)) bodyResp += line;

    if (status >= 400) {
        throw runtime_error("[CLIENT] API ошибка " + to_string(status));
    }

    return bodyResp;
}

string ExchangeAPI::createUser(const string& username) {
    json req;
    req["username"] = username;

    string resp = sendRequest("POST", "/user", req);
    json j = json::parse(resp);

    lock_guard<mutex> lock(userkeyMtx);
    userKey = j.at("key").get<string>();
    return userKey;
}

void ExchangeAPI::setUserKey(const string& key) {
    userKey = key;
}

json ExchangeAPI::getLots() {
    string response = sendRequest("GET", "/lot");
    return json::parse(response);
}

json ExchangeAPI::getPairs() {
    string response = sendRequest("GET", "/pair");
    return json::parse(response);
}

json ExchangeAPI::getBalance() {
    if (userKey.empty()) {
        throw runtime_error("[CLIENT] Не установлен ключ пользователя");
    }
    
    Vector<string> headerVector;
    headerVector.push_back("X-USER-KEY: " + userKey);
    
    string response = sendRequest("GET", "/balance", json(), headerVector);
    return json::parse(response);
}

json ExchangeAPI::getAllOrders() {
    string response = sendRequest("GET", "/order");
    return json::parse(response);
}

int ExchangeAPI::createOrder(int pairId, double quantity, double price, const string& type) {
    if (userKey.empty()) {
        throw runtime_error("[CLIENT] Не установлен ключ пользователя");
    }
    
    json request;
    request["pair_id"] = pairId;
    request["quantity"] = quantity;
    request["price"] = price;
    request["type"] = type;
    
    Vector<string> headers;
    headers.push_back("X-USER-KEY: " + userKey);
    
    try {
        string response = sendRequest("POST", "/order", request, headers);
        json result = json::parse(response);
        return result.at("order_id").get<int>();
    }
    catch (const json::out_of_range& exc) {
        throw runtime_error("[CLIENT] Ошибка при создании ордера: ответ не содержит order_id");
    }
    catch (const json::exception& e) {
        throw runtime_error("[CLIENT] Ошибка при создании ордера: " + string(e.what()));
    }
}

bool ExchangeAPI::deleteOrder(int orderId) {
    if (userKey.empty()) {
        throw runtime_error("[CLIENT] Не установлен ключ пользователя");
    }
    
    json request;
    request["order_id"] = orderId;
    
    Vector<string> headers;
    headers.push_back("X-USER-KEY: " + userKey);
    
    try {
        string response = sendRequest("DELETE", "/order", request, headers);
        json result = json::parse(response);
        return result.contains("order_id");
    } 
    catch (const exception& e) {
        return false;
    }
}

double ExchangeAPI::getBalanceInRUB() {
    json lots = getLots();
    json balances = getBalance();
    
    int rubId = -1;
    for (const auto& lot : lots) {
        if (lot["name"] == "RUB") {
            rubId = lot["lot_id"];
            break;
        }
    }
    
    if (rubId == -1) return 0.0;
    
    for (const auto& balance : balances) {
        if (balance["lot_id"] == rubId) {
            return balance["quantity"].get<double>();
        }
    }
    
    return 0.0;
}

json ExchangeAPI::getActiveOrder(int pairId) {
    json allOrders = getAllOrders();
    json result = json::array();
    
    for (const auto& order : allOrders) {
        string closed = order["closed"];
        bool isEmpty = true;
        for (char c : closed) {
            if (!isspace(static_cast<unsigned char>(c))) {
                isEmpty = false;
                break;
            }
        }
        
        if (!isEmpty) {
            continue;
        }
        
        if (pairId == -1 || order["pair_id"] == pairId) {
            result.push_back(order);
        }
    }
    
    return result;
}