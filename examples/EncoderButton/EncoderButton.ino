#include "STM32encoder.h"

STM32encoder enc(TIM2);     // create a managed encoder by TIM2. You have to use TIM input pin to connect encoder to. 
                            // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins
void setup() {
  Serial.begin(115200);
  enc.setbutton(PA2);       // attach pin to button manager
}



void loop() {
  switch(enc.button()){     // read button and reset state
    case TIM_BTN_EVT_CLICK:
      Serial.printf("Click detected!\n");
    break;
    case TIM_BTN_EVT_LONG:
      Serial.printf("Long press detected!\n");
    break;
  }
}
