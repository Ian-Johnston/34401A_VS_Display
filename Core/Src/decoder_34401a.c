#include "decoder_34401a.h"
#include "main.h"     // for FP_* pin defines
#include <ctype.h>
#include <string.h>

// Match original config.h
#define MAX_SCK_DELAY_US 1500u  // 1.5ms

// FIFO size must be power of 2 for simple wrap
#define BYTE_FIFO_SIZE 64u
#define BYTE_FIFO_MASK (BYTE_FIFO_SIZE - 1u)

// ===== Extracted data =====
volatile char     dmm_main[16];
volatile uint16_t dmm_ann_state;
volatile int16_t  dmm_bar;
volatile uint8_t  dmm_bar_style;        // 0=POSITIVE, 1=FULLSCALE
volatile uint32_t dmm_new_data_counter;

// ===== Internal sniff state =====
static volatile uint8_t  byte_len;
static volatile uint8_t  input_acc, output_acc;

typedef struct {
    uint8_t in;
    uint8_t out;
} sniff_byte_t;

static volatile sniff_byte_t byte_fifo[BYTE_FIFO_SIZE];
static volatile uint8_t fifo_wr;
static volatile uint8_t fifo_rd;

static uint32_t last_us;

// ===== Frame buffers & parse =====
static uint8_t input_buf[100];
static uint8_t output_buf[100];
static uint8_t buf_len;

// ===== Debug =====
static volatile uint8_t dbg_ann_len;
static volatile uint8_t dbg_ann_b0;
static volatile uint8_t dbg_ann_b1;
static volatile uint8_t dbg_ann_b2;
static volatile uint8_t dbg_ann_b3;
static volatile uint8_t dbg_ann_b4;
static volatile uint8_t dbg_ann_b5;
static volatile uint8_t dbg_ann_b6;
static volatile uint8_t dbg_ann_b7;
static volatile uint8_t dbg_btn_len;
static volatile uint8_t dbg_btn_o0;
static volatile uint8_t dbg_btn_o1;
static volatile uint8_t dbg_btn_o2;
static volatile uint8_t dbg_btn_o3;
static volatile uint8_t dbg_btn_o4;
static volatile uint8_t dbg_btn_o5;
static volatile uint8_t dbg_btn_i0;
static volatile uint8_t dbg_btn_i1;
static volatile uint8_t dbg_btn_i2;
static volatile uint8_t dbg_btn_i3;
static volatile uint8_t dbg_btn_i4;
static volatile uint8_t dbg_btn_i5;
static volatile uint32_t dbg_btn_code_last;
static volatile uint32_t dbg_btn_code_prev;
static volatile uint32_t dbg_btn_count;
static volatile uint32_t dbg_byte_overrun_count = 0;
volatile uint32_t dmm_main_counter;
volatile uint32_t dmm_ann_counter;
volatile uint32_t dmm_bar_counter;

// ===== SHIFT handling =====
static uint32_t shift_window_start_us = 0;
static uint8_t  shift_press_count = 0;
static bool     shift_window_active = false;

typedef enum {
    FRAME_INIT,
    FRAME_UNKNOWN,
    FRAME_MESSAGE,
    FRAME_ANNUNCIATORS,
    FRAME_CONTROL,
    FRAME_BUTTON
} frame_state_t;

static frame_state_t frame_state = FRAME_INIT;

// ===== MESSAGE assembly (like Eventhandler::messageByte) =====
static uint8_t msg_idx = 0;
static bool need_reset = true;
static char msg_work[16];

static void dmm_putc_safe(char c)
{
    if (msg_idx < 14u) {
        msg_work[msg_idx++] = c;
    }
}

