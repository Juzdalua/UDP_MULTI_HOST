#include "Core.h"
#include <iostream>
#include "Utils.h"
#include "RecvPacketInfo.h"

Core::Core(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: _name(name), _ip(ip), _port(port), _scheduledSendIp(clientIp), _scheduledSendPort(clientPort), _running(false), _socket(INVALID_SOCKET)
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

void Core::sendScheduled(const std::vector<unsigned char>& buffer, const std::string& targetIp, unsigned short targetPort)
{
}

void Core::receiveLoop()
{
	while (_running) {

		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(clientAddr);
		std::vector<unsigned char> buffer(1024);
		int recvLen = 0;

		try
		{
			recvLen = recvfrom(_socket, (char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
			if (recvLen == SOCKET_ERROR) {
				int errorCode = WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK || errorCode == WSAETIMEDOUT || errorCode == WSAECONNRESET) {
					continue;
				}
				/*if (errorCode == 10054)
				{
					continue;
				}*/
				std::cerr << "Failed to receive data: " << errorCode << std::endl;
				continue;
			}

			if (recvLen < sizeof(RecvPacketHeader))
			{
				std::cerr << "Data Size Not Enough: received " << recvLen << " bytes" << std::endl;
				continue;
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << std::string(e.what()) << '\n';
			Utils::LogError("Core::receiveLoop RECV error: " + std::string(e.what()), "Core::receiveLoop");
			continue;
		}

		try
		{
			unsigned short sNetVersion = *reinterpret_cast<const unsigned short*>(buffer.data()); // 2바이트
			short sMask = *reinterpret_cast<const short*>(buffer.data() + sizeof(unsigned short)); // 2바이트
			unsigned char bSize = *(buffer.data() + sizeof(unsigned short) + sizeof(short)); // 1바이트

			if ((int)bSize > recvLen)
			{
				std::cerr << "Data Size Not Enough: expected " << (int)bSize << " bytes, received " << recvLen << " bytes" << std::endl;
				continue;
			}

			buffer.resize(recvLen);
			std::cout << std::endl << "Received data: " << recvLen << " bytes" << std::endl;  // 추가된 로그

			std::cout << "sNetVersion: " << sNetVersion << '\n';
			std::cout << "sMask: " << sMask << '\n';
			std::cout << "bSize: " << (int)bSize << '\n';

			// 데이터 버퍼 체크
			if (bSize <= 0) return;
			std::vector<unsigned char> dataBuffer((int)bSize - sizeof(RecvPacketHeader));
			std::memcpy(dataBuffer.data(), buffer.data() + 5, (int)bSize - sizeof(RecvPacketHeader));

			//ProcessPacket::handlePacket(buffer);
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
		sockaddr_in targetAddr;
		targetAddr.sin_family = AF_INET;
		targetAddr.sin_addr.s_addr = inet_addr(targetIp.c_str());
		targetAddr.sin_port = htons(targetPort);

		std::cout << "[SEND] " << targetIp << ":" << targetPort << '\n';
		int a = sendto(_socket, (const char*)buffer.data(), buffer.size(), 0, (SOCKADDR*)&targetAddr, sizeof(targetAddr));
		if (a == SOCKET_ERROR) {
			std::cout << "[SEND ERROR] " << WSAGetLastError() << '\n';
		}
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Core::sendTo Send error: " + std::string(e.what()), "Core::sendToHost");
	}
}

void Core::sendLoop()
{
	while (_running) {
	}
}

void Core::handlePacket(const std::vector<unsigned char>& buffer)
{
}

/*-----------------
	Handle Core
-----------------*/
HandleCore::HandleCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = sizeof(SendPacketHeader) + sizeof(SendHandlePacket);
	_sMask = 0x0011;
	_tick = 10; // 100hz

	_directSendIp = Utils::getEnv("UE_IP");
	_directSendPort = stoi(Utils::getEnv("UE_RECV_HANDLE_PORT"));
}

void HandleCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendHandlePacket) + sizeof(SendPacketHeader);
		std::vector<unsigned char> buffer(bufferSize); 

		// 헤더 메모리 복사
		std::memcpy(buffer.data(), &_sNetVersion, sizeof(unsigned short));   // 0~1 바이트에 sNetVersion
		std::memcpy(buffer.data() + 2, &_sMask, sizeof(unsigned short));      // 2~3 바이트에 sMask
		std::memcpy(buffer.data() + 4, &_bSize, sizeof(unsigned char));       // 4~5 바이트에 bSize

		// 데이터 메모리 복사
		std::memcpy(buffer.data() + 5, &_sendHandlePacket, sizeof(SendHandlePacket));
		//std::memcpy(buffer.data() + 5, &_sendHandlePacket.simState, sizeof(unsigned __int32));    // 5~8 바이트에 simState
		//std::memcpy(buffer.data() + 9, &_sendHandlePacket.velocity, sizeof(float));               // 9~12 바이트에 velocity
		//std::memcpy(buffer.data() + 13, &_sendHandlePacket.wheelAngleVelocityLF, sizeof(float));  // 13~16 바이트에 wheelAngleVelocityLF
		//std::memcpy(buffer.data() + 17, &_sendHandlePacket.wheelAngleVelocityRF, sizeof(float));  // 17~20 바이트에 wheelAngleVelocityRF
		//std::memcpy(buffer.data() + 21, &_sendHandlePacket.wheelAngleVelocityLB, sizeof(float));  // 21~24 바이트에 wheelAngleVelocityLB
		//std::memcpy(buffer.data() + 25, &_sendHandlePacket.wheelAngleVelocityRB, sizeof(float));  // 25~28 바이트에 wheelAngleVelocityRB
		//std::memcpy(buffer.data() + 29, &_sendHandlePacket.targetAngle, sizeof(float));           // 29~32 바이트에 targetAngle

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void HandleCore::handlePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		unsigned __int32 status = *reinterpret_cast<const unsigned __int32*>(buffer.data() + sizeof(RecvPacketHeader));
		float steerAngle = *reinterpret_cast<const float*>(buffer.data() + sizeof(RecvPacketHeader) + sizeof(unsigned __int32));
		float steerAngleRate = *reinterpret_cast<const float*>(buffer.data() + sizeof(RecvPacketHeader) + sizeof(unsigned __int32) + sizeof(float));

		std::cout << " status: " << status;
		std::cout << " steerAngle: " << steerAngle;
		std::cout << " steerAngleRate: " << steerAngleRate;
		std::cout << std::endl;

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("HandleCore::handlePacke Parse error: " + std::string(e.what()), "HandleCore::handlePacket");
	}

}

