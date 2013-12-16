/*
	First revision November 2013
	PIRON Martin - ISET 3e elo 
	Prof. M. Jeanray
	
	ÂµC used: Atmega168PA
	Connections: 
* LCD (4 data pins mode) on PORTD
	RS, R/W and E (4,5,6 LCD) on pins 30,31, 32 (PD0-2)
	D4-D7 (11-14 LCD) on pins 2,9,10,11 (PD4-7).
	
* IR led: pin 13 (PB1) to ground through a 100 Ohm resistor.

* IR receiver: the 3-pin type, with 1-OUT, 2-GND, 3-VCC. OUT connected to PB0(pin12).

* 4 switches on PC0-3 (pins 23-26) 

RTC code modified for Timer2, original from AVR Application Note 134 (v1.01)
 */ 
//******************************************//
//	Defines
//******************************************//
#define F_CPU 1000000	//CPU clock 1Mhz (set through the fuses: internal osc 8Mhz, divider by 8)

//******************************************//
//	Includes
//******************************************//
#include <avr/io.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lcd_lib_Piron_Martin.h"

//******************************************//
//	Prototypes
//******************************************//
char not_leap(void);
void RTCinit(void);	//Launching the timer2 to interrupt each 1 second wih an asynchronous 32768kHz
void power_saving_magic(void);	//disable systems that consume our precious batteries
void power_save(void);	//Launch Power Save Mode for real

	//IR
void IRsend_SonyShutter(void);

//******************************************//
//	structure for keeping time (RTC)
//******************************************//
typedef struct{ 
unsigned char second;   //enter the current time, date, month, and year
unsigned char minute;
unsigned char hour;                                     
unsigned char date;       
unsigned char month;
unsigned int year;      
            }time;

 time t;    
 
 //******************************************//
 //	Shutter codes for the devices used
 //******************************************//
//SONY : SIRC protocol
	// Code to send Shutter release command B4B8F
	int ShutterCodeBits[] = {1,0,1,1,0,1,0,0,1,0,1,1,1,0,0,0,1,1,1,1};

	// Code to send 2 X delay Shutter release command ECB8F (Not used)
	int Shutter2CodeBits[] = {1,1,1,0,1,1,0,0,1,0,1,1,1,0,0,0,1,1,1,1};
//Hi-Fi : 


//*******************************************//
//	Interrupts
//*******************************************//
volatile int wakeUp = 0;	//Tells us if we push a button, that means we can launch the entire program with menus. If we don't push a button, µc still wakes up each second but does nothing but go to sleep.
volatile int c=0;	//to avoid a double detection of the switch push (when it is pushed, then released).
 //RTC : Timer2 in asynchronous Mode (Quartz 32768Hz, active even in power save mode)
ISR(TIMER2_OVF_vect) //overflow interrupt vector (each second: clock 32768Hz, prescaler 128 -> overflow when 256).
{ 
    if (++t.second==60)        //keep track of time, date, month, and year
    {
        t.second=0;
        if (++t.minute==60) 
        {
            t.minute=0;
            if (++t.hour==24)
            {
                t.hour=0;
                if (++t.date==32)
                {
                    t.month++;
                    t.date=1;
                }
                else if (t.date==31) 
                {                    
                    if ((t.month==4) || (t.month==6) || (t.month==9) || (t.month==11)) 
                    {
                        t.month++;
                        t.date=1;
                    }
                }
                else if (t.date==30)
                {
                    if(t.month==2)
                    {
                       t.month++;
                       t.date=1;
                    }
                }              
                else if (t.date==29) 
                {
                    if((t.month==2) && (not_leap()))
                    {
                        t.month++;
                        t.date=1;
                    }                
                }                          
                if (t.month==13)
                {
                    t.month=1;
                    t.year++;
                }
            }
        }
    }  
    //PORTD=~(((t.second&0x01)|t.minute<<1)|t.hour<<7); //Show time on 8 LEDS
}  
ISR(PCINT1_vect)	//When switch pressed on PORTC (PCINT 8 - 14). Problem: ok it wakes it up, but it slows down the RTC. Not cool, but sustainable because we won't use the button too often.
{
	c++;
	//IRsend_SonyShutter();
	if(c==2)
	{
		wakeUp=1;
		c=0;
	}
}

