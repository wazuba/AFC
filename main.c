/**********************************************************************
* ETM Electromatic Inc.
* 
* FileName:        main.c
* Dependencies:    Header (.h) files if applicable, see below
* Processor:       dsPIC30F2023
* Compiler:        MPLAB® C30 v3.00 or higher
*
* REVISION HISTORY:
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Author            Date      Comments on this revision
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Jason Henry	  06/29/12  	Customer R LINAC First Release
*                             
*                             
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*
* NOTES:
*	This program utilizes the following:
*
**********************************************************************/
#include "stepper.h"
#include <dsp.h>

_FOSCSEL(FRC_PLL);							/* Internal FRC oscillator with PLL */
_FOSC(CSW_ON_FSCM_OFF & FRC_HI_RANGE);      /* Set up for internal fast RC 14.55MHz clock multiplied by X32 PLL  FOSC = 14.55e6*32/8 = 58.2MHz FCY = FOSC/2 = 29.1MHz*/
_FGS(CODE_PROT_OFF);              			/* Disable Code Protection */
_FICD(ICS_PGD);					  			/* Enable Primary ICP pins */
_FWDT(FWDTEN_ON);				  			/* Enable Watch Dog */
_FPOR(PWRT_128);

int state;
unsigned int IMotor1;
unsigned int IMotor2;			  
int warmUpComplete;
int autoControl;
int manualControl;
unsigned int homeAddress;
int triggerINT;
int motorOrDelay;
int setDir;
int stepSize;
unsigned int motorStep;
unsigned int motorPos; 
unsigned int motorPosAddress;
int errorArray[BUFFER_SIZE];
int strPTR;
int endPTR;
int bufferFull;
int error;
unsigned int sampleTrigger;
faultFlags faultStatus;
int sigma;
int delta;
unsigned int homePos;
unsigned int count;
int heatPerPulse;
int prevReflectedPower;
int reflectedPower;
int thermalError;
int coolRate;
unsigned int thermalCounter;
unsigned int prevThermalCounter;
int target;
unsigned long heatAccumulator;
int counterA;
int counterB;
 
/*
Variable Declaration required for each PID controller in your application
*/
/* Declare a PID Data Structure named, fooPID */
tPID fooPID;
/* The fooPID data structure contains a pointer to derived coefficients in X-space and */
/* pointer to controler state (history) samples in Y-space. So declare variables for the */
/* derived coefficients and the controller history samples */
fractional abcCoefficient[3] __attribute__ ((section (".xbss, bss, xmemory")));
fractional controlHistory[3] __attribute__ ((section (".ybss, bss, ymemory")));
/* The abcCoefficients referenced by the fooPID data structure */
/* are derived from the gain coefficients, Kp, Ki and Kd */
/* So, declare Kp, Ki and Kd in an array */
fractional kCoeffs[] = {0,0,0};

/*----------------------------------------------------------------------------*/



int main (void)
{
	
	while(OSCCONbits.LOCK!=1);	/* Wait for PLL to lock */


	__delay32(EEPROM_DELAY*10);	/* Wait for EEPROMs to settle */
	
	__delay32(30000000);
    state = STATE_INIT;

	while(1)
	{
		CLRWDT()
		stateMachine();	/* call state machine */
		  	
	}
	
	

}


/******************************************************************************
* Function:     stateMachine()
*
* Output:		None
*
* Overview:		The state machine controls the AFC states: INIT, WARM_UP, MAN, AFC and FAULT
*               Transitions between states are processed here
*
* Note:			None
*******************************************************************************/

