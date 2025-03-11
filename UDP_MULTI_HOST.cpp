#include <iostream>
#include <memory>
#include "Utils.h"
#include "Core.h"

int main()
{
	try
	{
		Utils::EnvInit();
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Main env init error: " + std::string(e.what()), "main");
		return 0;
	}

	std::vector<std::shared_ptr<Core>> cores;

	// 호스트 세팅
	std::string hostIp = Utils::getEnv("HOST_IP");
	int hostHandlePorts = stoi(Utils::getEnv("HOST_INNO_HANDLE_PORT"));
	int hostCabinControlPorts = stoi(Utils::getEnv("HOST_INNO_CABIN_CONTROL_PORT"));
	int hostCanbinSwitchPorts = stoi(Utils::getEnv("HOST_INNO_CABIN_SWITCH_PORT"));
	int hostMotionPorts = stoi(Utils::getEnv("HOST_INNO_MOTION_PORT"));

	// INNO Client 세팅
	std::string handleIp = Utils::getEnv("HANDLE_IP");
	int handlePort = stoi(Utils::getEnv("HANDLE_PORT"));

	std::string cabinControlIp = Utils::getEnv("CABIN_CONTROL_IP");
	int cabinControlPort = stoi(Utils::getEnv("CABIN_CONTROL_PORT"));

	std::string cabinSwitchIp = Utils::getEnv("CABIN_SWITCH_IP");
	int cabinSwitchPort = stoi(Utils::getEnv("CABIN_SWITCH_PORT"));

	std::string motionIp = Utils::getEnv("MOTION_IP");
	int motionPort = stoi(Utils::getEnv("MOTION_PORT"));

	try
	{
		std::shared_ptr<HandleCore> innoHandleCore = std::make_shared<HandleCore>("INNO_HANDLE", hostIp, hostHandlePorts, handleIp, handlePort, PeerType::INNO);
		std::shared_ptr<CabinControlCore> innoCanbinControlCore = std::make_shared<CabinControlCore>("INNO_CABIN_CONTROL", hostIp, hostCabinControlPorts, cabinControlIp, cabinControlPort, PeerType::INNO);
		std::shared_ptr<CanbinSwitchCore> innoCanbinSwitchCore = std::make_shared<CanbinSwitchCore>("INNO_CABIN_SWITCH", hostIp, hostCanbinSwitchPorts, cabinSwitchIp, cabinSwitchPort, PeerType::INNO);
		std::shared_ptr<MotionCore> innoMotionCore = std::make_shared<MotionCore>("INNO_MOTION", hostIp, hostMotionPorts, motionIp, motionPort, PeerType::INNO);
		cores.emplace_back(innoHandleCore);
		cores.emplace_back(innoCanbinControlCore);
		cores.emplace_back(innoCanbinSwitchCore);
		cores.emplace_back(innoMotionCore);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Main inno init error: " + std::string(e.what()), "main");
		return 0;
	}

	// UE5 Client 세팅
	int hostUeHandlePorts = stoi(Utils::getEnv("HOST_UE_HANDLE_PORT"));
	int hostUeCabinControlPorts = stoi(Utils::getEnv("HOST_UE_CABIN_CONTROL_PORT"));
	int hostUeCanbinSwitchPorts = stoi(Utils::getEnv("HOST_UE_CABIN_SWITCH_PORT"));
	int hostUeMotionPorts = stoi(Utils::getEnv("HOST_UE_MOTION_PORT"));

	std::string ueIp = Utils::getEnv("UE_IP");
	int ueHandlePort = stoi(Utils::getEnv("UE_HANDLE_PORT"));
	int ueCabinControlPort = stoi(Utils::getEnv("UE_CABIN_CONTROL_PORT"));
	int ueCabinSwitchPort = stoi(Utils::getEnv("UE_CABIN_SWITCH_PORT"));
	int ueMotionPort = stoi(Utils::getEnv("UE_MOTION_PORT"));

	try
	{
		std::shared_ptr<HandleCore> ueHandleCore = std::make_shared<HandleCore>("UE_HANDLE", hostIp, hostUeHandlePorts, ueIp, ueHandlePort, PeerType::UE);
		std::shared_ptr<CabinControlCore> ueCanbinControlCore = std::make_shared<CabinControlCore>("UE_CABIN_CONTROL", hostIp, hostUeCabinControlPorts, ueIp, ueCabinControlPort, PeerType::UE);
		std::shared_ptr<CanbinSwitchCore> ueCanbinSwitchCore = std::make_shared<CanbinSwitchCore>("UE_CABIN_SWITCH", hostIp, hostUeCanbinSwitchPorts, ueIp, ueCabinSwitchPort, PeerType::UE);
		std::shared_ptr<MotionCore> ueMotionCore = std::make_shared<MotionCore>("UE_MOTION", hostIp, hostUeMotionPorts, ueIp, ueMotionPort, PeerType::UE);
		cores.emplace_back(ueHandleCore);
		cores.emplace_back(ueCanbinControlCore);
		cores.emplace_back(ueCanbinSwitchCore);
		cores.emplace_back(ueMotionCore);
	}
	catch (const std::exception& e)
	{
		Utils::LogError("Main UE init error: " + std::string(e.what()), "main");
		return 0;
	}

	// 호스트 스레드 구동
	for (auto& core : cores) {
		core->start();
	}

	// 메인스레드 대기
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	// Stop all cores
	for (auto& core : cores) {
		core->stop();
	}

	return 0;
}