/*-----------------
	CabinControl Core
-----------------*/
CabinControlCore::CabinControlCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = sizeof(SendPacketHeader) + sizeof(SendCabinControlPacket);
	_sMask = 0x0012;
	_tick = 1000; // 1hz
	//_tick = 100; // 10hz

	_directSendIp = Utils::getEnv("UE_IP");
	_directSendPort = stoi(Utils::getEnv("UE_RECV_CABIN_CONTROL_PORT"));
}

void CabinControlCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendCabinControlPacket) + sizeof(SendPacketHeader);
		std::vector<unsigned char> buffer(bufferSize);

		// 헤더 메모리 복사
		std::memcpy(buffer.data(), &_sNetVersion, sizeof(unsigned short));   // 0~1 바이트에 sNetVersion
		std::memcpy(buffer.data() + 2, &_sMask, sizeof(unsigned short));      // 2~3 바이트에 sMask
		std::memcpy(buffer.data() + 4, &_bSize, sizeof(unsigned char));       // 4~5 바이트에 bSize

		// 데이터 메모리 복사
		std::memcpy(buffer.data() + 5, &_sendCabinControlPacket, sizeof(_sendCabinControlPacket)); 
		//std::memcpy(buffer.data() + 5, &_sendCabinControlPacket.command, sizeof(_sendCabinControlPacket.command));   // 5~6 바이트에 command
		//std::memcpy(buffer.data() + 7, &_sendCabinControlPacket.avtivation, sizeof(_sendCabinControlPacket.avtivation));  // 7~8 바이트에 avtivation
		//std::memcpy(buffer.data() + 9, &_sendCabinControlPacket.handleStrength, sizeof(_sendCabinControlPacket.handleStrength));  // 9~10 바이트에 handleStrength
		//std::memcpy(buffer.data() + 11, &_sendCabinControlPacket.seatBeltStrength, sizeof(_sendCabinControlPacket.seatBeltStrength));  // 11~12 바이트에 seatBeltStrength
		//std::memcpy(buffer.data() + 13, &_sendCabinControlPacket.manual, sizeof(_sendCabinControlPacket.manual));  // 13~14 바이트에 manual
		//std::memcpy(buffer.data() + 15, &_sendCabinControlPacket.height, sizeof(_sendCabinControlPacket.height));   // 15~18 바이트에 height
		//std::memcpy(buffer.data() + 19, &_sendCabinControlPacket.width, sizeof(_sendCabinControlPacket.width));   // 19~22 바이트에 width
		//std::memcpy(buffer.data() + 23, &_sendCabinControlPacket.seatHeight, sizeof(_sendCabinControlPacket.seatHeight)); // 23~26 바이트에 seatHeight

		//sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void CabinControlCore::handlePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		unsigned __int16 status = *reinterpret_cast<const unsigned __int16*>(buffer.data() + sizeof(RecvPacketHeader));  // 2바이트
		float carHeight = *reinterpret_cast<const float*>(buffer.data() + sizeof(RecvPacketHeader) + sizeof(unsigned __int16));  // 4바이트
		float carWidth = *reinterpret_cast<const float*>(buffer.data() + sizeof(RecvPacketHeader) + sizeof(unsigned __int16) + sizeof(float));  // 4바이트
		float seatWidth = *reinterpret_cast<const float*>(buffer.data() + sizeof(RecvPacketHeader) + sizeof(unsigned __int16) + sizeof(float) * 2);  // 4바이트

		std::cout << " status: " << status;
		std::cout << " carHeight: " << carHeight;
		std::cout << " carWidth: " << carWidth;
		std::cout << " seatWidth: " << seatWidth;
		std::cout << std::endl;

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CabinControlCore::handlePacke Parse error: " + std::string(e.what()), "CabinControlCore::handlePacket");
	}
}

