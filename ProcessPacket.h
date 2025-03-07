#pragma once
#include <memory>
#include "Core.h"

class ProcessPacket
{
public:
	static void handlePacket(const std::vector<unsigned char>& buffer);

public:
	// sMask = 1
	static void handleSteerPacket(const std::vector<unsigned char>& buffer);

	// sMask = 2
	static void handleCabinControlPacket(const std::vector<unsigned char>& buffer);

	// sMask = 3
	static void handleCabinSwitchPacket(const std::vector<unsigned char>& buffer);


public:
	static std::shared_ptr<HandleCore> _handleCore;
	static std::shared_ptr<CabinControlCore> _canbinControlCore;
	static std::shared_ptr<CanbinSwitchCore> _canbinSwitchCore;
	static std::shared_ptr<MotionCore> _motionCore;
};

