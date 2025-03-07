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

#include "SendPackerInfo.h"

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
	virtual void sendLoop();

	std::string _name;
	std::string _ip;
	unsigned short _port;
	std::string _clientIp;
	unsigned short _clientPort;

	SOCKET _socket;

	std::thread _receiveThread;
	std::thread _processThread;

	std::atomic<bool> _running = false;

protected:
	long long _lastSendMs = 0;
	int _tick = 10;

	unsigned short _sNetVersion = 2025;
	unsigned short _sMask = 0x0000;
	int _bSize = 0;
};

/*-----------------
	Handle Core
-----------------*/
class HandleCore : public Core
{
public:
	HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort);
	virtual void sendToHost(const std::vector<unsigned char>& buffer);
	virtual void sendLoop();

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

private:

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

private:
};