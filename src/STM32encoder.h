#ifndef STM32encoder_h
#define STM32encoder_h

#include "Arduino.h"
#include "stm32yyxx_hal_conf.h"
#include <cfloat>					// for float and double boundaries
#include "stdint.h"					// for integers boundaries	


#define u8	uint8_t
#define u16	uint16_t
#define u32	uint32_t
#define i8	int8_t
#define i16	int16_t
#define i32	int32_t

//#define ENC_DEBUG					// enable irqtime debug functions

enum eTIMType {
	TIM_MANAGED,
	TIM_FREEWHEEL
};

enum _ebindType {
	BIND_NONE,
	BIND_INT8,
	BIND_UINT8,
	BIND_INT16,
	BIND_UINT16,
	BIND_INT32,
	BIND_UINT32,
	BIND_FLOAT,
	BIND_DOUBLE
};


typedef struct {
	TIM_HandleTypeDef 	htim;
	volatile u32 		oldmillis;
	volatile i32 		pos;
	volatile bool 		dir;
	volatile i16 		speed = 0;
	volatile bool 		isUpdated = false;

	eTIMType 			mode;
	bool				isStarted = false;

	u16 				dynamic = 0;
	u16 				stepLimit = 0;	
	bool				dynamicPos = false;

	_ebindType 			bindType = BIND_NONE;		// bind managementt
	void* 				bind = NULL;
	i16					intBindSteps = 1;			
	float				floatBindSteps = 1.0;
	double				doubleBindSteps = 1.0;

	i32					intMin = 0;					
	i32					intMax = 0;
	u32					uintMin = 0;
	u32					uintMax = 0;
	float				floatMin = 0;
	float				floatMax = 0;
	double				doubleMin = 0;
	double				doubleMax = 0;
	bool				circular	= false;
	void 				(*linked)(void) = NULL;	// attach management
#ifdef ENC_DEBUG
	volatile 			u32 irqtime =0;
#endif
} STM32statusType;


#define ENCT_VERSION	903

class STM32encoder {
	
	public:
	STM32encoder(TIM_TypeDef *Instance, u8 _ICxFilter = 0, u16 _pulseTicks = 3);
	STM32encoder(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter = 0, u16 _pulseTicks = UINT16_MAX);
	//~STM32encoder();  // destructor
	u32     version();
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
	STM32statusType st;
	bool	init(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks);
};
#endif
