#include "Core.h"
#include <iostream>
#include "Utils.h"
#include "RecvPacketInfo.h"
#include "SendPacketInfo.h"

std::unordered_set<std::string> headerIncludeClass = { "INNO_HANDLE", "INNO_CABIN_CONTROL", "INNO_CABIN_SWITCH" };
std::unordered_set<std::string> headerExcludeClass = { "INNO_MOTION", "UE_HANDLE", "UE_CABIN_CONTROL", "UE_CABIN_SWITCH", "UE_MOTION", "TIMEMACHINE" };
std::vector<SendTimemachinePacket> sendTimemachinePkt;
double sendTimemachineTick = 33.3;
std::atomic<bool> isRunTimemachine = false;

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
		if (isRunTimemachine.load())
		{
			for (auto pkt : sendTimemachinePkt)
			{
				if (!isRunTimemachine.load()) break;

				commonSendPacket->_sendHandlePacket.targetAngle = pkt.steering;
				commonSendPacket->_sendHandlePacket.velocity = pkt.velocity;
				std::vector<unsigned char> buffer(bufferSize);
				std::memcpy(buffer.data(), &commonSendPacket->_sendHandlePacket, sizeof(SendHandlePacket));
				sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sendTimemachineTick)));
			}
			isRunTimemachine.store(false);
		}

		// 일반 주행
		else
		{
			if (now - _lastSendMs < _tick) continue;
			_lastSendMs = now;

			std::vector<unsigned char> buffer(bufferSize);
			std::memcpy(buffer.data(), &commonSendPacket->_sendHandlePacket, sizeof(SendHandlePacket));

			sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
		}
	}
}

void HandleCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		/*SteerPacket steerPacket = { 0 };
		std::memcpy(&steerPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SteerPacket));

		std::cout << " status: " << steerPacket.status;
		std::cout << " steerAngle: " << steerPacket.steerAngle;
		std::cout << " steerAngleRate: " << steerPacket.steerAngleRate;
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
		std::memcpy(buffer.data(), &commonSendPacket->_sendCabinControlPacket, sizeof(SendCabinControlPacket));

		sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CabinControlCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		/*CabinControlPacket cabinControlPacket = { 0 };
		std::memcpy(&cabinControlPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(CabinControlPacket));

		std::cout << " status: " << cabinControlPacket.status;
		std::cout << " carHeight: " << cabinControlPacket.carHeight;
		std::cout << " carWidth: " << cabinControlPacket.carWidth;
		std::cout << " seatWidth: " << cabinControlPacket.seatWidth;
		std::cout << " digitalInput: " << cabinControlPacket.digitalInput;
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
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendCabinSwitchPacket);
		std::vector<unsigned char> buffer(bufferSize);

		std::memcpy(buffer.data(), &commonSendPacket->_sendCabinSwitchPacket, sizeof(SendCabinSwitchPacket));

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CanbinSwitchCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		/*CabinSwitchPacket cabinSwitchPacket = {0};
		std::memcpy(&cabinSwitchPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(CabinSwitchPacket));

		std::cout << " GearTriger: " << (int)cabinSwitchPacket.GearTriger;
		std::cout << " GearP: " << (int)cabinSwitchPacket.GearP;
		std::cout << " Left_Paddle_Shift: " << (int)cabinSwitchPacket.Left_Paddle_Shift;
		std::cout << " Right_Paddle_Shift: " << (int)cabinSwitchPacket.Right_Paddle_Shift;
		std::cout << " Crs: " << (int)cabinSwitchPacket.Crs;
		std::cout << " voice: " << (int)cabinSwitchPacket.voice;
		std::cout << " phone: " << (int)cabinSwitchPacket.phone;
		std::cout << " mode: " << (int)cabinSwitchPacket.mode;
		std::cout << " modeUp: " << (int)cabinSwitchPacket.modeUp;
		std::cout << " modeDown: " << (int)cabinSwitchPacket.modeDown;
		std::cout << " volumeMute: " << (int)cabinSwitchPacket.volumeMute;
		std::cout << " volumeWheel: " << (int)cabinSwitchPacket.volumeWheel;
		std::cout << " Menu: " << (int)cabinSwitchPacket.Menu;
		std::cout << " MenuWheelbtn: " << (int)cabinSwitchPacket.MenuWheelbtn;
		std::cout << " Menuwheel: " << (int)cabinSwitchPacket.Menuwheel;
		std::cout << " bookmark: " << (int)cabinSwitchPacket.bookmark;
		std::cout << " Lamp_TrnSigLftSwSta: " << (int)cabinSwitchPacket.Lamp_TrnSigLftSwSta;
		std::cout << " Lamp_TrnSigRtSwSta: " << (int)cabinSwitchPacket.Lamp_TrnSigRtSwSta;
		std::cout << " Light: " << (int)cabinSwitchPacket.Light;
		std::cout << " Lamp_HdLmpHiSwSta1: " << (int)cabinSwitchPacket.Lamp_HdLmpHiSwSta1;
		std::cout << " Lamp_HdLmpHiSwSta2: " << (int)cabinSwitchPacket.Lamp_HdLmpHiSwSta2;
		std::cout << " Wiper_FrWiperMist: " << (int)cabinSwitchPacket.Wiper_FrWiperMist;
		std::cout << " Wiper_FrWiperWshSwSta: " << (int)cabinSwitchPacket.Wiper_FrWiperWshSwSta;
		std::cout << " Wiper_FrWiperWshSwSta2: " << (int)cabinSwitchPacket.Wiper_FrWiperWshSwSta2;
		std::cout << " Wiper_RrWiperWshSwSta: " << (int)cabinSwitchPacket.Wiper_RrWiperWshSwSta;
		std::cout << " NGB: " << (int)cabinSwitchPacket.NGB;
		std::cout << " DriveModeSw: " << (int)cabinSwitchPacket.DriveModeSw;
		std::cout << " LeftN: " << (int)cabinSwitchPacket.LeftN;
		std::cout << " RightN: " << (int)cabinSwitchPacket.RightN;
		std::cout << " HOD_Dir_Status: " << (int)cabinSwitchPacket.HOD_Dir_Status;
		std::cout << " FWasher: " << (int)cabinSwitchPacket.FWasher;
		std::cout << " Parking: " << (int)cabinSwitchPacket.Parking;
		std::cout << " SeatBelt1: " << (int)cabinSwitchPacket.SeatBelt1;
		std::cout << " SeatBelt2: " << (int)cabinSwitchPacket.SeatBelt2;
		std::cout << " EMG: " << (int)cabinSwitchPacket.EMG;
		std::cout << " Key: " << (int)cabinSwitchPacket.Key;
		std::cout << " Trunk: " << (int)cabinSwitchPacket.Trunk;
		std::cout << " VDC: " << (int)cabinSwitchPacket.VDC;
		std::cout << " Booster: " << (int)cabinSwitchPacket.Booster;
		std::cout << " Plus: " << (int)cabinSwitchPacket.Plus;
		std::cout << " Right: " << (int)cabinSwitchPacket.Right;
		std::cout << " Minus: " << (int)cabinSwitchPacket.Minus;
		std::cout << " Voice: " << (int)cabinSwitchPacket.Voice;
		std::cout << " OK: " << (int)cabinSwitchPacket.OK;
		std::cout << " Left: " << (int)cabinSwitchPacket.Left;
		std::cout << " Phone: " << (int)cabinSwitchPacket.Phone;
		std::cout << " PlusSet: " << (int)cabinSwitchPacket.PlusSet;
		std::cout << " Distance: " << (int)cabinSwitchPacket.Distance;
		std::cout << " MinusSet: " << (int)cabinSwitchPacket.MinusSet;
		std::cout << " LFA: " << (int)cabinSwitchPacket.LFA;
		std::cout << " SCC: " << (int)cabinSwitchPacket.SCC;
		std::cout << " CC: " << (int)cabinSwitchPacket.CC;
		std::cout << " DriveMode: " << (int)cabinSwitchPacket.DriveMode;
		std::cout << " LightHeight: " << (int)cabinSwitchPacket.LightHeight;
		std::cout << " ACCpedal: " << (int)cabinSwitchPacket.ACCpedal;
		std::cout << " Brakepedal: " << (int)cabinSwitchPacket.Brakepedal;
		std::cout << " bMask: " << (int)cabinSwitchPacket.bMask;
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
		//std::cout << "Current Gear: " << commonSendPacket->_sendCabinSwitchPacket.currentGear << '\n';
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

		// 타임머신 리플레이
		if (isRunTimemachine.load())
		{
			for (auto pkt : sendTimemachinePkt)
			{
				if (!isRunTimemachine.load()) break;

				commonSendPacket->_sendMotionPacket.psi = pkt.yaw;
				commonSendPacket->_sendMotionPacket.theta = pkt.pitch;
				commonSendPacket->_sendMotionPacket.phi = pkt.roll;
				commonSendPacket->_sendMotionPacket.xAcc = pkt.xAcc;
				commonSendPacket->_sendMotionPacket.yAcc = pkt.yAcc;
				commonSendPacket->_sendMotionPacket.zAcc = pkt.zAcc;
				commonSendPacket->_sendMotionPacket.rDot = pkt.rDot;
				commonSendPacket->_sendMotionPacket.vehicleSpeed = pkt.vehicleSpeed;
				commonSendPacket->_sendMotionPacket.FrameCounter++;

				std::vector<unsigned char> buffer(bufferSize);
				std::memcpy(buffer.data(), &commonSendPacket->_sendMotionPacket, sizeof(SendMotionPacket));
				sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sendTimemachineTick)));
			}
			isRunTimemachine.store(false);
		}

		// 일반 주행
		else
		{
			if (now - _lastSendMs < _tick) continue;
			_lastSendMs = now;

			std::vector<unsigned char> buffer(bufferSize);
			std::memcpy(buffer.data(), &commonSendPacket->_sendMotionPacket, sizeof(SendMotionPacket));
			commonSendPacket->_sendMotionPacket.FrameCounter++;

			sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
		}
	}
}

void MotionCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		std::memcpy(&commonRecvPacket->_recvMotionPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(MotionPacket));

		MotionPacket motionPacket = { 0 };
		std::memcpy(&motionPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(MotionPacket));

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
		/*std::cout << "FrameCounter: " << commonSendPacket->_sendMotionPacket.FrameCounter << '\n';
		std::cout << "motionCommand: " << commonSendPacket->_sendMotionPacket.motionCommand << '\n';
		std::cout << "turb10AmpZ: " << commonSendPacket->_sendMotionPacket.turb10AmpZ << '\n';*/
	}
	catch (const std::exception& e)
	{
		Utils::LogError("MotionCore::handleUePacket Parse error: " + std::string(e.what()), "MotionCore::handleUePacket");
	}
}