void stateMachine(void)
{
	
	/******************************************************************************
	* INIT State
	* Executes at boot up or when the processor is reset.
	*
	******************************************************************************/
	if (state == STATE_INIT)
	{
		initPeripherals();
		initInterrupts();
		
		unsigned int checkRead;
	
		checkRead = M24LC64FReadWord(CHECK_ADDRESS, M24LC64F_ADDRESS_0);
		__delay32(EEPROM_DELAY*10);
		
		if (checkRead)
		{
			homePos = 0;	
			M24LC64FWriteWord(HOME_ADDRESS, homePos ,M24LC64F_ADDRESS_0);
			__delay32(EEPROM_DELAY*10);
			M24LC64FWriteWord(CHECK_ADDRESS, homePos ,M24LC64F_ADDRESS_0);
			__delay32(EEPROM_DELAY*10);
		}
		
		homePos = M24LC64FReadWord(HOME_ADDRESS, M24LC64F_ADDRESS_0);			/* Retrieve the stored delay value */
		//homePos = 0;
		__delay32(EEPROM_DELAY*10);
		motorPos = M24LC64FReadWord(MOTOR_POS_ADDRESS, M24LC64F_ADDRESS_0);			/* Retrieve the stored Motor Position value */
		//motorPos = 0;
		__delay32(EEPROM_DELAY*10);
		initPWM();
		initADC();
		initTMR();
		indexMotor(homePos, MAX_STEPS); //MAX_STEPS is 250, moveMotor() steps 4 motor steps at a time.
	}
	/* End of INIT State*/
	
	/******************************************************************************
	* WARM_UP State
	* As long as the warm up complete signal is not given the AFC will remain in
	* this state.
	******************************************************************************/
	
	/*if (state == STATE_WARM_UP)
	{
		triggerINT = disableINT0();
		//updateAnalogOut(homePos);
		bufferFull = 0;
		
	}*/

	/******************************************************************************
	* MAN State
	* Executes if the user selected MAN using the proper hardware control signals
	* This state enables the user to manually change configuration parameters and
	* the position of the motor.
	******************************************************************************/
	
	if (state == STATE_MAN)
	{
	
		if (triggerINT || IFS0bits.INT0IF) 			/* Disabling the external interrupt INT0, only needed in AFC state */
		{
			triggerINT = disableINT0();
		}
		
		if (!MAN_DELAY_CTRL == 1)					/* Check if we are setting Motor Position (= 1) or Delay (= 0) */
		{
			motorOrDelay = 0;
			homePos = motorPos;
			updateAnalogOut(homePos);
			M24LC64FWriteWord(HOME_ADDRESS, homePos ,M24LC64F_ADDRESS_0);
		}
		else
		{
			motorOrDelay = 1;
			updateAnalogOut(motorPos);
		}
	
		if (!MAN_DELAY_UP == 1)								
		{
			
			__delay32(EEPROM_DELAY*10); 			/* Debouncing (50ms Delay) can be made faster for practical application */
			if (motorOrDelay == 1)
			{
				unsigned int countTemp = 0;
				count =0;
				//do{	
				while(!MAN_DELAY_UP == 1)
				{
				//__delay32(EEPROM_DELAY*4);
				setDir = FORWARD;
				motorPos++;
				moveMotor(setDir, TIMER_PERIOD2);
				while(count==countTemp);
				countTemp++;
				updateAnalogOut(motorPos);
				}
				 
				//M24LC64FWriteWord(MOTOR_POS_ADDRESS, motorPos, M24LC64F_ADDRESS_0);
				
				//}while (count <500);
				setDir = STOP;
				moveMotor(setDir, TIMER_PERIOD2);
			}
			else
			{
				
				if (homePos >= MAX_STEPS)
					homePos = MAX_STEPS;
				else
					homePos ++; 						
			
				updateAnalogOut(homePos);
				M24LC64FWriteWord(HOME_ADDRESS, homePos ,M24LC64F_ADDRESS_0);
				
			}
		}
	
		if (!MAN_DELAY_DOWN == 1)
		{
			__delay32(EEPROM_DELAY*10);				/* Debouncing (50ms Delay) can be made faster for practical application */
			if (motorOrDelay == 1)
			{
				unsigned int countTemp = 0;
				count =0;
				//do{	
				while(!MAN_DELAY_DOWN == 1)
				{
				//__delay32(EEPROM_DELAY*4);
				setDir = REVERSE;
				motorPos--;
				moveMotor(setDir, TIMER_PERIOD2);
				while(count==countTemp);
				countTemp++;
				updateAnalogOut(motorPos);
				}
				 
				//M24LC64FWriteWord(MOTOR_POS_ADDRESS, motorPos, M24LC64F_ADDRESS_0);
				
				//}while (count <500);
				setDir = STOP;
				moveMotor(setDir, TIMER_PERIOD2);				
			}
			else
			{
				
			
				if (homePos <= MIN_DELAY)
					homePos = 0;
				else
					homePos --;							
				
				updateAnalogOut(homePos);
				M24LC64FWriteWord(HOME_ADDRESS, homePos ,M24LC64F_ADDRESS_0);
			}
		}
	
	}
	/* End of MAN State*/
	
		
	/******************************************************************************
	* AFC State
	* Executes if the user selected AFC using the proper hardware control signals
	* This state locks all manual control and the AFC controls the motor position
	******************************************************************************/
	
	if (state == STATE_AFC)
	{
		if (!MAN_DELAY_UP == 1)
		{
			indexMotor(motorPos, MAX_STEPS);
		}
		if (!MAN_DELAY_DOWN == 1)
		{
			indexMotor(homePos, MAX_STEPS);
		}
		
		heatPerPulse = 0;
		
		if (sampleTrigger)
		{
			sampleTrigger = 0;
			
			prevReflectedPower = 0;
			
			/* The ADC is triggered by a special comparison event of the PWM module */
			ADCPC2bits.SWTRG5 = 1; /*Trigger ADC to convert AN11 (Heat Per Pulse input from PLC) */
			while(!ADSTATbits.P5RDY);
		
			heatPerPulse = ADCBUF11;
			
			ADCPC0bits.SWTRG0 = 1; /*Trigger ADC to convert AN0 (Reflected port) */
			while(!ADSTATbits.P0RDY);
			
			prevReflectedPower = reflectedPower;
			
			reflectedPower = ADCBUF0;
			
			// delta = ADCBUF1;
			
			ADSTATbits.P0RDY = 0;           /* Clear the ADSTAT bits */
			//ADSTATbits.P1RDY = 0;
			ADSTATbits.P5RDY = 0;
			 
			/*__asm__ volatile ("clr B");
			__asm__ volatile ("lac %0,B":"+r"(heatPerPulse));
			__asm__ volatile ("sftac B,#16");
			__asm__ volatile ("add A");*/
			
			heatAccumulator += heatPerPulse;
						
			/*No buffer*/
			error = (int)(reflectedPower - prevReflectedPower);
			
			/* Filtering using a FIFO buffer*/
			
			/*errorArray[strPTR] = (int)(reflectedPower - prevReflectedPower);
			strPTR++;
			if (strPTR >= endPTR)
				{
					bufferFull = 1;
					strPTR = 0;
				}
			if (bufferFull == 1)
				{
					int i;
					for (i = 0; i < endPTR; i++)
					{
						error += errorArray[i];
					}
					error = error >> 5; //BUFFER_SIZE = 32 -> 2^5 = 32
				}*/
		/*	else
				{
					int j;
					for (j = 0; j < strPTR; j++)
					{
						error += errorArray[j];
					}
					error = error/strPTR;
				}*/
			triggerINT = enableINT0();
			//calcError();						/* Calculate Error based on A/D results for reflected power*/
			//moveMotor(setDir, motorStep);
			//while (T2CONbits.TON);
		}

			
			if (thermalCounter == 50000) //Using the PWM special event interrupt to increment every TMR1 period match (~20us) 5000*20uS ~= 100mS
			{
				
				/*Reduce the accumulator by the cool rate coef.*/
				heatAccumulator -= (heatAccumulator>>COOL_SHIFTS)*COOL_RATE;

				
			}		
				/* Outer loop, calculates Thermal Drift effects and moves motor proactivley (Feedforward)*/
				target = homePos + calcThermalError() + error;
				moveMotorThermal();
		
	}
		  

	/* End of AFC State*/
	
	/******************************************************************************
	* FAULT State
	* Executes if any serious fault is detected
	*
	******************************************************************************/
	
	if (state == STATE_FAULT)
	{
		checkFaults();
	}

	checkFaults();

	checkState();
	
}
/* End of StateMachine() */

