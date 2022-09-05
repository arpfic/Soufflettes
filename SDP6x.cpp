/*
  SDP6x - A Low Pressor Sensor Library for Arduino.

  Supported Sensor modules:
    SDP600 series from Sensirion - http://www.futureelectronics.com/en/Technologies/Product.aspx?ProductID=SDP610125PASENSIRIONAG1050689&IM=0
  
  Created by Christopher Ladden at Modern Device on December 2009.
  Modified by Paul Badger March 2010
  Modified by www.misenso.com on October 2011:
  - code optimisation
  - compatibility with Arduino 1.0
  SDP6x Modified above by Antony Burness Jun 2016.
  - adapted for the I2C Pressure sensor
  - using example from sensirion https://www.sensirion.com/products/differential-pressure-sensors/all-documents-of-sensirions-differential-pressure-sensors-for-download/

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/ 

#include "SDP6x.h"
#include <inttypes.h>
#include "mbed.h"

#define SCALEFACTOR 60.0
// NOTE you will need to change the SCALEFACTOR to the appropriate value for your sensor
//  Don't forget the .0 at the end, it makes sure Arduino does not round the number
// SENSOR       | SDP6xx-500Pa and SDP5xx  |  SDP6x0-125Pa  |  SDP6x0-25Pa  |
// SCALEFACTOR  |         60.0             |     240.0      |     1200.0    | (1/Pa)

/******************************************************************************
 * Global Functions
 ******************************************************************************/

SDP6x::SDP6x(I2C &i2c_, char i2c_address)
    : i2c(i2c_), address(i2c_address)
{
    i2c.frequency(400000);
}

SDP6x::~SDP6x()
{
}

bool SDP6x::init()
{
    i2c.lock();
    i2c.start();
    if (!i2c.write(0x80, NULL, 0)){ // 0 is OK
        return true;
    } else {
        return false;
    }
    i2c.stop();
    i2c.unlock();
}
/**********************************************************
 * GetPressureDiff
 *  Gets the current Pressure Differential from the sensor.
 *
 * @return float - The Pressure in Pascal
 **********************************************************/
float SDP6x::GetPressureDiff(void)
{
  int16_t res;
  if (readSensor(ePresHoldCmd, (uint16_t*)&res)) {
    return ((float)(res)/SCALEFACTOR);
  } else {
      //return ((float)(res)/SCALEFACTOR);
      return -1000;
  }
}

/******************************************************************************
 * Private Functions
 ******************************************************************************/

bool SDP6x::readSensor(char command, uint16_t* res)
{
    uint16_t result;
    char data[3];

    i2c.lock();
    i2c.start();
    i2c.write(0x80, &command, 1);
    i2c.stop();
    i2c.unlock();
    
    // Let's read it back
    // Waiting ???
    ThisThread::sleep_for(50ms);
    // Go !
    i2c.read(address << 1, data, 3);

    if (CheckCrc(data, 2, data[2]) == NO_ERROR) {
        result = (data[0] << 8);
        result += data[1];
        *res = result; 
        return true;
    }
    return false;
}

void SDP6x::SetSensorResolution(etSensorResolutions resolution){
    uint16_t    userRegister; // advanced user register
    
    if (readSensor(eReadUserReg, &userRegister)) {
        userRegister &= 0xF1FF;
        userRegister |= (resolution & 0x07) << 9;
        writeSensor(userRegister);
    }
}

void SDP6x::writeSensor(uint16_t data){
    char bytes[2];

    bytes[0] = (data & 0xFF00) >> 8;
    bytes[1] = data & 0x00FF;
    
    i2c.lock();
    i2c.start();
    i2c.write(0x80);
    i2c.write(eWriteUserReg); // send the pointer location
    for(uint16_t i = 0; i < 2; i++)
        i2c.write(bytes[i]); //send the data
    i2c.stop();
    i2c.unlock();
}

//============================================================
// From sensirion App Note "CRC Checksum"
//calculates checksum for n bytes of data
//and compares it with expected checksum
//input: data[] checksum is built based on this data
// nbrOfBytes checksum is built for n bytes of data
// checksum expected checksum
//return: error: CHECKSUM_ERROR = checksum does not match
// 0 = checksum matches
//============================================================ 
PRES_SENSOR_ERROR SDP6x::CheckCrc(char data[], char nbrOfBytes, char checksum) {
    char crc = 0;
    char byteCtr;
    //calculates 8-Bit checksum with given polynomial
    for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr) { 
        crc ^= (data[byteCtr]);
        for (char bit = 8; bit > 0; --bit) { 
            if (crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
            else crc = (crc << 1);
        }
    }
    if (crc != checksum) return CHECKSUM_ERROR;
    else return NO_ERROR;
}


