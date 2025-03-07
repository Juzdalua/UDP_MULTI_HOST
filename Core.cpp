#include "Core.h"
#include <iostream>
#include "ProcessPacket.h"

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
	_processThread = std::thread(&Core::processLoop, this);
}

void Core::stop()
{
	if (_running) {
		_running = false;
		_queueCv.notify_all();
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
		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(clientAddr);
		std::vector<unsigned char> buffer(1024);

		int recvLen = recvfrom(_socket, (char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
		if (recvLen > 0) {
			buffer.resize(recvLen);
			std::lock_guard<std::mutex> lock(_queueMutex);
			_taskQueue.push(buffer);
			_queueCv.notify_one();
			std::cout << "Received data: " << recvLen << " bytes" << std::endl;  // 추가된 로그
		}
		else {
			int errorCode = WSAGetLastError();
			std::cerr << "Failed to receive data or no data received: "<<errorCode << std::endl;
			continue;
		}
	}
}

void Core::processLoop()
{
	while (_running) {
		std::unique_lock<std::mutex> lock(_queueMutex);
		_queueCv.wait(lock, [this]() { return !_taskQueue.empty(); });

		while (!_taskQueue.empty()) {
			std::vector<unsigned char> recvBuffer = _taskQueue.front();
			_taskQueue.pop();
			lock.unlock();

			// Process the data (e.g., send to another host)
			// Here you can add the logic to process and forward data
			// Example: send to another host
			//sendToHost(data, "127.0.0.1", 12345);

			ProcessPacket::handlePacket(recvBuffer) ;

			/*unsigned short sNetVersion = *reinterpret_cast<const unsigned short*>(&recvBuffer[0]);
			short sMask = *reinterpret_cast<const short*>(&recvBuffer[2]);
			unsigned char bSize = recvBuffer[4];

			std::cout << "bytesTransferred: " << recvBuffer.size() << '\n';
			std::cout << "Received Header - NetVersion: " << sNetVersion
				<< ", Mask: " << sMask
				<< ", bSize: " << (int)bSize << std::endl;*/

			lock.lock();
		}
	}
}

/*-----------------
	Handle Core
-----------------*/
HandleCore::HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
}

void HandleCore::sendToHost(const std::vector<unsigned char>& buffer)
{
	std::cout << "[SEND] "<< _clientIp.c_str()<<":"<< _clientPort<<'\n';
	sockaddr_in targetAddr;
	targetAddr.sin_family = AF_INET;
	/*targetAddr.sin_addr.s_addr = inet_addr(_clientIp.c_str());
	targetAddr.sin_port = htons(_clientPort);*/
	std::string ip = "192.168.10.101";
	int port = 1997;
	targetAddr.sin_addr.s_addr = inet_addr(ip.c_str());
	targetAddr.sin_port = htons(port);

	int a = sendto(_socket, (const char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
	if (a == SOCKET_ERROR) {
		std::cout << "[SEND ERROR] " << WSAGetLastError() << '\n';
	}
}

/*-----------------
	CabinControl Core
-----------------*/
CabinControlCore::CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{

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
}

void MotionCore::sendToHost(const std::vector<unsigned char>& buffer)
{
}
