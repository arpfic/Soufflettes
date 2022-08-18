#include "DigitalIn.h"
#include "DigitalOut.h"
#include "PinNames.h"
#include "PwmOut.h"
#include "Ticker.h"
#include "mbed.h"
#include <math.h>
#include "esc.h"
//#include "DSHOT150.h"
#include <cstdint>
#include "Adafruit_SSD1306.h" 

#define PC_DEBUG_ON         0
#define SSD_I2C_ADDRESS     0x78
#define SSD1306_ON          1
#define MAX_BUFFER          129
// Need to move into a config pot
#define THROTTLE_MIN        0.1f
#define ESC_THROTTLE_MAX    0.6f
#define THROTTLE_START      0.5f
#define POT_REFRESH_TIME    20ms
#define NPA_REFRESH_TIME    200ms

class I2CPreInit : public I2C
{
public:
    I2CPreInit(PinName sda, PinName scl) : I2C(sda, scl)
    {
        frequency(1000000);
        start();
    };
};
I2CPreInit gI2C(PB_7, PB_6); // SDA , SCL
Adafruit_SSD1306_I2c display(gI2C, PA_0);// Mystere

AnalogIn  pot(PA_4);
AnalogIn  npa(PA_5);
AnalogIn  adc_temp(ADC_TEMP);
AnalogIn  adc_vref(ADC_VREF);
//DSHOT150       soufflette(PE_9);
DigitalOut led(PB_3);
ESC       soufflette(PA_3);

char buffer[MAX_BUFFER];

Ticker PotsampleReady;
Ticker NpasampleReady;
bool pot_sample_timer_ready;
bool npa_sample_timer_ready;

void update_info_screen(char* info);
void update_esc_screen(float esc_speed, int npa_value);
void update_esc(float speed);
void update_pc_debug(float esc_speed, int npa_value);

void pot_sample_timer() {
    pot_sample_timer_ready = true;
}

void npa_sample_timer() {
    npa_sample_timer_ready = true;
}

int main() {
    display.clearDisplay();
    display.setTextCursor(0,0);
    display.printf ("-- SOUFFLETTE --");
    display.display();

    update_info_screen("wait for init...");

    float pot_value = 0.0f;
    unsigned short npa_value = 0;

    update_esc(pot_value);
    ThisThread::sleep_for(4000ms);

    update_info_screen("set to min speed");
    update_esc(THROTTLE_START);
    ThisThread::sleep_for(2000ms);
    update_info_screen("initialisaton ok");

    PotsampleReady.attach(&pot_sample_timer, POT_REFRESH_TIME);
    NpasampleReady.attach(&npa_sample_timer, NPA_REFRESH_TIME);

    // ENTERING LOOP, BUT CLEAN BEFORE :)
    display.clearDisplay();

    while (1) {
        if (pot_sample_timer_ready){
            pot_value = pot.read()*(1.0f - THROTTLE_MIN) + THROTTLE_MIN;
            if (pot_value > 1.0f) pot_value = 1.0f;
            update_esc(pot_value);
            update_esc_screen(pot_value, npa_value);
            pot_sample_timer_ready = false;
        }
        if (npa_sample_timer_ready){
            led = !led;
            npa_value = npa.read_u16();
            npa_sample_timer_ready = false;
            if(PC_DEBUG_ON == 1)
            update_pc_debug(pot_value, npa_value);
        }
    }
    return 0;
}

void update_info_screen(char* info){     
    display.setTextCursor(0,55);
    display.printf(info);
    // DISPLAY REFRESH
    display.display();
}

void update_esc(float speed){       
    float speed_actual = speed * ESC_THROTTLE_MAX;
    // SET SPEED
    soufflette.setThrottle(speed_actual);
    // ESC REFRESH
    soufflette.pulse();
}

void update_esc_screen(float esc_speed, int npa_value){     
    // ESC_SPEED REFRESH
    display.clearDisplay();
    display.setTextCursor(20,0);
    display.printf ("-- SOUFFLETTE --");
    display.setTextCursor(0,25);
    sprintf(buffer, "TEMP            ");
    sprintf(buffer + strlen(buffer), "%05f", (adc_temp.read()*100));
    display.printf(buffer);
    display.setTextCursor(0,35);
    sprintf(buffer, "PRESSURE        ");
    sprintf(buffer + strlen(buffer), "%05d", npa_value);
    display.printf(buffer);
    display.setTextCursor(0,45);
    sprintf(buffer, "SPEED           ");
    sprintf(buffer + strlen(buffer), "%03d", (int)(esc_speed*100.0));
    display.printf(buffer);
    // DRAW LINE
    float esc_x_line = 126.0*esc_speed;
    display.drawFastHLine(0, 60, (int)(esc_x_line), 1);
    display.drawFastHLine(0, 61, (int)(esc_x_line), 1);
    display.drawFastHLine(0, 62, (int)(esc_x_line), 1);
    display.drawFastHLine(0, 63, (int)(esc_x_line), 1);
    //uint8_t y = 58;
    
    //display.drawLine(0, y,   (int)(esc_x_line), y,   SSD1306::Normal, false);
    //display.drawLine(0, y+1, (int)(esc_x_line), y+1, SSD1306::Normal, false);
    //display.drawLine(0, y+2, (int)(esc_x_line), y+2, SSD1306::Normal, false);
    //display.drawLine(0, y+3, (int)(esc_x_line), y+3, SSD1306::Normal, false);
    //display.drawLine(0, y+4, (int)(esc_x_line), y+4, SSD1306::Normal, false);    
    // DISPLAY REFRESH
    display.display();
}

void update_pc_debug(float esc_speed, int npa_value){
    sprintf(buffer, "TEMP = ");
    sprintf(buffer + strlen(buffer), "%05f ", (adc_temp.read()*100));
    sprintf(buffer + strlen(buffer), "| PRESSURE = ");
    sprintf(buffer + strlen(buffer), "%05d ", npa_value);
    sprintf(buffer + strlen(buffer), "| SPEED = ");
    sprintf(buffer + strlen(buffer), "%03d\n", (int)(esc_speed*100.0));
    printf(buffer);
}