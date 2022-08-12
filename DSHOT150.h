/*
    Copyright (c) 2019 Blake West, Kevin Tseng, Tyler Brown, Mason Totri
 
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
 
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
 
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/
#ifndef MBED_DSHOT15_H
#define MBED_DSHOT15_H
#include "mbed.h"
/** DSHOT150 converts a digital IO pin to act like a DSHOT pulse
 * This is done through writing one's and zero's a specific amount 
 * to acheive the correct pulse width in the given duty cycle.
 *
 *
 * Example
 * @code
 * #include "mbed.h"
 * #include "DSHOT150.h"
 *
 * DSHOT150 motor( p21 );
 *
 * int main() {
 *
 *      motor.arm();
 *      motor.get_tel( true );
 *      for( float i = 0; i < 1; i+=0.1){
 *          motor.throttle( i );
 *      }
 *
 *
 *  }
 *  @endcode
 *
 *  This example will step the motor from 0 to 100% in 10% intervals
 *
 */
class DSHOT150
{
public:
    DSHOT150(PinName pin); //Constructor that takes in 
    void throttle(float speed); //Throttle value as a percentage [0,1]
    void get_tel(bool v); //Telemetry request function. Set to true if wanting to receive telemetry from the ESC
    void arm(); //Arming comand for the motor
    
private:
    void send_packet(); //Sends the packet
    void check_sum(unsigned int v); //Calculates the check sum, and inserts it into the packet
    void write_zero(); 
    void write_one();
    bool packet[16]; //Packet of data that is being sent
    DigitalOut _pin; //Pin that is being used.
    unsigned int tel;
};
#endif