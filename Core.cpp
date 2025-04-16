#include "Core.h"
#include <iostream>
#include "Utils.h"
#include "RecvPacketInfo.h"
#include "SendPacketInfo.h"

std::unordered_set<std::string> headerIncludeClass = { "INNO_HANDLE", "INNO_CABIN_CONTROL", "INNO_CABIN_SWITCH" };
std::unordered_set<std::string> headerExcludeClass = { "INNO_MOTION", "UE_HANDLE", "UE_CABIN_CONTROL", "UE_CABIN_SWITCH", "UE_MOTION", "TIMEMACHINE" };
std::map<int, std::vector<SendTimemachinePacket>> sendTimemachinePktByTimestamp;

int _timemachinePacketSize = 0;
std::atomic<bool> _isTimemachineMode = false;
std::atomic<bool> _isHandleComplete = false;
std::atomic<bool> _isCabinSwitchComplete = false;
std::atomic<bool> _isMotionComplete = false;

Core::Core(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: _name(name), _ip(ip), _port(port), _scheduledSendIp(clientIp), _scheduledSendPort(clientPort), _peerType(peerType), _running(false), _socket(INVALID_SOCKET)
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
	try
	{
		// Initialize socket
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			std::cerr << "WSAStartup failed" << '\n';
			return;
		}

		_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (_socket == INVALID_SOCKET) {
			std::cerr << "Socket creation failed" << '\n';
			return;
		}

		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = inet_addr(_ip.c_str());
		serverAddr.sin_port = htons(_port);

		if (bind(_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			std::cerr << "Bind failed" << '\n';
			return;
		}

		_running.store(true);
		_receiveThread = std::thread(&Core::receiveLoop, this);
		if (_peerType == PeerType::INNO)  _processThread = std::thread(&Core::sendLoop, this);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Core start error: " + std::string(e.what()), "Core::start");
	}

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

void Core::receiveLoop()
{
	while (_running)
	{
		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(clientAddr);
		std::vector<unsigned char> buffer(1024);
		int recvLen = 0;

		// recv logic
		try
		{
			recvLen = recvfrom(_socket, (char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
			if (recvLen == SOCKET_ERROR)
			{
				int errorCode = WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK || errorCode == WSAETIMEDOUT || errorCode == WSAECONNRESET)
				{
					continue;
				}
				/*if (errorCode == 10054)
				{
					continue;
				}*/
				std::cerr << "Failed to receive data: " << errorCode << '\n';
				continue;
			}

			//std::cout << '\n' << "[RECV] " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << " -> " << _ip << ":" << _port << " [" << _name << "]" << '\n';

			if (headerIncludeClass.find(_name) != headerIncludeClass.end())
			{
				if (recvLen < sizeof(RecvPacketHeader))
				{
					std::cerr << "Data Size Not Enough / recvLen: " << recvLen << '\n';
					continue;
				}
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << std::string(e.what()) << '\n';
			Utils::LogError("Core::receiveLoop RECV error: " + std::string(e.what()), "Core::receiveLoop");
			continue;
		}

		// Parse buffer
		try
		{
			// Header 에러 체크
			if (headerIncludeClass.find(_name) == headerIncludeClass.end() && headerExcludeClass.find(_name) == headerExcludeClass.end())
			{
				std::cerr << "Header unordered_set error" << '\n';
				continue;
			}

			// Header 포함 패킷
			if (headerIncludeClass.find(_name) != headerIncludeClass.end())
			{
				RecvPacketHeader recvPacketHeader = { 0 };
				std::memcpy(&recvPacketHeader, buffer.data(), sizeof(RecvPacketHeader));

				if ((int)recvPacketHeader.bSize != recvLen)
				{
					std::cerr << "Data Size Not Enough / bSize: " << (int)recvPacketHeader.bSize << ", recvLen: " << recvLen << '\n';
					continue;
				}

				buffer.resize((int)recvPacketHeader.bSize);
				//std::cout << "Recv Len: " << recvLen << " / sNetVersion: " << recvPacketHeader.sNetVersion << " / sMask: " << recvPacketHeader.sMask << " / bSize: " << (int)recvPacketHeader.bSize << '\n';
			}

			// Header 없는 패킷
			else if (headerExcludeClass.find(_name) != headerExcludeClass.end())
			{
				int recvPacketSize = 0;
				if (_name == "INNO_MOTION" && typeid(*this) == typeid(MotionCore))
				{
					MotionCore* motionCore = dynamic_cast<MotionCore*>(this);
					recvPacketSize = motionCore->_recvPacketSize;
				}
				else if (_name == "UE_HANDLE" && typeid(*this) == typeid(HandleCore))
				{
					HandleCore* handleCore = dynamic_cast<HandleCore*>(this);
					recvPacketSize = handleCore->_recvPacketSize;
				}
				else if (_name == "UE_CABIN_CONTROL" && typeid(*this) == typeid(CabinControlCore))
				{
					CabinControlCore* cabinControlCore = dynamic_cast<CabinControlCore*>(this);
					recvPacketSize = cabinControlCore->_recvPacketSize;
				}
				else if (_name == "UE_CABIN_SWITCH" && typeid(*this) == typeid(CanbinSwitchCore))
				{
					CanbinSwitchCore* canbinSwitchCore = dynamic_cast<CanbinSwitchCore*>(this);
					recvPacketSize = canbinSwitchCore->_recvPacketSize;
				}
				else if (_name == "UE_MOTION" && typeid(*this) == typeid(MotionCore))
				{
					MotionCore* motionCore = dynamic_cast<MotionCore*>(this);
					recvPacketSize = motionCore->_recvPacketSize;
				}

				else if (_name == "TIMEMACHINE" && typeid(*this) == typeid(TimemachineCore))
				{
					TimemachineCore* timemachineCore = dynamic_cast<TimemachineCore*>(this);
					recvPacketSize = timemachineCore->_recvPacketSize;
				}

				if (recvPacketSize != recvLen)
				{
					std::cerr << "Data Size Not Enough [" << _name << "] / recvPacketSize: " << recvPacketSize << ", recvLen : " << recvLen << '\n';
					continue;
				}

				buffer.resize(recvPacketSize);
			}

			handlePacket(buffer);
		}
		catch (const std::exception& e)
		{
			Utils::LogError("Core::receiveLoop Header Parse error: " + std::string(e.what()), "Core::receiveLoop");
			continue;
		}
	}
}

void Core::sendTo(const std::vector<unsigned char>& buffer, const std::string& targetIp, unsigned short targetPort)
{
	try
	{
		sockaddr_in targetAddr = { 0 };
		targetAddr.sin_family = AF_INET;
		targetAddr.sin_addr.s_addr = inet_addr(targetIp.c_str());
		targetAddr.sin_port = htons(targetPort);

		//std::cout << "[SEND] " << _ip << ":" << _port << " [" << _name << "] -> " << targetIp << ":" << targetPort << '\n';
		int sendToHost = sendto(_socket, (const char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
		if (sendToHost == SOCKET_ERROR) {
			std::cout << "[SEND ERROR] " << WSAGetLastError() << '\n';
		}
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Core::sendTo Send error: " + std::string(e.what()), "Core::sendToHost");
	}
}

void Core::handlePacket(const std::vector<unsigned char>& buffer)
{
	switch (_peerType)
	{
	case PeerType::DEFAULT:
		break;
	case PeerType::INNO:
		handleInnoPacket(buffer);
		break;
	case PeerType::UE:
		handleUePacket(buffer);
		break;
	}
}

/*-----------------
	Handle Core
-----------------*/
HandleCore::HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_tick = 10; // 100hz

	if (_peerType == PeerType::INNO)
	{
		_directSendIp = Utils::getEnv("UE_IP");
		_directSendPort = stoi(Utils::getEnv("UE_HANDLE_PORT"));
	}
	else if (_peerType == PeerType::UE)
	{
		_recvPacketSize = sizeof(SendHandlePacket);
	}
}

void HandleCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		const int bufferSize = sizeof(SendHandlePacket);

		// 타임머신 리플레이
		if (_isTimemachineMode.load(std::memory_order_acquire) && !_isHandleComplete.load())
		{
			//std::cout << "START HANDLE" << '\n';
			auto localCopy = sendTimemachinePktByTimestamp;
			for (const auto& [idx, pkt] : localCopy)
			{
				if (pkt.size() == 0) continue;

				int pktSize = pkt.size();
				double interval = static_cast<double>(1000) / static_cast<double>(pktSize);

				for (int i = 0; i < pktSize; i++)
				{
					if (!_isTimemachineMode.load(std::memory_order_acquire))
					{
						_isHandleComplete.store(true, std::memory_order_release);
						//std::cout << "BREAK HANDLE" << '\n';
						break;
					}

					commonSendPacket->_sendHandlePacket.targetAngle = pkt[i].steering;
					commonSendPacket->_sendHandlePacket.velocity = pkt[i].velocity;

					/*std::cout << "taget angle: " << commonSendPacket->_sendHandlePacket.targetAngle << '\n';
					std::cout << "velocity: " << commonSendPacket->_sendHandlePacket.velocity << '\n';
					std::cout << '\n';*/

					std::vector<unsigned char> buffer(bufferSize);
					std::memcpy(buffer.data(), &commonSendPacket->_sendHandlePacket, sizeof(SendHandlePacket));
					sendTo(buffer, _scheduledSendIp, _scheduledSendPort);

					if (i < pktSize - 1)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval)));
					}

				}
				if (_isHandleComplete.load(std::memory_order_acquire)) break;
			}
			//std::cout << "HANDLE DONE" << '\n';
			_isHandleComplete.store(true, std::memory_order_release);
		}

		// 일반 주행
		else
		{
			if (now - _lastSendMs < _tick) continue;
			_lastSendMs = now;

			std::vector<unsigned char> buffer(bufferSize);
			/*std::memcpy(buffer.data(), &commonSendPacket->_sendHandlePacket, sizeof(SendHandlePacket));
			std::cout << "taget angle: " << commonSendPacket->_sendHandlePacket.targetAngle << '\n';
			std::cout << "velocity: " << commonSendPacket->_sendHandlePacket.velocity << '\n';
			std::cout << '\n';*/

			sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
		}
	}
}

void HandleCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonRecvPacket->_recvSteerPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SteerPacket));

		/*std::cout << " status: " << &commonRecvPacket->_recvSteerPacket.status;
		std::cout << " steerAngle: " << &commonRecvPacket->_recvSteerPacket.steerAngle;
		std::cout << " steerAngleRate: " << &commonRecvPacket->_recvSteerPacket.steerAngleRate;
		std::cout << '\n';*/

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("HandleCore::handleInnoPacket Parse error: " + std::string(e.what()), "HandleCore::handleInnoPacket");
	}
}

void HandleCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonSendPacket->_sendHandlePacket, buffer.data(), sizeof(SendHandlePacket));
		/*std::cout << "simState: " << commonSendPacket->_sendHandlePacket.simState << '\n';
		std::cout << "velocity: " << commonSendPacket->_sendHandlePacket.velocity << '\n';
		std::cout << "wheelAngleVelocityLF: " << commonSendPacket->_sendHandlePacket.wheelAngleVelocityLF << '\n';
		std::cout << "wheelAngleVelocityRF: " << commonSendPacket->_sendHandlePacket.wheelAngleVelocityRF << '\n';
		std::cout << "wheelAngleVelocityLB: " << commonSendPacket->_sendHandlePacket.wheelAngleVelocityLB << '\n';
		std::cout << "wheelAngleVelocityRB: " << commonSendPacket->_sendHandlePacket.wheelAngleVelocityRB << '\n';
		std::cout << "targetAngle: " << commonSendPacket->_sendHandlePacket.targetAngle << '\n';*/
	}
	catch (const std::exception& e)
	{
		Utils::LogError("HandleCore::handleUePacket Parse error: " + std::string(e.what()), "HandleCore::handleUePacket");
	}
}

