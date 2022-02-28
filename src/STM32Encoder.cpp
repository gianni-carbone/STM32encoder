#include "STM32Encoder.h"

TIM_HandleTypeDef htim;

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

volatile u32 	_oldmillis;
volatile i32 	_pos;
volatile bool 	_dir;
volatile i16 	_speed = 0;
volatile bool 	_isUpdated = false;

eTIMType 		_mode;
bool			_isStarted = false;

u16 	_dynamic = 0;
u16 	_stepLimit = 0;	
bool	_dynamicPos = false;

_ebindType 	_bindType = BIND_NONE;		// bind managementt
void* 	_bind = NULL;
i16		_intBindSteps = 1;			
float	_floatBindSteps = 1.0;
double	_doubleBindSteps = 1.0;

i32		_intMin = 0;					
i32		_intMax = 0;
u32		_uintMin = 0;
u32		_uintMax = 0;
float	_floatMin = 0;
float	_floatMax = 0;
double	_doubleMin = 0;
double	_doubleMax = 0;
bool	_circular	= false;
void 	(*_linked)(void) = NULL;	// attach management

#ifdef ENC_DEBUG
	volatile u32 _irqtime =0;
#endif

#define TRIM_INT8(v)			v=(v<INT8_MIN)?INT8_MIN:(v>INT8_MAX)?INT8_MAX:v
#define TRIM_INT16(v)			v=(v<INT16_MIN)?INT16_MIN:(v>INT16_MAX)?INT16_MAX:v
#define IS_INCREMENTABLE(c,s,l)	(c<(l-s))
#define IS_DECREMENTABLE(c,s,l)	(c>(l-s))
#define	DO_STEP(c,s,min,max)	if(s>0) {if (IS_INCREMENTABLE(c,s,max)) c+=s; else c=_circular?min:max;} else {if(IS_DECREMENTABLE(c,s,min)) c+=s; else c=_circular?max:min;}

// takes aprox 	4uS with integers
//  			12uS with floats and doubles
//		TODO: recalc value at release

void _timerIrq(void){ 

#ifdef ENC_DEBUG	
	_irqtime = getCurrentMicros();
#endif

	_dir = __HAL_TIM_IS_TIM_COUNTING_DOWN(&htim);
	
	i16	_steps = 1;
	
	u32 _newmillis = millis();
	
	if (_newmillis==_oldmillis) {
		_speed = 1000;
	} else {
		_speed = 1000 / (_newmillis-_oldmillis);	
	}
	
	if (_dynamic != 0) {
		_steps = ((_speed*_dynamic)/100);
		if (_steps == 0) {
			_steps = 1;
		} else {
			if ((_stepLimit!=0) && (_steps>_stepLimit)) _steps=_stepLimit;
		}
	}
	
	_steps = _dir?-_steps:_steps;
	_pos += _dynamicPos?_steps:(_dir?-1:1);
	
	if (_bindType!=BIND_NONE) {
		i32 _intSteps;
		float _floatSteps;
		double _doubleSteps;
		
		if (_bindType == BIND_FLOAT) 
			_floatSteps = ((float)_steps * _floatBindSteps);
		else if (_bindType == BIND_DOUBLE) 
			_doubleSteps = ((double)_steps * _doubleBindSteps);
		else
			_intSteps = ((i32)_steps * (i32)_intBindSteps);
		
		switch(_bindType) {
			case BIND_INT8: 
				TRIM_INT8(_intSteps);
				DO_STEP(*(i8*)_bind, _intSteps, _intMin, _intMax)
			break;
			
			case BIND_UINT8: 
				TRIM_INT8(_intSteps);
				DO_STEP(*(u8*)_bind, _intSteps, _uintMin, _uintMax)
			break;
			
			case BIND_INT16: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(i16*)_bind, _intSteps, _intMin, _intMax)
			break;
			
			case BIND_UINT16: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(u16*)_bind, _intSteps, _uintMin, _uintMax)
			break;
			
			case BIND_INT32: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(i32*)_bind, _intSteps, _intMin, _intMax)
			break;
			
			case BIND_UINT32: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(u32*)_bind, _intSteps, _uintMin, _uintMax)
			break;
			
			case BIND_FLOAT: 
				DO_STEP(*(float*)_bind, _floatSteps, _floatMin, _floatMax)
			break;
			
			case BIND_DOUBLE: 
				DO_STEP(*(double*)_bind, _doubleSteps, _doubleMin, _doubleMax)
			break;
		}
	}
	
	if (_linked!=NULL) _linked();
	
	_oldmillis = _newmillis;
