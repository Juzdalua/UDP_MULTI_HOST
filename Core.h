#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

class Core {
public:
    Core(const std::string& name, const std::string& ip, unsigned short port);
    ~Core();

    void start();
    void stop();
    void sendToHost(const std::vector<unsigned char>& data, const std::string& targetIp, unsigned short targetPort);

private:
    void receiveLoop();
    void processLoop();

    std::string _name;
    std::string _ip;
    unsigned short _port;

    SOCKET _socket;

    std::thread _receiveThread;
    std::thread _processThread;

    std::queue<std::vector<unsigned char>> _taskQueue;
    std::mutex _queueMutex;
    std::condition_variable _queueCv;

    std::atomic<bool> _running = false;
};