/*******************************************************************************
* Function:     checkState()
*
* Output:		None
*
* Overview:		Function checks the status of the relevant control signals and 
*				sets the appropriate state
* Note:			None
*******************************************************************************/
void checkState(void)
{
	if (state != STATE_FAULT)
	{
		/*if (!WARM_UP == 1)				// Check if Warm Up stage is complete.
		{
			warmUpComplete = 1;
			faultStatus.warmUpFault = 0;
		}
		else
		{
			warmUpComplete = 0;
			state = STATE_WARM_UP;
			faultStatus.warmUpFault = 1;
		}*/
		
		if (MAN_AFC_SELECT == 1) 		// Check if Manual or AFC control is selected 1 = AFC and 0 = Manual
		{
			autoControl = 1;
			manualControl = 0;
		}
		else
		{
			manualControl = 1;
			autoControl = 0;
		}
			
		if (autoControl)
			{
				state = STATE_AFC;
				if (!triggerINT)
					{
						triggerINT = enableINT0();
					}
			}
		if (manualControl)
			state = STATE_MAN;
	}
	
}

/******************************************************************************
* Function:     calcError()
*
* Output:		None
*
* Overview:		The function calculates the error using the Sigma and Delta 
*				signals that are read by the ADC.
* Note:			None
*******************************************************************************/