#ifdef ENC_DEBUG	
	_irqtime = getCurrentMicros() - _irqtime;
#endif	
	_isUpdated = true;
}


STM32Encoder::STM32Encoder(TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	_isStarted = init(TIM_MANAGED, Instance, _ICxFilter, _pulseTicks);
}  

STM32Encoder::STM32Encoder(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	_isStarted = init(_timMode, Instance, _ICxFilter, _pulseTicks);
}

bool	STM32Encoder::init(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks) {
	if (!IS_TIM_IC_FILTER(_ICxFilter)) _ICxFilter = 0x00;							// check filter is valid

	TIM_Encoder_InitTypeDef sConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	
	htim.Instance = Instance;
	htim.Init.Prescaler = 0;
	htim.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim.Init.Period = _pulseTicks; 															// generate an irq every 4 ticks
	htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
	sConfig.IC1Polarity = TIM_ICPOLARITY_FALLING;
	sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC1Filter = _ICxFilter;
	sConfig.IC2Polarity = TIM_ICPOLARITY_FALLING;
	sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC2Filter = _ICxFilter;
	
	HAL_TIM_Encoder_MspInit(&htim);

	if (_timMode == TIM_MANAGED) {
		/*
			HardwareTimer *ht = new HardwareTimer(Instance);											// Instantiate HardwareTimer object. Thanks to 'new' instanciation, HardwareTimer is not destructed when function is finished.
			ht->attachInterrupt(std::bind(_revIRQUpdate, &_timer, &_pos, &_prevmillis, &_speed));		// we use std::bind to pass arguments
		*/	
		// due to the HardwareTimer implementation in arduino, the HAL weak void isr is consumed so we have to use HardwareTimer instead
		HardwareTimer *ht = new HardwareTimer(Instance);						// Instantiate HardwareTimer object. Thanks to 'new' instanciation, HardwareTimer is not destructed when function is finished.
		ht->attachInterrupt(_timerIrq);
	} else if (_timMode == TIM_FREEWHEEL) {
#ifdef TIM1
		if (Instance == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
#endif
#ifdef TIM2
		if (Instance == TIM2) __HAL_RCC_TIM2_CLK_ENABLE();
#endif
#ifdef TIM3
		if (Instance == TIM3) __HAL_RCC_TIM3_CLK_ENABLE();
#endif
#ifdef TIM4
		if (Instance == TIM4) __HAL_RCC_TIM4_CLK_ENABLE();
#endif
#ifdef TIM5
		if (Instance == TIM5) __HAL_RCC_TIM5_CLK_ENABLE();
#endif
#ifdef TIM6
		if (Instance == TIM6) __HAL_RCC_TIM6_CLK_ENABLE();
#endif
#ifdef TIM7
		if (Instance == TIM7) __HAL_RCC_TIM7_CLK_ENABLE();
#endif
#ifdef TIM8
		if (Instance == TIM8) __HAL_RCC_TIM8_CLK_ENABLE();
#endif
#ifdef TIM9
		if (Instance == TIM9) __HAL_RCC_TIM9_CLK_ENABLE();
#endif
#ifdef TIM10
		if (Instance == TIM10) __HAL_RCC_TIM10_CLK_ENABLE();
#endif
#ifdef TIM11
		if (Instance == TIM11) __HAL_RCC_TIM11_CLK_ENABLE();
#endif
#ifdef TIM12
		if (Instance == TIM12) __HAL_RCC_TIM12_CLK_ENABLE();
#endif
#ifdef TIM13
		if (Instance == TIM13) __HAL_RCC_TIM13_CLK_ENABLE();
#endif
#ifdef TIM14
		if (Instance == TIM14) __HAL_RCC_TIM14_CLK_ENABLE();
#endif
	}
	
	if (HAL_TIM_Encoder_Init(&htim, &sConfig) != HAL_OK)					
	  return false;

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim, &sMasterConfig) != HAL_OK)
		return false;
	
	if (_timMode == TIM_MANAGED) {
		if(HAL_TIM_Encoder_Start_IT(&htim, TIM_CHANNEL_ALL)!=HAL_OK)			
			return false;
	} else if (_timMode == TIM_FREEWHEEL) {
		if(HAL_TIM_Encoder_Start(&htim, TIM_CHANNEL_1)!=HAL_OK)			
			return false;
		__HAL_TIM_SET_COUNTER(&htim,0);
	} else return false;

	_pos = 0;
	_oldmillis = 0;
	_dir = false;
	_mode = _timMode;
	
	return true;
}