//###################################################
//###################################################
//###################################################
//	S T A R T  M A I N  P R O G R A M
int main(void)
{
	power_saving_magic();
	
//**********************************//
//	Init variables
//**********************************//
//	In/Out:
	//TODO: Question : To save maximum power, do I have to put unused pins on outputs or inputs? 
DDRD=0xFF;	//LEDS to test RTC
DDRC=0x00;	//Switches to test
DDRB=0x02;	//PB1 as an output (IR LED), PB0 as an input (IR detector)

PORTD=0xFF;	//LEDS OFF (on stk500 it's 0=ON)
PORTC=0xFF; //Enable internal pull-ups for the switches
	//TODO: Put all the non-used outputs to 0 to save power.
PORTB &= ~(1<<PORTB1);	//IR LED output 0

t.year=2013;
t.month=12;
t.date=5;
t.hour=18;
t.minute=55;

//	RTC:
	RTCinit();

//	Buttons interrupt to wake it up by hand:
	cli();
	PCICR |= (1<<PCIE1);	//Enable Pin Change Interrupt 1 (for PCINT 8-14 = PORTC)
	PCMSK1 = 0xFF;			// Enable all buttons on PORTC to fire interrupt PCI1.
	sei();

//	LCD initialisation :
	LCDinit();
	LCDhome();
	LCDcursorOFF();
	
//	IR init
	IRsend_enableIRout();

//*********************************//
//	While loop
//********************************//
 while(1)
 {
 	//All the outputs low to minimize power consumption
	power_save();
	
	//PORTB ^= (1<<PINB1);	//XOR: flips the bit PINB1, no matter what state it is. Just as a way to see where we are, we light up and down an LED.

	///////Here we will detect if the 5V pushed by the global 2-state switch is ON or OFF. Thus we know if we have to wake up or not. 

	/* if(!(PORTC&(1<<PINC0)))	//if PINC0=0 (thus if we push switch0)
	{
		
	} */
	if(wakeUp == 1)	//Only launch the program with menus etc if we push on a button. Else it wakes up because of the timer2OVF, in this case it just needs to go to sleep.
	{
		wakeUp=0;	//reset the flag
		IRsend_SonyShutter();
		
		LCDinit();	//Reinit LCD as it wakes up too. If we don't, it will talk like shit. 
		//LCDclr();
		LCDhome();

    }//end of (if(wakeUp==1)
	
 }//end of while(1)
}

///////////////////////////////////
//	RTC
///////////////////////////////////
char not_leap(void)      //check for leap year
{
	if (!(t.year%100))
	return (char)(t.year%400);
	else
	return (char)(t.year%4);
}

void RTCinit()
{	
	int temp0,temp1;
	for(temp0=0;temp0<0x0040;temp0++);	//Wait for external clock crystal to stabilize
	{
		for(temp1=0;temp1<0xFFFF;temp1++);
	}
	
	cli();
	TCCR2B = 0x00;	//disable interrupt while we set it up
	TIMSK2 &=~((1<<TOIE2)|(1<<OCIE2A)|(1<<OCIE2B));     //Disable TC2 interrupt
	/////ATTENTION : TO DISABLE WHEN ON QUARTZ!
	ASSR |= (1<<EXCLK);			//To test with STK500, I don't use a quartz
	//but an external oscillator! Thus it will only work
	//if we set this Enable External Clock Input to 1 !!
	/////////////////////////////
	ASSR |= (1<<AS2);           //set Timer/Counter2 to be asynchronous from the CPU clock
	//with a second external clock(32,768kHz)driving it.
	TCNT2 = 0x00;
	TCCR2A = 0x00;				//Normal mode, no Compare Match Output Mode
	TCCR2B = 0x05;              //prescale the timer to be clock source / 128 to make it
	//exactly 1 second for every overflow to occur
	while(ASSR&0x1F);           //Wait until TC2 is updated

	TIMSK2 |= (1<<TOIE2);      //set 8-bit Timer/Counter0 Overflow Interrupt Enable
	sei();                     //set the Global Interrupt Enable Bit
}

////////////////////////////////////
//	Power-Save sequence
////////////////////////////////////
void power_save(void)	//Launch Power Save Mode for real
{
		//To make sure all goes well as in datasheet p.153: we first write a dummy value to TCCR2x, TCNT2 or OCR2x, we wait unil the corresponding Update Busy Flag in ASSR returns to zero, and then finally we can enter Power-save mode.
		TCNT2=0x05;
		while(ASSR&(1<<TCN2UB));
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);	//Configuring sleep mode: power save mode.
		sleep_enable();	//Go to sleep. Will wake up from time overflow interrupt.
		sleep_cpu();
		_NOP();	//No operation, "for synchronization", they say on the datasheet.
		sleep_disable();
}

void power_saving_magic(void)	//disable systems that consume our precious batteries
{
	cli();	
	//Power Saving magic: see datasheet p.42 (10.10)
		//ADC disable
	//PRADC = 1;
		//Analog Comparator disable
	power_adc_disable();
		//Brown-Out Detector (BOD) disable
	sleep_bod_disable();	//Disables BOD when sleeping. But we could also disable it when idle in our case (with a regulator).
		//Watchdog Timer Disable
	wdt_disable();
		//Port pins configured to no power consumption: no pull-up??
	
		//On-chip Debug System disable : OK (DWEN fuse already disabled)	
		//SPI and TWI disable
	power_spi_disable(); power_twi_disable();
		//Unused timers disable
	power_timer1_disable();
	
	sei();
}

//////////////IR toolbox////////////////////////////////////////////////////////
	//Ken shirriff: based on PWM timer
