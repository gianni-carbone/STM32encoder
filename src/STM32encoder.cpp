#include "STM32encoder.h"

#define TRIM_INT8(v)			v=(v<INT8_MIN)?INT8_MIN:(v>INT8_MAX)?INT8_MAX:v
#define TRIM_INT16(v)			v=(v<INT16_MIN)?INT16_MIN:(v>INT16_MAX)?INT16_MAX:v
#define IS_INCREMENTABLE(c,s,l)	(c<(l-s))
#define IS_DECREMENTABLE(c,s,l)	(c>(l-s))
#define	DO_STEP(c,s,min,max)	if(s>0) {if (IS_INCREMENTABLE(c,s,max)) c+=s; else c=st->circular?min:max;} else {if(IS_DECREMENTABLE(c,s,min)) c+=s; else c=st->circular?max:min;}

// irqtime for STM32F103 @72MHz
// takes aprox 	4uS with integers
//  			10uS with floats and doubles

void _timerIrq(enc_status_t* st){ 

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
			_floatSteps = ((float)_steps * st->floatBindSteps[st->currentScale]);
		else if (st->bindType == BIND_DOUBLE) 
			_doubleSteps = ((double)_steps * st->doubleBindSteps[st->currentScale]);
		else if (st->bindType != BIND_NONE) 
			_intSteps = ((i32)_steps * (i32)st->intBindSteps[st->currentScale]);
		
		switch(st->bindType) {
			case BIND_NONE: break;
			
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
	
	if (st->tickFuncPtr!=NULL) st->tickFuncPtr();										// execute user attached function if any
	
	st->oldmillis = _newmillis;
#ifdef ENC_DEBUG	
	st->irqtime = getCurrentMicros() - st->irqtime;
#endif	
	st->isUpdated = true;
}

void _buttonIrq(enc_status_t* st){ 
	if (st->buttonPin == 0) return;
	
	u32  _newmillis = millis();
	
	if (!((st->buttonGpioPort->IDR) & (st->buttonGpioPin))) {							// pressed. direct read IDR of buttonGpioPort
		
		switch(st->btnFSM){
			case ENC_FSM_RESET: 				
				st->btnFSM = ENC_FSM_PRESS1;
			break;
		}
		st->pressTime = _newmillis;
	
	} else {																			// depressed
		
		switch(st->btnFSM){
			case ENC_FSM_PRESS1:										
				if ((_newmillis - st->pressTime) > BTN_LONG_TIME) {
					st->btnFSM = ENC_FSM_RESET;
					st->btnEvt = BTN_EVT_LONG;
				} else if ((_newmillis - st->pressTime) > BTN_PRESS_TIME) {
					st->btnFSM = ENC_FSM_RESET;
					st->btnEvt = BTN_EVT_CLICK;
					if (st->buttonFunction == BTN_STEP) {								// special function
						if (st->currentScale<(st->scaleSize-1)) 
							st->currentScale++; 
						else 
							st->currentScale=0;
					}
					if (st->buttonFuncPtr!=NULL) st->buttonFuncPtr();					// execute user attached function if any
				} else {
					st->btnFSM = ENC_FSM_RESET;
				}
			break;
		}
		st->depresTime = _newmillis;
	}
}
	

STM32encoder::STM32encoder(TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	st.isStarted = init(ENC_MANAGED, Instance, _ICxFilter, _pulseTicks);
}  

STM32encoder::STM32encoder(enc_mode_t _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks){
	st.isStarted = init(_timMode, Instance, _ICxFilter, _pulseTicks);
}

bool STM32encoder::init(enc_mode_t _timMode, TIM_TypeDef *Instance, u8 _ICxFilter, u16 _pulseTicks) {
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

	if (_timMode == ENC_MANAGED) {
		// due to the HardwareTimer implementation in arduino, the HAL weak void isr is consumed so we have to use HardwareTimer instead
		HardwareTimer *ht = new HardwareTimer(Instance);	// Instantiate HardwareTimer object. Thanks to 'new' instanciation, HardwareTimer is not destructed when function is finished.
		ht->attachInterrupt(std::bind(_timerIrq, &st));		// we use std::bind to pass arguments
	} else if (_timMode == ENC_FREEWHEEL) {
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
	
	if (_timMode == ENC_MANAGED) {
		if(HAL_TIM_Encoder_Start_IT(&st.htim, TIM_CHANNEL_ALL)!=HAL_OK)			
			return false;
	} else if (_timMode == ENC_FREEWHEEL) {
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

bool STM32encoder::setButton(u32 _p, btn_function_t _f, float _v0, float _v1, float _v2, float _v3, float _v4) {
	
	if (!_p) return false;
	
	if (_f == BTN_STEP) {
		if (st.bindType == BIND_NONE) {
			return false;
		} else if (st.bindType == BIND_FLOAT) {
			if (_v4 != 0.0) st.floatBindSteps[4] = _v4;
			if (_v3 != 0.0) st.floatBindSteps[3] = _v3;
			if (_v2 != 0.0) st.floatBindSteps[2] = _v2;
			if (_v1 != 0.0) st.floatBindSteps[1] = _v1;
			if (_v0 != 0.0) st.floatBindSteps[0] = _v0;
		} else if (st.bindType == BIND_DOUBLE) {
			if (_v4 != 0.0) st.doubleBindSteps[4] = _v4;
			if (_v3 != 0.0) st.doubleBindSteps[3] = _v3;
			if (_v2 != 0.0) st.doubleBindSteps[2] = _v2;
			if (_v1 != 0.0) st.doubleBindSteps[1] = _v1;
			if (_v0 != 0.0) st.doubleBindSteps[0] = _v0;
		} else {
			if (_v4 != 0.0) st.intBindSteps[4] = _v4;
			if (_v3 != 0.0) st.intBindSteps[3] = _v3;
			if (_v2 != 0.0) st.intBindSteps[2] = _v2;
			if (_v1 != 0.0) st.intBindSteps[1] = _v1;
			if (_v0 != 0.0) st.intBindSteps[0] = _v0;
		}
		
		if (_v4 != 0.0) st.scaleSize = 5;
		else if (_v3 != 0.0) st.scaleSize = 4;
		else if (_v2 != 0.0) st.scaleSize = 3;
		else if (_v1 != 0.0) st.scaleSize = 2;
		else if (_v0 != 0.0) st.scaleSize = 1;
		st.currentScale = 0;
	}
	
	
	if (st.buttonPin) detachInterrupt(st.buttonPin);
	st.buttonPin = _p;
	pinMode(st.buttonPin, INPUT_PULLUP);
	
	PinName pn = digitalPinToPinName(st.buttonPin);
	st.buttonGpioPort = get_GPIO_Port(STM_PORT(pn));
	st.buttonGpioPin = STM_GPIO_PIN(pn);
	attachInterrupt(st.buttonPin, std::bind(_buttonIrq, &st), CHANGE);
	st.buttonFunction = _f;
	
	return true;
}

enc_events_t STM32encoder::button(void){
	enc_events_t e = st.btnEvt;
	if (e != BTN_EVT_NONE) {
		st.btnFSM = ENC_FSM_RESET;
		st.btnEvt = BTN_EVT_NONE;
		st.pressTime = 0;
		st.depresTime = 0;
	}
	return e;
}

u8	STM32encoder::scaleId(void){
	return st.currentScale;
}

void STM32encoder::scaleId(u8 _s){
	if (_s<=(st.scaleSize-1))
		st.currentScale = _s;
	else 
		st.currentScale = st.scaleSize-1;
}

u8	STM32encoder::scaleSize(void){
	return st.scaleSize;
}

bool	STM32encoder::attachButton(void (*_f)(void)) {
	if (st.buttonPin == 0) return false;
	st.buttonFuncPtr = _f;
	return true;
}

u32	STM32encoder::version(void) {
	return  ENCT_VERSION;
}

bool STM32encoder::isStarted(void) {
	return  st.isStarted;
}

bool STM32encoder::isUpdated(void) {
	bool _u=st.isUpdated; 
	st.isUpdated = false; 
	return  _u;
}

i32 STM32encoder::pos(void) {
	if (st.mode == ENC_MANAGED) {
		return  st.pos;
	} else if (st.mode == ENC_FREEWHEEL) {
		return __HAL_TIM_GET_COUNTER(&st.htim);
	} else return 0;
}

void STM32encoder::pos(i32 _p) {
	if (st.mode == ENC_MANAGED) {
		st.pos = _p;
	} else if (st.mode == ENC_FREEWHEEL) {
		if (_p<0) 
			_p=0;
		else if (_p>UINT16_MAX)
			_p=UINT16_MAX;
		__HAL_TIM_SET_COUNTER(&st.htim,_p);
	} 
}

bool 	STM32encoder::dir() {
	if (st.mode == ENC_MANAGED) {
		return !st.dir;
	} else if (st.mode == ENC_FREEWHEEL) {
		return !__HAL_TIM_IS_TIM_COUNTING_DOWN(&st.htim);
	} else return true;
}

i16 	STM32encoder::speed(void) {return  st.speed;}

void 	STM32encoder::dynamic(u16 _s, u16 _l, bool _dp) {
	st.dynamic = _s; 
	if (_l<1) {
		st.stepLimit = 1;
	} else if(_l>1000) {
		st.stepLimit = 1000;
	} else 
		st.stepLimit = _l;
	st.dynamicPos = _dp;
}

u16 	STM32encoder::dynamic() {return st.dynamic;}

void 	STM32encoder::circular(bool _c) {
	if (st.mode != ENC_MANAGED) return; 
	st.circular = _c;
}

bool 	STM32encoder::circular(void) {
	if (st.mode == ENC_MANAGED) {
		return st.circular;
	} else if (st.mode == ENC_FREEWHEEL) {
		return true;
	} else return true;
}

void 	STM32encoder::attach(void (*_f)(void)) {
	if (st.mode != ENC_MANAGED) return; 
	st.tickFuncPtr = _f;
}

void 	STM32encoder::detach(void) {st.tickFuncPtr = NULL;}
	
void STM32encoder::unbind() {	
	st.bindType = BIND_NONE;
	st.bind = NULL;
}

void STM32encoder::bind(i8* _p, i16 _s, i8 _min, i8 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT8;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32encoder::bind(u8* _p, i16 _s, u8 _min, u8 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT8;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32encoder::bind(i16* _p, i16 _s, i16 _min, i16 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT16;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32encoder::bind(u16* _p, i16 _s, u16 _min, u16 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT16;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32encoder::bind(i32* _p, i16 _s, i32 _min, i32 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_INT32;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.intMin = _min;
		st.intMax = _max;
	}
}

void STM32encoder::bind(u32* _p, i16 _s, u32 _min, u32 _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_UINT32;
		st.bind = _p;
		st.intBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.uintMin = _min;
		st.uintMax = _max;
	}
}

void STM32encoder::bind(float_t* _p, float _s, float _min, float _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_FLOAT;
		st.bind = _p;
		st.floatBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.floatMin = _min;
		st.floatMax = _max;
	}
}

void STM32encoder::bind(double_t* _p, double _s, double _min, double _max) {
	if (st.mode != ENC_MANAGED) return;
	if (_p!=NULL) {
		st.bindType = BIND_DOUBLE;
		st.bind = _p;
		st.doubleBindSteps[0] = _s;
		st.scaleSize = 1;
		st.currentScale = 0;
		st.doubleMin = _min;
		st.doubleMax = _max;
	}
}

#ifdef ENC_DEBUG
u32 	STM32encoder::irqtime(void) {return  st.irqtime;}
#endif
