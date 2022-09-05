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
#include "SDP6x.h"

#define AUTO_MODE                         0
#define PC_DEBUG_ON                       1
#define SSD_I2C_ADDRESS                   0x78
#define SSD1306_ON                        1
#define MAX_BUFFER                        129
// Need to move into a config pot
#define THROTTLE_MIN                      0.15f
#define ESC_THROTTLE_MAX                  0.6f
#define THROTTLE_START                    0.5f
#define SCREEN_REFRESH_TIME               100ms
#define POT_REFRESH_TIME                  10ms
#define POT_MODE_REFRESH_STEPS            0.04f
#define NPA_REFRESH_TIME                  20ms
#define SDP_REFRESH_TIME                  50ms
#define AUTO_MODE_REFRESH_TIME            5ms
#define AUTO_MODE_REFRESH_STEPS           0.01f

class I2CPreInit : public I2C
{
public:
    I2CPreInit(PinName sda, PinName scl) : I2C(sda, scl)
    {
        frequency(1000000);
        start();
    };
};

I2CPreInit                                gI2C(PB_4, PA_7); // SDA , SCL
Adafruit_SSD1306_I2c                      display(gI2C, PB_1);// Mystere
ESC                                       soufflette(PA_3);
I2C                                       sdpI2C(PB_7, PB_6);  
SDP6x                                     sdp(sdpI2C);

AnalogIn                                  pot(PA_4);
AnalogIn                                  npa(PA_0);
AnalogIn                                  adc_temp(ADC_TEMP);
AnalogIn                                  adc_vref(ADC_VREF);
//DSHOT150                                  soufflette(PE_9);
DigitalOut                                led(PB_3);

char buffer[MAX_BUFFER];

Ticker ScreenRefreshReady;
Ticker PotsampleReady;
Ticker NpasampleReady;
Ticker SdpsampleReady;
Ticker AutoModeReady;
bool screen_refresh_timer_ready;
bool pot_sample_timer_ready;
bool npa_sample_timer_ready;
bool sdp_sample_timer_ready;
bool auto_mode_ready;
float auto_mode;

void update_info_screen(char* info);
void update_esc_screen(float esc_speed, int npa_value, float sdp_value);
void update_esc(float speed);
void update_pc_debug(float esc_speed, int npa_value, float sdp_value);

void screen_refresh_timer() {
    screen_refresh_timer_ready = true;
}

void pot_sample_timer() {
    pot_sample_timer_ready = true;
}

void npa_sample_timer() {
    npa_sample_timer_ready = true;
}

void sdp_sample_timer() {
    sdp_sample_timer_ready = true;
}

void auto_mode_timer() {
    auto_mode_ready = true;
}

int main() {
    auto_mode = THROTTLE_START;
    memset(buffer, 0, sizeof(buffer));

    display.clearDisplay();
    display.setTextCursor(0,0);
    display.printf ("-- SOUFFLETTE --");
    display.display();

    led = 1;

    update_info_screen("wait for init...");

    float pot_value = 0.0f;
    float sdp_value = 0.0f;
    unsigned short npa_value = 0;

    update_esc(0.0f);

    ThisThread::sleep_for(2000ms);
    if (sdp.init()){
        update_info_screen("sdp7xx on...");
    }
    ThisThread::sleep_for(2000ms);

    update_info_screen("set to min speed");
    update_esc(THROTTLE_START);
    ThisThread::sleep_for(2000ms);
    update_info_screen("initialisaton ok");

    ScreenRefreshReady.attach(&screen_refresh_timer, SCREEN_REFRESH_TIME);
    PotsampleReady.attach(&pot_sample_timer, POT_REFRESH_TIME);
    NpasampleReady.attach(&npa_sample_timer, NPA_REFRESH_TIME);
    SdpsampleReady.attach(&sdp_sample_timer, SDP_REFRESH_TIME);
    AutoModeReady.attach(&auto_mode_timer, AUTO_MODE_REFRESH_TIME);

    // ENTERING LOOP, BUT CLEAN BEFORE :)
    display.clearDisplay();

    while (1) {
        if (screen_refresh_timer_ready){
            led = !led;
            update_esc_screen(pot_value, npa_value, sdp_value);
            screen_refresh_timer_ready = false;
        }
        if (pot_sample_timer_ready){
            pot_value = pot.read()*(1.0f - THROTTLE_MIN) + THROTTLE_MIN;
            if (pot_value > 1.0f) pot_value = 1.0f;
            if (!AUTO_MODE) update_esc(pot_value);
            pot_sample_timer_ready = false;
        }
        if (npa_sample_timer_ready){
            npa_value = npa.read_u16();
            npa_sample_timer_ready = false;
            if(PC_DEBUG_ON == 1)
            update_pc_debug(pot_value, npa_value, sdp_value);
        }
        if (sdp_sample_timer_ready){
            sdp_value = sdp.GetPressureDiff();
            sdp_sample_timer_ready = false;
            if(PC_DEBUG_ON == 1)
            update_pc_debug(pot_value, npa_value, sdp_value);
        }
        if (AUTO_MODE && auto_mode_ready){
            if (npa_value > pot.read_u16()){
                auto_mode = auto_mode - AUTO_MODE_REFRESH_STEPS ;
                if (auto_mode <= THROTTLE_MIN) auto_mode = THROTTLE_MIN;
                update_esc(auto_mode);
            } else {
                auto_mode = auto_mode + AUTO_MODE_REFRESH_STEPS;
                if (auto_mode > 1.0f) auto_mode = 1.0f;
                update_esc(auto_mode);
            }
            auto_mode_ready = false;
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

void update_esc_screen(float esc_speed, int npa_value, float sdp_value){     
    // ESC_SPEED REFRESH
    display.clearDisplay();
    display.setTextCursor(20,0);
    display.printf ("-- SOUFFLETTE --");
    display.setTextCursor(0,15);
    sprintf(buffer, "TEMP            ");
    sprintf(buffer + strlen(buffer), "%05f", (adc_temp.read()*100));
    display.printf(buffer);
    display.setTextCursor(0,25);
    sprintf(buffer, "DIFF PRESS      ");
    sprintf(buffer + strlen(buffer), "%05f", sdp_value);
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

void update_pc_debug(float esc_speed, int npa_value, float sdp_value){
    sprintf(buffer, "TEMP = ");
    sprintf(buffer + strlen(buffer), "%05f ", (adc_temp.read()*100));
    sprintf(buffer + strlen(buffer), "| DIFF PRESS = ");
    sprintf(buffer + strlen(buffer), "%f ", sdp_value);
    sprintf(buffer + strlen(buffer), "| PRESSURE = ");
    sprintf(buffer + strlen(buffer), "%05d ", npa_value);
    sprintf(buffer + strlen(buffer), "| SPEED = ");
    sprintf(buffer + strlen(buffer), "%03d\n", (int)(esc_speed*100.0));
    printf(buffer);
}