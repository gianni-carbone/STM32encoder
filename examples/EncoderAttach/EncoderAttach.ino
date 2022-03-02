#include "STM32encoder.h"

STM32encoder enc(TIM2);		// create a managed encoder by TIM2. You have to use TIM input pin to connect encoder to. 
                          // i.e. for STM32F103 TIM2 has PA0 and PA1 inputs. See device documentation for other TIMs input pins

bool led_status = false;

void irq_callback(void) {
  digitalWrite(LED_BUILTIN, led_status?LOW:HIGH);
  led_status = !led_status;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  enc.attach(&irq_callback);
}

void loop() {
}
