#include "smartBot.h"
#include "exchangeAPI.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace std;

static atomic<bool> running(true);
static SmartBot* botPtr = nullptr;

void signalHandler(int signum) {
    if (signum == SIGINT) {
        running.store(false);
        if (botPtr) {
            botPtr->stop();
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    try {
        signal(SIGINT, signalHandler);
        SmartBot bot("neverov");
        botPtr = &bot;

        bot.run();

        cout << "Запуск.\n";

        while (running.load()) {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
    catch (const exception& e) {
        cerr << "[SMART-MAIN] Ошибка: " << e.what() << endl;
        return 1;
    }

    cout << "Отключен.\n";
    return 0;
}