void IRsend_enableIRout(int khz)
{
	//inspired by Ken Shirrif's library for Arduino. Makes use of the PWM output of timer0.
	
	// Enables IR output.  The khz value controls the modulation frequency in kilohertz.
	// The IR output will be on pin 3 (OC2B).
	// This routine is designed for 36-40KHz; if you use it for other values, it's up to you
	// to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
	// TIMER0 is used in phase-correct PWM mode, with OCR0A controlling the frequency and OCR0B
	// controlling the duty cycle.
	// There is no prescaling, so (the output frequency is 16MHz / (2 * OCR0A) )	No,here it's 1Mhz.
	// To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
	// A few hours staring at the ATmega documentation and this will all make sense.
	// See my Secrets of Arduino PWM at http://arcfn.com/2009/07/secrets-of-arduino-pwm.html for details.

	
	// Disable the Timer0 Interrupt (which is used for receiving IR)
	TIMSK0 &= ~_BV(TOIE0); //Timer0 Overflow Interrupt disable
	
	PORTB &= ~(1<<PORTB1); // When not sending PWM, we want it low
	
	// COM0A = 00: disconnect OC0A
	// COM0B = 00: disconnect OC0B; to send signal set to 10: OC0B non-inverted
	// WGM0 = 101: phase-correct PWM with OCRA as top
	// CS0 = 000: no prescaling
	TCCR0A = _BV(WGM00);
	TCCR0B = _BV(WGM02) | _BV(CS00);

	// The top value for the timer.  The modulation frequency will be F_CPU / 2 / OCR2A.
	OCR0A = F_CPU / 2 / khz / 1000;		//TODO: understand that better
	OCR0B = OCR0A / 3; // 33% duty cycle
}

	//Dumb way: based on delays (carefully tuned with a scope)
void burst()	//Only one burst of 40kHz
{
	PORTB |= (1<<PINB1);	//Send 1. Takes 2µs
	_delay_us(5);	//To test: it should be ok, if not tweak it between 9 and 13...
	PORTB &= ~(1<<PINB1);	//Send 0. 3µs?
	_delay_us(4);	//Same thing as above
	//Must take 25~26µs in total. See it on the scope.
}
void quiet()	//No burst, at 40kHz.
{
	PORTB &= ~(1<<PINB1);	//Takes 2µs
	_delay_us(5);	//To test: it should be ok, if not tweak it between 9 and 13...
	PORTB &= ~(1<<PINB1);	//3µs?
	_delay_us(4);	//Same thing as above
	//Must take 25~26µs in total. See it on the scope.
}
void IRpulse(int microsecs)	//Sends a 38 kHz pulse to PORTB1 for x microseconds
{
	//1 burst = 26µs. Thus,
	while(microsecs > 0)	//38 kHz means about 13µsec high and 13 µs low
	{
		burst();
		//So we count 26µs down (or 25? //TODO : tweak it)
		microsecs -=26;
	}
}
void IRquiet(int microsecs)	//Does not send anything to PORTB1, with exact timing for x microseconds.
{
	while(microsecs > 0)
	{
		quiet();
		//So we count 26µs down (or 25? //TODO : tweak it)
		microsecs -=26;
	}
}
void IRsend_Sony_One()	//Burst timing is 1.2ms burst, 600us quiet.
{
	IRpulse(1160);	//Should be 1200
	IRquiet(480);	//should be 600
}
void IRsend_Sony_Zero()	//600us burst, 600us quiet.
{
	IRpulse(570);	//should be 600
	IRquiet(415);	//should be 600
}
/* Routine to send header data burst
// This allows the IR reciever to set its AGC (Gain)
// Header Burst Timing is 96 * 0.025uS = 2.4mS
// Quiet Timing is 24 * 0.025uS = 600uS	*/
void IRsend_header(void)
{
	IRpulse(2280);	//should be 2400
	IRquiet(460);	//600 doesn't do really 600 but 760. Has to do with IRpulse() that takes a lil bit too much time with the while().
}

void IRsend_SonyShutter(void)	//Attention, it may need tweaking of the frequency : SIRC uses 40kHz, not 38.
{
	//Launches the correct sequence grabbed from the sensor (via Arduino, 'caus we need the serial port to see the sequence...) described in table ShutterCodeBits
	/*Sony codes use SIRC protocol, find more about it on http://www.sbprojects.com/knowledge/ir/sirc.php . */
	//Grabbed on 
	/*Send command 3 times (Sony specs)*/
	
	//LED témoin ON;
	for(int i=1; i<=4; i++)
	{
		IRsend_header();	//2400µs burst, 600µs quiet.
		for(int i=0; i<=3; i++)	//Loop to send the bits of the ShutterCodeBits table 3 times (as in Sony specs)
		{
			if(ShutterCodeBits[i] == 1)
			{
				IRsend_Sony_One();
			}
			else
			{
				IRsend_Sony_Zero();
			}
		}
	}
	//LED témoin OFF;
}
