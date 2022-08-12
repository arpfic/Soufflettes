#include "DSHOT150.h"
#include "mbed.h"
DSHOT150::DSHOT150(PinName pin) : _pin(pin)
{
    _pin = 0;
    tel = 0;
}
void DSHOT150::write_zero()
{
    int i = 0;
    while (i<19) {
        _pin.write(1);
        i++;
    }
    while (i<51) {
        _pin.write(0);
        i++;
    }
}
void DSHOT150::write_one()
{
    int i = 0;
    while (i<40) {
        _pin.write(1);
        i++;
    }
    while (i<51) {
        _pin.write(0);
        i++;
    }
}
void DSHOT150::check_sum(unsigned int v)
{
    v = v<<1;
    v = v|tel;
    uint8_t cs;
    for( int i = 0; i < 3; ++i){
        cs ^= v;
        v>>=4;
    }
    cs&=0xF;
    packet[15] = cs&0x1;
    packet[14] = cs&0x2;
    packet[13] = cs&0x4;
    packet[12] = cs&0x8;
}
void DSHOT150::get_tel(bool v)
{
    tel = v == true? 1 : 0;
}
void DSHOT150::send_packet()
{
    for(int j = 0; j < 1000; ++j) {
        for(int i = 0; i < 16; ++i) {
            if(packet[i])
                write_one();
            else
                write_zero();
        }
        wait_us(500);
    }
}
void DSHOT150::arm(){
    throttle(0.25);
    throttle(0);
}
void DSHOT150::throttle(float speed)
{
    unsigned int val;
    speed = speed > 1 ? 1 : speed; //Bound checking and restricitng of the input
    speed = speed < 0 ? 0 : speed; //Anything below 0 is converted to zero
                                   //Anything above 1 is converted to one 
                                   
    val = (unsigned int)(speed * 2000); //Scale the throttle value. 0 - 48 are reserved for the motor
    val +=48;                           //Throttle of zero starts at 48
    
    check_sum(val); //Calculate the checksum and insert it into the packet
    
    for(int i = 10; i >= 0; --i) {  //Insert the throttle bits into the packet
        packet[i] = val&0x1;
        val = val>>1;
    }
    
    packet[11] = tel;   //Set the telemetry bit in the packet
    
    send_packet();
}