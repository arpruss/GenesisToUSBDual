// This needs version 0.94 or later of the USB Composite library: https://github.com/arpruss/USBComposite_stm32f1
#include <libmaple/iwdg.h>
#include "SegaController.h"
#include <USBComposite.h>

#define USB_DISCONNECT_DELAY 500 // for some devices, may want something smaller
#define LED PC13
#define START_COMBO
#ifdef START_COMBO
#define START_ONLY_ON_RELEASE // the start button only shows up on release, and only if it wasn't used to trigger a key combination
#define START_DEPRESSION_TIME 200
#endif

const uint32_t watchdogSeconds = 6;

// Looking straight on at female socket (or from back of male jack):

// 5 4 3 2 1
//  9 8 7 6

// 3.3 PA3 PA2 PA1 PA0
//   PA6 GND PA5 PA4

// 3.3 PB3 PB5 PB4 PB6
//   PB9 GND PB8 PB7


// pins:                    7,   1,   2,   3,   4,   6,   9
SegaController sega(      PA5, PA0, PA1, PA2, PA3, PA4, PA6);
SegaController segaSecond(PB8, PB6, PB4, PB5, PB3, PB7, PB9);

#define NUM_INPUTS 2

SegaController* inputs[] = { &sega, &segaSecond };

USBXBox360W<4> XBox360;

uint32_t lastDataTime[2] = { 0, 0 };

struct start_data {
  uint32 time;
  boolean pressed;
  boolean combo;
} start[2] = { {0} };

/*
 *     SC_CTL_ON    = 1, // The controller is connected
    SC_BTN_UP    = 2,
    SC_BTN_DOWN  = 4,
    SC_BTN_LEFT  = 8,
    SC_BTN_RIGHT = 16,
    SC_BTN_START = 32,
    SC_BTN_A     = 64,
    SC_BTN_B     = 128,
    SC_BTN_C     = 256,
    SC_BTN_X     = 512,
    SC_BTN_Y     = 1024,
    SC_BTN_Z     = 2048,
    SC_BTN_MODE  = 4096,

 */

const struct remap_item {
  uint16_t xbox;
  bool worksWithStart;
} remap_retroarch[16] = {
  { 0xFFFF, true },
  { 0xFFFF | XBOX_DUP, false },
  { 0xFFFF | XBOX_DDOWN, false },
  { 0xFFFF | XBOX_DLEFT, false },
  { 0xFFFF | XBOX_DRIGHT, false },
  { XBOX_START, false },
  { XBOX_A, true }, // A
  { XBOX_B, true }, // B
  { XBOX_X, true }, // C
  { XBOX_LSHOULDER, false }, // X
  { XBOX_Y, false }, // Y
  { XBOX_RSHOULDER, false }, // Z  
  { XBOX_GUIDE, true }, // MODE
  { 0xFFFF, true },
  { 0xFFFF, true },
  { 0xFFFF, true }
};

const struct remap_item * remap = remap_retroarch;

inline int16_t range10u16s(uint16_t x) {
  return (((int32_t)(uint32_t)x - 512) * 32767 + 255) / 512;
}

void reset(USBXBox360WController* c) {
  c->X(0);
  c->Y(0);
  c->buttons(0);
}

void setup() {
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  USBComposite.setProductString("GenesisToUSB");
  USBComposite.setDisconnectDelay(USB_DISCONNECT_DELAY);
  XBox360.begin();
  for (uint8 n = 0 ; n < 2 ; n++) {
    USBXBox360WController* c = &XBox360.controllers[n];
    c->setManualReportMode(true);
  }
}

void loop() {
  iwdg_feed();
  
  if (! USBComposite)
    return;
    
  bool active = false;

  for (uint32 n = 0 ; n < NUM_INPUTS ; n++) {
    word state = inputs[n]->getState();
    USBXBox360WController* c = &XBox360.controllers[n];
    
    if (state & SC_CTL_ON) {
      lastDataTime[n] = millis();
      active = true;
      int16 x = 0;
      if (! (state & SC_BTN_START)) {
        if (state & SC_BTN_LEFT)
          x = -32768;
        else if (state & SC_BTN_RIGHT)
          x = 32767;
      }
      c->X(x);

      int16 y = 0;
      if (! (state & SC_BTN_START)) {
        if (state & SC_BTN_UP) 
            y = 32767;
        else if (state & SC_BTN_DOWN) 
            y = -32768;
      }
      c->Y(y);
  
      c->buttons(0);
      struct start_data *s = start + n;
      uint16_t mask = 1;
      for (int i = 0; i < 16; i++, mask <<= 1) {
#ifdef START_ONLY_ON_RELEASE
        if ((state & SC_BTN_START) && ! remap[i].worksWithStart)
          continue;
#endif
        uint16_t xb = remap[i].xbox;
        if (xb != 0xFFFF && (state & mask))
          c->button(xb, 1);
      }
#ifdef START_COMBO
      if (state & SC_BTN_START) {
        s->pressed = true;
        s->time = millis();
        if (state & SC_BTN_LEFT) {
          c->button(XBOX_DLEFT, 1);
          c->X(0);
          s->combo = true;
        }
        if (state & SC_BTN_RIGHT) {
          c->button(XBOX_DRIGHT, 1);
          c->X(0);
          s->combo = true;
        }
        if (state & SC_BTN_UP) {
          c->button(XBOX_DUP, 1);
          c->Y(0);
          s->combo = true;
        }
        if (state & SC_BTN_DOWN) {
          c->button(XBOX_DDOWN, 1);
          c->Y(0);
          s->combo = true;
        }
        if (state & SC_BTN_X) {
          c->button(XBOX_BACK, 1);
          s->combo = true;
        }
        if (state & SC_BTN_Y) {
          c->button(XBOX_LSHOULDER, 1);
          s->combo = true;
        }
        if (state & SC_BTN_Z) {
          c->button(XBOX_R3, 1);
          s->combo = true;
        }
      }
      else {
#ifdef START_ONLY_ON_RELEASE
        uint32 t = millis();
        if (s->pressed) {
          if (s->combo) 
            s->time = 0;
          else
            s->time = t;
        }
        if (s->time != 0 && t <= s->time + START_DEPRESSION_TIME)
          c->button(XBOX_START, 1);
        else
          s->time = 0;
        s->pressed = false;
        s->combo = false;
#endif        
      }
#endif
      c->send();
    }
    else if (c->isConnected() && millis() - lastDataTime[n] >= 2000) {
       // we hold the last state for 2 seconds, in case something's temporarily wrong with the transmission 
       // but then we just clear the data
       reset(c);
       c->send();
       c->connect(false);
    }
  }
  digitalWrite(LED,active?0:1);
}

