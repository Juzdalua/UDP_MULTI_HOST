#pragma once
//#include "RecvPacketInfo.h"

#include <memory>
#include <string>

/*------------------------------
	Handle
------------------------------*/
#pragma pack(push, 1)
struct SendHandlePacket
{
	unsigned __int32 simState;          // 4바이트
	float velocity;                     // 4바이트
	float wheelAngleVelocityLF;         // 4바이트
	float wheelAngleVelocityRF;         // 4바이트
	float wheelAngleVelocityLB;         // 4바이트
	float wheelAngleVelocityRB;         // 4바이트
	float targetAngle;                  // 4바이트
};
#pragma pack(pop)

/*------------------------------
	CabinControl
------------------------------*/
#pragma pack(push, 1) 
struct SendCabinControlPacket {
	unsigned __int16 command;
	unsigned __int16 activationFlag; // 0-모두off 1-핸들off 2-핸들on 3-벨트off 4-벨트_0_on 5-벨트_1_on 6-벨트_2_on
	unsigned __int16 handleStrength;
	unsigned __int16 seatBeltStrength;
	unsigned __int16 manual;
	float height;
	float width;
	float seatHeight;
};
#pragma pack(pop)

/*------------------------------
	CabinSwitch
------------------------------*/
#pragma pack(push, 1) 
struct SendCabinSwitchPacket {
	unsigned char currentGear;
	unsigned char brakeLamp;
	unsigned char leftLamp;
	unsigned char rightLamp;
	unsigned char alertLamp;
	unsigned char reverseLamp;
	unsigned char headLamp;
};
#pragma pack(pop)

/*------------------------------
	Motion
------------------------------*/
#pragma pack(push, 1) 
struct SendMotionPacket {
	uint32_t FrameCounter;
	uint32_t motionCommand;
	float xAcc;
	float yAcc;
	float zAcc;
	float pDot;        // Z angular acceleration
	float qDot;        // Y angular acceleration
	float rDot;        // X angular acceleration
	float p;           // Z angular velocity
	float q;           // Y angular velocity
	float r;           // X angular velocity
	float psi;         // Yaw angle
	float theta;       // Pitch angle
	float phi;         // Roll angle
	float vehicleSpeed;
	uint32_t Flags;
	float xAccelerationAddPilot;
	float yAccelerationAddPilot;
	float zAccelerationAddPilot;
	float directXPos;
	float directYPos;
	float directZPos;
	float directYawPos;
	float directPitchPos;
	float directRollPos;
	float xPosCogToPilot;
	float yPosCogToPilot;
	float zPosCogToPilot;
	float directYawFactor;
	float directPitchFactor;
	float directRollFactor;
	float directPitchAtCOG;
	float directRollAtCOG;
	float directPitchByTilt;
	float directRollByTilt;
	float bump1;
	float bump2;
	float bump3;
	float trub01Frequency;
	float turb01AmpX;
	float turb01AmpY;
	float turb01AmpZ;
	float trub02Frequency;
	float turb02AmpX;
	float turb02AmpY;
	float turb02AmpZ;
	float trub03Frequency;
	float turb03AmpX;
	float turb03AmpY;
	float turb03AmpZ;
	float trub04Frequency;
	float turb04AmpX;
	float turb04AmpY;
	float turb04AmpZ;
	float trub05Frequency;
	float turb05AmpX;
	float turb05AmpY;
	float turb05AmpZ;
	float trub06Frequency;
	float turb06AmpX;
	float turb06AmpY;
	float turb06AmpZ;
	float trub07Frequency;
	float turb07AmpX;
	float turb07AmpY;
	float turb07AmpZ;
	float trub08Frequency;
	float turb08AmpX;
	float turb08AmpY;
	float turb08AmpZ;
	float trub09Frequency;
	float turb09AmpX;
	float turb09AmpY;
	float turb09AmpZ;
	float trub10Frequency;
	float turb10AmpX;
	float turb10AmpY;
	float turb10AmpZ;
};
#pragma pack(pop)

#pragma pack(push, 1) 
struct SendTimemachinePacket {
	std::string timestamp;
	double steering;
	double velocity;
	double roll;
	double pitch;
	double yaw;
	double xAcc;
	double yAcc;
	double zAcc;
	double rDot;
	double vehicleSpeed;
	unsigned char currentGear;
	unsigned char brakeLamp;
	unsigned char leftLamp;
	unsigned char rightLamp;
	unsigned char alertLamp;
	unsigned char reverseLamp;
	unsigned char headLamp;
};
#pragma pack(pop)

/*------------------------------
	CheckConnectin
------------------------------*/
#pragma pack(push, 1)
struct SendCheckConnectionPacket {
	unsigned __int32 handleStatus;
	float steerAngle;
	unsigned __int16 cabinControlStatus;
	unsigned __int16 cabinControlDigitalInput;
	unsigned char cabinSwitchAccPedal;
	uint32_t motionFrameCounter;
	uint32_t motionStatus;
	uint32_t motionErrorLevel;
	uint32_t motionErrorCode;
};
#pragma pack(pop)

/*------------------------------
	CommonSendPacket
------------------------------*/
class CommonSendPacket
{
public:
	SendHandlePacket _sendHandlePacket = { 0 };
	SendCabinControlPacket _sendCabinControlPacket = { 0 };
	SendCabinSwitchPacket _sendCabinSwitchPacket = { 'P',0,0,0,0,0,0 };
	SendMotionPacket _sendMotionPacket = { 0 };
	SendCheckConnectionPacket _sendCheckConnectionPacket = { 0 };
};
extern std::shared_ptr<CommonSendPacket> commonSendPacket;
