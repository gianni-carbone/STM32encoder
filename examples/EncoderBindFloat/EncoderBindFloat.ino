#include "STM32encoder.h"

STM32encoder enc(TIM2);               // create a managed encoder by TIM2. 
                                      // You have to use TIM input pin to connect encoder to. 
                                      // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins

float  myVar = 0.0;

void setup() {
  Serial.begin(115200);
  enc.bind(&myVar, 0.01, -1.0, 1.0);  // bind myVar to encoder by inc/dec ratio of 0.01 and limit to [-1.0, 1.0] range of values
}

void loop() {
  if (enc.isUpdated()) {
    Serial.printf(
        "pos:%5ld dir:%2d myVar:%2.2f\n"
        , enc.pos()
        , enc.dir()
        , myVar
    );
  }
}
