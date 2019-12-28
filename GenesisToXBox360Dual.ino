// This needs version 0.92 or later of the USB Composite library: https://github.com/arpruss/USBComposite_stm32f1
#include <libmaple/iwdg.h>
#include "SegaController.h"
#include <USBComposite.h>

/*
 * Sketch uses 20460 bytes (83%) of program storage space. Maximum is 24576 bytes.
Global variables use 4248 bytes (41%) of dynamic memory, leaving 5992 bytes for local variables. Maximum is 10240 bytes.
 */

#define USB_DISCONNECT_DELAY 2048 // works with Raspberry PI 3+ ; for other devices, may want something smaller
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

bool segaActive = false;
bool segaSecondActive = false;

#define MAX_INPUTS 2

#define SWITCH_MODE_DELAY 1500

USBMultiXBox360<2> XBox360Dual;
USBXBox360 XBox360Single;

uint32 numOutputs = 0;
SegaController* inputs[MAX_INPUTS];
USBXBox360Controller* outputs[MAX_INPUTS];

enum ControlMode {
  MODE_UNDEFINED,
  MODE_NONE,
  MODE_DUAL,
  MODE_SINGLE_FIRST,
  MODE_SINGLE_SECOND
} mode = MODE_UNDEFINED;

ControlMode upcomingMode = MODE_NONE;
uint32 newUpcomingModeTime = 0;


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

void setMode(ControlMode m) {
  if (mode == m)
    return;

  if (m == MODE_DUAL) {
    if (numOutputs == 1)
      XBox360Single.end();
    if (numOutputs != 2)
      XBox360Dual.begin();

    numOutputs = 2;
    outputs[0] = &XBox360Dual.controllers[0];
    outputs[1] = &XBox360Dual.controllers[1];
    inputs[0] = &sega;
    inputs[1] = &segaSecond;
  }
  else if (m == MODE_SINGLE_FIRST || m == MODE_SINGLE_SECOND || m == MODE_NONE) {
    if (numOutputs == 2) {
      XBox360Dual.end();
    }
    if (numOutputs != 1) {
      XBox360Single.begin();
    }

    numOutputs = 1;
    outputs[0] = &XBox360Single;
    if (m == MODE_SINGLE_FIRST || m == MODE_NONE)
      inputs[0] = &sega;
    else
      inputs[0] = &segaSecond;
  }

  mode = m;
  upcomingMode = mode;
  
  for (uint8 n = 0 ; n < numOutputs ; n++) {
    USBXBox360Controller* c = outputs[n];
    reset(c);
    c->send();
    c->setManualReportMode(true);
  }
}

ControlMode determine(word s1, word s2) {
  if (s1 & SC_CTL_ON) {
    if (s2 & SC_CTL_ON) {
      digitalWrite(PC13,0);
      return MODE_DUAL;
    }
    else 
      return MODE_SINGLE_FIRST;
  }
  else {
    if (s2 & SC_CTL_ON) {
      return MODE_SINGLE_SECOND;
    }
    else {
      return MODE_NONE;
    }
  }
}

void setup() {
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  USBComposite.setProductString("GenesisToUSB");
  USBComposite.setDisconnectDelay(USB_DISCONNECT_DELAY);

  setMode(determine(sega.getState(),segaSecond.getState()));
}

void loop() {
  iwdg_feed();
  bool active = false;

  if (mode != upcomingMode && millis() >= newUpcomingModeTime + SWITCH_MODE_DELAY)
    setMode(upcomingMode);

  word s1 = sega.getState();
  word s2 = segaSecond.getState();

  ControlMode m = determine(s1,s2);

  if (m != upcomingMode) {
    upcomingMode = m;
    newUpcomingModeTime = millis();
  }

  for (uint32 n = 0 ; n < numOutputs ; n++) {
    word state = inputs[n] == &sega ? s1 : s2;
    USBXBox360Controller* c = outputs[n];
    
    if (state & SC_CTL_ON) {
      //lastDataTime[n] = millis();
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
#if 0
    else if (millis() - lastDataTime[n] >= 5000) {
       // we hold the last state for 5 seconds, in case something's temporarily wrong with the transmission 
       // but then we just clear the data
       reset(c);
    }
#endif     
    c->send();
  }
  digitalWrite(LED,active?0:1);
}