void calcError()
{
		
		fooPID.controlReference = Q15(0) ;           /*Set the Reference Input for your controller */
		fooPID.measuredOutput = Q15(error/5) ;    
		PID(&fooPID);	
		if ((Fract2Float(fooPID.controlOutput)*0x007F > 0) && (error < -SMALL_ERROR))
		{
		setDir = REVERSE;
		motorPos--; 
		}
		else if ((Fract2Float(fooPID.controlOutput)*0x007F < 0) && (error > SMALL_ERROR))
		{
		setDir = FORWARD;
		motorPos++;
		}
		else
		{
			setDir = STOP;
		}

}
/* End of calcError()*/

/*******************************************************************************
* Function:     calcThermalError()
*
* Output:		Calculated Thermal Error
*
* Overview:		Function calculates the error from the thermal drift over time
* 				Updates motorStep for moveMotorThermal() function
* Note:			None
*******************************************************************************/
int calcThermalError()
{
	int tempError;
	/*prevThermalError = thermalError;
	__asm__ volatile ("clr B");
	__asm__ volatile ("add B");
	__asm__ volatile ("mov %0,ACCBL":"+r"(thermalError));
	tempError = thermalError - prevThermalError;*/
	tempError = ((heatAccumulator>>MOTOR_PRE_SHIFTS) * MOTOR_RATE) >> MOTOR_POST_SHIFTS;
	
	return tempError;
	
	
}
/* End of calcThermalError()*/


/*******************************************************************************
* Function:     moveMotorThermal()
*
* Output:		None
*
* Overview:		This function creates the needed PWM signals to drive the motor
*				in either direction at a fixed speed.
*				Using Timer2 as a period for the pulse seen by the motor, the 
*				function changes the duty cycles of the PWM pairs to create a
*				drive signal for the motor.
* Note:			None
*******************************************************************************/
void moveMotorThermal()
{

	if ((motorPos - target) < -SMALL_THERMAL_ERROR)
	{
		PR2 = TIMER_PERIOD2;
		TMR2	= 0;
		T2CONbits.TON 	= 1;
		setDir = REVERSE;
		motorPos--;
		//motorError += MOTOR_RATE; //needs to be scaled by the frequency to steps ratio
		while (T2CONbits.TON);		
	}
	else if ((motorPos - target) > SMALL_THERMAL_ERROR)
	{
		PR2 = TIMER_PERIOD2;
		TMR2	= 0;
		T2CONbits.TON 	= 1;
		setDir = FORWARD;
		motorPos++;
		//motorError -= MOTOR_RATE; //needs to be scaled by the frequency to steps ratio
		while (T2CONbits.TON);
	}
	else
	{
		PR2 = TIMER_PERIOD2;
		TMR2	= 0;
		T2CONbits.TON 	= 1;
		setDir = STOP;
		while (T2CONbits.TON);
	}	
	
}
/* End of moveMotorThermal()*/
/******************************************************************************
* Function:     moveMotor()
*
* Output:		None
*
* Overview:		This functions creates the needed PWM signals to drive the motor
*				in either direction at a fixed speed.
*				Using Timer2 as a period for the pulse seen by the motor, the 
*				function changes the duty cycles of the PWM pairs to create a
*				drive signal for the motor.
* Note:			None
*******************************************************************************/
void moveMotor( int direction, unsigned int speed )
{
	PR2 = TIMER_PERIOD2;
	TMR2	= 0;
	T2CONbits.TON 	= 1;
}
/* End of moveMotor() */

