#include "DigitalIn.h"
#include "DigitalOut.h"
#include "PinNames.h"
#include "PwmOut.h"
#include "Ticker.h"
#include "mbed.h"
#include <math.h>
#include "esc.h"
#include "MIDIMessage.h"
//#include "DSHOT150.h"
#include <cstdint>
#include "Adafruit_SSD1306.h" 
#include "SDP6x.h"

#define AUTO_MODE                         0
#define AUTO_RESET                        0
#define PC_DEBUG_ON                       0
#define SSD_I2C_ADDRESS                   0x78
#define SSD1306_ON                        1
#define MAX_BUFFER                        129
// Need to move into a config pot
#define THROTTLE_MIN                      0.15f
#define ESC_THROTTLE_MAX                  0.6f
#define THROTTLE_START                    0.5f
#define SCREEN_REFRESH_TIME               0.1f //sec
#define POT_REFRESH_TIME                  0.01f
#define POT_MODE_REFRESH_STEPS            0.04f
#define NPA_REFRESH_TIME                  0.02f
#define SDP_REFRESH_TIME                  0.05f
#define AUTO_MODE_REFRESH_TIME            0.005f
#define AUTO_MODE_REFRESH_STEPS           0.005f
#define ESC_SPEED_STEP                    0.15f
#define WARNING_PRESSURE                  11000
#define WARNING_DIFF_PRESSURE             3.00f

/* -----------------------------------------------------------------------------
 * MIDI stuf
 */
#define MIDI_UART_TX                            PA_9
#define MIDI_UART_RX                            PA_10
#define MIDIMAIL_SIZE                           64
#define MIDI_MAX_MSG_LENGTH                     4 // Max message size. SysEx can be up to 65536.
#define CONT_CTRL                               176

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
ESC                                       soufflette(PA_8);
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
int  warning_count_before_reset;
float actual_speed;
float auto_mode;
// MIDI Serial & Thread and Ticker for working with different packet lengths
static RawSerial midi_din(MIDI_UART_TX, MIDI_UART_RX);
Thread midiTask;

/* Mailbox of MIDI Packets */
typedef struct {
    uint8_t    midiPacket[MIDI_MAX_MSG_LENGTH]; /* AD result of measured voltage */
    int        midiPacketlenght = 0;
} midiPacket_t;
Mail<midiPacket_t, MIDIMAIL_SIZE>  rx_midiPacket_box;
midiPacket_t                       *rx_midi_outbox;
int                                packetbox_full = 0;
int                                rx_midi_Statusbyte = 0;

// MIDI variables
volatile int            rx_idx = 0;
uint8_t                 rx_buffer[MIDI_MAX_MSG_LENGTH + 1];

void update_info_screen(char* info);
void update_esc_screen(float esc_speed, int npa_value, float sdp_value);
void init_esc(float speed);
void update_esc(float speed);
void update_pc_debug(float esc_speed, int npa_value, float sdp_value);
bool test_esc(int npa_value, float sdp_value);
// Callback for MIDI RX
void on_rx_interrupt();
// Regrouping MIDI RX task in a Thread
void midi_task();
void midi_send_contctrl(char cc_number, char value);
/* -- debug v2*/
int debug_midicount = 0;

