#ifndef MY_BLE_CPP_myBms
#define MY_BLE_CPP_

#include "MyBLE.hpp"

#define commSerial Serial

//---- global variables ----
boolean doConnect = false;
boolean BLE_client_connected = false;
boolean doScan = false;

packBasicInfoStruct packBasicInfo; // here shall be the latest data got from BMS
packEepromStruct packEeprom;       // here shall be the latest data got from BMS
packCellInfoStruct packCellInfo;   // here shall be the latest data got from BMS

const byte cBasicInfo3 = 3; // type of packet 3= basic info
const byte cCellInfo4 = 4;  // type of packet 4= individual cell info

// static unsigned long previousMillis = 0;
// static const long interval = 2000;

// static bool toggle = false;
bool newPacketReceived = false;

//  ----- BLE stuff -----
BLERemoteCharacteristic *pRemoteCharacteristic;
BLEAdvertisedDevice *myDevice;
BLERemoteService *pRemoteService;
// The remote service we wish to connect to. Needs check/change when other BLE module used.
BLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
BLEUUID charUUID_tx("0000ff02-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module
BLEUUID charUUID_rx("0000ff01-0000-1000-8000-00805f9b34fb"); // xiaoxiang bms original module

//const String TAG = "GLOBAL";







bool bmsProcessPacket(byte *packet)
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

bool bleCollectPacket(char *data, uint32_t dataSize) // reconstruct packet from BLE incomming data, called by notifyCallback function
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

void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) // this is called when BLE server sents data via notofication
{
    // TRACE;
    // hexDump((char*)pData, length);
    bleCollectPacket((char *)pData, length);
}

MyAdvertisedDeviceCallbacks::MyAdvertisedDeviceCallbacks()
    : TAG("MyAdvertisedDeviceCallbacks")
{
}

void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice)
{
    LOGD(TAG, "BLE Advertised Device found: " + String(advertisedDevice.toString().c_str()));
    // LOGD(TAG, advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
        LOGD(TAG, "Found our server");
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;

    } // Found our server
}

MyClientCallback::MyClientCallback()
    : TAG("MyClientCallback")
{
}

void MyClientCallback::onConnect(BLEClient *pclient)
{
}

void MyClientCallback::onDisconnect(BLEClient *pclient)
{
    BLE_client_connected = false;
    LOGD(TAG, "onDisconnect");
    // lcdDisconnect();
}

MyBLE::MyBLE()
    : TAG("MyBLE"), previousMillis(0), interval(2000), toggle(false)
{
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

void MyBLE::printBasicInfo() // debug all data to uart
{
    // TRACE;
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
    commSerial.println();
}

void MyBLE::printCellInfo() // debug all data to uart
{
    // TRACE;
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
    commSerial.println();
}

void MyBLE::bleStartup()
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

bool MyBLE::connectToServer()
{
    // TRACE;
    LOGD(TAG, "Forming a connection to " + String(myDevice->getAddress().toString().c_str()));
    // lcdConnectingStatus(0);
    // LOGD(TAG, myDevice->getAddress().toString().c_str());
    pClient = BLEDevice::createClient();
    BLEClient *pClient = BLEDevice::createClient();
    LOGD(TAG, "Created client");
    // lcdConnectingStatus(1);
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
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
        LOGD(TAG, "The characteristic value was: " + String(value.c_str()));
        commSerial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

    return BLE_client_connected = true;
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
    if (doConnect == true)
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
            // LOGD(TAG, "showing Info");
            //  showInfoLcd();

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
        }
    }
    else if (doScan)
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