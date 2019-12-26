// This needs version 0.92 or later of the USB Composite library: https://github.com/arpruss/USBComposite_stm32f1
#include <libmaple/iwdg.h>
#include "SegaController.h"
#include <USBComposite.h>

#define LED PC13
#define START_ACTIVATES_DPAD

const uint32_t watchdogSeconds = 3;

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

USBMultiXBox360<2> XBox360;

uint32_t lastDataTime[2] = { 0, 0 };

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

const uint16_t remap_retroarch[16] = {
  0xFFFF,
  0xFFFF | XBOX_DUP,
  0xFFFF | XBOX_DDOWN,
  0xFFFF | XBOX_DLEFT,

  0xFFFF | XBOX_DRIGHT,
  XBOX_START,
  XBOX_A, // A
  XBOX_B, // B
  
  XBOX_X, // C
  XBOX_LSHOULDER, // X
  XBOX_Y, // Y
  XBOX_RSHOULDER, // Z
  
  XBOX_GUIDE, // MODE

  0xFFFF, 0xFFFF, 0xFFFF
};

const uint16_t* remap = remap_retroarch;

inline int16_t range10u16s(uint16_t x) {
  return (((int32_t)(uint32_t)x - 512) * 32767 + 255) / 512;
}

void reset(USBXBox360Controller* c) {
  c->X(0);
  c->Y(0);
  c->buttons(0);
}

void setup() {
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  USBComposite.setProductString("GenesisToUSB");
  XBox360.begin();
  for (uint8 n = 0 ; n < 2 ; n++) {
    USBXBox360Controller* c = &XBox360.controllers[n];
    reset(c);
    c->send();
    c->setManualReportMode(true);
  }
}

void loop() {
  iwdg_feed();
  bool active = false;

  for (uint32 n = 0 ; n < NUM_INPUTS ; n++) {
    word state = inputs[n]->getState();
    USBXBox360Controller* c = &XBox360.controllers[n];
    
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
      uint16_t mask = 1;
      for (int i = 0; i < 16; i++, mask <<= 1) {
        uint16_t xb = remap[i];
        if (xb != 0xFFFF && (state & mask))
          c->button(xb, 1);
      }
#ifdef START_ACTIVATES_DPAD      
      if (state & SC_BTN_START) {
        if (state & SC_BTN_LEFT) {
          c->button(XBOX_DLEFT, 1);
        }
        if (state & SC_BTN_RIGHT) {
          c->button(XBOX_DRIGHT, 1);
        }
        if (state & SC_BTN_UP) {
          c->button(XBOX_DUP, 1);
        }
        if (state & SC_BTN_DOWN) {
          c->button(XBOX_DDOWN, 1);
        }
      }
#endif
    }
    else if (millis() - lastDataTime[n] >= 5000) {
       // we hold the last state for 5 seconds, in case something's temporarily wrong with the transmission 
       // but then we just clear the data
       reset(c);
    }
    c->send();
  }
  digitalWrite(LED,active?0:1);
}