/*-----------------
	CabinControl Core
-----------------*/
CabinControlCore::CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_tick = 1000; // 1hz
	//_tick = 100; // 10hz

	if (_peerType == PeerType::INNO)
	{
		_directSendIp = Utils::getEnv("UE_IP");
		_directSendPort = stoi(Utils::getEnv("UE_CABIN_CONTROL_PORT"));
	}
	else if (_peerType == PeerType::UE)
	{
		_recvPacketSize = sizeof(SendCabinControlPacket);
	}
}

void CabinControlCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendCabinControlPacket);
		std::vector<unsigned char> buffer(bufferSize);

		// test
		//commonSendPacket->_sendCabinControlPacket.avtivation = 

		std::memcpy(buffer.data(), &commonSendPacket->_sendCabinControlPacket, sizeof(SendCabinControlPacket));

		//std::cout << commonSendPacket->_sendCabinControlPacket.command << '\n';

		sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CabinControlCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonRecvPacket->_recvCabinControlPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(CabinControlPacket));

		/*std::cout << " status: " << commonRecvPacket->_recvCabinControlPacket.status;
		std::cout << " carHeight: " << commonRecvPacket->_recvCabinControlPacket.carHeight;
		std::cout << " carWidth: " << commonRecvPacket->_recvCabinControlPacket.carWidth;
		std::cout << " seatWidth: " << commonRecvPacket->_recvCabinControlPacket.seatWidth;
		std::cout << " digitalInput: " << commonRecvPacket->_recvCabinControlPacket.digitalInput;
		std::cout << '\n';*/

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CabinControlCore::handleInnoPacket Parse error: " + std::string(e.what()), "CabinControlCore::handleInnoPacket");
	}
}

void CabinControlCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonSendPacket->_sendCabinControlPacket, buffer.data(), sizeof(SendCabinControlPacket));
		/*std::cout << "command: " << commonSendPacket->_sendCabinControlPacket.command << '\n';
		std::cout << "seatHeight: " << commonSendPacket->_sendCabinControlPacket.seatHeight << '\n';*/
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CabinControlCore::handleUePacket Parse error: " + std::string(e.what()), "CabinControlCore::handleUePacket");
	}
}

/*-----------------
	CanbinSwitch Core
-----------------*/
CanbinSwitchCore::CanbinSwitchCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_tick = 10;
	commonSendPacket->_sendCabinSwitchPacket.currentGear = 'P';

	if (_peerType == PeerType::INNO)
	{
		_directSendIp = Utils::getEnv("UE_IP");
		_directSendPort = stoi(Utils::getEnv("UE_CABIN_SWITCH_PORT"));
	}
	else if (_peerType == PeerType::UE)
	{
		_recvPacketSize = sizeof(SendCabinSwitchPacket);
	}
}

