# STM32encoder
 Arduino library to manage quadrature encoder using hardware timer for STM32 architecture

Arduino library for stm32 platforms that simplifies the use of rotary encoders. With a few lines of code it is possible to instantiate an encoder object by managing its properties and events. It is possible to link numeric variables to the encoder so that they are automatically increased and decreased. It is also possible to attach interrupt routines to be executed at each hardware device tick. The library also manages the rotation dynamics, calculating the rotation speed and possibly correcting the rate of increase and decrease based on it.

The library uses the "encoder mode" features of the advanced timers present in the STM32 mcu. Each instantiated object uses (consumes) a timer.

In general, each STM32 mcu has several timers available that offer the encoder mode, so it is possible to use multiple encoders connected to a single mcu. The maximum number of encoders depends on the mcu model.

The encoder object can be instantiated in "managed" or "freewheel" mode. In the first mode it is possible to exploit all the potential of the library. The second mode, while facilitating the use of the encoder, leaves the user to manage the higher level functions. By default, object instantiation is in "managed" mode

Available main methods in "managed" mode:

  .pos()        read (or set) the hardware position counter. The position counter is a signed 32 bit counter.
  
  .dir()        reads the last direction of rotation.
  
  .isUpdated()  returns a position change event. Resets the position change flag.
  
  .speed()      returns the last speed of rotation (in ticks per second)
  
  .bind()       "binds" a variable to the encoder. The value will be auto updated by rotation of the device.
  
  .attach()     "attach" a isr routine to the encoder. The routine will be executed at every tick of device.
  
  .dynamic()    defines the update dynamics of the position counter and / or the linked variable.
  
  .circular()   defines the update behavior when an extreme of the linked variable value has been reached.


Available methods in "freewheel" mode:

  .pos()        read (or set) the hardware position counter. The position counter, in this mode, is an unsigned 16 bit counter.
  
  .dir()        reads the last direction of rotation.