/* ========================================================================= */
void on_rx_interrupt() {
    uint8_t c;

    while ((midi_din.readable()) && rx_midi_outbox->midiPacketlenght < MIDI_MAX_MSG_LENGTH) {
        // let's poop
        c = midi_din.getc();

        /* -- debug v1
        debug_serial = c;
        midiTask.flags_set(0x1);*/

        // go elsewhere
        if(c == 254) return ;

        if (packetbox_full != 1) {
            // CASE1 : STATUT BYTE
            if (((c >> 4) & 0xF) == 0x8 || ((c >> 4) & 0xF) == 0x9 || ((c >> 4) & 0xF) == 0xB) {
                // Maybe we'll use it later with Running Status
                rx_midi_Statusbyte = c;

                // Put the Statut byte
                rx_midi_outbox->midiPacket[0] = c;
                rx_midi_outbox->midiPacketlenght = 1;
            // CASE2 : NoteON or NoteOFF, 3 bytes packets in every cases
            } else if (rx_midi_outbox->midiPacketlenght == 2){
                rx_midi_outbox->midiPacket[2] = c;
                rx_midi_outbox->midiPacketlenght = 3;

                // send the packet to mailbox
                rx_midiPacket_box.put(rx_midi_outbox);
                debug_midicount ++;
                // create the next packet
                rx_midi_outbox = rx_midiPacket_box.alloc();
                if (rx_midi_outbox != NULL) {
                    rx_midi_outbox->midiPacketlenght = 0;
                } else {
                    packetbox_full = 1;
                }
                // The Thread needs to wake up !
                midiTask.flags_set(0x1);
            // CASE3 : AllNoteOff
            } else if (rx_midi_outbox->midiPacketlenght == 1 && ((c & 0x7F) == 123)){
                rx_midi_outbox->midiPacket[1] = c;
                rx_midi_outbox->midiPacketlenght = 2;

                // send the packet to mailbox
                debug_midicount ++;
                rx_midiPacket_box.put(rx_midi_outbox);
                // create the next packet
                rx_midi_outbox = rx_midiPacket_box.alloc();
                if (rx_midi_outbox != NULL) {
                    rx_midi_outbox->midiPacketlenght = 0;
                } else {
                    packetbox_full = 1;
                }
                // The Thread needs to wake up !
                midiTask.flags_set(0x1);
            // CASE4 : Running state in case on non Statut byte (we check)
            } else if (rx_midi_outbox->midiPacketlenght == 0 && ((c >> 7) & 0x1) != 1){
                // First, write the previous Status byte
                rx_midi_outbox->midiPacket[0] = rx_midi_Statusbyte;
                // Then write the actual byte
                rx_midi_outbox->midiPacket[1] = c;
                rx_midi_outbox->midiPacketlenght = 2;
            } else if (rx_midi_outbox->midiPacketlenght > 0){
                // First, write the previous Status byte
                rx_midi_outbox->midiPacket[rx_midi_outbox->midiPacketlenght] = c;
                rx_midi_outbox->midiPacketlenght++;
            }
        }
    }
}

void midi_task() {
    midi_din.baud(31250);

    // create the first packet
    rx_midi_outbox = rx_midiPacket_box.alloc();
    if (rx_midi_outbox != NULL) {
        rx_midi_outbox->midiPacketlenght = 0;
    } else {
        packetbox_full = 1;
    }

    // Register a callback to process a Rx (receive) interrupt.
    midi_din.attach(&on_rx_interrupt, SerialBase::RxIrq);

    while (1) {
        ThisThread::flags_wait_any(0x1);

        /* -- debug v1
        char buffer[MAX_PQT_SENDLENGTH];
        tohex(&debug_serial, 1, &buffer[0], 3);
        if (debug_on) debug_OSC(buffer);*/

        osEvent midi_evt = rx_midiPacket_box.get(0);
        if (packetbox_full == 1){
            rx_midi_outbox = rx_midiPacket_box.alloc();
            if (rx_midi_outbox != NULL) {
                rx_midi_outbox->midiPacketlenght = 0;
                packetbox_full = 0;
            } else {
                packetbox_full = 1;
            }
        }

        if (midi_evt.status == osEventMail) {
            midiPacket_t *midi_inbox = (midiPacket_t *)midi_evt.value.p;

            MIDIMessage MIDIMsg;
            MIDIMsg.from_raw(midi_inbox->midiPacket, midi_inbox->midiPacketlenght);
/*
            if (MIDIMsg.channel() == MIDI_CHANNEL_A - 1) {
                switch (MIDIMsg.type()) {
                    case MIDIMessage::NoteOffType:
                        menu_main_midi_noteOfmidi_dinf_chA(MIDIMsg.key());
                        break;
                    case MIDIMessage::NoteOnType:
                        if (MIDIMsg.length > 3) {
                            if (MIDIMsg.velocity() > 0) {
                                menu_main_midi_noteOn_chA(MIDIMsg.key(), MIDIMsg.velocity());
                            } else {
                                menu_main_midi_noteOff_chA(MIDIMsg.key());
                            }
                        } else {
                            menu_main_midi_noteOn_chA_min(MIDIMsg.key());
                        }
                        break;
                    case MIDIMessage::AllNotesOffType:
                        menu_main_midi_allnoteOff();
                        break;
                    case MIDIMessage::ResetAllControllersType:
                        menu_tools_softreset();
                        break;
                    case MIDIMessage::ControlChangeType:
                        break;
                    case MIDIMessage::PitchWheelType:
                        break;
                    case MIDIMessage::SysExType:
                        break;
                    case MIDIMessage::ErrorType:
                        led_red = !led_red;
                        break;
                    default:
                        break;
                }
            }*/
            rx_midiPacket_box.free(midi_inbox);
            packetbox_full = 0;
        }
    }
}

