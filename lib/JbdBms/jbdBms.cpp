/*
 * \brief Source file for JbdBms class
 * \author rahmaevao and boboxx
 * \version 0.3
 * \date September 2019
 */

#include <jbdBms.h>
#include "SoftwareSerial.h"

// SoftwareSerial SerialBT(5, 17); // RX, TX

JbdBms::JbdBms(HardwareSerial *t_hardwareSerial)
{
  m_port = t_hardwareSerial;
  m_hwserial = true;
  static_cast<HardwareSerial *>(m_port)->begin(9600);
}

JbdBms::JbdBms(SoftwareSerial *t_softwareSerial)
{
  m_port = t_softwareSerial;
  m_hwserial = false;
  static_cast<SoftwareSerial *>(m_port)->begin(9600);
}

JbdBms::JbdBms(int rx, int tx)
{
  SoftwareSerial mySerial(rx, tx);
  m_port = &mySerial;
  m_hwserial = false;
  static_cast<SoftwareSerial *>(m_port)->begin(9600);
}

/*!
 * \brief Reading basic data from bms
 * \retval status of reading
 */
bool JbdBms::readBmsData()
{
  uint8_t responce[BMS_LEN_RESPONCE] = {0x0};

  sendReqBasicMessage();
  readResponce(responce);

  if (checkCheckSumRecieve(responce) == true)
  {
    parseReqBasicMessage(responce);
  }
  else
  {
    return false;
  }
  return true;
}
/*!
 * \brief Reading pack data from bms
 * \retval status of reading
 */
bool JbdBms::readPackData()
{
  uint8_t responce[BMS_LEN_RESPONCE] = {0x0};

  sendCellMessage();
  readResponce(responce);

  if (checkCheckSumRecieve(responce) == true)
  {
    parseReqPackMessage(responce);
  }
  else
  {
    return false;
  }
  return true;
}

float JbdBms::getVoltage()
{
  return m_voltage;
}

uint16_t JbdBms::getVoltage2()
{
  return uint_voltage;
}

float JbdBms::getCurrent()
{
  return m_current;
}

int16_t JbdBms::getCurrent2()
{
  return int_current;
}

float JbdBms::getResidualcap()
{
  return m_residualcap;
}

float JbdBms::getChargePercentage()
{
  return m_chargePercentage;
}

uint16_t JbdBms::getChargePercentage2()
{
  return m_chargePercentage;
}

uint16_t JbdBms::getProtectionState()
{
  return m_protectionState;
}

uint16_t JbdBms::getCycle()
{
  return m_cycle;
}

uint16_t JbdBms::getMosfet()
{
  return m_mosfet;
}

/**
 * \External Temp Probe
 */
float JbdBms::getTemp1()
{
  return m_Temp1;
}

int16_t JbdBms::getTemp1_2()
{
  return int_Temp1;
}

/**
 * \Onboard Temp
 */
float JbdBms::getTemp2()
{
  return int_Temp2;
}

int16_t JbdBms::getTemp2_2()
{
  return m_Temp1;
}

packCellInfoStruct JbdBms::getPackCellInfo()
{
  return m_packCellInfo;
}

