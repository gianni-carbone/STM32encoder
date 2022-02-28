#ifndef STM32Encoder_h
#define STM32Encoder_h

#define u8	uint8_t
#define u16	uint16_t
#define u32	uint32_t
#define i8	int8_t
#define i16	int16_t
#define i32	int32_t

enum eTIMType {
	TIM_MANAGED,
	TIM_FREEWHEEL
};


#include "Arduino.h"
#include "stm32yyxx_hal_conf.h"
#include <cfloat>					// for float and double boundaries
#include "stdint.h"					// for integers boundaries	

#define ENCT_VERSION	0x09
//#define ENC_DEBUG					// enable irqtime debug functions

class STM32Encoder {
	
	public:
	STM32Encoder(TIM_TypeDef *Instance, u8 _ICxFilter = 0, u16 _pulseTicks = 3);
	STM32Encoder(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter = 0, u16 _pulseTicks = UINT16_MAX);
	//~STM32Encoder();  // destructor
	bool	isStarted(void);								// return is started. If false then error occurs during inizialization
	bool	isUpdated(void);								// return a tick is done since last call. Function call resets the flag
	i32		pos(void);										// get the current absolute position 
	void 	pos(i32 _p);									// set the absolute position to the given value
	bool	dir(void);										// get the (last) direction of rotation		
	i16 	speed(void);									// get the (last) speed of revolution as ticks per second
	void 	circular(bool _c);								// set circular mode
	bool 	circular(void);									// get the circular mode
	
	void 	dynamic(u16 _s, u16 _l = 0, bool _dp = true);	// set the speed factor [step limit and dynamic pos]. Speed factor affects increment rate [and position counter] as (_speed*_dynamic)/100 [with limit]. Dynamic(0) = disable the feature
	u16 	dynamic(void);									// get the speed factor
	u16 	speedLimit();									// get the speed limit	

	void 	attach(void (*_f)(void));						// attach the given function to the isr (The function will be executed at the end of interrupt routine)
	void 	detach(void);									// detach the function
	
	void 	bind(i8* _p, i16 _s = 1, i8 _min = INT8_MIN, i8 _max = INT8_MAX);			// binds the given variable to the isr (variable management). The variable will be incremented or decremented in the isr routine by _s steps per tick. Affected by dynamic.
	void 	bind(u8* _p, i16 _s = 1, u8 _min = 0, u8 _max = UINT8_MAX);
	void 	bind(i16* _p, i16 _s = 1, i16 _min = INT16_MIN, i16 _max = INT16_MAX);
	void 	bind(u16* _p, i16 _s = 1, u16 _min = 0, u16 _max = UINT16_MAX);
	void 	bind(i32* _p, i16 _s = 1, i32 _min = INT32_MIN, i32 _max = INT32_MAX);
	void 	bind(u32* _p, i16 _s = 1, u32 _min = 0, u32 _max = UINT32_MAX);
	void 	bind(float* _p, float _s = 1.0, float _min = FLT_MIN, float _max = FLT_MAX);
	void 	bind(double* _p, double _s = 1.0, double _min = DBL_MIN, double _max = DBL_MAX);
	void 	unbind();									// unbind the variable

#ifdef ENC_DEBUG
	u32 irqtime(void);
#endif

	private:
	bool	init(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks);
};
#endif