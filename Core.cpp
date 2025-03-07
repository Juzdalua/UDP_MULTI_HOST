#include "Core.h"
#include <iostream>

Core::Core(const std::string& name, const std::string& ip, unsigned short port)
	: _name(name), _ip(ip), _port(port), _running(false), _socket(INVALID_SOCKET)
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

void Core::sendToHost(const std::vector<unsigned char>& data, const std::string& targetIp, unsigned short targetPort)
{
	sockaddr_in targetAddr;
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_addr.s_addr = inet_addr(targetIp.c_str());
	targetAddr.sin_port = htons(targetPort);

	sendto(_socket, (const char*)data.data(), data.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
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

			unsigned short sNetVersion = *reinterpret_cast<const unsigned short*>(&recvBuffer[0]);
			short sMask = *reinterpret_cast<const short*>(&recvBuffer[2]);
			unsigned char bSize = recvBuffer[4];

			std::cout << "bytesTransferred: " << recvBuffer.size() << '\n';
			std::cout << "Received Header - NetVersion: " << sNetVersion
				<< ", Mask: " << sMask
				<< ", bSize: " << (int)bSize << std::endl;

			lock.lock();
		}
	}
}
