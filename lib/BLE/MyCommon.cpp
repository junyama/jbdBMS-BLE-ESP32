#ifndef MY_COMMON_CPP
#define MY_COMMON_CPP

#include "MyCommon.hpp"
#include "ESPDateTime.h"

const String TAG = "MyCommon";

void LOGD(String tag, String text)
{
    Serial.print("[" + DateTime.toString() + "] ");
    Serial.print(tag + ": ");
    Serial.println(text);
}

int16_t two_ints_into16(int highbyte, int lowbyte) // turns two bytes into a single long integer
{
    // TRACE;
    int16_t result = (highbyte);
    result <<= 8;                // Left shift 8 bits,
    result = (result | lowbyte); // OR operation, merge the two
    return result;
}

bool processBasicInfo(packBasicInfoStruct *output, byte *data, unsigned int dataLen)
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

    output->CapacityRemainWh = (output->CapacityRemainAh * c_cellNominalVoltage) / 1000000 * packCellInfo.NumOfCells;

    output->Temp1 = (((uint16_t)two_ints_into16(data[23], data[24])) - 2731);
    output->Temp2 = (((uint16_t)two_ints_into16(data[25], data[26])) - 2731);
    output->BalanceCodeLow = (two_ints_into16(data[12], data[13]));
    output->BalanceCodeHigh = (two_ints_into16(data[14], data[15]));
    output->MosfetStatus = ((byte)data[20]);

    return true;
}

bool processCellInfo(packCellInfoStruct *output, byte *data, unsigned int dataLen)
{
    // TRACE;
    uint16_t _cellSum;
    uint16_t _cellMin = 5000;
    uint16_t _cellMax = 0;
    uint16_t _cellAvg;
    uint16_t _cellDiff;

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

    for (uint8_t q = 0; q < output->NumOfCells; q++)
    {
        uint32_t disbal = abs(output->CellMedian - output->CellVolt[q]);
        // output->CellColorDisbalance[q] = getPixelColorHsv(mapHue(disbal, c_cellMaxDisbalance, 0), 255, 255);
    }
    return true;
}

bool isPacketValid(byte *packet) // check if packet is valid
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



#endif /* MY_COMMON_CPP */