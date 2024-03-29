#ifndef MY_BLE_CPP_myBms
#define MY_BLE_CPP_

#include "MyBLE.hpp"

using namespace MyLOG;

//#include "MyAdvertisedDeviceCallbacks.hpp"
//#include "MyClientCallback.hpp"

#define commSerial Serial

//---- global variables ----
// boolean doConnect = false;
// boolean BLE_client_connected = false;
// boolean doScan = false;

// packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
//  packEepromStruct packEeprom;       // here shall be the latest data got from BMS
// packCellInfoStruct packCellInfo; // here shall be the latest data got from BMS

// static unsigned long previousMillis = 0;
// static const long interval = 2000;

// static bool toggle = false;
// bool newPacketReceived = false;

//  ----- BLE stuff -----
// BLERemoteCharacteristic *pRemoteCharacteristic; //m
// BLEAdvertisedDevice *myDevice;
// BLERemoteService *pRemoteService; //m
//  The remote service we wish to connect to. Needs check/change when other BLE module used.
// BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
// BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module //m
// BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module //m

/*
MyBLE::MyBLE()
{
}
*/

const String MyBLE::TAG = "MyBLE";

const long MyBLE::interval = 2000;
unsigned long MyBLE::previousMillis = 0;
bool MyBLE::toggle = false;
byte MyBLE::ctrlCommand = 0;
byte MyBLE::commandParam = 0;

BLEClient *MyBLE::pClient;
BLERemoteCharacteristic *MyBLE::pRemoteCharacteristic;
BLERemoteService *MyBLE::pRemoteService;
MyAdvertisedDeviceCallbacks *MyBLE::myAdvertisedDeviceCallbacks;
MyClientCallback *MyBLE::myClientCallback;

BLEUUID MyBLE::serviceUUID = BLEUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
BLEUUID MyBLE::charUUID_tx = BLEUUID("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
BLEUUID MyBLE::charUUID_rx = BLEUUID("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

// const int32_t MyBLE::c_cellNominalVoltage = 3700;
// const uint16_t MyBLE::c_cellAbsMin = 3000;
// const uint16_t MyBLE::c_cellAbsMax = 4200;
// const int32_t MyBLE::c_packMaxWatt = 1250;
// const uint16_t MyBLE::c_cellMaxDisbalance = 1500;

bool MyBLE::newPacketReceived = false;

packBasicInfoStruct MyBLE::packBasicInfo;
packCellInfoStruct MyBLE::packCellInfo;
char *MyBLE::deviceName;

int16_t MyBLE::two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
    // TRACE;
    int16_t result = (highbyte);
    result <<= 8;                // Left shift 8 bits,
    result = (result | lowbyte); // OR operation, merge the two
    return result;
}

bool MyBLE::processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen)
{
    // TRACE;
    //  Expected data len
    // if (dataLen != 0x1B)
    if (dataLen != 0x1D) // changed by Jun
    {
        LOGD(TAG, "BasicInfo data length invalid: " + String(dataLen));
        return false;
    }

    output->Volts = ((uint32_t)two_ints_into16(data[0], data[1])) * 10; // Resolution 10 mV -> convert to milivolts   eg 4895 > 48950mV
    output->Amps = ((int32_t)two_ints_into16(data[2], data[3])) * 10;   // Resolution 10 mA -> convert to miliamps

    output->Watts = output->Volts * output->Amps / 1000000; // W

    output->CapacityRemainAh = ((uint16_t)two_ints_into16(data[4], data[5])) * 10;
    output->CapacityRemainPercent = ((uint8_t)data[19]);

    // output->CapacityRemainWh = (output->CapacityRemainAh * c_cellNominalVoltage) / 1000000 * packCellInfo.NumOfCells;

    output->Temp1 = (((uint16_t)two_ints_into16(data[23], data[24])) - 2731);
    output->Temp2 = (((uint16_t)two_ints_into16(data[25], data[26])) - 2731);
    output->BalanceCodeLow = (two_ints_into16(data[12], data[13]));
    output->BalanceCodeHigh = (two_ints_into16(data[14], data[15]));
    output->MosfetStatus = ((byte)data[20]);

    return true;
}