void CanbinSwitchCore::sendLoop()
{
	while (_running)
	{
		while (_running)
		{
			long long now = Utils::GetNowTimeMs();
			const int bufferSize = sizeof(SendCabinSwitchPacket);

			if (_isTimemachineMode.load(std::memory_order_acquire) && !_isCabinSwitchComplete.load())
			{
				//std::cout << "START CabinSwitch" << '\n';

				auto localCopy = sendTimemachinePktByTimestamp;
				for (const auto& [idx, pkt] : localCopy)
				{
					if (pkt.size() == 0) continue;

					int pktSize = pkt.size();
					double interval = static_cast<double>(1000) / static_cast<double>(pktSize);

					for (int i = 0; i < pktSize; i++)
					{
						if (!_isTimemachineMode.load(std::memory_order_acquire))
						{
							_isCabinSwitchComplete.store(true, std::memory_order_release);
							//std::cout << "BREAK Cabin" << '\n';
							break;
						}

						commonSendPacket->_sendCabinSwitchPacket.currentGear = pkt[i].currentGear;
						commonSendPacket->_sendCabinSwitchPacket.brakeLamp = pkt[i].brakeLamp;
						commonSendPacket->_sendCabinSwitchPacket.leftLamp = pkt[i].leftLamp;
						commonSendPacket->_sendCabinSwitchPacket.rightLamp = pkt[i].rightLamp;
						commonSendPacket->_sendCabinSwitchPacket.alertLamp = pkt[i].alertLamp;
						commonSendPacket->_sendCabinSwitchPacket.reverseLamp = pkt[i].reverseLamp;
						commonSendPacket->_sendCabinSwitchPacket.headLamp = pkt[i].headLamp;

						std::vector<unsigned char> buffer(bufferSize);
						std::memcpy(buffer.data(), &commonSendPacket->_sendCabinSwitchPacket, sizeof(SendCabinSwitchPacket));
						sendTo(buffer, _scheduledSendIp, _scheduledSendPort);

						if (i < pktSize - 1)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval)));
						}
					}

					if (_isCabinSwitchComplete.load(std::memory_order_acquire)) break;
				}
				//std::cout << "CabinSwitch DONE" << '\n';
				_isCabinSwitchComplete.store(true, std::memory_order_release);
			}

			// 일반 주행
			else
			{
				if (now - _lastSendMs < _tick) continue;
				_lastSendMs = now;

				std::vector<unsigned char> buffer(bufferSize);
				std::memcpy(buffer.data(), &commonSendPacket->_sendCabinSwitchPacket, sizeof(SendCabinSwitchPacket));

				//commonSendPacket->_sendCabinSwitchPacket.alertLamp = 0;

				/*std::cout << "SEND LOOP" << '\n';
				std::cout << "currentGear: " << commonSendPacket->_sendCabinSwitchPacket.currentGear << '\n';
				std::cout << "brakeLamp: " << commonSendPacket->_sendCabinSwitchPacket.brakeLamp - 0 << '\n';
				std::cout << "leftLamp: " << commonSendPacket->_sendCabinSwitchPacket.leftLamp - 0 << '\n';
				std::cout << "rightLamp: " << commonSendPacket->_sendCabinSwitchPacket.rightLamp - 0 << '\n';
				std::cout << "alertLamp: " << commonSendPacket->_sendCabinSwitchPacket.alertLamp - 0 << '\n';
				std::cout << "reverseLamp: " << commonSendPacket->_sendCabinSwitchPacket.reverseLamp - 0 << '\n';
				std::cout << "headLamp: " << commonSendPacket->_sendCabinSwitchPacket.headLamp - 0 << '\n';
				std::cout << '\n';*/

				sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
			}
		}
	}
}

void CanbinSwitchCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonRecvPacket->_recvCabinSwitchPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(CabinSwitchPacket));

		/*std::cout << " GearTriger: " << (int)commonRecvPacket->_recvCabinSwitchPacket.GearTriger;
		std::cout << " GearP: " << (int)commonRecvPacket->_recvCabinSwitchPacket.GearP;
		std::cout << " Left_Paddle_Shift: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Left_Paddle_Shift;
		std::cout << " Right_Paddle_Shift: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Right_Paddle_Shift;
		std::cout << " Crs: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Crs;
		std::cout << " voice: " << (int)commonRecvPacket->_recvCabinSwitchPacket.voice;
		std::cout << " phone: " << (int)commonRecvPacket->_recvCabinSwitchPacket.phone;
		std::cout << " mode: " << (int)commonRecvPacket->_recvCabinSwitchPacket.mode;
		std::cout << " modeUp: " << (int)commonRecvPacket->_recvCabinSwitchPacket.modeUp;
		std::cout << " modeDown: " << (int)commonRecvPacket->_recvCabinSwitchPacket.modeDown;
		std::cout << " volumeMute: " << (int)commonRecvPacket->_recvCabinSwitchPacket.volumeMute;
		std::cout << " volumeWheel: " << (int)commonRecvPacket->_recvCabinSwitchPacket.volumeWheel;
		std::cout << " Menu: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Menu;
		std::cout << " MenuWheelbtn: " << (int)commonRecvPacket->_recvCabinSwitchPacket.MenuWheelbtn;
		std::cout << " Menuwheel: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Menuwheel;
		std::cout << " bookmark: " << (int)commonRecvPacket->_recvCabinSwitchPacket.bookmark;
		std::cout << " Lamp_TrnSigLftSwSta: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Lamp_TrnSigLftSwSta;
		std::cout << " Lamp_TrnSigRtSwSta: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Lamp_TrnSigRtSwSta;
		std::cout << " Light: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Light;
		std::cout << " Lamp_HdLmpHiSwSta1: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Lamp_HdLmpHiSwSta1;
		std::cout << " Lamp_HdLmpHiSwSta2: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Lamp_HdLmpHiSwSta2;
		std::cout << " Wiper_FrWiperMist: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Wiper_FrWiperMist;
		std::cout << " Wiper_FrWiperWshSwSta: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Wiper_FrWiperWshSwSta;
		std::cout << " Wiper_FrWiperWshSwSta2: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Wiper_FrWiperWshSwSta2;
		std::cout << " Wiper_RrWiperWshSwSta: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Wiper_RrWiperWshSwSta;
		std::cout << " NGB: " << (int)commonRecvPacket->_recvCabinSwitchPacket.NGB;
		std::cout << " DriveModeSw: " << (int)commonRecvPacket->_recvCabinSwitchPacket.DriveModeSw;
		std::cout << " LeftN: " << (int)commonRecvPacket->_recvCabinSwitchPacket.LeftN;
		std::cout << " RightN: " << (int)commonRecvPacket->_recvCabinSwitchPacket.RightN;
		std::cout << " HOD_Dir_Status: " << (int)commonRecvPacket->_recvCabinSwitchPacket.HOD_Dir_Status;
		std::cout << " FWasher: " << (int)commonRecvPacket->_recvCabinSwitchPacket.FWasher;
		std::cout << " Parking: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Parking;
		std::cout << " SeatBelt1: " << (int)commonRecvPacket->_recvCabinSwitchPacket.SeatBelt1;
		std::cout << " SeatBelt2: " << (int)commonRecvPacket->_recvCabinSwitchPacket.SeatBelt2;
		std::cout << " EMG: " << (int)commonRecvPacket->_recvCabinSwitchPacket.EMG;
		std::cout << " Key: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Key;
		std::cout << " Trunk: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Trunk;
		std::cout << " VDC: " << (int)commonRecvPacket->_recvCabinSwitchPacket.VDC;
		std::cout << " Booster: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Booster;
		std::cout << " Plus: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Plus;
		std::cout << " Right: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Right;
		std::cout << " Minus: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Minus;
		std::cout << " Voice: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Voice;
		std::cout << " OK: " << (int)commonRecvPacket->_recvCabinSwitchPacket.OK;
		std::cout << " Left: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Left;
		std::cout << " Phone: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Phone;
		std::cout << " PlusSet: " << (int)commonRecvPacket->_recvCabinSwitchPacket.PlusSet;
		std::cout << " Distance: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Distance;
		std::cout << " MinusSet: " << (int)commonRecvPacket->_recvCabinSwitchPacket.MinusSet;
		std::cout << " LFA: " << (int)commonRecvPacket->_recvCabinSwitchPacket.LFA;
		std::cout << " SCC: " << (int)commonRecvPacket->_recvCabinSwitchPacket.SCC;
		std::cout << " CC: " << (int)commonRecvPacket->_recvCabinSwitchPacket.CC;
		std::cout << " DriveMode: " << (int)commonRecvPacket->_recvCabinSwitchPacket.DriveMode;
		std::cout << " LightHeight: " << (int)commonRecvPacket->_recvCabinSwitchPacket.LightHeight;
		std::cout << " ACCpedal: " << (int)commonRecvPacket->_recvCabinSwitchPacket.ACCpedal;
		std::cout << " Brakepedal: " << (int)commonRecvPacket->_recvCabinSwitchPacket.Brakepedal;
		std::cout << " bMask: " << (int)commonRecvPacket->_recvCabinSwitchPacket.bMask;
		std::cout << '\n';*/

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CanbinSwitchCore::handleInnoPacket Parse error: " + std::string(e.what()), "CanbinSwitchCore::handleInnoPacket");
	}
}

void CanbinSwitchCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonSendPacket->_sendCabinSwitchPacket, buffer.data(), sizeof(SendCabinSwitchPacket));
		
		/*std::cout << "HANDLE UE" << '\n';
		std::cout << "currentGear: " << commonSendPacket->_sendCabinSwitchPacket.currentGear << '\n';
		std::cout << "brakeLamp: " << commonSendPacket->_sendCabinSwitchPacket.brakeLamp - 0 << '\n';
		std::cout << "leftLamp: " << commonSendPacket->_sendCabinSwitchPacket.leftLamp - 0 << '\n';
		std::cout << "rightLamp:" << commonSendPacket->_sendCabinSwitchPacket.rightLamp - 0 << '\n';
		std::cout << "alertLamp: " << commonSendPacket->_sendCabinSwitchPacket.alertLamp - 0 << '\n';
		std::cout << "reverseLamp: " << commonSendPacket->_sendCabinSwitchPacket.reverseLamp - 0 << '\n';
		std::cout << "headLamp: " << commonSendPacket->_sendCabinSwitchPacket.headLamp - 0 << '\n';
		std::cout << '\n';*/
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CanbinSwitchCore::handleUePacket Parse error: " + std::string(e.what()), "CanbinSwitchCore::handleUePacket");
	}
}

