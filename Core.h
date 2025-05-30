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
#include <unordered_set>
#include <map>

#include "SendPacketInfo.h"

#pragma comment(lib, "ws2_32.lib")

enum class PeerType : UINT {
	DEFAULT = 0,
	INNO,
	UE,
};

extern std::unordered_set<std::string> headerIncludeClass;
extern std::unordered_set<std::string> headerExcludeClass;

class Core {
public:
	Core(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	virtual ~Core();

	void start();
	void stop();

protected:
	void receiveLoop();
	void sendTo(const std::vector<unsigned char>& buffer, const std::string& targetIp, unsigned short targetPort);
	virtual void sendLoop() = 0;

	virtual void handlePacket(const std::vector<unsigned char>& buffer);
	virtual void handleInnoPacket(const std::vector<unsigned char>& buffer) = 0;
	virtual void handleUePacket(const std::vector<unsigned char>& buffer) = 0;

	std::string _name = "";
	std::string _ip = "";
	unsigned short _port;

	std::string _scheduledSendIp = "";
	unsigned short _scheduledSendPort = 0;

	std::string _directSendIp = "";
	unsigned short _directSendPort = 0;

	SOCKET _socket = INVALID_SOCKET;

	std::thread _receiveThread;
	std::thread _processThread;

	std::atomic<bool> _running = false;

protected:

	long long _lastSendMs = 0;
	int _tick = 0;

	PeerType _peerType = PeerType::DEFAULT;

public:
	int _recvPacketSize = 0;
};

/*-----------------
	Handle Core
-----------------*/
class HandleCore : public Core
{
public:
	HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;
};

/*-----------------
	CabinControl Core
-----------------*/
class CabinControlCore : public Core
{
public:
	CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;

	void setHandleHaptic(bool isOn);
	void setSeatBeltHaptic(bool isOn, int interval = 0);

	//uint16_t _hapticBit = 0b0000'0000'0000'0000;
	uint16_t _beltHapticOffBit = 0b0000'0000'0000'0001;
	int _adjustHandleStrength = 13'000;
	int _adjustSeatBeltStrength = 11'000;
};

/*-----------------
	CanbinSwitch Core
-----------------*/
class CanbinSwitchCore : public Core
{
public:
	CanbinSwitchCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;
};

/*-----------------
	Motion Core
-----------------*/
class MotionCore : public Core
{
public:
	MotionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;
};

/*-----------------
	Timemachine Core
-----------------*/
class TimemachineCore : public Core
{
public:
	TimemachineCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;

	int _status = -1;
};

/*-----------------
	CheckConnection Core
-----------------*/
class CheckConnectionCore : public Core
{
public:
	CheckConnectionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType);
	void sendLoop() override;

	void handleInnoPacket(const std::vector<unsigned char>& buffer) override;
	void handleUePacket(const std::vector<unsigned char>& buffer) override;
};

extern std::map<int, std::vector<SendTimemachinePacket>> sendTimemachinePktByTimestamp;

extern int _timemachinePacketSize;
extern std::atomic<bool> _isTimemachineMode;
extern std::atomic<bool> _isHandleComplete;
extern std::atomic<bool> _isCabinSwitchComplete;
extern std::atomic<bool> _isMotionComplete;