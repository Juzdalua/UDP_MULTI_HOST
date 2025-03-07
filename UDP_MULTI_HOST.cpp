#include <iostream>
#include <memory>
#include "Utils.h"
#include "Core.h"
#include "ProcessPacket.h"

int main()
{
	Utils::EnvInit();

	// 호스트 세팅
	std::vector<std::shared_ptr<Core>> cores;
	std::string hostIp = Utils::getEnv("HOST_IP");
	int hostHandlePorts = stoi(Utils::getEnv("HOST_HANDLE_PORT"));
	int hostCabinControlPorts = stoi(Utils::getEnv("HOST_CABIN_CONTROL_PORT"));
	int hostCanbinSwitchPorts = stoi(Utils::getEnv("HOST_CABIN_SWITCH_PORT"));
	int hostMotionPorts = stoi(Utils::getEnv("HOST_MOTION_PORT"));

	// Client 세팅
	std::string handleIp = Utils::getEnv("HANDLE_IP");
	int handlePort = stoi(Utils::getEnv("HANDLE_PORT"));

	std::string cabinControlIp = Utils::getEnv("CABIN_CONTROL_IP");
	int cabinControlPort = stoi(Utils::getEnv("CABIN_CONTROL_PORT"));

	std::string cabinSwitchIp = Utils::getEnv("CABIN_SWITCH_IP");
	int cabinSwitchPort = stoi(Utils::getEnv("CABIN_SWITCH_PORT"));

	std::string motionIp = Utils::getEnv("MOTION_IP");
	int motionPort = stoi(Utils::getEnv("MOTION_PORT"));

	std::shared_ptr<HandleCore> handleCore = std::make_shared<HandleCore>("HANDLE", hostIp, hostHandlePorts, handleIp, handlePort);
	std::shared_ptr<CabinControlCore> canbinControlCore = std::make_shared<CabinControlCore>("CABIN_CONTROL", hostIp, hostCabinControlPorts, cabinControlIp, cabinControlPort);
	std::shared_ptr<CanbinSwitchCore> canbinSwitchCore = std::make_shared<CanbinSwitchCore>("CABIN_SWITCH", hostIp, hostCanbinSwitchPorts, cabinSwitchIp, cabinSwitchPort);
	std::shared_ptr<MotionCore> motionCore = std::make_shared<MotionCore>("MOTION", hostIp, hostMotionPorts, motionIp, motionPort);
	cores.emplace_back(handleCore);
	cores.emplace_back(canbinControlCore);
	cores.emplace_back(canbinSwitchCore);
	cores.emplace_back(motionCore);

	ProcessPacket::_handleCore = handleCore;
	ProcessPacket::_canbinControlCore = canbinControlCore;
	ProcessPacket::_canbinSwitchCore = canbinSwitchCore;
	ProcessPacket::_motionCore = motionCore;

	// 호스트 스레드 구동
	for (auto& core : cores) {
		core->start();
	}

	while (true) {}

	// Stop all cores
	for (auto& core : cores) {
		core->stop();
	}

	return 0;
}
