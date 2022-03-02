#include "STM32encoder.h"

STM32encoder enc(TIM2, 0x07);   // create a managed encoder by TIM2. 
                                // You have to use TIM input pin to connect encoder to. 
                                // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins
								                // we use digital filter of 0x07 here to mitigate glitches

int32_t prevPos = 0;
int16_t myVar = 64;
int16_t prevmyVar = myVar;

void setup() {
  Serial.begin(115200);
  enc.bind(&myVar);             // bind myVar to the encoder [with default inc/dec ratio of one and no limits]
  enc.dynamic(20, 30, false);   // set a dynamic factor of 20 and a step limit of 30. Disable dynamic for position counter (counter will follow always mechanical absolute position).
                                // disabling dynamic for position counter affects bind variable only. Position counter will be inc/dec always by one 
                                // Steps per tick are: (_speed*_speedFactor)/100. Minimum value is always one inc/dec per tick
                                // The max inc/dec will be limited to 30 per tick
                                // Examples: 
                                //        for a speed of 1 tick/second -> (1*20)/100 = 0 ->             one inc/dec (increased to a minimum of one)
                                //        for a speed of 10 tick/second -> (10*20)/100 = 200/100  ->    two inc/dec
                                //        for a speed of 90 tick/second -> (90*20)/100 = 1800/100  ->   18 inc/dec
                                //        for a speed of 180 tick/second -> (180*20)/100 = 3600/100  -> 30 inc/dec (limited to 30 by step limit)
                                
}

void loop() {
  if (enc.isUpdated()) {
    int32_t pos = enc.pos();
    Serial.printf(
        "pos:%5ld myVar:%5d speed:%2d pos delta:%2ld myVar delta:%3ld\n"
        , pos
        , myVar
        , enc.speed()
        , pos - prevPos
        , myVar - prevmyVar
    );
    prevPos = pos;
    prevmyVar = myVar;
  }
}
