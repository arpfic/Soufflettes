#include "DigitalIn.h"
#include "PinNames.h"
#include "PwmOut.h"
#include "Ticker.h"
#include "mbed.h"
#include <math.h>
#include "esc.h"
#include "DSHOT150.h"
#include <cstdint>
#include "SSD1306.h"

#define MAX_BUFFER      129

//AnalogIn  pot(PA_3);
//PwmOut    pot_led(PB_0);
//ESC       apnee(PE_9);

AnalogIn  pot(PA_4);
//PwmOut    pot_led(PB_3);
ESC       apnee(PA_3);
SSD1306   display(PB_7, PB_6);

char buffer[MAX_BUFFER];
//DSHOT150       apnee(PE_9);

void update_screen(unsigned short value);

int main() {
    display.setSpeed(SSD1306::Fast);
    display.init();

    display.clearScreen();
    display.setBrightness(255);

    display.setCursor(1,1);
    display.printf ("- SOUFLETTE -");
    update_screen(0);


    apnee.setThrottle(pot.read());
    apnee.pulse();

    unsigned short value = 0;

    while (1) {
        apnee.setThrottle(pot.read());
        apnee.pulse();
        value = pot.read_u16();
        update_screen(value);
        ThisThread::sleep_for(20ms);
    }
    return 0;
}

void update_screen(unsigned short value){        
        display.setCursor(4,0);
        sprintf(buffer, "pot -> ");
        sprintf(buffer + strlen(buffer), "%05d", value);
        display.printf(buffer);

        display.refreshDisplay();
}