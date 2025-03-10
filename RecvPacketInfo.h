#pragma once

#pragma pack(push,1)
struct RecvPacketHeader
{
	unsigned short sNetVersion;
	short sMask;
	unsigned char bSize;
};
#pragma pack(pop)

#pragma pack(push,1)
struct SteerPacket {
	unsigned __int32 status;
	float steerAngle;
	float steerAngleRate;
};
#pragma pack(pop)

#pragma pack(push,1)
struct CabinControlPacket {
	unsigned __int16 status;
	float carHeight;
	float carWidth;
	float seatWidth;
};
#pragma pack(pop)

#pragma pack(push,1)
struct CabinSwitchPacket {
	unsigned char GearTriger;                // 1: �Ʒ��� ��ĭ, 2: �Ʒ��� ��ĭ, 3: �߰�, 4: ���� ��ĭ, 5: ���� ��ĭ
	unsigned char GearP;                     // 2: Off, 1: On
	unsigned char Left_Paddle_Shift;         // 0: Off, 1: On
	unsigned char Right_Paddle_Shift;        // 0: Off, 1: On
	unsigned char Crs;                       // 1: �ӵ� +, 2: �ӵ� -, 3: ���� �Ÿ�, 4: �ӵ� ��ư Ŭ��, 8: ũ����
	unsigned char voice;                     // 0: Off, 1: On
	unsigned char phone;                     // 0: Off, 1: On
	unsigned char mode;                      // 0: Off, 1: On
	unsigned char modeUp;                    // 0: Off, 1: On
	unsigned char modeDown;                  // 0: Off, 1: On
	unsigned char volumeMute;                // 0: Off, 1: On
	unsigned char volumeWheel;               // 0: Off, 1: Down, 2: Up
	unsigned char Menu;                      // 0: Off, 1: On
	unsigned char MenuWheelbtn;              // 0: Off, 1: On
	unsigned char Menuwheel;                 // 0: Off, 1: Down, 2: Up
	unsigned char bookmark;                  // 0: Off, 1: On
	unsigned char Lamp_TrnSigLftSwSta;      // 0: Off, 1: On
	unsigned char Lamp_TrnSigRtSwSta;       // 0: Off, 1: On
	unsigned char Light;                     // 0: Off, 1: Auto, 2: ����Ʈ1, 3: ����Ʈ2
	unsigned char Lamp_HdLmpHiSwSta1;       // 0: Off, 1: On (push)
	unsigned char Lamp_HdLmpHiSwSta2;       // 0: Off, 1: On (pull)
	unsigned char Wiper_FrWiperMist;        // 0: Off, 1: On
	unsigned char Wiper_FrWiperWshSwSta;    // 0: Off, 1: Auto, 2: Lo, 3: Hi
	unsigned char Wiper_FrWiperWshSwSta2;   // 0~4 (Level)
	unsigned char Wiper_RrWiperWshSwSta;    // 0: Off, 1: Lo, 2: Hi
	unsigned char NGB;                       // 0: Off, 1: On
	unsigned char DriveModeSw;               // 0: Off, 1: On
	unsigned char LeftN;                     // 0: Off, 1: On
	unsigned char RightN;                    // 0: Off, 1: On
	unsigned char HOD_Dir_Status;            // 0: �̰���, 1~10 (��Ƽ� ����)
	unsigned char FWasher;                   // 0: Off, 1: On
	unsigned char Parking;                   // 0: Off, 1: pull, 2: push
	unsigned char SeatBelt1;                 // 0: Off, 1: On
	unsigned char SeatBelt2;                 // 0: Off, 1: On
	unsigned char EMG;                       // 0: Off, 1: On
	unsigned char Key;                       // 0: Off, 1: On
	unsigned char Trunk;                     // 0: Off, 1: On
	unsigned char VDC;                       // 0: Off, 1: On
	unsigned char Booster;                   // 0: Off, 1: On
	unsigned char Plus;                      // 0: Off, 1: On
	unsigned char Right;                     // 0: Off, 1: On
	unsigned char Minus;                     // 0: Off, 1: On
	unsigned char Voice;                     // 0: Off, 1: On
	unsigned char OK;                        // 0: Off, 1: On
	unsigned char Left;                       // 0: Off, 1: On
	unsigned char Phone;                      // 0: Off, 1: On
	unsigned char PlusSet;                   // 0: Off, 1: On
	unsigned char Distance;                  // 0: Off, 1: On
	unsigned char MinusSet;                  // 0: Off, 1: On
	unsigned char LFA;                       // 0: Off, 1: On
	unsigned char SCC;                       // 0: Off, 1: On
	unsigned char CC;                        // 0: Off, 1: On
	unsigned char DriveMode;                 // 0: Off, 1: On
	unsigned char LightHeight;               // 0~3 (Level)
	unsigned char ACCpedal;                  // 0~255
	unsigned char Brakepedal;                // 0~255
	unsigned char bMask;                     // (unsigned char)0xef
};
#pragma pack(pop)

#pragma pack(push,1)
struct MotionPacket {
	uint32_t FrameCounter1;
	uint32_t motionStatus1;
	uint32_t errorLevel1;
	uint32_t errorCode1;
	uint32_t ioInfo1;

	float xPosition1;
	float yPosition1;
	float zPosition1;
	float yawPosition1;
	float pitchPosition1;
	float rollPosition1;

	float xSpeed1;
	float ySpeed1;
	float zSpeed1;
	float yawSpeed1;
	float pitchSpeed1;
	float rollSpeed1;

	float xAcc1;
	float yAcc1;
	float zAcc1;
	float yawAcc1;
	float pitchAcc1;
	float rollAcc1;

	float actuator1Length1;
	float actuator2Length1;
	float actuator3Length1;
	float actuator4Length1;
	float actuator5Length1;
	float actuator6Length1;

	float analogInput1_1;
	float analogInput2_1;
	float analogInput3_1;
	float analogInput4_1;
};
#pragma pack(pop)