/*-----------------
	Motion Core
-----------------*/
MotionCore::MotionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_tick = 10; // 100hz
	//_recvPacketSize = sizeof(MotionPacket) + sizeof(RecvPacketHeader);
	_recvPacketSize = sizeof(MotionPacket);

	if (_peerType == PeerType::INNO)
	{
		_directSendIp = Utils::getEnv("UE_IP");
		_directSendPort = stoi(Utils::getEnv("UE_MOTION_PORT"));
	}
	else if (_peerType == PeerType::UE)
	{
		_recvPacketSize = sizeof(SendMotionPacket);
	}
}

void MotionCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		const int bufferSize = sizeof(SendMotionPacket);

		if (_isTimemachineMode.load(std::memory_order_acquire) && !_isMotionComplete.load())
		{
			//std::cout << "START MOTION" << '\n';

			auto localCopy = sendTimemachinePktByTimestamp;
			for (const auto& [idx, pkt] : localCopy)
			{
				if (pkt.size() == 0) continue;

				int pktSize = pkt.size();
				double interval = static_cast<double>(1000) / static_cast<double>(pktSize);

				for (int i = 0; i < pktSize; i++)
				{
					if (!_isTimemachineMode.load(std::memory_order_acquire))
					{
						_isMotionComplete.store(true, std::memory_order_release);
						//std::cout << "BREAK MOTION" << '\n';
						break;
					}

					commonSendPacket->_sendMotionPacket.psi = pkt[i].yaw;
					commonSendPacket->_sendMotionPacket.theta = pkt[i].pitch;
					commonSendPacket->_sendMotionPacket.phi = pkt[i].roll;
					commonSendPacket->_sendMotionPacket.xAcc = pkt[i].xAcc;
					commonSendPacket->_sendMotionPacket.yAcc = pkt[i].yAcc;
					commonSendPacket->_sendMotionPacket.zAcc = pkt[i].zAcc;
					commonSendPacket->_sendMotionPacket.rDot = pkt[i].rDot;
					commonSendPacket->_sendMotionPacket.vehicleSpeed = pkt[i].vehicleSpeed;
					commonSendPacket->_sendMotionPacket.FrameCounter++;

					std::vector<unsigned char> buffer(bufferSize);
					std::memcpy(buffer.data(), &commonSendPacket->_sendMotionPacket, sizeof(SendMotionPacket));
					sendTo(buffer, _scheduledSendIp, _scheduledSendPort);

					if (i < pktSize - 1)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval)));
					}

				}

				if (_isMotionComplete.load(std::memory_order_acquire)) break;
			}
			//std::cout << "MOTION DONE" << '\n';
			_isMotionComplete.store(true, std::memory_order_release);
		}
		// 타임머신 리플레이
		//if (isRunTimemachine.load() && !_isMotionComplete.load())
		//{
		//	std::cout << "MOTION CHECK SIZE = " << sendTimemachinePkt.size() << '\n';
		//	auto localCopy = sendTimemachinePkt;
		//	for (size_t i = 0; i < localCopy.size(); ++i)
		//	{
		//		ii = i;
		//		if (!isRunTimemachine.load())
		//		{
		//			std::cout << "MOTION CHECK SIZE = " << localCopy.size() << '\n';
		//			std::cout << "BREAK MO " << i << '\n';
		//			break;
		//		}

		//		commonSendPacket->_sendMotionPacket.psi = localCopy[i].yaw;
		//		commonSendPacket->_sendMotionPacket.theta = localCopy[i].pitch;
		//		commonSendPacket->_sendMotionPacket.phi = localCopy[i].roll;
		//		commonSendPacket->_sendMotionPacket.xAcc = localCopy[i].xAcc;
		//		commonSendPacket->_sendMotionPacket.yAcc = localCopy[i].yAcc;
		//		commonSendPacket->_sendMotionPacket.zAcc = localCopy[i].zAcc;
		//		commonSendPacket->_sendMotionPacket.rDot = localCopy[i].rDot;
		//		commonSendPacket->_sendMotionPacket.vehicleSpeed = localCopy[i].vehicleSpeed;
		//		commonSendPacket->_sendMotionPacket.FrameCounter++;

		//		std::vector<unsigned char> buffer(bufferSize);
		//		std::memcpy(buffer.data(), &commonSendPacket->_sendMotionPacket, sizeof(SendMotionPacket));
		//		sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
		//	}

		//	std::cout << "DONE MO " << ii << '\n';
		//	//isRunTimemachine.store(false);
		//	_isMotionComplete.store(true);
		//	if (_isHandleComplete.load()) isRunTimemachine.store(false);
		//}

		// 일반 주행
		else
		{
			if (now - _lastSendMs < _tick) continue;
			_lastSendMs = now;

			std::vector<unsigned char> buffer(bufferSize);
			std::memcpy(buffer.data(), &commonSendPacket->_sendMotionPacket, sizeof(SendMotionPacket));
			commonSendPacket->_sendMotionPacket.FrameCounter++;
			//std::cout << commonSendPacket->_sendMotionPacket.motionCommand << '\n';

			sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
		}
	}
}

void MotionCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonRecvPacket->_recvMotionPacket, buffer.data(), sizeof(MotionPacket));
		/*std::cout << '\n';
		std::cout << "RECV motionStatus1: " << commonRecvPacket->_recvMotionPacket.motionStatus << "\n";
		std::cout << "errorLevel1: " << commonRecvPacket->_recvMotionPacket.errorLevel << "\n";
		std::cout << "errorCode1: " << commonRecvPacket->_recvMotionPacket.errorCode << "\n";*/

		/*std::cout << "FrameCounter1: " << motionPacket.FrameCounter << " ";
		std::cout << "motionStatus1: " << motionPacket.motionStatus << " ";
		std::cout << "errorLevel1: " << motionPacket.errorLevel << " ";
		std::cout << "errorCode1: " << motionPacket.errorCode << " ";
		std::cout << "ioInfo1: " << motionPacket.ioInfo << " ";

		std::cout << "xPosition1: " << motionPacket.xPosition << " ";
		std::cout << "yPosition1: " << motionPacket.yPosition << " ";
		std::cout << "zPosition1: " << motionPacket.zPosition << " ";
		std::cout << "yawPosition1: " << motionPacket.yawPosition << " ";
		std::cout << "pitchPosition1: " << motionPacket.pitchPosition << " ";
		std::cout << "rollPosition1: " << motionPacket.rollPosition << " ";

		std::cout << "xSpeed1: " << motionPacket.xSpeed << " ";
		std::cout << "ySpeed1: " << motionPacket.ySpeed << " ";
		std::cout << "zSpeed1: " << motionPacket.zSpeed << " ";
		std::cout << "yawSpeed1: " << motionPacket.yawSpeed << " ";
		std::cout << "pitchSpeed1: " << motionPacket.pitchSpeed << " ";
		std::cout << "rollSpeed1: " << motionPacket.rollSpeed << " ";

		std::cout << "xAcc1: " << motionPacket.xAcc << " ";
		std::cout << "yAcc1: " << motionPacket.yAcc << " ";
		std::cout << "zAcc1: " << motionPacket.zAcc << " ";
		std::cout << "yawAcc1: " << motionPacket.yawAcc << " ";
		std::cout << "pitchAcc1: " << motionPacket.pitchAcc << " ";
		std::cout << "rollAcc1: " << motionPacket.rollAcc << " ";

		std::cout << "actuator1Length1: " << motionPacket.actuator1Length << " ";
		std::cout << "actuator2Length1: " << motionPacket.actuator2Length << " ";
		std::cout << "actuator3Length1: " << motionPacket.actuator3Length << " ";
		std::cout << "actuator4Length1: " << motionPacket.actuator4Length << " ";
		std::cout << "actuator5Length1: " << motionPacket.actuator5Length << " ";
		std::cout << "actuator6Length1: " << motionPacket.actuator6Length << " ";

		std::cout << "analogInput1_1: " << motionPacket.analogInput1 << " ";
		std::cout << "analogInput2_1: " << motionPacket.analogInput2 << " ";
		std::cout << "analogInput3_1: " << motionPacket.analogInput3 << " ";
		std::cout << "analogInput4_1: " << motionPacket.analogInput4 << '\n';*/

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("MotionCore::handleInnoPacket Parse error: " + std::string(e.what()), "MotionCore::handleInnoPacket");
	}
}

void MotionCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		if (commonRecvPacket->_recvMotionPacket.motionStatus > 10) return;

		std::memcpy(&commonSendPacket->_sendMotionPacket, buffer.data(), sizeof(SendMotionPacket));
		//std::cout << "SEND motionCommand: " << commonSendPacket->_sendMotionPacket.motionCommand << '\n';
		/*std::cout << "FrameCounter: " << commonSendPacket->_sendMotionPacket.FrameCounter << '\n';
		std::cout << "motionCommand: " << commonSendPacket->_sendMotionPacket.motionCommand << '\n';
		std::cout << "psi: " << commonSendPacket->_sendMotionPacket.psi << '\n';
		std::cout << "theta: " << commonSendPacket->_sendMotionPacket.theta << '\n';
		std::cout << "phi: " << commonSendPacket->_sendMotionPacket.phi << '\n';*/
	}
	catch (const std::exception& e)
	{
		Utils::LogError("MotionCore::handleUePacket Parse error: " + std::string(e.what()), "MotionCore::handleUePacket");
	}
}

