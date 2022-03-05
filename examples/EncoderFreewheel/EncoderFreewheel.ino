#include "STM32encoder.h"

STM32encoder enc(ENC_FREEWHEEL, TIM2);			  // create an umanaged encoder by TIM2 in freewheel mode (no interrupt managed). 
                                                  // in freewheel mode the timer just decode the encoder outputs, and update tim value according it. No further action is done.
                                                  // in this mode you can just read position (0..65535 circular) and direction. WARN: the position is inc/dec by encoder typical pulse per tick (normally 4)
                                                  // for this reason you have to divide position by pulse per tick to have the tick count
                                                  // You have to use TIM input pin to connect encoder to. 
                                                  // i.e. for STM32F103 TIM2 uses PA0 and PA1 for inputs. See device documentation for other TIMs input pins

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.printf("dir:%u pos:%u\n", enc.dir(), enc.pos());
  delay(1000);
}