void midi_send_contctrl(char cc_number, char value) {
    midi_din.putc(CONT_CTRL);
    midi_din.putc(cc_number);
    midi_din.putc(value);
}

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

bool test_esc(int npa_value, float sdp_value) {
    if(fabs(sdp_value) < WARNING_DIFF_PRESSURE && npa_value < WARNING_PRESSURE){
#if AUTO_RESET == 1
        if (warning_count_before_reset > 20)
        NVIC_SystemReset();
        warning_count_before_reset++;
#endif
        return false;
    } else {
        return true;
        warning_count_before_reset = 0;
    }
}

int main() {
    // Launch MIDI stuf
    midiTask.start(midi_task);
    midiTask.set_priority(osPriorityAboveNormal2);

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
    warning_count_before_reset = 0;

    init_esc(0.0f);

    ThisThread::sleep_for(2000);
    if (sdp.init()){
        update_info_screen("sdp7xx on....");
    }
    ThisThread::sleep_for(2000);

    update_info_screen("set to min speed");
    init_esc(THROTTLE_START);
    ThisThread::sleep_for(2000);
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

void init_esc(float speed){       
    float speed_real = speed * ESC_THROTTLE_MAX;
    // SET SPEED
    soufflette.setThrottle(speed_real);
    // ESC REFRESH
    soufflette.pulse();
}

void update_esc(float speed){       
    float speed_real = speed * ESC_THROTTLE_MAX;
    if (fabs(actual_speed - speed_real) < ESC_SPEED_STEP) {
        actual_speed = speed_real;
    } else if (actual_speed < speed_real) {
        actual_speed = actual_speed + ESC_SPEED_STEP;
    } else {
        actual_speed = actual_speed - ESC_SPEED_STEP;
    }
    // SET SPEED
    soufflette.setThrottle(actual_speed);
    // ESC REFRESH
    soufflette.pulse();
}

void update_esc_screen(float esc_speed, int npa_value, float sdp_value){     
    // ESC_SPEED REFRESH
    display.clearDisplay();
    display.setTextCursor(20,0);
    if(test_esc(npa_value, sdp_value)) {
        display.printf ("-- SOUFFLETTE --");
    } else {
        display.printf ("WARN: ESC OFF ??");        
    }
    display.setTextCursor(0,15);
    sprintf(buffer, "MIDI            ");
    sprintf(buffer + strlen(buffer), "%05d", debug_midicount);
    //    sprintf(buffer, "TEMP            ");
    //sprintf(buffer + strlen(buffer), "%05f", (adc_temp.read()*100));
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
    float esc_x_line = (126.0*actual_speed)/ESC_THROTTLE_MAX;
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