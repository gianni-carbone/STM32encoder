# STM32encoder
 Arduino library to manage quadrature encoder using hardware timer for STM32 architecture

Arduino library for stm32 platforms that simplifies the use of rotary encoders. With a few lines of code it is possible to instantiate an encoder object by managing its properties and events. It is possible to link numeric variables to the encoder so that they are automatically increased and decreased. It is also possible to attach interrupt routines to be executed at each hardware device tick. The library also manages the rotation dynamics, calculating the rotation speed and possibly correcting the rate of increase and decrease based on it.
