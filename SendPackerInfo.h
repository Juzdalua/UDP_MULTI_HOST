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
    unsigned __int32 simState;          // 4����Ʈ
    float velocity;                     // 4����Ʈ
    float wheelAngleVelocityLF;         // 4����Ʈ
    float wheelAngleVelocityRF;         // 4����Ʈ
    float wheelAngleVelocityLB;         // 4����Ʈ
    float wheelAngleVelocityRB;         // 4����Ʈ
    float targetAngle;                  // 4����Ʈ
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