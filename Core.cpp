#include "Core.h"
#include <iostream>
#include "ProcessPacket.h"
#include "Utils.h"
#include "RecvPacketInfo.h"

Core::Core(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: _name(name), _ip(ip), _port(port), _clientIp(clientIp), _clientPort(clientPort), _running(false), _socket(INVALID_SOCKET)
{
	std::cout << "[" << _name << "]";
	std::cout << " UDP Server running on";
	std::cout << " IP: " << ip;
	std::cout << " PORT: " << port;
	std::cout << '\n';
}

Core::~Core()
{
	stop();

}

void Core::start()
{
	// Initialize socket
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed" << std::endl;
		return;
	}

	_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (_socket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed" << std::endl;
		return;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(_ip.c_str());
	serverAddr.sin_port = htons(_port);

	if (bind(_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Bind failed" << std::endl;
		return;
	}

	_running.store(true);
	_receiveThread = std::thread(&Core::receiveLoop, this);
	_processThread = std::thread(&Core::sendLoop, this);
}

void Core::stop()
{
	if (_running) {
		_running = false;
		if (_receiveThread.joinable()) {
			_receiveThread.join();
		}
		if (_processThread.joinable()) {
			_processThread.join();
		}
		closesocket(_socket);
		WSACleanup();
	}
}

void Core::sendToHost(const std::vector<unsigned char>& buffer, const std::string& targetIp, unsigned short targetPort)
{
	sockaddr_in targetAddr;
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_addr.s_addr = inet_addr(targetIp.c_str());
	targetAddr.sin_port = htons(targetPort);

	sendto(_socket, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
}

void Core::receiveLoop()
{
	while (_running) {
		try
		{
			sockaddr_in clientAddr;
			int clientAddrSize = sizeof(clientAddr);
			std::vector<unsigned char> buffer(1024);

			int recvLen = recvfrom(_socket, (char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
			if (recvLen == SOCKET_ERROR) {
				int errorCode = WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK || errorCode == WSAETIMEDOUT || errorCode == WSAECONNRESET) {
					continue;  // 무시하고 계속 수신
				}
				/*if (errorCode == 10054) 
				{
					continue;
				}*/
				std::cerr << "Failed to receive data: " << errorCode << std::endl;
				continue;  // 다른 오류인 경우 반복문 계속
			}

			if (recvLen < sizeof(RecvPacketHeader))
			{
				std::cerr << "Data Size Not Enough: received " << recvLen << " bytes" << std::endl;
				continue;
			}

			//unsigned short sNetVersion = *reinterpret_cast<const unsigned short*>(buffer.data()); // 2바이트
			//short sMask = *reinterpret_cast<const short*>(buffer.data() + sizeof(unsigned short)); // 2바이트
			unsigned char bSize = *(buffer.data() + sizeof(unsigned short) + sizeof(short)); // 1바이트

			if ((int)bSize > recvLen)
			{
				std::cerr << "Data Size Not Enough: expected " << (int)bSize << " bytes, received " << recvLen << " bytes" << std::endl;
				continue;
			}

			buffer.resize(recvLen);
			std::cout << "Received data: " << recvLen << " bytes" << std::endl;  // 추가된 로그
			ProcessPacket::handlePacket(buffer);
		}
		catch (const std::exception& e)
		{
			std::cerr << std::string(e.what()) << '\n';
			continue;
		}
	}
}

void Core::sendLoop()
{
	while (_running) {
	}
}

/*-----------------
	Handle Core
-----------------*/
HandleCore::HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = 33;
	_sMask = 0x0011;
}

void HandleCore::sendToHost(const std::vector<unsigned char>& buffer)
{
	// TEST JSON TO UE
	_clientIp = "192.168.10.101";
	_clientPort = 1997;

	UINT size = _bSize + sizeof(JsonPacketHeader);
	UINT id = 6002;
	UINT seq = 0;

	std::vector<unsigned char> newBuffer;

	// size (4 bytes)
	newBuffer.resize(newBuffer.size() + sizeof(size));
	std::memcpy(newBuffer.data() + newBuffer.size() - sizeof(size), &size, sizeof(size));

	// id (4 bytes)
	newBuffer.resize(newBuffer.size() + sizeof(id));
	std::memcpy(newBuffer.data() + newBuffer.size() - sizeof(id), &id, sizeof(id));

	// seq (4 bytes)
	newBuffer.resize(newBuffer.size() + sizeof(seq));
	std::memcpy(newBuffer.data() + newBuffer.size() - sizeof(seq), &seq, sizeof(seq));

	// 원래의 buffer 데이터를 추가
	newBuffer.insert(newBuffer.end(), buffer.begin(), buffer.end());

	// 전송
	std::cout << "[SEND] " << _clientIp.c_str() << ":" << _clientPort << '\n';
	sockaddr_in targetAddr;
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_addr.s_addr = inet_addr(_clientIp.c_str());
	targetAddr.sin_port = htons(_clientPort);

	int a = sendto(_socket, (const char*)newBuffer.data(), newBuffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
	if (a == SOCKET_ERROR) {
		std::cout << "[SEND ERROR] " << WSAGetLastError() << '\n';
	}

	return;

	//////////////////////////////////

	/*std::cout << "[SEND] " << _clientIp.c_str() << ":" << _clientPort << '\n';
	sockaddr_in targetAddr;
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_addr.s_addr = inet_addr(_clientIp.c_str());
	targetAddr.sin_port = htons(_clientPort);

	int result = sendto(_socket, (const char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
	if (result == SOCKET_ERROR) {
		std::cout << "[SEND ERROR] " << WSAGetLastError() << '\n';
	}*/
}

void HandleCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		std::vector<unsigned char> buffer(33);  // 전체 버퍼 크기: 헤더 5바이트 + 데이터 28바이트

		// 헤더 데이터
		//unsigned short sNetVersion = 2025;
		//unsigned char bSize = 33;  // 헤더 포함 전체 패킷 크기 49바이트
		//unsigned short sMask = 0x011;

		// 헤더 메모리 복사
		std::memcpy(buffer.data(), &_sNetVersion, sizeof(unsigned short));   // 0~1 바이트에 sNetVersion
		std::memcpy(buffer.data() + 2, &_sMask, sizeof(unsigned short));      // 2~3 바이트에 sMask
		std::memcpy(buffer.data() + 4, &_bSize, sizeof(unsigned char));       // 4~5 바이트에 bSize

		// 데이터 메모리 복사
		std::memcpy(buffer.data() + 5, &_sendHandlePacket.simState, sizeof(unsigned __int32));    // 5~8 바이트에 simState
		std::memcpy(buffer.data() + 9, &_sendHandlePacket.velocity, sizeof(float));               // 9~12 바이트에 velocity
		std::memcpy(buffer.data() + 13, &_sendHandlePacket.wheelAngleVelocityLF, sizeof(float));  // 13~16 바이트에 wheelAngleVelocityLF
		std::memcpy(buffer.data() + 17, &_sendHandlePacket.wheelAngleVelocityRF, sizeof(float));  // 17~20 바이트에 wheelAngleVelocityRF
		std::memcpy(buffer.data() + 21, &_sendHandlePacket.wheelAngleVelocityLB, sizeof(float));  // 21~24 바이트에 wheelAngleVelocityLB
		std::memcpy(buffer.data() + 25, &_sendHandlePacket.wheelAngleVelocityRB, sizeof(float));  // 25~28 바이트에 wheelAngleVelocityRB
		std::memcpy(buffer.data() + 29, &_sendHandlePacket.targetAngle, sizeof(float));           // 29~32 바이트에 targetAngle

		sendToHost(buffer);
	}
}

/*-----------------
	CabinControl Core
-----------------*/
CabinControlCore::CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = 27;
	_sMask = 0x0012;
}

void CabinControlCore::sendToHost(const std::vector<unsigned char>& buffer)
{
}

/*-----------------
	CanbinSwitch Core
-----------------*/
CanbinSwitchCore::CanbinSwitchCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = 0;
	_sMask = 0x0013;
}

void CanbinSwitchCore::sendToHost(const std::vector<unsigned char>& buffer)
{
}

/*-----------------
	Motion Core
-----------------*/
MotionCore::MotionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = 272;
	_sMask = 0x0014;
}

void MotionCore::sendToHost(const std::vector<unsigned char>& buffer)
{
}