// -----------------------------------------------------------------------------
// DWT micros (STM32F1): fast and simple
// -----------------------------------------------------------------------------
static inline void DWT_Init(void)
{
    // Enable TRC
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset cycle counter
    DWT->CYCCNT = 0;
    // Enable cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t micros32(void)
{
    // SystemCoreClock in Hz; convert cycles to microseconds
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static inline bool lastBytesAreEof(void)
{
    // same as decoder.cpp: 0x00 / 0xBB pair
    return (buf_len > 0 &&
        input_buf[buf_len - 1] == 0x00 &&
        output_buf[buf_len - 1] == 0xbb);
}

static inline void endFrame(void)
{
    buf_len = 0;
    frame_state = FRAME_UNKNOWN;
}

static void processShiftWindow(void)
{
    if (shift_window_active) {
        uint32_t now_us = micros32();

        // 300 ms quiet window to collect single/double presses
        if ((uint32_t)(now_us - shift_window_start_us) > 300000u) {
            if (shift_press_count & 1u) {
                dmm_ann_state ^= 0x0800u;
                dmm_new_data_counter++;
                dmm_ann_counter++;
            }

            shift_press_count = 0;
            shift_window_active = false;
        }
    }
}

static void updateBarGraphFromMessageFrame(void)
{
    // mirrors Decoder::updateBarGraph()

    // style: if input_buf[2] is digit -> POSITIVE else FULLSCALE
    uint8_t style = (isdigit((int)input_buf[2]) ? 0u : 1u);
    int16_t barvalue = 0;

    uint16_t st = (style == 0u) ? 2u : 3u;
    uint16_t c = 0;

    for (; c < ((style == 0u) ? 4u : 3u) && st < 8u; st++) {
        if (isdigit((int)input_buf[st])) {
            barvalue = (int16_t)(10 * barvalue + (int16_t)(input_buf[st] - '0'));
            c++;
        }
    }

    if (style == 1u && input_buf[2] == '-') {
        barvalue = (int16_t)(-barvalue);
    }

    // publish only if changed
    if (dmm_bar_style != style || dmm_bar != barvalue) {
        dmm_bar_style = style;
        dmm_bar = barvalue;
        dmm_new_data_counter++;
        dmm_bar_counter++;
    }
}

static void publishAnnunciators(uint8_t h, uint8_t l)
{
    uint16_t state = ((uint16_t)h << 8) | (uint16_t)l;

    // Preserve SHIFT bit (bit11) from local button tracking
    uint16_t new_state = (uint16_t)((state & 0xF7FFu) | (dmm_ann_state & 0x0800u));

    if (new_state != dmm_ann_state) {
        dmm_ann_state = new_state;
        dmm_new_data_counter++;
        dmm_ann_counter++;
    }
}

static void messageByte(uint8_t byte)
{
    if (need_reset) {
        msg_idx = 0;
        memset((void*)msg_work, ' ', 14);   // build a fixed-width field here
        msg_work[14] = '\0';
        msg_work[15] = '\0';
        need_reset = false;
    }

    switch (byte) {
    case 0x84: dmm_putc_safe('.'); break;
    case 0x86: dmm_putc_safe(','); break;

    case 0x8d:
        // original: mark previous char blinking; we ignore blinking
        // fall through to ':'
    case 0x8c:
        dmm_putc_safe(':');
        break;

    case 0x81:
        // control char: ignore
        break;

    case 0x00:
        // end of message handled at frame EOF
        break;

    default:
        dmm_putc_safe((char)byte);
        break;
    }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
void Decoder34401_Init(void)
{
    DWT_Init();

    // Clear outputs
    memset((void*)dmm_main, ' ', 14);
    dmm_main[14] = '\0';
    dmm_main[15] = '\0';

    memset((void*)msg_work, ' ', 14);
    msg_work[14] = '\0';
    msg_work[15] = '\0';

    dmm_ann_state = 0;
    dmm_bar = 0;
    dmm_bar_style = 0;
    dmm_new_data_counter = 0;

    // Internal
    byte_len = 0;
    input_acc = output_acc = 0;
    fifo_wr = 0;
    fifo_rd = 0;
    buf_len = 0;
    frame_state = FRAME_INIT;
    need_reset = true;

    shift_window_start_us = 0;
    shift_press_count = 0;
    shift_window_active = false;

    dbg_btn_code_last = 0;
    dbg_btn_code_prev = 0;
    dbg_btn_count = 0;
    dbg_byte_overrun_count = 0;

    dmm_main_counter = 0;
    dmm_ann_counter = 0;
    dmm_bar_counter = 0;

    last_us = micros32();
}

void Decoder34401_SckEdge(void)
{
    // mid-byte gap detection (power-on / pause)
    uint32_t now_us = micros32();
    if (byte_len != 0u && (uint32_t)(now_us - last_us) > MAX_SCK_DELAY_US) {
        byte_len = 0u;
    }
    last_us = now_us;

    // Read PB14/PB15 as 2-bit: (DIN, DOUT) or vice versa per original
    // Original: tinp = (IDR >> 14) & 0b11; output uses bit1 (PB15), input uses bit0 (PB14)
    uint32_t idr = FP_GPIO_Port->IDR;
    uint8_t tinp = (uint8_t)((idr >> 14) & 0x3u);

    output_acc = (uint8_t)((output_acc << 1) | (tinp >> 1));
    input_acc = (uint8_t)((input_acc << 1) | (tinp & 1u));

    byte_len++;
    if (byte_len == 8u) {
        uint8_t next_wr = (uint8_t)((fifo_wr + 1u) & BYTE_FIFO_MASK);

        if (next_wr == fifo_rd) {
            dbg_byte_overrun_count++;
        }
        else {
            byte_fifo[fifo_wr].in = input_acc;
            byte_fifo[fifo_wr].out = output_acc;
            fifo_wr = next_wr;
        }

        byte_len = 0u;
    }
}

void Decoder34401_Process(void)
{
    processShiftWindow();

    while (fifo_rd != fifo_wr) {
        uint8_t input_byte;
        uint8_t output_byte;

        input_byte = byte_fifo[fifo_rd].in;
        output_byte = byte_fifo[fifo_rd].out;
        fifo_rd = (uint8_t)((fifo_rd + 1u) & BYTE_FIFO_MASK);

        // consume byte
        input_buf[buf_len] = input_byte;
        output_buf[buf_len] = output_byte;
        buf_len++;

        switch (frame_state) {

        case FRAME_INIT:
            if (lastBytesAreEof()) {
                endFrame();
            }
            break;

        case FRAME_UNKNOWN:
            // BUTTON frame signature
            if (buf_len == 1u && input_buf[0] == 0x00 && output_buf[0] == 0x77) {
                frame_state = FRAME_BUTTON;
                break;
            }

            if (buf_len == 2u) {
                if (input_buf[0] == 0x00 && ((input_buf[1] & 0x7F) == 0x7F)) {
                    frame_state = FRAME_MESSAGE;
                    break;
                }
                else if (((input_buf[0] & 0x7F) == 0x7F) && input_buf[1] == 0x00) {
                    frame_state = FRAME_ANNUNCIATORS;
                    break;
                }
                else {
                    frame_state = FRAME_CONTROL;
                    break;
                }
            }
            break;

        case FRAME_MESSAGE:
            if (lastBytesAreEof()) {
                memcpy((void*)dmm_main, (const void*)msg_work, sizeof(dmm_main));
                dmm_new_data_counter++;
                dmm_main_counter++;
                need_reset = true;

                updateBarGraphFromMessageFrame();
                endFrame();
            }
            else {
                messageByte(input_buf[buf_len - 1u]);
            }
            break;

        case FRAME_ANNUNCIATORS:
            if (lastBytesAreEof()) {
                dbg_ann_len = buf_len;

                dbg_ann_b0 = (buf_len > 0u) ? input_buf[0] : 0u;
                dbg_ann_b1 = (buf_len > 1u) ? input_buf[1] : 0u;
                dbg_ann_b2 = (buf_len > 2u) ? input_buf[2] : 0u;
                dbg_ann_b3 = (buf_len > 3u) ? input_buf[3] : 0u;
                dbg_ann_b4 = (buf_len > 4u) ? input_buf[4] : 0u;
                dbg_ann_b5 = (buf_len > 5u) ? input_buf[5] : 0u;
                dbg_ann_b6 = (buf_len > 6u) ? input_buf[6] : 0u;
                dbg_ann_b7 = (buf_len > 7u) ? input_buf[7] : 0u;

                // same indices as original: input_buf[3], input_buf[2]
                if (buf_len >= 4u) {
                    publishAnnunciators(input_buf[3], input_buf[2]);
                }
                endFrame();
            }
            break;

        case FRAME_CONTROL:
            if (lastBytesAreEof()) {
                // You can later decode blink/control if you want; we ignore for now.
                endFrame();
            }
            break;

        case FRAME_BUTTON:
            if (input_buf[buf_len - 1u] == 0x66) {
                dbg_btn_len = buf_len;

                dbg_btn_o0 = (buf_len > 0u) ? output_buf[0] : 0u;
                dbg_btn_o1 = (buf_len > 1u) ? output_buf[1] : 0u;
                dbg_btn_o2 = (buf_len > 2u) ? output_buf[2] : 0u;
                dbg_btn_o3 = (buf_len > 3u) ? output_buf[3] : 0u;
                dbg_btn_o4 = (buf_len > 4u) ? output_buf[4] : 0u;
                dbg_btn_o5 = (buf_len > 5u) ? output_buf[5] : 0u;

                dbg_btn_i0 = (buf_len > 0u) ? input_buf[0] : 0u;
                dbg_btn_i1 = (buf_len > 1u) ? input_buf[1] : 0u;
                dbg_btn_i2 = (buf_len > 2u) ? input_buf[2] : 0u;
                dbg_btn_i3 = (buf_len > 3u) ? input_buf[3] : 0u;
                dbg_btn_i4 = (buf_len > 4u) ? input_buf[4] : 0u;
                dbg_btn_i5 = (buf_len > 5u) ? input_buf[5] : 0u;

                if (buf_len >= 3u) {
                    uint32_t code =
                        ((uint32_t)output_buf[0] << 16) |
                        ((uint32_t)output_buf[1] << 8) |
                        ((uint32_t)output_buf[2]);

                    dbg_btn_code_prev = dbg_btn_code_last;
                    dbg_btn_code_last = code;
                    dbg_btn_count++;

                    // SHIFT button code
                    if (code == 7839183u) {
                        uint32_t now_us = micros32();

                        if (!shift_window_active) {
                            shift_window_active = true;
                            shift_window_start_us = now_us;
                            shift_press_count = 1;
                        }
                        else {
                            shift_press_count++;
                            shift_window_start_us = now_us;   // extend window
                        }
                    }
                }
                endFrame();
            }
            break;

        default:
            endFrame();
            break;
        }

        // Avoid buffer overflow in bad sync conditions
        if (buf_len >= sizeof(input_buf)) {
            endFrame();
        }
    }
}