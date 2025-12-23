#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include "auxiliary.h"
#include "structures.h"
#include "file.h"
#include "Vector.h"
#include "api.h"

using namespace std;
using json = nlohmann::json;

bool headersComplete(const string& data) {
    for (size_t i = 0; i + 3 < data.size(); i++) {
        if (data[i] == '\r' && data[i + 1] == '\n' && data[i + 2] == '\r' && data[i + 3] == '\n') {
            return true;
        }
    }
    return false;
}

void handleClient(int clientSocket, DatabaseManager& dbManager, mutex& dbMutex) {
    char buffer[4096] = {0};
    cerr << "[INFO] Клиент подключен." << endl;
    string raw;
    ssize_t bytes;

    bool headersFinished = false;
    while ((bytes = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        raw.append(buffer, bytes);
        if (headersComplete(raw)) {
            headersFinished = true;
            break;
        }
    }

    if (raw.empty() || !headersFinished) {
        close(clientSocket);
        return;
    }

    stringstream ss(raw);
    string requestLine;
    getline(ss, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    string method, path, http;
    stringstream rl(requestLine);
    rl >> method >> path >> http;

    size_t contentLength = 0;
    string line;
    while (true) {
        if (!getline(ss, line)) {
            break;
        }

        if (line.empty() || line == "\r") {
            break;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.size() > 15 && line.substr(0, 15) == "Content-Length:") {
            try {
                string lenStr = line.substr(15);
                lenStr.erase(0, lenStr.find_first_not_of(" \t"));
                contentLength = stoi(lenStr);
            }
            catch (const exception& e) {
                contentLength = 0;
            }
        }
    }

    string body;
    if (contentLength > 0) {
        size_t headersEnd = 0;
        bool found = false;
        for (size_t i = 0; i + 3 < raw.size(); i++) {
            if (raw[i] == '\r' && raw[i+1] == '\n' && 
                raw[i+2] == '\r' && raw[i+3] == '\n') {
                headersEnd = i + 4;
                found = true;
                break;
            }
        }

        if (found && raw.size() > headersEnd) {
            body = raw.substr(headersEnd);
        }
        while (body.size() < contentLength) {
            bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                break;
            }
            body.append(buffer, bytes);
        }

        if (body.size() > contentLength) {
            body.resize(contentLength);
        }
    }

    string response;
    {
        string userKey;
        stringstream header(raw);
        string headerLine;
        while (getline(header, headerLine)) {
            if (headerLine.empty() || headerLine == "\r") {
                break;
            }
            if (headerLine.back() == '\r') {
                headerLine.pop_back();
            }
            
            if (headerLine.size() > 11 && headerLine.substr(0, 11) == "X-USER-KEY:") {
                userKey = headerLine.substr(11);
                while (!userKey.empty() && userKey[0] == ' ') {
                    userKey.erase(0, 1);
                }
                break;
            }
        }

        if (method == "POST" && path == "/user") {
            lock_guard<mutex> lock(dbMutex);
            response = handleCreateUser(dbManager, body);
        } 
        else if (method == "GET" && path == "/lot") {
            lock_guard<mutex> lock(dbMutex);
            response = handleGetLots(dbManager);
        }
        else if (method=="POST" && path=="/order") {
            lock_guard<mutex> lock(dbMutex);
            response = handleCreateOrder(dbManager, body, userKey);
        }
        else if (method == "GET" && path == "/order") {
            lock_guard<mutex> lock(dbMutex);
            response = handleGetOrders(dbManager);
        }
        else if (method=="DELETE" && path=="/order") {
            lock_guard<mutex> lock(dbMutex);
            response = handleDeleteOrder(dbManager, body, userKey);
        }
        else if (method == "GET" && path == "/pair") {
            lock_guard<mutex> lock(dbMutex);
            response = handleGetPairs(dbManager);
        }
        else if (method == "GET" && path == "/balance") {
            lock_guard<mutex> lock(dbMutex);
            response = handleGetBalance(dbManager, userKey);
        }
        else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n\r\n{\"error\":\"endpoint not found\"}";
        }
    }

    ssize_t totalSent = 0;
    ssize_t toSend = response.size();

    while (totalSent < toSend) {
        ssize_t sent = send(clientSocket, response.c_str() + totalSent, toSend - totalSent, 0);
        if (sent <= 0) {
            close(clientSocket);
            return;
        }
        totalSent += sent;
    }

    close(clientSocket);
    cerr << "[INFO] Клиент отключен." << endl;
}

int main() {
    ios::sync_with_stdio(false);
    freopen("server.log", "a", stderr);
    DatabaseManager dbManager;
    mutex dbMutex;

    try {
        initCryptoDatabase(dbManager);
    }
    catch (const exception& e) {
        cerr << "Ошибка инициализации биржи.\n";
        return 1;
    }

    int serverPort = 7432; // по умолчанию
    string serverIP = "127.0.0.1";
    ifstream cfgFile("config.json");
    if (cfgFile.is_open()) {
        try {
            json cfg;
            cfgFile >> cfg;
            serverPort = cfg.at("database_port").get<int>();
            serverIP = cfg.at("database_ip").get<string>();
        }
        catch (const exception& e) {
            cout << "Ошибка json. Порт и адрес по умолчанию.\n";
        }
    }
    cfgFile.close();

    if (serverIP == "localhost") {
        serverIP = "127.0.0.1";
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Ошибка создания сокета" << endl;
        return 1;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP.c_str(), &addr.sin_addr) <= 0) {
        cerr << "Неверный IP-адрес: " << serverIP << endl;
        close(serverSocket);
        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        cerr << "Ошибка bind()" << endl;
        close(serverSocket);
        return 1;
    }
    
    if (listen(serverSocket, 100) < 0) {
        cerr << "Ошибка listen()" << endl;
        close(serverSocket);
        return 1;
    }

    cout << "Сервер запущен. Порт: " << serverPort << endl;
    while (true) {
        sockaddr_in client;
        socklen_t cl = sizeof(client);
        int clientSkt = accept(serverSocket, reinterpret_cast<sockaddr*>(&client), &cl);
        if (clientSkt < 0) {
            cerr << "Ошибка подключения" << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(client.sin_port);
        
        cerr << "[INFO] Новое подключение: " << clientIP << ":" << clientPort << endl;

        thread clientThread(handleClient, clientSkt, ref(dbManager), ref(dbMutex));
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}