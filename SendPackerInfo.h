#pragma once

/*------------------------------
    Header
------------------------------*/
#pragma pack(push,1)
struct SendPacketHeader
{
	unsigned short sNetVersion;
	short sMask;
	unsigned char bSize;
};
#pragma pack(pop)

#pragma pack(push,1)
struct JsonPacketHeader
{
    UINT size;
    UINT id;
    UINT seq;
};
#pragma pack(pop)

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
    ControlCabin
------------------------------*/


/*------------------------------
    Motion
------------------------------*/


/*------------------------------
    
------------------------------*/