/******************************************************************************
* Function:     updateAnalogOut()
*
* Output:		None
*
* Overview:		Sends the requested data to the DAC (MCP4725F)
*
* Note:			None
*******************************************************************************/
void updateAnalogOut(unsigned int data)
{
	MCP4725FWriteWord(data<<2, MCP4725F_ADDRESS_0);
}
/* End of updateAnalogOut() */


/******************************************************************************
* Function:     disableINT0()
*
* Output:		int
*
* Overview:		Disables the external interrupt INT0
*
* Note:			None
*******************************************************************************/
int disableINT0(void)
{
	IEC0bits.INT0IE = 0;
	IFS0bits.INT0IF = 0;
	return 0;
}
/* End of disableINT0() */

/******************************************************************************
* Function:     enableINT0()
*
* Output:		int
*
* Overview:		Enables the external interrupt INT0
*
* Note:			None
*******************************************************************************/
int enableINT0(void)
{
	IEC0bits.INT0IE = 1;
	IFS0bits.INT0IF = 0;
	return 1;
}
/* End of enableINT0() */

/******************************************************************************
* Function:     checkFaults()
*
* Output:		None
*
* Overview:		Verifies if a fault condition exists and reports it
*
* Note:			None
*******************************************************************************/
void checkFaults(void)
{
	
	if (faultStatus.ocFault)
	{
		
		state = STATE_FAULT;
		PTCONbits.PTEN = 0;                /* Disable PWM Module */	
		//ADCPC0bits.SWTRG1 = 1; 								/*Trigger ADC to convert AN2 and AN3 */
		while(!IFS0bits.T1IF);
		IMotor1  = ADCBUF2;      							/* Get the conversion result */
		IMotor2  = ADCBUF3;
		ADSTATbits.P2RDY = 0;           					/* Clear the ADSTAT bits */
		ADSTATbits.P3RDY = 0;
		IFS0bits.T1IF	= 0;            /* Clear Timer1 Interrupt Flag */
		TMR1 = 0;
		if (((MAX_NEG_CURRENT < IMotor1) && (IMotor1 < MAX_POS_CURRENT))&&((MAX_NEG_CURRENT < IMotor2) &&(IMotor2 < MAX_POS_CURRENT)))
		{
			faultStatus.ocFault = 0;
			state = STATE_MAN;
			
			PTCONbits.PTEN = 1;
		}
		IEC1bits.INT2IE = 1;  /* Enable interrupt */
	}
	if (faultStatus.afcLostFault)
	{
		state = STATE_AFC;
		faultStatus.afcLostFault = 0;
	}

	FLT1 = 1;
	FLT2 = !((faultStatus.afcLostFault));
	FLT3 = !((faultStatus.ocFault));
	
}
/* End of checkFaults() */

/******************************************************************************
* Function:     indexMotor(int returnPosition, int stepsToStop)
*
* Output:		None
*
* Overview:		indexes motor to a known position
*
* Note:			None
*******************************************************************************/
void indexMotor(int returnPosition, int stepsToStop)
{
	unsigned int countTemp = 0;
	count =0;
	do{	
		//__delay32(EEPROM_DELAY*4);
		setDir = REVERSE;
		moveMotor(setDir, TIMER_PERIOD2);
		while(count==countTemp);
		countTemp++;
		updateAnalogOut(motorPos);
		}while (count < stepsToStop);
		setDir = STOP;
		moveMotor(setDir, TIMER_PERIOD2);	
		
		motorPos = 0;
		countTemp = 0;
		count =0;
		__delay32(EEPROM_DELAY*10);
	do{
		setDir = FORWARD;
		moveMotor(setDir, TIMER_PERIOD2);
		while(count==countTemp);
		motorPos++;
		countTemp++;
		updateAnalogOut(motorPos);
		}while (count < returnPosition);
		setDir = STOP;
		moveMotor(setDir, TIMER_PERIOD2);	
		
		M24LC64FWriteWord(MOTOR_POS_ADDRESS, motorPos, M24LC64F_ADDRESS_0);

}
/*
EOF
*/
