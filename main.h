#include "DigitalIn.h"
#include "DigitalOut.h"
#include "PinNames.h"
#include "PwmOut.h"
#include "Ticker.h"
#include "mbed.h"
#include <math.h>
#include "esc.h"
#include "nvstore.h"
#include "MIDIMessage.h"
//#include "DSHOT150.h"
#include <cstdint>
#include "Adafruit_SSD1306.h" 
#include "SDP6x.h"

#define AUTO_MODE                         0
#define AUTO_RESET                        0
#define NVSTORE_MIDI_CHAN_FORCE_STORE     0
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
#define ESC_SPEED_STEP                    0.13f
#define WARNING_PRESSURE                  11000
#define WARNING_DIFF_PRESSURE             3.00f
#define USER_BUTTON                       PA_1
#define BUTTON_DEBOUNCE                   0.2f
/* -----------------------------------------------------------------------------
 * MIDI stuf
 */
#define MIDI_UART_TX                      PA_9
#define MIDI_UART_RX                      PA_10
//#define MIDI_UART_TX                      PA_2
//#define MIDI_UART_RX                      PA_3
#define MIDIMAIL_SIZE                     64
#define MIDI_MAX_MSG_LENGTH               4 // Max message size. SysEx can be up to 65536.
#define CONT_CTRL                         176
#define NVSTORE_MIDI_CHAN_KEY             1
#define NVSTORE_MIDI_CHAN_FIRST_STORE     6
#define MIDIRECEIVE_CC_SPEED              4 
#define MIDISEND_REFRESH_TIME             0.25f
#define MIDISEND_CC_SPEED                 3
#define MIDISEND_CC_PRESSURE              4
#define MIDISEND_CC_VELO                  5

void update_info_screen(char* info);
void update_esc_screen(float esc_speed, int npa_value, float sdp_value);
void init_esc(float speed);
void update_esc(float speed);
void update_pc_debug(float esc_speed, int npa_value, float sdp_value);
void update_midi_infos(int npa_value, float sdp_value);
bool test_esc(int npa_value, float sdp_value);
// Callback for MIDI RX
void on_rx_interrupt();
void pb_hit_IN_interrupt ();
void pb_hit_OUT_interrupt ();

// Regrouping MIDI RX task in a Thread
// MIDI Serial & Thread and Ticker for working with different packet lengths
static RawSerial midi_din(MIDI_UART_TX, MIDI_UART_RX, 61250);
void midi_task();
void midi_send_ctrlchan(char cc_number, char value);
Thread midiTask;

// MIDI --------------------------
/* Mailbox of MIDI Packets */
typedef struct {
    uint8_t    midiPacket[MIDI_MAX_MSG_LENGTH]; /* AD result of measured voltage */
    int        midiPacketlenght = 0;
} midiPacket_t;
Mail<midiPacket_t, MIDIMAIL_SIZE>  rx_midiPacket_box;
midiPacket_t                       *rx_midi_outbox;
int                                packetbox_full = 0;
int                                rx_midi_Statusbyte = 0;
/* -- debug v2*/
int debug_midicount = 0;
int midi_value = 0;
// other
volatile int            rx_idx = 0;
uint8_t                 rx_buffer[MIDI_MAX_MSG_LENGTH + 1];
//--------------------------------

void print_return_code(int rc, int expected_rc)
{
    printf("Return code is %d ", rc);
    if (rc == expected_rc)
        printf("(as expected).\n");
    else
        printf("(expected %d!).\n", expected_rc);
}

class I2CPreInit : public I2C
{
public:
    I2CPreInit(PinName sda, PinName scl) : I2C(sda, scl)
    {
        frequency(1000000);
        start();
    };
};