// main functions
bool	STM32Encoder::isStarted(void) {
	return  _isStarted;
}

bool	STM32Encoder::isUpdated(void) {
	bool _u=_isUpdated; 
	_isUpdated = false; 
	return  _u;
}

i32 	STM32Encoder::pos(void) {
	if (_mode == TIM_MANAGED) {
		return  _pos;
	} else if (_mode == TIM_FREEWHEEL) {
		return __HAL_TIM_GET_COUNTER(&htim);
	} else return -1;
}

void 	STM32Encoder::pos(i32 _p) {
	if (_mode == TIM_MANAGED) {
		_pos = _p;
	} else if (_mode == TIM_FREEWHEEL) {
		if (_p<0) 
			_p=0;
		else if (_p>UINT16_MAX)
			_p=UINT16_MAX;
		__HAL_TIM_SET_COUNTER(&htim,_p);
	} 
}

bool 	STM32Encoder::dir() {
	if (_mode == TIM_MANAGED) {
		return !_dir;
	} else if (_mode == TIM_FREEWHEEL) {
		return !__HAL_TIM_IS_TIM_COUNTING_DOWN(&htim);
	} else return true;
}

i16 	STM32Encoder::speed(void) {return  _speed;}

void 	STM32Encoder::dynamic(u16 _s, u16 _l, bool _dp) {
	_dynamic = _s; 
	if (_l<1) {
		_stepLimit = 1;
	} else if(_l>1000) {
		_stepLimit = 1000;
	} else 
		_stepLimit = _l;
	_dynamicPos = _dp;
}

u16 	STM32Encoder::dynamic() {return _dynamic;}

void 	STM32Encoder::circular(bool _c) {
	if (_mode != TIM_MANAGED) return; 
	_circular = _c;
}

bool 	STM32Encoder::circular(void) {
	if (_mode == TIM_MANAGED) {
		return _circular;
	} else if (_mode == TIM_FREEWHEEL) {
		return true;
	} else return true;
}

void 	STM32Encoder::attach(void (*_f)(void)) {
	if (_mode != TIM_MANAGED) return; 
	_linked = _f;
}

void 	STM32Encoder::detach(void) {_linked = NULL;}
	
void STM32Encoder::unbind() {	
	_bindType = BIND_NONE;
	_bind = NULL;
}

void STM32Encoder::bind(i8* _p, i16 _s, i8 _min, i8 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_INT8;
		_bind = _p;
		_intBindSteps = _s;
		_intMin = _min;
		_intMax = _max;
	}
}

void STM32Encoder::bind(u8* _p, i16 _s, u8 _min, u8 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_UINT8;
		_bind = _p;
		_intBindSteps = _s;
		_uintMin = _min;
		_uintMax = _max;
	}
}

void STM32Encoder::bind(i16* _p, i16 _s, i16 _min, i16 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_INT16;
		_bind = _p;
		_intBindSteps = _s;
		_intMin = _min;
		_intMax = _max;
	}
}

void STM32Encoder::bind(u16* _p, i16 _s, u16 _min, u16 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_UINT16;
		_bind = _p;
		_intBindSteps = _s;
		_uintMin = _min;
		_uintMax = _max;
	}
}

void STM32Encoder::bind(i32* _p, i16 _s, i32 _min, i32 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_INT32;
		_bind = _p;
		_intBindSteps = _s;
		_intMin = _min;
		_intMax = _max;
	}
}

void STM32Encoder::bind(u32* _p, i16 _s, u32 _min, u32 _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_UINT32;
		_bind = _p;
		_intBindSteps = _s;
		_uintMin = _min;
		_uintMax = _max;
	}
}

void STM32Encoder::bind(float_t* _p, float _s, float _min, float _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_FLOAT;
		_bind = _p;
		_floatBindSteps = _s;
		_floatMin = _min;
		_floatMax = _max;
	}
}

void STM32Encoder::bind(double_t* _p, double _s, double _min, double _max) {
	if (_mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		_bindType = BIND_DOUBLE;
		_bind = _p;
		_doubleBindSteps = _s;
		_doubleMin = _min;
		_doubleMax = _max;
	}
}

#ifdef ENC_DEBUG
u32 	STM32Encoder::irqtime(void) {return  _irqtime;}
#endif
