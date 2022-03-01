#include "STM32Encoder.h"

#define TRIM_INT8(v)			v=(v<INT8_MIN)?INT8_MIN:(v>INT8_MAX)?INT8_MAX:v
#define TRIM_INT16(v)			v=(v<INT16_MIN)?INT16_MIN:(v>INT16_MAX)?INT16_MAX:v
#define IS_INCREMENTABLE(c,s,l)	(c<(l-s))
#define IS_DECREMENTABLE(c,s,l)	(c>(l-s))
#define	DO_STEP(c,s,min,max)	if(s>0) {if (IS_INCREMENTABLE(c,s,max)) c+=s; else c=st->circular?min:max;} else {if(IS_DECREMENTABLE(c,s,min)) c+=s; else c=st->circular?max:min;}

// irqtime for STM32F103 @72MHz
// takes aprox 	4uS with integers
//  			10uS with floats and doubles

void _timerIrq(STM32statusType* st){ 

#ifdef ENC_DEBUG	
	st->irqtime = getCurrentMicros();
#endif

	st->dir = __HAL_TIM_IS_TIM_COUNTING_DOWN(&st->htim);
	
	i16	_steps = 1;
	
	u32 _newmillis = millis();
	
	if (_newmillis==st->oldmillis) {
		st->speed = 1000;
	} else {
		st->speed = 1000 / (_newmillis-st->oldmillis);	
	}
	
	if (st->dynamic != 0) {
		_steps = ((st->speed*st->dynamic)/100);
		if (_steps == 0) {
			_steps = 1;
		} else {
			if ((st->stepLimit!=0) && (_steps>st->stepLimit)) _steps=st->stepLimit;
		}
	}
	
	_steps = st->dir?-_steps:_steps;
	st->pos += st->dynamicPos?_steps:(st->dir?-1:1);
	
	if (st->bindType!=BIND_NONE) {
		i32 _intSteps;
		float _floatSteps;
		double _doubleSteps;
		
		if (st->bindType == BIND_FLOAT) 
			_floatSteps = ((float)_steps * st->floatBindSteps);
		else if (st->bindType == BIND_DOUBLE) 
			_doubleSteps = ((double)_steps * st->doubleBindSteps);
		else
			_intSteps = ((i32)_steps * (i32)st->intBindSteps);
		
		switch(st->bindType) {
			case BIND_INT8: 
				TRIM_INT8(_intSteps);
				DO_STEP(*(i8*)st->bind, _intSteps, st->intMin, st->intMax)
			break;
			
			case BIND_UINT8: 
				TRIM_INT8(_intSteps);
				DO_STEP(*(u8*)st->bind, _intSteps, st->uintMin, st->uintMax)
			break;
			
			case BIND_INT16: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(i16*)st->bind, _intSteps, st->intMin, st->intMax)
			break;
			
			case BIND_UINT16: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(u16*)st->bind, _intSteps, st->uintMin, st->uintMax)
			break;
			
			case BIND_INT32: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(i32*)st->bind, _intSteps, st->intMin, st->intMax)
			break;
			
			case BIND_UINT32: 
				TRIM_INT16(_intSteps);
				DO_STEP(*(u32*)st->bind, _intSteps, st->uintMin, st->uintMax)
			break;
			
			case BIND_FLOAT: 
				DO_STEP(*(float*)st->bind, _floatSteps, st->floatMin, st->floatMax)
			break;
			
			case BIND_DOUBLE: 
				DO_STEP(*(double*)st->bind, _doubleSteps, st->doubleMin, st->doubleMax)
			break;
		}
	}
	
	if (st->linked!=NULL) st->linked();
	
	st->oldmillis = _newmillis;
#ifdef ENC_DEBUG	
	st->irqtime = getCurrentMicros() - st->irqtime;
#endif	
	st->isUpdated = true;
}


STM32Encoder::STM32Encoder(TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	st.isStarted = init(TIM_MANAGED, Instance, _ICxFilter, _pulseTicks);
}  

STM32Encoder::STM32Encoder(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	st.isStarted = init(_timMode, Instance, _ICxFilter, _pulseTicks);
}

