# STM32encoder

[![GitHub release](https://img.shields.io/github/v/release/gianni-carbone/STM32encoder.svg)](https://github.com/gianni-carbone/STM32encoder/releases)
[![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](#Contributing)
[![GitHub issues](https://img.shields.io/github/issues/gianni-carbone/STM32encoder.svg)](https://github.com/gianni-carbone/STM32encoder/issues)

## Description
Arduino library for the management of rotary encoders with STM32. This Arduino library simplifies the use of rotary encoders. It works with stm32 platforms.

This Arduino library makes it easy to use rotary encoders. With a few lines of code it is possible to instantiate an encoder object and fully manage it. You can link a variable so that it is increased and decreased by turning the knob. It is also possible to attach a user defined isr to be executed on each tick of knob. Optionally, the library can take into account the rotation dynamics, detecting the speed and correcting the rate of increase in proportion to it.

The library uses the "encoder mode" features of the advanced timers present in the STM32 mcu. Each instantiated object uses (consumes) a timer. In general, each STM32 mcu has several timers available that offer the encoder mode, so it is possible to use multiple encoders connected to a single mcu. The maximum number of encoders depends on the mcu model.

The encoder object can be instantiated in "managed" or "freewheel" mode. In the first mode it is possible to exploit all the potential of the library. The second mode, while facilitating the use of the encoder and keeping low mcu overheads, leaves the user to manage the higher level functions. By default, object instantiation is in "managed" mode. 

There is also a small set of functions for managing the knob button. Currently the library debounces the button and detects click and long click events. Alternatively, the knob button can be settled to vary the incRate on a predefined scale (eg 1, 10, 100) presets it via the setButton function.

Please take a look at the examples to get a better understanding of how it works.

## Brief of methods and properties
Available main methods in "managed" mode:
- .pos()        read (or set) the hardware position counter. The position counter is a signed 32 bit counter.
- .dir()        reads the last direction of rotation.
- .isUpdated()  returns a position change event. Resets the position change flag.
- .speed()      returns the last speed of rotation (in ticks per second)
- .bind()       "binds" a variable to the encoder. The value will be auto updated by rotation of the device.
- .attach()     "attach" a isr routine to the encoder. The routine will be executed at every tick of device.
- .dynamic()    defines the update dynamics of the position counter and / or the linked variable.
- .circular()   defines the update behavior when an extreme of the linked variable value has been reached.

Available methods in "freewheel" mode:

- .pos()        read (or set) the hardware position counter. The position counter, in this mode, is an unsigned 16 bit counter.
- .dir()        reads the last direction of rotation.

## Syntax of methods and properties
- **STM32Encoder(t, [f], [pt])**          Instantiate the encoder object by connecting it to the t (timer) , applying the digital filters f (uint8) and specifying the number of pulses per tick pt (uint16) that the encoder generates. The object is created in "Managed" mode. Standard encoders generate four pulses per tick, the default value for pt is therefore 4. The digital filter is a feature of the STM timers that allows you to reduce the glitches of mechanical devices at the expense of the maximum detectable speed. The allowed values range from 0 (no filters) to 0x7F, maximum filter value. The default value is 0 (no filters).
- **STM32Encoder(m, t, [f], [pt])**       Instantiate the encoder object in mode m, by connecting it to the t (timer), applying the digital filters f (uint8) and specifying the number of pulses per tick pt (uint16) that the encoder generates. In this case it is possible to activate the object in FREEWHEEL mode, specifying m = ENC_FREEWHEEL. The freewheel encoder will have a very simplified behavior, as mentioned at the beginning of the document. By specifying m = ENC_MANAGED, the behavior will be exactly the same as the declaration mode mentioned above
- **pos()**                               returns the counter (int32). The counter is an absolute counter that increase/decrease of 1 when dynamic counter is disabled (see below).
- **pos(p)**                              sets the counter to p (int32)
- **dir()**                               returns (bool) the last direction of rotation
- **isUpdated()**                         returns (bool) a position update available since last call. When called, this function resets the update flag.
- **speed()**                             returns (int16) the (last) speed of rotation in ticks per seconds (limited to 1000 ticks/s)
- **bind(var,[incRate], [min], [max])**   "bind" a variable to the encoder. The data type of var can be one of the following: int8, uint8, int16, uint16, int32, uint32, float, and double. incRate (int16 for integers, float or double for the remainder) determines the increment value of any linked variable in the measure of incRate * tick. for example an incrate of 2 increases the linked variable by two for each tick. An inc rate of 0.1 (in the case of a float or double) increases the variable by 0.1. It is possible to specify negative inc rates, in which case the linked variable is incremented in the opposite direction to the counter. [min], [max] determine the extremes of variation of the linked variable. For example with min = 0 and max = 10, the linked variable is varied between the values zero and ten. Once one extreme is reached, while the encoder is rotated, the variable stops or returns to the other extreme, according to what is set in the "circular" function (see below)
- **attach(f)**                           attach the user function f to the tickIsr. The function f will be called at every encoder tick, at the end of isr tick routine (after updating states). 
- **dynamic(r,[l],[c])**                  sets the dynamic increment behavior of the linked variable and / or the counter. Where r (uint16) is the multiplication rate with respect to the speed, l (uint16) is the maximum increase limit of the variable and c (bool) applies the dynamic behavior to the counter as well. Once the dynamics have been set, the linked variable will increase / decrease with a rate that depends on the incRate set with the "bind" function and on the rotation speed according to the constant r specified in this function. The increment function is as follows: steps = ((speed * r) / 100). The increment value of the linked variable will be steps * incRate, the counter is incremented exactly equal to steps (in case c = true) otherwise its increment value will always be equal to 1 per tick
- **circular(m)**                         m (bool) sets the circular increment behavior of the linked variable. When m = true, the linked variable behave in circular mode. When m = true, the linked variable behaves in a circular way, that is, once it reaches an extreme fixed in the "bind" function, continuing to rotate the encoder, it will return to the other extreme. eg, assuming an integer bind with fixed extremes between 0 and 10, by rotating it towards the encoder, the variable will follow the following increment scheme 0,1,2 ..., 10,0, .... in case m = false, while continuing to rotate the encoder, the variable would have remained at 10. Warn: the counter is always circular, regardless of m
- **setbutton(p,[f],[[r1,r2][..,r5]])**   Attach the button to pin p (pin), assigning it the function f (btn_function_t). If the assigned function is BTN_SCALE, then button click switches the scale of incremental values from r1 to r5 (you can specify minimum 2 values, maximum 5 in the setbutton call). In this case, the click of the button, in addition to generating the BTN_EVT_CLICK event, also changes the incRate on the linked variable. For example, the setButton(PA2, BTN_SCALE, 1, 10, 100), preceded by a bind call, sets a scale of increments of step value 1, 10, 100 per click (in a circular fashion). This means that by pressing the button connected to PA2, the rotation increasing passes from 1 to 10 and pressing again to 100. Useful if the variation is to operate on large intervals and you want to offer different increment values per detent, making them available with the pressing of the button knob.
- **button()**                            read the button event (btnEvent) and resets button state.
- **buttonAttach(f)**                     attach the user function f to the buttonIsr. The function f will be called at every button click, at the end of isr button routine (after updating states).

---

### Contributing

If you want to contribute to this project:
- Report bugs and errors
- Ask for enhancements
- Create issues and pull requests
- Tell other people about this library
