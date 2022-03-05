#include "STM32encoder.h"

STM32encoder enc(TIM2);     // create a managed encoder by TIM2. You have to use TIM input pin to connect encoder to. 
                            // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins

int16_t myVar = 0;

void setup() {
  Serial.begin(115200);
  enc.bind(&myVar);									  // must be called before enc.setbutton()		
  enc.setButton(PA2, BTN_STEP, 1, 10, 100);       // attach pin to button manager, assign the function scale and define bind steps to 1, 10, 100
}



void loop() {
  switch(enc.button()){     // read button and reset state
    case BTN_EVT_CLICK:
      Serial.printf("new scale ID: %u\n", enc.scaleId());
    break;
  }
  if (enc.isUpdated()) {
	Serial.printf("myVar: %i\n", myVar);
  }
}