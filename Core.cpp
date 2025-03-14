#include "Core.h"
#include <iostream>
#include "Utils.h"
#include "RecvPacketInfo.h"
#include "SendPacketInfo.h"

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

			std::cout << '\n' << "[RECV] " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << " -> " << _ip << ":" << _port << " [" << _name << "]" << '\n';

			if (recvLen < sizeof(RecvPacketHeader))
			{
				std::cerr << "Data Size Not Enough / recvLen: " << recvLen << '\n';
				continue;
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
			// Header 포함 패킷
			if (_name != "INNO_MOTION")
			{
				RecvPacketHeader recvPacketHeader = { 0 };
				std::memcpy(&recvPacketHeader, buffer.data(), sizeof(RecvPacketHeader));

				if ((int)recvPacketHeader.bSize != recvLen)
				{
					std::cerr << "Data Size Not Enough / bSize: " << (int)recvPacketHeader.bSize << ", recvLen: " << recvLen << '\n';
					continue;
				}

				buffer.resize((int)recvPacketHeader.bSize);
				std::cout << "Recv Len: " << recvLen << " / sNetVersion: " << recvPacketHeader.sNetVersion << " / sMask: " << recvPacketHeader.sMask << " / bSize: " << (int)recvPacketHeader.bSize << '\n';
			}

			// Header 없는 패킷
			else if(_name == "INNO_MOTION" && typeid(*this) == typeid(MotionCore))
			{
				MotionCore* motionCore = dynamic_cast<MotionCore*>(this);
				if (motionCore->_recvPacketSize != recvLen)
				{
					std::cerr << "Data Size Not Enough / bSize: " << (int)motionCore->_recvPacketSize << ", recvLen: " << recvLen << '\n';
					continue;
				}

				buffer.resize(motionCore->_recvPacketSize);
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

		std::cout << "[SEND] " << _ip << ":" << _port << " [" << _name << "] -> " << targetIp << ":" << targetPort << '\n';
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
}

void HandleCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendHandlePacket);
		std::vector<unsigned char> buffer(bufferSize);
		std::memcpy(buffer.data(), &commonPacket->_sendHandlePacket, sizeof(SendHandlePacket));

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void HandleCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		SteerPacket steerPacket = { 0 };
		std::memcpy(&steerPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SteerPacket));

		std::cout << " status: " << steerPacket.status;
		std::cout << " steerAngle: " << steerPacket.steerAngle;
		std::cout << " steerAngleRate: " << steerPacket.steerAngleRate;
		std::cout << '\n';

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
		std::memcpy(&commonPacket->_sendHandlePacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SendHandlePacket));
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
		std::memcpy(buffer.data(), &commonPacket->_sendCabinControlPacket, sizeof(SendCabinControlPacket));

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CabinControlCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		CabinControlPacket cabinControlPacket = { 0 };
		std::memcpy(&cabinControlPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(CabinControlPacket));

		std::cout << " status: " << cabinControlPacket.status;
		std::cout << " carHeight: " << cabinControlPacket.carHeight;
		std::cout << " carWidth: " << cabinControlPacket.carWidth;
		std::cout << " seatWidth: " << cabinControlPacket.seatWidth;
		std::cout << '\n';

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
		std::memcpy(&commonPacket->_sendCabinControlPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SendCabinControlPacket));
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

		std::memcpy(buffer.data(), &commonPacket->_sendCabinSwitchPacket, sizeof(SendCabinSwitchPacket));

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CanbinSwitchCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		CabinSwitchPacket cabinSwitchPacket = { 0 };
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
		std::cout << '\n';

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
		std::memcpy(&commonPacket->_sendCabinSwitchPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SendCabinSwitchPacket));
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
	_recvPacketSize = sizeof(MotionPacket);

	if (_peerType == PeerType::INNO)
	{
		_directSendIp = Utils::getEnv("UE_IP");
		_directSendPort = stoi(Utils::getEnv("UE_MOTION_PORT"));
	}
}

void MotionCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendMotionPacket);
		std::vector<unsigned char> buffer(bufferSize);
		std::memcpy(buffer.data(), &commonPacket->_sendMotionPacket, sizeof(SendMotionPacket));

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void MotionCore::handleInnoPacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		MotionPacket motionPacket = { 0 };
		std::memcpy(&motionPacket, buffer.data(), sizeof(MotionPacket));

		std::cout << "FrameCounter1: " << motionPacket.FrameCounter1 << " ";
		std::cout << "motionStatus1: " << motionPacket.motionStatus1 << " ";
		std::cout << "errorLevel1: " << motionPacket.errorLevel1 << " ";
		std::cout << "errorCode1: " << motionPacket.errorCode1 << " ";
		std::cout << "ioInfo1: " << motionPacket.ioInfo1 << " ";

		std::cout << "xPosition1: " << motionPacket.xPosition1 << " ";
		std::cout << "yPosition1: " << motionPacket.yPosition1 << " ";
		std::cout << "zPosition1: " << motionPacket.zPosition1 << " ";
		std::cout << "yawPosition1: " << motionPacket.yawPosition1 << " ";
		std::cout << "pitchPosition1: " << motionPacket.pitchPosition1 << " ";
		std::cout << "rollPosition1: " << motionPacket.rollPosition1 << " ";

		std::cout << "xSpeed1: " << motionPacket.xSpeed1 << " ";
		std::cout << "ySpeed1: " << motionPacket.ySpeed1 << " ";
		std::cout << "zSpeed1: " << motionPacket.zSpeed1 << " ";
		std::cout << "yawSpeed1: " << motionPacket.yawSpeed1 << " ";
		std::cout << "pitchSpeed1: " << motionPacket.pitchSpeed1 << " ";
		std::cout << "rollSpeed1: " << motionPacket.rollSpeed1 << " ";

		std::cout << "xAcc1: " << motionPacket.xAcc1 << " ";
		std::cout << "yAcc1: " << motionPacket.yAcc1 << " ";
		std::cout << "zAcc1: " << motionPacket.zAcc1 << " ";
		std::cout << "yawAcc1: " << motionPacket.yawAcc1 << " ";
		std::cout << "pitchAcc1: " << motionPacket.pitchAcc1 << " ";
		std::cout << "rollAcc1: " << motionPacket.rollAcc1 << " ";

		std::cout << "actuator1Length1: " << motionPacket.actuator1Length1 << " ";
		std::cout << "actuator2Length1: " << motionPacket.actuator2Length1 << " ";
		std::cout << "actuator3Length1: " << motionPacket.actuator3Length1 << " ";
		std::cout << "actuator4Length1: " << motionPacket.actuator4Length1 << " ";
		std::cout << "actuator5Length1: " << motionPacket.actuator5Length1 << " ";
		std::cout << "actuator6Length1: " << motionPacket.actuator6Length1 << " ";

		std::cout << "analogInput1_1: " << motionPacket.analogInput1_1 << " ";
		std::cout << "analogInput2_1: " << motionPacket.analogInput2_1 << " ";
		std::cout << "analogInput3_1: " << motionPacket.analogInput3_1 << " ";
		std::cout << "analogInput4_1: " << motionPacket.analogInput4_1 << '\n';

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
		std::memcpy(&commonPacket->_sendMotionPacket, buffer.data() + sizeof(RecvPacketHeader), sizeof(SendMotionPacket));
	}
	catch (const std::exception& e)
	{
		Utils::LogError("MotionCore::handleUePacket Parse error: " + std::string(e.what()), "MotionCore::handleUePacket");
	}
}

