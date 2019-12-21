#include <libmaple/iwdg.h>
#include <SegaController.h>
#include <USBXBox360.h>
#include "USBXBox360second.h"
#undef USBXBox360

#define LED PC13

const uint32_t watchdogSeconds = 3;

// NB: connect A10 to TX, A9 to RX

SegaController sega(PA5, PA0, PA1, PA2, PA3, PA4, PA6);
SegaController segaSecond(PB5, PB0, PB1, PB2, PB3, PB4, PB6);

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
  XBox360.registerComponent();
  XBox360second.registerComponent();
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
  for (uint8 i = 0 ; i < NUM_INPUTS ; i++) {
    word state = inputs[i]->getState();
    if (state & SC_CTL_ON) {
      active = true;
      if (state & SC_BTN_LEFT) 
          X(i, -32768);
      else if (state & SC_BTN_RIGHT)
          X(i, 32767);
      else
          X(i, 0);
  
      if (state & SC_BTN_UP) 
          Y(i, 32767);
      else if (state & SC_BTN_DOWN) 
          Y(i, -32768);
      else
          Y(i, 0);
  
      buttons(i, 0);
      uint16_t mask = 1;
      for (int i = 0; i < 16; i++, mask <<= 1) {
        uint16_t xb = remap[i];
        if (xb != 0xFFFF && (state & mask))
          button(i, xb, 1);
      }
      if (state & XBOX_START) {
        if (state & SC_BTN_LEFT) {
          button(i, XBOX_DLEFT, 1);
        }
        if (state & SC_BTN_RIGHT) {
          button(i, XBOX_DRIGHT, 1);
        }
        if (state & SC_BTN_UP) {
          button(i, XBOX_DUP, 1);
        }
        if (state & SC_BTN_DOWN) {
          button(i, XBOX_DDOWN, 1);
        }
      }
      send(i);
    }
  }
  digitalWrite(LED,active?0:1);
}

