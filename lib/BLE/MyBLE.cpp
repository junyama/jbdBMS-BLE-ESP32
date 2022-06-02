#ifndef MY_BLE_CPP_myBms
#define MY_BLE_CPP_

#include <Arduino.h>
//#include <MyBMS.h>
#include "ESPDateTime.h"

#include "BLEDevice.h"
#include "mydatatypes.h"
//#include "BMS_process_data.cpp"

#define commSerial Serial

// HardwareSerial commSerial(0);
HardwareSerial bmsSerial(1);

//---- global variables ----
static boolean doConnect = false;
static boolean BLE_client_connected = false;
static boolean doScan = false;

packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
packEepromStruct packEeprom;       // here shall be the latest data got from BMS
packCellInfoStruct packCellInfo;   // here shall be the latest data got from BMS

const byte cBasicInfo3 = 3; // type of packet 3= basic info
const byte cCellInfo4 = 4;  // type of packet 4= individual cell info

unsigned long previousMillis = 0;
const long interval = 2000;

bool toggle = false;
bool newPacketReceived = false;

//  ----- BLE stuff -----
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;
BLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
static BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
static BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

static String TAG = "GLOBAL";
static void LOGD(String tag, String text)
{
    Serial.print("[" + DateTime.toString() + "] ");
    Serial.print(tag + ": ");
    Serial.println(text);
}

static int16_t two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
    // TRACE;
    int16_t result = (highbyte);
    result <<= 8;                // Left shift 8 bits,
    result = (result | lowbyte); // OR operation, merge the two
    return result;
}

static bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen)
{
    // TRACE;
    //  Expected data len
    if (dataLen != 0x1B)
    {
        return false;
    }

    output->Volts = ((uint32_t)two_ints_into16(data[0], data[1])) * 10; // Resolution 10 mV -> convert to milivolts   eg 4895 > 48950mV
    output->Amps = ((int32_t)two_ints_into16(data[2], data[3])) * 10;   // Resolution 10 mA -> convert to miliamps

    output->Watts = output->Volts * output->Amps / 1000000; // W

    output->CapacityRemainAh = ((uint16_t)two_ints_into16(data[4], data[5])) * 10;
    output->CapacityRemainPercent = ((uint8_t)data[19]);

    output->CapacityRemainWh = (output->CapacityRemainAh * c_cellNominalVoltage) / 1000000 * packCellInfo.NumOfCells;

    output->Temp1 = (((uint16_t)two_ints_into16(data[23], data[24])) - 2731);
    output->Temp2 = (((uint16_t)two_ints_into16(data[25], data[26])) - 2731);
    output->BalanceCodeLow = (two_ints_into16(data[12], data[13]));
    output->BalanceCodeHigh = (two_ints_into16(data[14], data[15]));
    output->MosfetStatus = ((byte)data[20]);

    return true;
}

static bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen)
{
    // TRACE;
    uint16_t _cellSum;
    uint16_t _cellMin = 5000;
    uint16_t _cellMax = 0;
    uint16_t _cellAvg;
    uint16_t _cellDiff;

    output->NumOfCells = dataLen / 2; // Data length * 2 is number of cells !!!!!!

    // go trough individual cells
    for (byte i = 0; i < dataLen / 2; i++)
    {
        output->CellVolt[i] = ((uint16_t)two_ints_into16(data[i * 2], data[i * 2 + 1])); // Resolution 1 mV
        _cellSum += output->CellVolt[i];
        if (output->CellVolt[i] > _cellMax)
        {
            _cellMax = output->CellVolt[i];
        }
        if (output->CellVolt[i] < _cellMin)
        {
            _cellMin = output->CellVolt[i];
        }

        // output->CellColor[i] = getPixelColorHsv(mapHue(output->CellVolt[i], c_cellAbsMin, c_cellAbsMax), 255, 255);
    }
    output->CellMin = _cellMin;
    output->CellMax = _cellMax;
    output->CellDiff = _cellMax - _cellMin; // Resolution 10 mV -> convert to volts
    output->CellAvg = _cellSum / output->NumOfCells;

    //----cell median calculation----
    uint16_t n = output->NumOfCells;
    uint16_t i, j;
    uint16_t temp;
    uint16_t x[n];

    for (uint8_t u = 0; u < n; u++)
    {
        x[u] = output->CellVolt[u];
    }

    for (i = 1; i <= n; ++i) // sort data
    {
        for (j = i + 1; j <= n; ++j)
        {
            if (x[i] > x[j])
            {
                temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }

    if (n % 2 == 0) // compute median
    {
        output->CellMedian = (x[n / 2] + x[n / 2 + 1]) / 2;
    }
    else
    {
        output->CellMedian = x[n / 2 + 1];
    }

    for (uint8_t q = 0; q < output->NumOfCells; q++)
    {
        uint32_t disbal = abs(output->CellMedian - output->CellVolt[q]);
        // output->CellColorDisbalance[q] = getPixelColorHsv(mapHue(disbal, c_cellMaxDisbalance, 0), 255, 255);
    }
    return true;
}

static bool isPacketValid(byte *packet) // check if packet is valid
{
    // TRACE;
    if (packet == nullptr)
    {
        return false;
    }

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    int checksumLen = pHeader->dataLen + 2; // status + data len + data

    if (pHeader->start != 0xDD)
    {
        return false;
    }

    int offset = 2; // header 0xDD and command type are skipped

    byte checksum = 0;
    for (int i = 0; i < checksumLen; i++)
    {
        checksum += packet[offset + i];
    }

    // printf("checksum: %x\n", checksum);

    checksum = ((checksum ^ 0xFF) + 1) & 0xFF;
    // printf("checksum v2: %x\n", checksum);

    byte rxChecksum = packet[offset + checksumLen + 1];

    if (checksum == rxChecksum)
    {
        // printf("Packet is valid\n");
        return true;
    }
    else
    {
        // printf("Packet is not valid\n");
        // printf("Expected value: %x\n", rxChecksum);
        return false;
    }
}

static bool bmsProcessPacket(byte *packet)
{
    // TRACE;
    bool isValid = isPacketValid(packet);

    if (isValid != true)
    {
        LOGD(TAG, "Invalid packer received");
        return false;
    }

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    byte *data = packet + sizeof(bmsPacketHeaderStruct); // TODO Fix this ugly hack
    unsigned int dataLen = pHeader->dataLen;

    bool result = false;

    // |Decision based on pac ket type (info3 or info4)
    switch (pHeader->type)
    {
    case cBasicInfo3:
    {
        // Process basic info
        result = processBasicInfo(&packBasicInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    case cCellInfo4:
    {
        result = processCellInfo(&packCellInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }

    default:
        result = false;
        commSerial.printf("Unsupported packet type detected. Type: %d", pHeader->type);
    }

    return result;
}

static bool bleCollectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data, called by notifyCallback function
{
    // TRACE;
    static uint8_t packetstate = 0; // 0 - empty, 1 - first half of packet received, 2- second half of packet received
    static uint8_t packetbuff[40] = {0x0};
    static uint32_t previousDataSize = 0;
    bool retVal = false;
    // hexDump(data,dataSize);

    if (data[0] == 0xdd && packetstate == 0) // probably got 1st half of packet
    {
        packetstate = 1;
        previousDataSize = dataSize;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i] = data[i];
        }
        retVal = false;
    }

    if (data[dataSize - 1] == 0x77 && packetstate == 1) // probably got 2nd half of the packet
    {
        packetstate = 2;
        for (uint8_t i = 0; i < dataSize; i++)
        {
            packetbuff[i + previousDataSize] = data[i];
        }
        retVal = false;
    }

    if (packetstate == 2) // got full packet
    {
        uint8_t packet[dataSize + previousDataSize];
        memcpy(packet, packetbuff, dataSize + previousDataSize);

        bmsProcessPacket(packet); // pass pointer to retrieved packet to processing function
        packetstate = 0;
        retVal = true;
    }
    return retVal;
}

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) // this is called when BLE server sents data via notofication
{
    // TRACE;
    // hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{ // this is called by some underlying magic
  // Called for each advertising BLE server.
private:
    const String TAG = "MyAdvertisedDeviceCallbacks";

public:
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        LOGD(TAG, "BLE Advertised Device found:");
        LOGD(TAG, advertisedDevice.toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
        {
            LOGD(TAG, "Found our server");
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;

        } // Found our server
    }     // onResult
};        // MyAdvertisedDeviceCallbacks

class MyClientCallback : public BLEClientCallbacks
{ // this is called on connect / disconnect by some underlying magic+

    void onConnect(BLEClient *pclient)
    {
    }

    void onDisconnect(BLEClient *pclient)
    {
        BLE_client_connected = false;
        LOGD(TAG, "onDisconnect");
        // lcdDisconnect();
    }
};

class MyBLE
{
private:
    const String TAG = "MyBLE";
    void bmsGetInfo3()
    {
        // TRACE;
        //  header status command length data checksum footer
        //    DD     A5      03     00    FF     FD      77
        uint8_t data[7] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
        // bmsSerial.write(data, 7);
        sendCommand(data, sizeof(data));
        // commSerial.println("Request info3 sent");
    }

    void bmsGetInfo4()
    {
        // TRACE;
        //   DD  A5 04 00  FF  FC  77
        uint8_t data[7] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};
        // bmsSerial.write(data, 7);
        sendCommand(data, sizeof(data));
        // commSerial.println("Request info4 sent");
    }

    void printBasicInfo() // debug all data to uart
    {
        // TRACE;
        Serial.printf("Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
        Serial.printf("Amps: %f\n", (float)packBasicInfo.Amps / 1000);
        Serial.printf("CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
        Serial.printf("CapacityRemainPercent: %d\n", packBasicInfo.CapacityRemainPercent);
        Serial.printf("Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
        Serial.printf("Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
        Serial.printf("Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
        Serial.printf("Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
        Serial.printf("Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);
    }

public:
    MyBLE()
    {
    }

    void bleStartup()
    {
        BLEDevice::init("");

        // Retrieve a Scanner and set the callback we want to use to be informed when we
        // have detected a new device.  Specify that we want active scanning and start the
        // scan to run for 5 seconds.
        BLEScan *pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setInterval(1349);
        pBLEScan->setWindow(449);
        pBLEScan->setActiveScan(true);
        pBLEScan->start(5, true);
    }
    /*
    static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) // this is called when BLE server sents data via notofication
    {
        // TRACE;
        // hexDump((char*)pData, length);
        bleCollectPacket((char *)pData, length);
    }
    */

    bool connectToServer()
    {
        // TRACE;
        LOGD(TAG, "Forming a connection to ");
        // lcdConnectingStatus(0);
        LOGD(TAG, myDevice->getAddress().toString().c_str());
        BLEClient *pClient = BLEDevice::createClient();
        LOGD(TAG, " - Created client");
        // lcdConnectingStatus(1);
        pClient->setClientCallbacks(new MyClientCallback());

        // Connect to the remove BLE Server.
        pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
        LOGD(TAG, " - Connected to server");
        // lcdConnectingStatus(2);
        //  Obtain a reference to the service we are after in the remote BLE server.
        //  BLERemoteService*
        pRemoteService = pClient->getService(serviceUUID);
        if (pRemoteService == nullptr)
        {
            LOGD(TAG, "Failed to find our service UUID: ");
            // lcdConnectingStatus(3);
            LOGD(TAG, serviceUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
        LOGD(TAG, " - Found our service");
        // lcdConnectingStatus(4);

        // Obtain a reference to the characteristic in the service of the remote BLE server.
        pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_rx);
        if (pRemoteCharacteristic == nullptr)
        {
            LOGD(TAG, "Failed to find our characteristic UUID: ");
            // lcdConnectingStatus(5);
            LOGD(TAG, charUUID_rx.toString().c_str());
            pClient->disconnect();
            return false;
        }
        LOGD(TAG, " - Found our characteristic");
        // lcdConnectingStatus(6);
        //  Read the value of the characteristic.
        if (pRemoteCharacteristic->canRead())
        {
            std::string value = pRemoteCharacteristic->readValue();
            LOGD(TAG, "The characteristic value was: ");
            LOGD(TAG, value.c_str());
            commSerial.println(value.c_str());
        }

        if (pRemoteCharacteristic->canNotify())
            pRemoteCharacteristic->registerForNotify(notifyCallback);

        BLE_client_connected = true;
    }

    void bleRequestData()
    {
        // If the flag "doConnect" is true then we have scanned for and found the desired
        // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
        // connected we set the connected flag to be true.
        if (doConnect == true)
        {
            if (connectToServer())
            {
                LOGD(TAG, "We are now connected to the BLE Server.");
                // lcdConnected();
            }
            else
            {
                // lcdConnectionFailed();
            }
            doConnect = false;
        }

        // If we are connected to a peer BLE Server, update the characteristic each time we are reached
        // with the current time since boot.
        if (BLE_client_connected == true)
        {

            unsigned long currentMillis = millis();
            if ((currentMillis - previousMillis >= interval || newPacketReceived)) // every time period or when packet is received
            {
                previousMillis = currentMillis;
                // showInfoLcd();

                if (toggle) // alternate info3 and info4
                {
                    bmsGetInfo3();
                    // showBasicInfo();
                    newPacketReceived = false;
                }
                else
                {
                    bmsGetInfo4();
                    // showCellInfo();
                    newPacketReceived = false;
                }
                toggle = !toggle;
            }
        }
        else if (doScan)
        {
            BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
        }
#

        // bmsSimulate();
    }

    void sendCommand(uint8_t *data, uint32_t dataLen)
    {
        // TRACE;

        pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_tx);

        if (pRemoteCharacteristic)
        {
            pRemoteCharacteristic->writeValue(data, dataLen);
            // Serial.println("bms request sent");
        }
        else
        {
            LOGD(TAG, "Remote TX characteristic not found");
        }
    }

    bool readBmsData()
    {
    }

    bool readPackData();

    uint16_t getVoltage() {}
    int16_t getCurrent() {}
    int16_t getCycle() {}
    uint16_t getCellBalance() {}
    float getChargePercentage() {}
    uint16_t getMosfet() {}
    int16_t getTemp1() {}
    int16_t getTemp2() {}

    packCellInfoStruct getPackCellInfo() {}
};

#endif /* MY_BLE_CPP_ */