bool MyBLE::processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen)
{
    // TRACE;
    uint16_t _cellSum = 0;
    uint16_t _cellMin = 5000;
    uint16_t _cellMax = 0;
    // uint16_t _cellAvg;
    // uint16_t _cellDiff;

    output->NumOfCells = dataLen / 2; // Data length * 2 is number of cells !!!!!!

    // go trough individual cells
    // for (byte i = 0; i < dataLen / 2; i++)
    for (byte i = 0; i < output->NumOfCells; i++)
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
    /*
    for (uint8_t q = 0; q < output->NumOfCells; q++)
    {
        uint32_t disbal = abs(output->CellMedian - output->CellVolt[q]);
        // output->CellColorDisbalance[q] = getPixelColorHsv(mapHue(disbal, c_cellMaxDisbalance, 0), 255, 255);
    }
    */
    return true;
}

bool MyBLE::processDeviceInfo(char *output, byte *data, unsigned int dataLen)
{
    for (byte i = 0; i < data[0]; i++)
    {
        output[i] = data[i];
    }
    LOGD(TAG, "Device Name: " + String(output));
    return true;
}

byte MyBLE::calcChecksum(byte *packet)
{
    /*
    if (packet == nullptr)
    {
        return false;
    }
    */

    bmsPacketHeaderStruct *pHeader = (bmsPacketHeaderStruct *)packet;
    int checksumLen = pHeader->dataLen + 2; // status + data len + data

    /*
    if (pHeader->start != 0xDD)
    {
        return false;
    }
    */

    int offset = 2; // header 0xDD and command type are skipped

    byte checksum = 0;
    for (int i = 0; i < checksumLen; i++)
    {
        checksum += packet[offset + i];
    }

    // printf("checksum: %x\n", checksum);

    return checksum = ((checksum ^ 0xFF) + 1) & 0xFF;
    // printf("checksum v2: %x\n", checksum);
}

bool MyBLE::isPacketValid(byte *packet) // check if packet is valid
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

    /*
    byte checksum = 0;
    for (int i = 0; i < checksumLen; i++)
    {
        checksum += packet[offset + i];
    }
    */
    // printf("checksum: %x\n", checksum);

    //checksum = ((checksum ^ 0xFF) + 1) & 0xFF;
    // printf("checksum v2: %x\n", checksum);

    byte rxChecksum = packet[offset + checksumLen + 1];
    //byte checksum = calcChecksum(packet);


    //if (checksum == rxChecksum)
    if (calcChecksum(packet) == rxChecksum)

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

bool MyBLE::bmsProcessPacket(byte *packet)
{
    const byte cBasicInfo3 = 3;    // type of packet 3= basic info
    const byte cCellInfo4 = 4;     // type of packet 4= individual cell info
    const byte cMOSFETCtrl = 0xE1; // type of packet E1= MOSFET Control
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
        MyLOG::DISABLE_LOGD = true;
        LOGD(TAG, "bmsProcessPacket, process BasicInfo");
        MyLOG::DISABLE_LOGD = false;
        result = processBasicInfo(&packBasicInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }
    case cCellInfo4:
    {
        MyLOG::DISABLE_LOGD = true;
        LOGD(TAG, "bmsProcessPacket, process CellInfo");
        MyLOG::DISABLE_LOGD = false;
        result = processCellInfo(&packCellInfo, data, dataLen);
        newPacketReceived = true;
        break;
    }
    case cMOSFETCtrl:
    {
        LOGD(TAG, "bmsProcessPacket, process MOSFETCtrl");
        // result = processDeviceInfo(deviceName, data, dataLen);
        newPacketReceived = true;
        break;
    }
    default:
        result = false;
        char buff[256];
        sprintf(buff, "Unsupported packet type detected. Type: %d", pHeader->type);
        LOGD(TAG, buff);
    }
    return result;
}

bool MyBLE::bleCollectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data, called by notifyCallback function
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