bool	STM32Encoder::init(eTIMType _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks) {
	if (!IS_TIM_IC_FILTER(_ICxFilter)) _ICxFilter = 0x00;							// check filter is valid

	TIM_Encoder_InitTypeDef sConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	
	st.htim.Instance = Instance;
	st.htim.Init.Prescaler = 0;
	st.htim.Init.CounterMode = TIM_COUNTERMODE_UP;
	st.htim.Init.Period = _pulseTicks; 															// generate an irq every 4 ticks
	st.htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	st.htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
	sConfig.IC1Polarity = TIM_ICPOLARITY_FALLING;
	sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC1Filter = _ICxFilter;
	sConfig.IC2Polarity = TIM_ICPOLARITY_FALLING;
	sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC2Filter = _ICxFilter;
	
	HAL_TIM_Encoder_MspInit(&st.htim);

	if (_timMode == TIM_MANAGED) {
		// due to the HardwareTimer implementation in arduino, the HAL weak void isr is consumed so we have to use HardwareTimer instead
		HardwareTimer *ht = new HardwareTimer(Instance);	// Instantiate HardwareTimer object. Thanks to 'new' instanciation, HardwareTimer is not destructed when function is finished.
		ht->attachInterrupt(std::bind(_timerIrq, &st));		// we use std::bind to pass arguments
/*		
		HardwareTimer *ht = new HardwareTimer(Instance);						// Instantiate HardwareTimer object. Thanks to 'new' instanciation, HardwareTimer is not destructed when function is finished.
		ht->attachInterrupt(_timerIrq);
*/
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
	
	if (HAL_TIM_Encoder_Init(&st.htim, &sConfig) != HAL_OK)					
	  return false;

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&st.htim, &sMasterConfig) != HAL_OK)
		return false;
	
	if (_timMode == TIM_MANAGED) {
		if(HAL_TIM_Encoder_Start_IT(&st.htim, TIM_CHANNEL_ALL)!=HAL_OK)			
			return false;
	} else if (_timMode == TIM_FREEWHEEL) {
		if(HAL_TIM_Encoder_Start(&st.htim, TIM_CHANNEL_1)!=HAL_OK)			
			return false;
		__HAL_TIM_SET_COUNTER(&st.htim,0);
	} else return false;

	st.pos = 0;
	st.oldmillis = 0;
	st.dir = false;
	st.mode = _timMode;
	
	return true;
}


// main functions
u32	STM32Encoder::version(void) {
	return  ENCT_VERSION;
}

bool	STM32Encoder::isStarted(void) {
	return  st.isStarted;
}

bool	STM32Encoder::isUpdated(void) {
	bool _u=st.isUpdated; 
	st.isUpdated = false; 
	return  _u;
}

i32 	STM32Encoder::pos(void) {
	if (st.mode == TIM_MANAGED) {
		return  st.pos;
	} else if (st.mode == TIM_FREEWHEEL) {
		return __HAL_TIM_GET_COUNTER(&st.htim);
	} else return 0;
}

void 	STM32Encoder::pos(i32 _p) {
	if (st.mode == TIM_MANAGED) {
		st.pos = _p;
	} else if (st.mode == TIM_FREEWHEEL) {
		if (_p<0) 
			_p=0;
		else if (_p>UINT16_MAX)
			_p=UINT16_MAX;
		__HAL_TIM_SET_COUNTER(&st.htim,_p);
	} 
}

bool 	STM32Encoder::dir() {
	if (st.mode == TIM_MANAGED) {
		return !st.dir;
	} else if (st.mode == TIM_FREEWHEEL) {
		return !__HAL_TIM_IS_TIM_COUNTING_DOWN(&st.htim);
	} else return true;
}

i16 	STM32Encoder::speed(void) {return  st.speed;}

void 	STM32Encoder::dynamic(u16 _s, u16 _l, bool _dp) {
	st.dynamic = _s; 
	if (_l<1) {
		st.stepLimit = 1;
	} else if(_l>1000) {
		st.stepLimit = 1000;
	} else 
		st.stepLimit = _l;
	st.dynamicPos = _dp;
}

u16 	STM32Encoder::dynamic() {return st.dynamic;}

void 	STM32Encoder::circular(bool _c) {
	if (st.mode != TIM_MANAGED) return; 
	st.circular = _c;
}

bool 	STM32Encoder::circular(void) {
	if (st.mode == TIM_MANAGED) {
		return st.circular;
	} else if (st.mode == TIM_FREEWHEEL) {
		return true;
	} else return true;
}

void 	STM32Encoder::attach(void (*_f)(void)) {
	if (st.mode != TIM_MANAGED) return; 
	st.linked = _f;
}

void 	STM32Encoder::detach(void) {st.linked = NULL;}
	
void STM32Encoder::unbind() {	
	st.bindType = BIND_NONE;
	st.bind = NULL;
}

void STM32Encoder::bind(i8* _p, i16 _s, i8 _min, i8 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT8;
		st.bind = _p;
		st.intBindSteps = _s;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32Encoder::bind(u8* _p, i16 _s, u8 _min, u8 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT8;
		st.bind = _p;
		st.intBindSteps = _s;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32Encoder::bind(i16* _p, i16 _s, i16 _min, i16 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT16;
		st.bind = _p;
		st.intBindSteps = _s;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32Encoder::bind(u16* _p, i16 _s, u16 _min, u16 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT16;
		st.bind = _p;
		st.intBindSteps = _s;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32Encoder::bind(i32* _p, i16 _s, i32 _min, i32 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT32;
		st.bind = _p;
		st.intBindSteps = _s;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32Encoder::bind(u32* _p, i16 _s, u32 _min, u32 _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT32;
		st.bind = _p;
		st.intBindSteps = _s;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32Encoder::bind(float_t* _p, float _s, float _min, float _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_FLOAT;
		st.bind = _p;
		st.floatBindSteps = _s;
		st.floatMin = _min;
		st.floatMax = _max;
	}
}

void STM32Encoder::bind(double_t* _p, double _s, double _min, double _max) {
	if (st.mode != TIM_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_DOUBLE;
		st.bind = _p;
		st.doubleBindSteps = _s;
		st.doubleMin = _min;
		st.doubleMax = _max;
	}
}

#ifdef ENC_DEBUG
u32 	STM32Encoder::irqtime(void) {return  st.irqtime;}
#endif
