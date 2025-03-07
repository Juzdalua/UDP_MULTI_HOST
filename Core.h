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
    Core(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
    virtual ~Core();

    void start();
    void stop();
    virtual void sendToHost(const std::vector<unsigned char>& buffer, const std::string& targetIp, unsigned short targetPort);

protected:
    void receiveLoop();
    void processLoop();

    std::string _name;
    std::string _ip;
    unsigned short _port;
    std::string _clientIp;
    unsigned short _clientPort;

    SOCKET _socket;

    std::thread _receiveThread;
    std::thread _processThread;

    std::queue<std::vector<unsigned char>> _taskQueue;
    std::mutex _queueMutex;
    std::condition_variable _queueCv;

    std::atomic<bool> _running = false;
};

/*-----------------
    Handle Core
-----------------*/
struct SendHandlePacket {
    UINT32 simState;
    float velocity;
    float wheelAngleVelocityLF;
    float wheelAngleVelocityRF;
    float wheelAngleVelocityLB;
    float wheelAngleVelocityRB;
    float targetAngle;

};
class HandleCore : public Core 
{
public:
    HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
    virtual void sendToHost(const std::vector<unsigned char>& buffer);

private:
    SendHandlePacket _sendHandlePacket = { 0 };
};

/*-----------------
    CabinControl Core
-----------------*/
class CabinControlCore : public Core
{
public:
    CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
    virtual void sendToHost(const std::vector<unsigned char>& buffer);
};

/*-----------------
    CanbinSwitch Core
-----------------*/
class CanbinSwitchCore : public Core
{
public:
    CanbinSwitchCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
    virtual void sendToHost(const std::vector<unsigned char>& buffer);
};

/*-----------------
    Motion Core
-----------------*/
class MotionCore : public Core
{
public:
    MotionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
    virtual void sendToHost(const std::vector<unsigned char>& buffer);
};