void JbdBms::sendReqBasicMessage()
{
  uint8_t reqMessage[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  m_port->write(reqMessage, 7);
}

void JbdBms::sendCellMessage()
{
  uint8_t reqMessage[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
  m_port->write(reqMessage, 7);
}

void JbdBms::parseReqBasicMessage(uint8_t *t_message)
{

  // 10mV
  uint_voltage = two_ints_into16(t_message[4], t_message[5]);
  m_voltage = (float)uint_voltage / 100.0;
  // 10mA
  int_current = two_ints_into16(t_message[6], t_message[7]);
  m_current = (float)int_current;
  // 10mAh
  m_residualcap = (float)two_ints_into16(t_message[8], t_message[9]) / 100.0;

  if (m_current > 32768)
  {
    m_current = -((int)m_current xor 0xffff) * 10.0;
  }
  else
  {
    m_current = m_current * 10.0;
  }

  m_chargePercentage = t_message[23];

  m_mosfet = t_message[24];
  m_protectionState = ((float)two_ints_into16(t_message[20], t_message[21]));
  m_cycle = ((float)two_ints_into16(t_message[12], t_message[13]));

  // 10C
  int_Temp1 = two_ints_into16(t_message[27], t_message[28]) - 2731;
  m_Temp1 = (float)int_Temp1 / 10.00f;

  // 10C
  int_Temp2 = two_ints_into16(t_message[27], t_message[28]) - 2731;
  m_Temp2 = (float)int_Temp2 / 10.00f;
}

void JbdBms::parseReqPackMessage(uint8_t *t_message)
{ // packCellInfoStruct * t_packCellInfo) {
  uint16_t _cellSum = 0;
  uint16_t _CellLow = 5000; // 5v
  uint16_t _CellHigh = 0;

  m_packCellInfo.NumOfCells = t_message[3] / 2; // Data length * 2 is number of cells !!!!!!

  byte offset = 4;
  for (byte i = 0; i < m_packCellInfo.NumOfCells; i++)
  {
    m_packCellInfo.CellVoltage[i] = ((uint16_t)two_ints_into16(t_message[i * 2 + offset], t_message[i * 2 + 1 + offset])); // Data length * 2 is number of cells !!!!!!

    _cellSum += m_packCellInfo.CellVoltage[i];

    if (m_packCellInfo.CellVoltage[i] > _CellHigh)
    {
      _CellHigh = m_packCellInfo.CellVoltage[i];
    }
    if (m_packCellInfo.CellVoltage[i] < _CellLow)
    {
      _CellLow = m_packCellInfo.CellVoltage[i];
    }

    m_packCellInfo.CellLow = _CellLow;
    m_packCellInfo.CellHigh = _CellHigh;
    m_packCellInfo.CellDiff = _CellHigh - _CellLow; // Resolution 10 mV -> convert to volts
    m_packCellInfo.CellAvg = _cellSum / m_packCellInfo.NumOfCells;
  }
}

bool JbdBms::readResponce(uint8_t *t_outMessage)
{
  uint8_t i = 0;
  bool findBeginByte = false;
  uint32_t statrTime = millis();
  while (i <= BMS_LEN_RESPONCE - 1)
  {
    if (abs((millis() - statrTime) > getMaxTimeout()))
    {
      return false;
    }
    if (m_port->available() > 0)
    {
      // if (Serial2.available() > 0) {
      uint8_t thisByte = m_port->read();
      // uint8_t thisByte = Serial2.read();
      if (thisByte == 0xDD)
      {
        findBeginByte = true;
      }
      if (findBeginByte)
      {
        t_outMessage[i] = thisByte;
        i++;
      }
    }
  }
  return true;
}

/**
 * \brief Compute and cheking check sum in message
 * \param [in] t_message
 * \return Fact of passing the test:
 *  true - check passed;
 *  false - check not passed
 */
bool JbdBms::checkCheckSumRecieve(uint8_t *t_message)
{

  uint16_t checkSumCompute;
  uint16_t checkSumAccepted;
  uint8_t lengthData;
  uint8_t startIndexCS;

  if (t_message[2] != 0) // Status OK
    return false;

  lengthData = t_message[3];
  checkSumCompute = computeCrc16JbdChina(t_message, BMS_LEN_RESPONCE);
  startIndexCS = lengthData + 4;

  checkSumAccepted = (t_message[startIndexCS] << 8) | t_message[startIndexCS + 1];

  if (checkSumCompute != checkSumAccepted)
    return false;
  return true;
}

/**
 * \brief Compute check sum in message for JBD protocol semiModbus :)
 * \param[in] puchMsg Message buffer containing binary data to be used for
 *            generating the CRC.
 * \param[in] usDataLen The quantity of bytes in the message buffer.
 * \return The function returns the CRC.
 */
uint16_t JbdBms::computeCrc16JbdChina(uint8_t *puchMsg, uint8_t usDataLen)
{
  uint8_t lengthData = puchMsg[3];
  uint16_t summa = 0;

  for (int i = 4; i < lengthData + 4; i++)
    summa = summa + puchMsg[i];

  uint16_t checkSum = (summa + lengthData - 1) ^ 0xFFFF;
  return checkSum;
}

uint32_t JbdBms::getMaxTimeout()
{
  return 250;
}

/**
 * \brief Build one uint16_t out of two uint8_t
 */
uint16_t JbdBms::two_ints_into16(int highbyte, int lowbyte)
{ // turns two bytes into a single long integer
  uint16_t a16bitvar = (highbyte);
  a16bitvar <<= 8;                   // Left shift 8 bits,
  a16bitvar = (a16bitvar | lowbyte); // OR operation, merge the two
  return a16bitvar;
}