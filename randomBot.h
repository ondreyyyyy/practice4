#ifndef RANDOMBOT_H
#define RANDOMBOT_H

#include "exchangeAPI.h"
#include "Vector.h"
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>

using namespace std;

class RandomBot {
private:
    ExchangeAPI api;
    string username;
    string userKey;
    atomic<bool> running;
    Vector<int> createdOrderId;
    thread workerThread;
    mutex mtx;
    mutex orderMtx;
    condition_variable cv;

    string generateUsername(const string& basename);
    void workerFunc();

public:
    RandomBot(const string& name = "neverov");
    ~RandomBot();
    
    void run();
    void stop();
    
    double getRUBBalance();
    void printStats();
};

#endif