/*-----------------
	CanbinSwitch Core
-----------------*/
CanbinSwitchCore::CanbinSwitchCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = 0;
	_sMask = 0x0013;	

	_directSendIp = Utils::getEnv("UE_IP");
	_directSendPort = stoi(Utils::getEnv("UE_RECV_CABIN_SWITCH_PORT"));
}

void CanbinSwitchCore::handlePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		unsigned char GearTriger = buffer[0 + sizeof(RecvPacketHeader)];  // 1: 아래로 두칸, 2: 아래로 한칸, 3: 중간, 4: 위로 한칸, 5: 위로 두칸
		unsigned char GearP = buffer[1 + sizeof(RecvPacketHeader)];        // 2: Off, 1: On
		unsigned char Left_Paddle_Shift = buffer[2 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On
		unsigned char Right_Paddle_Shift = buffer[3 + sizeof(RecvPacketHeader)]; // 0: Off, 1: On
		unsigned char Crs = buffer[4 + sizeof(RecvPacketHeader)];          // 1: 속도 +, 2: 속도 -, 3: 앞차 거리, 4: 속도 버튼 클릭, 8: 크루즈
		unsigned char voice = buffer[5 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char phone = buffer[6 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char mode = buffer[7 + sizeof(RecvPacketHeader)];         // 0: Off, 1: On
		unsigned char modeUp = buffer[8 + sizeof(RecvPacketHeader)];       // 0: Off, 1: On
		unsigned char modeDown = buffer[9 + sizeof(RecvPacketHeader)];     // 0: Off, 1: On
		unsigned char volumeMute = buffer[10 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On
		unsigned char volumeWheel = buffer[11 + sizeof(RecvPacketHeader)]; // 0: Off, 1: Down, 2: Up
		unsigned char Menu = buffer[12 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char MenuWheelbtn = buffer[13 + sizeof(RecvPacketHeader)]; // 0: Off, 1: On
		unsigned char Menuwheel = buffer[14 + sizeof(RecvPacketHeader)];   // 0: Off, 1: Down, 2: Up
		unsigned char bookmark = buffer[15 + sizeof(RecvPacketHeader)];    // 0: Off, 1: On
		unsigned char Lamp_TrnSigLftSwSta = buffer[16 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On
		unsigned char Lamp_TrnSigRtSwSta = buffer[17 + sizeof(RecvPacketHeader)];   // 0: Off, 1: On
		unsigned char Light = buffer[18 + sizeof(RecvPacketHeader)];       // 0: Off, 1: Auto, 2: 라이트1, 3: 라이트2
		unsigned char Lamp_HdLmpHiSwSta1 = buffer[19 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On (push)
		unsigned char Lamp_HdLmpHiSwSta2 = buffer[20 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On (pull)
		unsigned char Wiper_FrWiperMist = buffer[21 + sizeof(RecvPacketHeader)];   // 0: Off, 1: On
		unsigned char Wiper_FrWiperWshSwSta = buffer[22 + sizeof(RecvPacketHeader)]; // 0: Off, 1: Auto, 2: Lo, 3: Hi
		unsigned char Wiper_FrWiperWshSwSta2 = buffer[23 + sizeof(RecvPacketHeader)]; // 0~4 (Level)
		unsigned char Wiper_RrWiperWshSwSta = buffer[24 + sizeof(RecvPacketHeader)]; // 0: Off, 1: Lo, 2: Hi
		unsigned char NGB = buffer[25 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char DriveModeSw = buffer[26 + sizeof(RecvPacketHeader)];  // 0: Off, 1: On
		unsigned char LeftN = buffer[27 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char RightN = buffer[28 + sizeof(RecvPacketHeader)];       // 0: Off, 1: On
		unsigned char HOD_Dir_Status = buffer[29 + sizeof(RecvPacketHeader)];  // 0: 미감지, 1~10 (스티어링 감지)
		unsigned char FWasher = buffer[30 + sizeof(RecvPacketHeader)];      // 0: Off, 1: On
		unsigned char Parking = buffer[31 + sizeof(RecvPacketHeader)];      // 0: Off, 1: pull, 2: push
		unsigned char SeatBelt1 = buffer[32 + sizeof(RecvPacketHeader)];    // 0: Off, 1: On
		unsigned char SeatBelt2 = buffer[33 + sizeof(RecvPacketHeader)];    // 0: Off, 1: On
		unsigned char EMG = buffer[34 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char Key = buffer[35 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char Trunk = buffer[36 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char VDC = buffer[37 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char Booster = buffer[38 + sizeof(RecvPacketHeader)];      // 0: Off, 1: On
		unsigned char Plus = buffer[39 + sizeof(RecvPacketHeader)];         // 0: Off, 1: On
		unsigned char Right = buffer[40 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char Minus = buffer[41 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char Voice = buffer[42 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char OK = buffer[43 + sizeof(RecvPacketHeader)];           // 0: Off, 1: On
		unsigned char Left = buffer[44 + sizeof(RecvPacketHeader)];         // 0: Off, 1: On
		unsigned char Phone = buffer[45 + sizeof(RecvPacketHeader)];        // 0: Off, 1: On
		unsigned char PlusSet = buffer[46 + sizeof(RecvPacketHeader)];      // 0: Off, 1: On
		unsigned char Distance = buffer[47 + sizeof(RecvPacketHeader)];     // 0: Off, 1: On
		unsigned char MinusSet = buffer[48 + sizeof(RecvPacketHeader)];     // 0: Off, 1: On
		unsigned char LFA = buffer[49 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char SCC = buffer[50 + sizeof(RecvPacketHeader)];          // 0: Off, 1: On
		unsigned char CC = buffer[51 + sizeof(RecvPacketHeader)];           // 0: Off, 1: On
		unsigned char DriveMode = buffer[52 + sizeof(RecvPacketHeader)];    // 0: Off, 1: On
		unsigned char LightHeight = buffer[53 + sizeof(RecvPacketHeader)];  // 0~3 (Level)
		unsigned char ACCpedal = buffer[54 + sizeof(RecvPacketHeader)];     // 0~255
		unsigned char Brakepedal = buffer[55 + sizeof(RecvPacketHeader)];   // 0~255
		unsigned char bMask = buffer[56 + sizeof(RecvPacketHeader)];        // (unsigned char)0xef

		std::cout << " GearTriger: " << (int)GearTriger;
		std::cout << " GearP: " << (int)GearP;
		std::cout << " Left_Paddle_Shift: " << (int)Left_Paddle_Shift;
		std::cout << " Right_Paddle_Shift: " << (int)Right_Paddle_Shift;
		std::cout << " Crs: " << (int)Crs;
		std::cout << " voice: " << (int)voice;
		std::cout << " phone: " << (int)phone;
		std::cout << " mode: " << (int)mode;
		std::cout << " modeUp: " << (int)modeUp;
		std::cout << " modeDown: " << (int)modeDown;
		std::cout << " volumeMute: " << (int)volumeMute;
		std::cout << " volumeWheel: " << (int)volumeWheel;
		std::cout << " Menu: " << (int)Menu;
		std::cout << " MenuWheelbtn: " << (int)MenuWheelbtn;
		std::cout << " Menuwheel: " << (int)Menuwheel;
		std::cout << " bookmark: " << (int)bookmark;
		std::cout << " Lamp_TrnSigLftSwSta: " << (int)Lamp_TrnSigLftSwSta;
		std::cout << " Lamp_TrnSigRtSwSta: " << (int)Lamp_TrnSigRtSwSta;
		std::cout << " Light: " << (int)Light;
		std::cout << " Lamp_HdLmpHiSwSta1: " << (int)Lamp_HdLmpHiSwSta1;
		std::cout << " Lamp_HdLmpHiSwSta2: " << (int)Lamp_HdLmpHiSwSta2;
		std::cout << " Wiper_FrWiperMist: " << (int)Wiper_FrWiperMist;
		std::cout << " Wiper_FrWiperWshSwSta: " << (int)Wiper_FrWiperWshSwSta;
		std::cout << " Wiper_FrWiperWshSwSta2: " << (int)Wiper_FrWiperWshSwSta2;
		std::cout << " Wiper_RrWiperWshSwSta: " << (int)Wiper_RrWiperWshSwSta;
		std::cout << " NGB: " << (int)NGB;
		std::cout << " DriveModeSw: " << (int)DriveModeSw;
		std::cout << " LeftN: " << (int)LeftN;
		std::cout << " RightN: " << (int)RightN;
		std::cout << " HOD_Dir_Status: " << (int)HOD_Dir_Status;
		std::cout << " FWasher: " << (int)FWasher;
		std::cout << " Parking: " << (int)Parking;
		std::cout << " SeatBelt1: " << (int)SeatBelt1;
		std::cout << " SeatBelt2: " << (int)SeatBelt2;
		std::cout << " EMG: " << (int)EMG;
		std::cout << " Key: " << (int)Key;
		std::cout << " Trunk: " << (int)Trunk;
		std::cout << " VDC: " << (int)VDC;
		std::cout << " Booster: " << (int)Booster;
		std::cout << " Plus: " << (int)Plus;
		std::cout << " Right: " << (int)Right;
		std::cout << " Minus: " << (int)Minus;
		std::cout << " Voice: " << (int)Voice;
		std::cout << " OK: " << (int)OK;
		std::cout << " Left: " << (int)Left;
		std::cout << " Phone: " << (int)Phone;
		std::cout << " PlusSet: " << (int)PlusSet;
		std::cout << " Distance: " << (int)Distance;
		std::cout << " MinusSet: " << (int)MinusSet;
		std::cout << " LFA: " << (int)LFA;
		std::cout << " SCC: " << (int)SCC;
		std::cout << " CC: " << (int)CC;
		std::cout << " DriveMode: " << (int)DriveMode;
		std::cout << " LightHeight: " << (int)LightHeight;
		std::cout << " ACCpedal: " << (int)ACCpedal;
		std::cout << " Brakepedal: " << (int)Brakepedal;
		std::cout << " bMask: " << (int)bMask;
		std::cout << std::endl;

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("CanbinSwitchCore::handlePacke Parse error: " + std::string(e.what()), "CanbinSwitchCore::handlePacket");
	}
}

/*-----------------
	Motion Core
-----------------*/
MotionCore::MotionCore(const std::string& name, const std::string& ip, unsigned short port, const std::string& clientIp, unsigned short clientPort)
	: Core(name, ip, port, clientIp, clientPort)
{
	_bSize = sizeof(SendPacketHeader) + sizeof(SendMotionPacket);
	_sMask = 0x0014;
	_tick = 10; // 100hz

	_directSendIp = Utils::getEnv("UE_IP");
	_directSendPort = stoi(Utils::getEnv("UE_RECV_MOTION_PORT"));
}

void MotionCore::sendLoop()
{
	while (_running)
	{
		long long now = Utils::GetNowTimeMs();
		if (now - _lastSendMs < _tick) continue;

		_lastSendMs = now;

		const int bufferSize = sizeof(SendMotionPacket) + sizeof(SendPacketHeader);
		std::vector<unsigned char> buffer(bufferSize);

		// 헤더 메모리 복사
		std::memcpy(buffer.data(), &_sNetVersion, sizeof(unsigned short));   // 0~1 바이트에 sNetVersion
		std::memcpy(buffer.data() + 2, &_sMask, sizeof(unsigned short));      // 2~3 바이트에 sMask
		std::memcpy(buffer.data() + 4, &_bSize, sizeof(unsigned char));       // 4~5 바이트에 bSize

		// 데이터 메모리 복사
		std::memcpy(buffer.data() + 5, &_sendMotionPacket, sizeof(SendMotionPacket)); // 5~ 바이트에 SendMotionPacket 전체

		sendTo(buffer, _scheduledSendIp, _scheduledSendPort);
	}
}

void MotionCore::handlePacket(const std::vector<unsigned char>& buffer)
{
	try
	{
		uint32_t FrameCounter1 = *reinterpret_cast<const uint32_t*>(&buffer[5]);
		uint32_t motionStatus1 = *reinterpret_cast<const uint32_t*>(&buffer[9]);
		uint32_t errorLevel1 = *reinterpret_cast<const uint32_t*>(&buffer[13]);
		uint32_t errorCode1 = *reinterpret_cast<const uint32_t*>(&buffer[17]);
		uint32_t ioInfo1 = *reinterpret_cast<const uint32_t*>(&buffer[21]);

		float xPosition1 = *reinterpret_cast<const float*>(&buffer[25]);
		float yPosition1 = *reinterpret_cast<const float*>(&buffer[29]);
		float zPosition1 = *reinterpret_cast<const float*>(&buffer[33]);
		float yawPosition1 = *reinterpret_cast<const float*>(&buffer[37]);
		float pitchPosition1 = *reinterpret_cast<const float*>(&buffer[41]);
		float rollPosition1 = *reinterpret_cast<const float*>(&buffer[45]);

		float xSpeed1 = *reinterpret_cast<const float*>(&buffer[49]);
		float ySpeed1 = *reinterpret_cast<const float*>(&buffer[53]);
		float zSpeed1 = *reinterpret_cast<const float*>(&buffer[57]);
		float yawSpeed1 = *reinterpret_cast<const float*>(&buffer[61]);
		float pitchSpeed1 = *reinterpret_cast<const float*>(&buffer[65]);
		float rollSpeed1 = *reinterpret_cast<const float*>(&buffer[69]);

		float xAcc1 = *reinterpret_cast<const float*>(&buffer[73]);
		float yAcc1 = *reinterpret_cast<const float*>(&buffer[77]);
		float zAcc1 = *reinterpret_cast<const float*>(&buffer[81]);
		float yawAcc1 = *reinterpret_cast<const float*>(&buffer[85]);
		float pitchAcc1 = *reinterpret_cast<const float*>(&buffer[89]);
		float rollAcc1 = *reinterpret_cast<const float*>(&buffer[93]);

		float actuator1Length1 = *reinterpret_cast<const float*>(&buffer[97]);
		float actuator2Length1 = *reinterpret_cast<const float*>(&buffer[101]);
		float actuator3Length1 = *reinterpret_cast<const float*>(&buffer[105]);
		float actuator4Length1 = *reinterpret_cast<const float*>(&buffer[109]);
		float actuator5Length1 = *reinterpret_cast<const float*>(&buffer[113]);
		float actuator6Length1 = *reinterpret_cast<const float*>(&buffer[117]);

		float analogInput1_1 = *reinterpret_cast<const float*>(&buffer[121]);
		float analogInput2_1 = *reinterpret_cast<const float*>(&buffer[125]);
		float analogInput3_1 = *reinterpret_cast<const float*>(&buffer[129]);
		float analogInput4_1 = *reinterpret_cast<const float*>(&buffer[133]);

		std::cout << "FrameCounter1: " << FrameCounter1 << " ";
		std::cout << "motionStatus1: " << motionStatus1 << " ";
		std::cout << "errorLevel1: " << errorLevel1 << " ";
		std::cout << "errorCode1: " << errorCode1 << " ";
		std::cout << "ioInfo1: " << ioInfo1 << " ";

		std::cout << "xPosition1: " << xPosition1 << " ";
		std::cout << "yPosition1: " << yPosition1 << " ";
		std::cout << "zPosition1: " << zPosition1 << " ";
		std::cout << "yawPosition1: " << yawPosition1 << " ";
		std::cout << "pitchPosition1: " << pitchPosition1 << " ";
		std::cout << "rollPosition1: " << rollPosition1 << " ";

		std::cout << "xSpeed1: " << xSpeed1 << " ";
		std::cout << "ySpeed1: " << ySpeed1 << " ";
		std::cout << "zSpeed1: " << zSpeed1 << " ";
		std::cout << "yawSpeed1: " << yawSpeed1 << " ";
		std::cout << "pitchSpeed1: " << pitchSpeed1 << " ";
		std::cout << "rollSpeed1: " << rollSpeed1 << " ";

		std::cout << "xAcc1: " << xAcc1 << " ";
		std::cout << "yAcc1: " << yAcc1 << " ";
		std::cout << "zAcc1: " << zAcc1 << " ";
		std::cout << "yawAcc1: " << yawAcc1 << " ";
		std::cout << "pitchAcc1: " << pitchAcc1 << " ";
		std::cout << "rollAcc1: " << rollAcc1 << " ";

		std::cout << "actuator1Length1: " << actuator1Length1 << " ";
		std::cout << "actuator2Length1: " << actuator2Length1 << " ";
		std::cout << "actuator3Length1: " << actuator3Length1 << " ";
		std::cout << "actuator4Length1: " << actuator4Length1 << " ";
		std::cout << "actuator5Length1: " << actuator5Length1 << " ";
		std::cout << "actuator6Length1: " << actuator6Length1 << " ";

		std::cout << "analogInput1_1: " << analogInput1_1 << " ";
		std::cout << "analogInput2_1: " << analogInput2_1 << " ";
		std::cout << "analogInput3_1: " << analogInput3_1 << " ";
		std::cout << "analogInput4_1: " << analogInput4_1 << std::endl;

		sendTo(buffer, _directSendIp, _directSendPort);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("MotionCore::handlePacke Parse error: " + std::string(e.what()), "MotionCore::handlePacket");
	}
}