void MyBLE::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) // this is called when BLE server sents data via notofication
{
    // TRACE;
    // hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

void MyBLE::bmsGetInfo3()
{
    // TRACE;
    //  header status command length data checksum footer
    //    DD     A5      03     00    FF     FD      77
    uint8_t data[7] = {0xdd, 0xa5, 0x3, 0x0, 0xff, 0xfd, 0x77};
    // bmsSerial.write(data, 7);
    sendCommand(data, sizeof(data));
    // commSerial.println("Request info3 sent");
}

void MyBLE::bmsGetInfo4()
{
    // TRACE;
    //   DD  A5 04 00  FF  FC  77
    uint8_t data[7] = {0xdd, 0xa5, 0x4, 0x0, 0xff, 0xfc, 0x77};
    // bmsSerial.write(data, 7);
    sendCommand(data, sizeof(data));
    // commSerial.println("Request info4 sent");
}

void MyBLE::bmsMosfetCtrl()
{
    LOGD(TAG, "bmsMosfetCtrl: " + String(commandParam));
    byte mosfetParam = commandParam ^ 3;
    uint8_t packet[9] = {0xdd, 0x5a, 0xe1, 0x02, 0x00, 0x00, 0xff, 0x00, 0x77};
    packet[5] = mosfetParam;
    packet[7] = calcChecksum(packet);
    sendCommand(packet, sizeof(packet));

    char buff[5];
    String printStr = "";
    for (byte i = 0; i < 9; i++) {
        sprintf(buff, "0x%x ", packet[i]);
        String str = buff;
        printStr += str;
    }
    LOGD(TAG, printStr +" sent");
    /*
    switch (commandParam)
    {
    case 0:
    {
        //   DD  5A E1 02  00  03 FF 1A  77
        uint8_t data[9] = {0xdd, 0x5a, 0xe1, 0x02, 0x00, 0x03, 0xff, 0x1a, 0x77};
        Serial.printf("MyBLE::bmsMosfetCtrl(), mosfetParam: %x checksum: %x\n", mosfetParam, calcChecksum(data));
        // bmsSerial.write(data, 7);
        // delay(500);
        sendCommand(data, sizeof(data));
        // commSerial.println("Request info4 sent");
        // packBasicInfo.MosfetStatus &= ~1;
        LOGD(TAG, "charge = OFF, discharge = OFF");
        break;
    }
    case 1:
    {
        //   DD  5A E1 02  00  02 FF 1B  77
        uint8_t data[9] = {0xdd, 0x5a, 0xe1, 0x02, 0x00, 0x02, 0xff, 0x1b, 0x77};
        Serial.printf("MyBLE::bmsMosfetCtrl(), mosfetParam: %x checksum: %x\n", mosfetParam, calcChecksum(data));
        // delay(500);
        sendCommand(data, sizeof(data));
        packBasicInfo.MosfetStatus &= ~1;
        // delay(500);
        LOGD(TAG, "charge = ON, discharge = OFF");
        break;
    }
    case 2:
    {
        // DD  5A E1 02  00  01 FF 1C  77
        uint8_t data[9] = {0xdd, 0x5a, 0xe1, 0x02, 0x00, 0x01, 0xff, 0x1c, 0x77};
        Serial.printf("MyBLE::bmsMosfetCtrl(), mosfetParam: %x checksum: %x\n", mosfetParam, calcChecksum(data));
        // bmsSerial.write(data, 7);
        sendCommand(data, sizeof(data));
        // commSerial.println("Request info4 sent");
        // packBasicInfo.MosfetStatus &= ~1;
        // delay(500);
        LOGD(TAG, "charge = OFF, discharge = ON");
        break;
    }
    case 3:
    {
        //   DD  5A E1 02  00  00 FF 1D  77
        uint8_t data[9] = {0xdd, 0x5a, 0xe1, 0x02, 0x00, 0x00, 0xff, 0x1d, 0x77};
        Serial.printf("MyBLE::bmsMosfetCtrl(), mosfetParam: %x checksum: %x\n", mosfetParam, calcChecksum(data));
        sendCommand(data, sizeof(data));
        packBasicInfo.MosfetStatus |= 1;
        // delay(500);
        LOGD(TAG, "charge = ON, discharge = ON");
        break;
    }
    }
    */
}

void MyBLE::printBasicInfo() // debug all data to uart
{
    if (!MyLOG::DISABLE_LOGD)
    {
        commSerial.println();
        LOGD(TAG, "BasicInfo Beging >>>>>>>>>>");
        Serial.printf("Total voltage: %f\n", (float)packBasicInfo.Volts / 1000);
        Serial.printf("Amps: %f\n", (float)packBasicInfo.Amps / 1000);
        Serial.printf("CapacityRemainAh: %f\n", (float)packBasicInfo.CapacityRemainAh / 1000);
        Serial.printf("CapacityRemainPercent: %d\n", packBasicInfo.CapacityRemainPercent);
        Serial.printf("Temp1: %f\n", (float)packBasicInfo.Temp1 / 10);
        Serial.printf("Temp2: %f\n", (float)packBasicInfo.Temp2 / 10);
        Serial.printf("Balance Code Low: 0x%x\n", packBasicInfo.BalanceCodeLow);
        Serial.printf("Balance Code High: 0x%x\n", packBasicInfo.BalanceCodeHigh);
        Serial.printf("Mosfet Status: 0x%x\n", packBasicInfo.MosfetStatus);
        LOGD(TAG, "BasicInfo END <<<<<<<<<<<<<");
    }
}

void MyBLE::printCellInfo() // debug all data to uart
{
    if (!MyLOG::DISABLE_LOGD)
    {
        commSerial.println();
        LOGD(TAG, "CellInfo Beging >>>>>>>>>>");
        commSerial.printf("Number of cells: %u\n", packCellInfo.NumOfCells);
        for (byte i = 1; i <= packCellInfo.NumOfCells; i++)
        {
            commSerial.printf("Cell no. %u", i);
            commSerial.printf("   %f\n", (float)packCellInfo.CellVolt[i - 1] / 1000);
        }
        commSerial.printf("Max cell volt: %f\n", (float)packCellInfo.CellMax / 1000);
        commSerial.printf("Min cell volt: %f\n", (float)packCellInfo.CellMin / 1000);
        commSerial.printf("Difference cell volt: %f\n", (float)packCellInfo.CellDiff / 1000);
        commSerial.printf("Average cell volt: %f\n", (float)packCellInfo.CellAvg / 1000);
        commSerial.printf("Median cell volt: %f\n", (float)packCellInfo.CellMedian / 1000);
        LOGD(TAG, "CellInfo END <<<<<<<<<<<<<");
    }
}

void MyBLE::bleStartup()
{
    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan *pBLEScan = BLEDevice::getScan();
    myAdvertisedDeviceCallbacks = new MyAdvertisedDeviceCallbacks(serviceUUID);
    pBLEScan->setAdvertisedDeviceCallbacks(myAdvertisedDeviceCallbacks);
    // pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, true);
}

bool MyBLE::connectToServer()
{
    // TRACE;
    LOGD(TAG, "Forming a connection to " + String(myAdvertisedDeviceCallbacks->myDevice->getAddress().toString().c_str()));
    // lcdConnectingStatus(0);
    // LOGD(TAG, myDevice->getAddress().toString().c_str());
    pClient = BLEDevice::createClient();
    BLEClient *pClient = BLEDevice::createClient();
    LOGD(TAG, "Created client");
    // lcdConnectingStatus(1);
    myClientCallback = new MyClientCallback();
    pClient->setClientCallbacks(myClientCallback);
    // pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myAdvertisedDeviceCallbacks->myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    LOGD(TAG, "Connected to server");
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
    LOGD(TAG, "Found our service");
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
    LOGD(TAG, "Found our characteristic");
    // lcdConnectingStatus(6);
    //  Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead())
    {
        std::string value = pRemoteCharacteristic->readValue();
        commSerial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

    return myClientCallback->BLE_client_connected = true;
}

void MyBLE::disconnectFromServer() // does not work as intended, but automatically reconnected
{
    pClient->disconnect();
    // BLE_client_connected = false;
    LOGD(TAG, "disconnected from the BLE Server.");
}

void MyBLE::bleRequestData()
{
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (myAdvertisedDeviceCallbacks->doConnect == true)
    {
        if (connectToServer())
        {
            LOGD(TAG, "connected to the BLE Server.");
            // lcdConnected();
        }
        else
        {
            LOGD(TAG, "failed to connect to the BLE Server.");
            // lcdConnectionFailed();
        }
        myAdvertisedDeviceCallbacks->doConnect = false;
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (myClientCallback->BLE_client_connected == true)
    {

        unsigned long currentMillis = millis();
        if ((currentMillis - previousMillis >= interval || newPacketReceived)) // every time period or when packet is received
        {
            previousMillis = currentMillis;
            // LOGD(TAG, "showing Info");
            //  showInfoLcd();
            switch (ctrlCommand) // ctrlCommand or alternate Info3 and Info4
            {
            case 1:
            {
                bmsMosfetCtrl();
                ctrlCommand = 0;
                newPacketReceived = false;
                break;
            }
            case 2:
            {
                disconnectFromServer();
                ctrlCommand = 0;
                newPacketReceived = false;
                break;
            }
            default:
            {
                if (toggle) // alternate info3 and info4
                {
                    bmsGetInfo3();
                    // LOGD(TAG, "showing BasicInfo");
                    //  showBasicInfo();
                    newPacketReceived = false;
                }
                else
                {
                    bmsGetInfo4();
                    // LOGD(TAG, "showing CellInfo");
                    //  showCellInfo();
                    newPacketReceived = false;
                }
                toggle = !toggle;
                break;
            }
            }
        }
    }
    else if (myAdvertisedDeviceCallbacks->doScan)
    {
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }

    // bmsSimulate();
}

void MyBLE::sendCommand(uint8_t *data, uint32_t dataLen)
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

#endif /* MY_BLE_CPP_ */