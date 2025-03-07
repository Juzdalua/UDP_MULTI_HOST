#include <iostream>
#include "Utils.h"
#include "Core.h"
#include <memory>

int main()
{

	Utils::EnvInit();

	/*ServerCore server(SERVER_IP, SERVER_PORT);
	if (!server.init()) {
		std::cerr << "Server initialization failed." << std::endl;
		return 1;
	}

	std::cout << "UDP IOCP Server running on";
	std::cout << " IP: " << SERVER_IP;
	std::cout << " PORT: " << SERVER_PORT;
	std::cout << '\n';

	server.startServer();




	server.stopServer();*/

	// 호스트 세팅
	std::vector<std::shared_ptr<Core>> cores;
	std::string ip = Utils::getEnv("HOST_IP");
	int handlePorts = stoi(Utils::getEnv("HANDLE_PORT"));
	int cabinControlPorts = stoi(Utils::getEnv("CABIN_CONTROL_PORT"));
	int canbinSwitchPorts = stoi(Utils::getEnv("CABIN_SWITCH_PORT"));
	int motionPorts = stoi(Utils::getEnv("MOTION_PORT"));

	std::shared_ptr<Core> handleCore = std::make_shared<Core>("HANDLE", ip, handlePorts);
	std::shared_ptr<Core> canbinControlCore = std::make_shared<Core>("CABIN_CONTROL", ip, cabinControlPorts);
	std::shared_ptr<Core> canbinSwitchCore = std::make_shared<Core>("CABIN_SWITCH", ip, canbinSwitchPorts);
	std::shared_ptr<Core> motionCore = std::make_shared<Core>("MOTION", ip, motionPorts);
	cores.emplace_back(handleCore);
	cores.emplace_back(canbinControlCore);
	cores.emplace_back(canbinSwitchCore);
	cores.emplace_back(motionCore);

	// Create Core instances for each host
	for (auto& core : cores) {
		core->start();
	}

	while (true) {}
	// Stop all cores
	/*for (auto& core : cores) {
		core->stop();
	}*/

	return 0;
}
