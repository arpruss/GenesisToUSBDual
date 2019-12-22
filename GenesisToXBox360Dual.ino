// This needs version 0.92 or later of the USB Composite library: https://github.com/arpruss/USBComposite_stm32f1
#include <libmaple/iwdg.h>
#include "SegaController.h"
#include <USBXBox360.h>
#include "USBXBox360second.h"
#undef USBXBox360

#define LED PC13
#define START_ACTICATES_DPAD

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

USBXBox360 XBox360;
USBXBox360second XBox360second;

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
  XBOX_B, // A
  XBOX_A, // B
  
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

void setup() {
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  USBComposite.setProductString("GenesisToXBox360");
  XBox360second.registerComponent();
  XBox360.registerComponent();
  XBox360.setManualReportMode(true);
  XBox360second.setManualReportMode(true);
  USBComposite.begin();
}

void button(uint8 controller, uint8 buttonNumber, uint8 value) {
  if (controller == 0)
    XBox360.button(buttonNumber, value);
  else
    XBox360second.button(buttonNumber, value);
}

void buttons(uint8 controller, uint16 values) {
  if (controller == 0)
    XBox360.buttons(values);
  else
    XBox360second.buttons(values);
}

void X(uint8 controller, uint16 value) {
  if (controller == 0)
    XBox360.X(value);
  else
    XBox360second.X(value);
}

void Y(uint8 controller, uint16 value) {
  if (controller == 0)
    XBox360.Y(value);
  else
    XBox360second.Y(value);
}

void send(uint8 controller) {
  if (controller == 0)
    XBox360.send();
  else
    XBox360second.send();
}

void loop() {
  iwdg_feed();
  bool active = false;
  for (uint8 c = 0 ; c < NUM_INPUTS ; c++) {
    word state = inputs[c]->getState();
    if (state & SC_CTL_ON) {
      active = true;
      int16 x = 0;
      if (! (state & SC_BTN_START)) {
        if (state & SC_BTN_LEFT)
          x = -32768;
        else if (state & SC_BTN_RIGHT)
          x = 32767;
      }
      X(c,x);

      int16 y = 0;
      if (! (state & SC_BTN_START)) {
        if (state & SC_BTN_UP) 
            y = 32767;
        else if (state & SC_BTN_DOWN) 
            y = -32768;
      }
      Y(c,y);
  
      buttons(c, 0);
      uint16_t mask = 1;
      for (int i = 0; i < 16; i++, mask <<= 1) {
        uint16_t xb = remap[i];
        if (xb != 0xFFFF && (state & mask))
          button(c, xb, 1);
      }
#ifdef START_ACTIVATES_DPAD      
      if (state & SC_BTN_START) {
        if (state & SC_BTN_LEFT) {
          button(c, XBOX_DLEFT, 1);
        }
        if (state & SC_BTN_RIGHT) {
          button(c, XBOX_DRIGHT, 1);
        }
        if (state & SC_BTN_UP) {
          button(c, XBOX_DUP, 1);
        }
        if (state & SC_BTN_DOWN) {
          button(c, XBOX_DDOWN, 1);
        }
      }
#endif      
      send(c);
    }
  }
  digitalWrite(LED,active?0:1);
}

