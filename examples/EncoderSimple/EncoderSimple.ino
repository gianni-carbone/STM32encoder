#include "STM32encoder.h"

STM32encoder enc(TIM2);						// create a manager encoder by TIM2. 
                                  // You have to use TIM input pin to connect encoder to. 
                                  // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (enc.isUpdated()) {
    Serial.printf("pos:%ld\n", enc.pos());
  }
}
