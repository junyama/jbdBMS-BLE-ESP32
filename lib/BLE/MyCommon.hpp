#ifndef MY_COMMON_HPP
#define MY_COMMON_HPP

#include <Arduino.h>
//#include "ESPDateTime.h"
#include "BLEDevice.h"

typedef struct
{
	byte start;
	byte type;
	byte status;
	byte dataLen;
} bmsPacketHeaderStruct;

typedef struct
{
	uint16_t Volts; // unit 1mV
	int32_t Amps;	// unit 1mA
	int32_t Watts;	// unit 1W
	uint16_t CapacityRemainAh;
	uint8_t CapacityRemainPercent; // unit 1%
	uint32_t CapacityRemainWh;	   // unit Wh
	uint16_t Temp1;				   // unit 0.1C
	uint16_t Temp2;				   // unit 0.1C
	uint16_t BalanceCodeLow;
	uint16_t BalanceCodeHigh;
	uint8_t MosfetStatus;
} packBasicInfoStruct;

typedef struct
{
	uint8_t NumOfCells;
	uint16_t CellVolt[15]; // cell 1 has index 0 :-/
	uint16_t CellMax;
	uint16_t CellMin;
	uint16_t CellDiff; // difference between highest and lowest
	uint16_t CellAvg;
	uint16_t CellMedian;
	uint32_t CellColor[15];
	uint32_t CellColorDisbalance[15]; // green cell == median, red/violet cell => median + c_cellMaxDisbalance
} packCellInfoStruct;

struct packEepromStruct
{
	uint16_t POVP;
	uint16_t PUVP;
	uint16_t COVP;
	uint16_t CUVP;
	uint16_t POVPRelease;
	uint16_t PUVPRelease;
	uint16_t COVPRelease;
	uint16_t CUVPRelease;
	uint16_t CHGOC;
	uint16_t DSGOC;
};

//#define STRINGBUFFERSIZE 300
// char stringBuffer[STRINGBUFFERSIZE];

const int32_t c_cellNominalVoltage = 3700; // mV

const uint16_t c_cellAbsMin = 3000;
const uint16_t c_cellAbsMax = 4200;

const int32_t c_packMaxWatt = 1250;

const uint16_t c_cellMaxDisbalance = 1500; // 200; // cell different by this value from cell median is getting violet (worst) color

//---- global variables ----
extern boolean doConnect;
extern boolean BLE_client_connected;
extern boolean doScan;

extern packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
extern packEepromStruct packEeprom;		  // here shall be the latest data got from BMS
extern packCellInfoStruct packCellInfo;	  // here shall be the latest data got from BMS

extern boolean BLE_client_connected;
extern bool newPacketReceived;

//  ----- BLE stuff -----
extern BLERemoteCharacteristic *pRemoteCharacteristic;
extern BLEAdvertisedDevice *myDevice;
extern BLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
extern BLEUUID serviceUUID; // xiaoxiang bms original module
extern BLEUUID charUUID_tx; // xiaoxiang bms original module
extern BLEUUID charUUID_rx; // xiaoxiang bms original module

extern const String TAG;
extern void LOGD(String tag, String text);
extern int16_t two_ints_into16(int highbyte, int lowbyte); // turns two bytes into a single long integer
extern bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen);
extern bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen);
extern bool isPacketValid(byte *packet); // check if packet is valid



#endif /* MY_COMMON_HPP */