TimemachineCore::TimemachineCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort, PeerType peerType)
	: Core(name, ip, port, clientIp, clientPort, peerType)
{
	_recvPacketSize = sizeof(CustomCorePacket);
	_tick = 33.3;
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
	/*std::cout << "status: " << commonRecvPacket->_recvCustomCorePacket.status << '\n';
	std::cout << "customer_id: " << commonRecvPacket->_recvCustomCorePacket.customer_id << '\n';*/

	switch (commonRecvPacket->_recvCustomCorePacket.status)
	{
		{
	case 0:
		commonRecvPacket->_recvCustomCorePacket = { 0 };
		sendTimemachinePkt.clear();
		isRunTimemachine.store(false);
		break;
		}

		{
	case 1:
		if (sendTimemachinePkt.size() != 0) break;

		auto csvFile = Utils::LoadCSVFiles(commonRecvPacket->_recvCustomCorePacket.customer_id);

		bool isHeader = true;
		for (const auto& row : csvFile)
		{
			if (isHeader) {
				isHeader = false;
				continue;
			}
			if (row.size() < 5) continue; // row가 부족하면 무시

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

			sendTimemachinePkt.push_back(data);
		}
		isRunTimemachine.store(false);

		break;
		}

		{
	case 2:
		isRunTimemachine.store(true);
		break;
		}
	}
}

