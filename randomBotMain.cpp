#include "randomBot.h"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace std;

atomic<bool> stopSignal(false);

void signalHandler(int signum) {
    stopSignal.store(true);
}

int main() {
    signal(SIGINT, signalHandler);
    
    try {
        RandomBot bot("random");
        bot.run();
        
        while (!stopSignal.load()) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        bot.stop();
        cout << "\n----------------------------------------" << endl;
        cout << "Финальная статистика:" << endl;
        bot.printStats();
        
    } catch (const exception& e) {
        cerr << "Ошибка в работе бота: " << e.what() << endl;
        return 1;
    }
    
    cout << "Бот успешно остановлен." << endl;
    return 0;
}