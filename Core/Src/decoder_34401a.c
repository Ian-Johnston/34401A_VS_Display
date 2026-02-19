#include "decoder_34401a.h"
#include "main.h"     // for FP_* pin defines
#include <ctype.h>
#include <string.h>

// Match original config.h
#define MAX_SCK_DELAY_US 1500u  // 1.5ms

// ===== Extracted data =====
volatile char     dmm_main[16];
volatile uint16_t dmm_ann_state;
volatile int16_t  dmm_bar;
volatile uint8_t  dmm_bar_style;        // 0=POSITIVE, 1=FULLSCALE
volatile uint32_t dmm_new_data_counter;

// ===== Internal sniff state =====
static volatile uint8_t  byte_len;
static volatile bool     byte_ready;
static volatile bool     byte_not_read;
static volatile uint8_t  input_byte, output_byte, input_acc, output_acc;

static uint32_t last_us;

// ===== Frame buffers & parse =====
static uint8_t input_buf[100];
static uint8_t output_buf[100];
static uint8_t buf_len;

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


static void dmm_putc_safe(char c)
{
    if (msg_idx < (sizeof(dmm_main) - 1u)) {
        dmm_main[msg_idx++] = c;
        dmm_main[msg_idx] = '\0';
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
          input_buf[buf_len - 1]  == 0x00 &&
          output_buf[buf_len - 1] == 0xbb);
}

static inline void endFrame(void)
{
  buf_len = 0;
  frame_state = FRAME_UNKNOWN;
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
  }
}

static void publishAnnunciators(uint8_t h, uint8_t l)
{
  uint16_t state = ((uint16_t)h << 8) | (uint16_t)l;

  // Mirror Annunciators::update() behaviour:
  // last_state = (state & 0xF7FF) | (last_state & 0x0800)
  // i.e. preserve SHIFT bit (bit11) as locally tracked via button frames.
  uint16_t new_state = (uint16_t)((state & 0xF7FFu) | (dmm_ann_state & 0x0800u));

  if (new_state != dmm_ann_state) {
    dmm_ann_state = new_state;
    dmm_new_data_counter++;
  }
}

static void toggleShift(void)
{
  dmm_ann_state ^= 0x0800u;
  dmm_new_data_counter++;
}

static void clearShift(void)
{
  if (dmm_ann_state & 0x0800u) {
    toggleShift();
  }
}

static void messageByte(uint8_t byte)
{
    if (need_reset) {
        msg_idx = 0;
        memset((void*)dmm_main, 0, sizeof(dmm_main));
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
        // end of message
        dmm_main[msg_idx < sizeof(dmm_main) ? msg_idx : (sizeof(dmm_main) - 1u)] = '\0';
        dmm_new_data_counter++;
        need_reset = true;
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
  memset((void*)dmm_main, 0, sizeof(dmm_main));
  dmm_ann_state = 0;
  dmm_bar = 0;
  dmm_bar_style = 0;
  dmm_new_data_counter = 0;

  // Internal
  byte_len = 0;
  byte_ready = false;
  byte_not_read = false;
  input_acc = output_acc = 0;
  buf_len = 0;
  frame_state = FRAME_INIT;
  need_reset = true;

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
  input_acc  = (uint8_t)((input_acc  << 1) | (tinp & 1u));

  byte_len++;
  if (byte_len == 8u) {
    if (byte_ready) {
      byte_not_read = true;
    }
    input_byte = input_acc;
    output_byte = output_acc;
    byte_len = 0u;
    byte_ready = true;
  }
}

void Decoder34401_Process(void)
{
  // Optional: detect overruns
  if (byte_not_read) {
    // You can watch this in Live Watch if you add a global later.
    byte_not_read = false;
  }

  if (!byte_ready) {
    return;
  }

  // consume byte
  input_buf[buf_len]  = input_byte;
  output_buf[buf_len] = output_byte;
  byte_ready = false;
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
        } else if (((input_buf[0] & 0x7F) == 0x7F) && input_buf[1] == 0x00) {
          frame_state = FRAME_ANNUNCIATORS;
          break;
        } else {
          frame_state = FRAME_CONTROL;
          break;
        }
      }
      break;

    case FRAME_MESSAGE:
      if (lastBytesAreEof()) {
        updateBarGraphFromMessageFrame();
        endFrame();
      } else {
        messageByte(input_buf[buf_len - 1u]);
      }
      break;

    case FRAME_ANNUNCIATORS:
      if (lastBytesAreEof()) {
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
      // Original: if (input_buf[last] == 0x66) then use output_buf[1], [2]
      if (input_buf[buf_len - 1u] == 0x66) {
        if (buf_len >= 3u) {
          uint8_t h = output_buf[1];
          uint8_t l = output_buf[2];

          // Mirror original button logic for SHIFT annunciator:
          // if (h==0x9D && l==0xCF) toggleShift()
          // else if (l==0xE9 || l==0xBB || l==0x7D) clearShift()
          if (h == 0x9D && l == 0xCF) {
            toggleShift();
          } else if (l == 0xE9 || l == 0xBB || l == 0x7D) {
            clearShift();
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