/*-----------------
	Timemachine Core
-----------------*/
TimemachineCore::TimemachineCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_recvPacketSize = sizeof(CustomCorePacket);
	_tick = 33.3;

	_directSendIp = Utils::getEnv("UE_IP");
	_directSendPort = stoi(Utils::getEnv("UE_TIMEMACHINE_PORT"));
}

void TimemachineCore::sendLoop()
{
}

void TimemachineCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
}

void TimemachineCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
	std::memcpy(&commonRecvPacket->_recvCustomCorePacket, buffer.data(), sizeof(CustomCorePacket));
	/*std::cout << '\n';
	std::cout << "status: " << commonRecvPacket->_recvCustomCorePacket.status << '\n';
	std::cout << "customer_id: " << commonRecvPacket->_recvCustomCorePacket.customer_id << '\n';*/

	//std::cout << _directSendIp << ":" << _directSendPort << '\n';

	if (commonRecvPacket->_recvCustomCorePacket.status == _status)
	{
		sendTo(buffer, _directSendIp, _directSendPort);
		return;
	}

	_status = commonRecvPacket->_recvCustomCorePacket.status;
	sendTo(buffer, _directSendIp, _directSendPort);

	switch (commonRecvPacket->_recvCustomCorePacket.status)
	{

	case 0:
	{
		//std::cout << "STATUS 0 BREAK" << '\n';
		commonRecvPacket->_recvCustomCorePacket = { 0 };
		sendTimemachinePktByTimestamp.clear();
		_timemachinePacketSize = 0;
		_isHandleComplete.store(true);
		_isMotionComplete.store(true);
		_isTimemachineMode.store(false, std::memory_order_release);
		break;
	}

	case 1:
	{
		if (sendTimemachinePktByTimestamp.size() != 0) break;

		auto csvFile = Utils::LoadCSVFiles(commonRecvPacket->_recvCustomCorePacket.customer_id);
		if (csvFile.size() < 2) return;

		_timemachinePacketSize = csvFile.size() - 1;

		bool isHeader = true;
		std::string timestamp = csvFile[1][0];
		int idx = 0;

		for (const auto& row : csvFile)
		{
			if (isHeader) {
				isHeader = false;
				continue;
			}
			if (row.size() < 18) continue; // row가 부족하면 무시

			if (timestamp != row[0])
			{
				timestamp = row[0];
				idx++;
			}

			SendTimemachinePacket data;
			data.timestamp = row[0];
			data.steering = std::stod(row[1]);
			data.velocity = std::stod(row[2]);
			data.roll = std::stod(row[3]);
			data.pitch = std::stod(row[4]);
			data.yaw = std::stod(row[5]);
			data.xAcc = std::stod(row[6]);
			data.yAcc = std::stod(row[7]);
			data.zAcc = std::stod(row[8]);
			data.rDot = std::stod(row[9]);
			data.vehicleSpeed = std::stod(row[10]);
			data.currentGear = row[11][0];
			data.brakeLamp = std::stoi(row[12]);
			data.leftLamp = std::stoi(row[13]);
			data.rightLamp = std::stoi(row[14]);
			data.alertLamp = std::stoi(row[15]);
			data.reverseLamp = std::stoi(row[16]);
			data.headLamp = std::stoi(row[17]);

			sendTimemachinePktByTimestamp[idx].push_back(data);
		}

		break;
	}

	case 2:
	{
		_isHandleComplete.store(false);
		_isMotionComplete.store(false);
		_isTimemachineMode.store(true, std::memory_order_release);
	}
	}
}

CheckConnectionCore::CheckConnectionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)

{
	_tick = 500;
}

void CheckConnectionCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendCheckConnectionPacket);
		commonSendPacket->_sendCheckConnectionPacket.handleStatus = commonRecvPacket->_recvSteerPacket.status;
		commonSendPacket->_sendCheckConnectionPacket.cabinControlStatus = commonRecvPacket->_recvCabinControlPacket.status;
		commonSendPacket->_sendCheckConnectionPacket.cabinControldigitalInput = commonRecvPacket->_recvCabinControlPacket.digitalInput;
		commonSendPacket->_sendCheckConnectionPacket.cabinSwitchAccPedal = commonRecvPacket->_recvCabinSwitchPacket.ACCpedal;
		commonSendPacket->_sendCheckConnectionPacket.motionFrameCounter = commonRecvPacket->_recvMotionPacket.FrameCounter;
		commonSendPacket->_sendCheckConnectionPacket.motionStatus = commonRecvPacket->_recvMotionPacket.motionStatus;
		commonSendPacket->_sendCheckConnectionPacket.motionErrorLevel = commonRecvPacket->_recvMotionPacket.errorLevel;
		commonSendPacket->_sendCheckConnectionPacket.motionErrorCode = commonRecvPacket->_recvMotionPacket.errorCode;
		
		std::vector<unsigned char> buffer(bufferSize);
		std::memcpy(buffer.data(), &commonSendPacket->_sendCheckConnectionPacket, sizeof(SendCheckConnectionPacket));

		sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CheckConnectionCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
}

void CheckConnectionCore::handleUePacket(const std::vector<unsigned char>& buffer)
{
}
