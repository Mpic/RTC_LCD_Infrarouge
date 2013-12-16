//*****************************************************************************
//
// File Name	: 'lcd_lib_Piron_Martin.c'
// Title		: 4 bit LCd interface
// Author		: Scienceprog.com - Copyright (C) 2007
// Created		: 2007-06-18
// Revised		: 2007-06-18
// Version		: 1.0
// Target MCU	: Atmel AVR series
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//26 mars 2013	Modifié par Martin Piron: ajout d'une fonction d'envoi de chaîne de caractères plus simple LCDsendString
//avril 2013 ajout de LCDsendNumber qui simplifie l'envoi de nombres (plusieurs chiffres) à l'afficheur.
//*****************************************************************************
#include "lcd_lib_Piron_Martin.h"
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
//#include <stdarg.h>	//pour traiter le nombre variable d'arguments dans lcdsendNumber(int,...)

void LCDsendChar(uint8_t ch)		//Sends Char to LCD
{
	LDP=(ch&0b11110000);
	LCP|=1<<LCD_RS;
	LCP|=1<<LCD_E;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);	
	LCP&=~(1<<LCD_RS);
	_delay_ms(10);
	LDP=((ch&0b00001111)<<4);
	LCP|=1<<LCD_RS;
	LCP|=1<<LCD_E;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);	
	LCP&=~(1<<LCD_RS);
	_delay_ms(10);
}
void LCDsendCommand(uint8_t cmd)	//Sends Command to LCD
{
	LDP=(cmd&0b11110000);
	LCP|=1<<LCD_E;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);
	_delay_ms(10);
	LDP=((cmd&0b00001111)<<4);	//pourquoi mettre les 4 bits à 0? et bien, pour être sûr dans le shift à gauche de ne pas retrouver les valeurs des 4 bits forts dans les nouveaux 4 bits faibles.
	LCP|=1<<LCD_E;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);
	_delay_ms(10);
}
void LCDinit(void)//Initializes LCD
{
	_delay_ms(15);
	LDP=0x00;
	LCP=0x00;
	LDDR|=1<<LCD_D7|1<<LCD_D6|1<<LCD_D5|1<<LCD_D4; //on met en sortie le registre de direction quand =1. Ici, LDDR = LDDR | 11110000. Donc, on ne touche pas aux bits de poids faible, et on met les forts à 1.
	LCDR|=1<<LCD_E|1<<LCD_RW|1<<LCD_RS;
   //---------one------
	LDP=0<<LCD_D7|0<<LCD_D6|1<<LCD_D5|1<<LCD_D4; //4 bit mode
	LCP|=1<<LCD_E|0<<LCD_RW|0<<LCD_RS;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);
	_delay_ms(10);
	//-----------two-----------
	LDP=0<<LCD_D7|0<<LCD_D6|1<<LCD_D5|1<<LCD_D4; //4 bit mode
	LCP|=1<<LCD_E|0<<LCD_RW|0<<LCD_RS;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);
	_delay_ms(10);
	//-------three-------------
	LDP=0<<LCD_D7|0<<LCD_D6|1<<LCD_D5|0<<LCD_D4; //4 bit mode
	LCP|=1<<LCD_E|0<<LCD_RW|0<<LCD_RS;		
	_delay_ms(10);
	LCP&=~(1<<LCD_E);
	_delay_ms(10);
	//--------4 bit--dual line---------------
	LCDsendCommand(0b00101000);
   //-----increment address, cursor shift------
	LCDsendCommand(0b00001110);


}			
void LCDclr(void)				//Clears LCD
{
	LCDsendCommand(1<<LCD_CLR); 
}
void LCDhome(void)			//LCD cursor home
{
	LCDsendCommand(1<<LCD_HOME);
}
void LCDstring(uint8_t* data, uint8_t nBytes)	//Outputs string to LCD
{
register uint8_t i;

	// check to make sure we have a good pointer
	if (!data) return; //si le pointeur ne renvoie rien, on arrête.

	// print data
	for(i=0; i<nBytes; i++)
	{
		LCDsendChar(data[i]);
	}
}
int LCDsendString(char* data) //CUSTOM Outputs char string to LCD. Retourne la longueur de la chaîne (utilse pour ne rafraîchir écran que lorsque chaîne à envoyer est plus courte que la précédente)
{
	register uint8_t i;

	// check to make sure we have a good pointer
	if (!data) return; //si le pointeur ne renvoie rien, on arrête.

	// print data
	for(i=0; data[i]!=0x00; i++)
	{
		LCDsendChar(data[i]);
		if(i==15)
		{
			LCDGotoXY(0,1);
		}
	}
	return i;
}

//Trouver le nombre de chiffres d'un nombre:
int NumberSize (int i)
{
	int c = 0;
		
	if(i < 0)	{
		i = -i;
	}
		
	do	{
		c++;
	}
	while((i /= 10) > 0);
	return c;
}

int LCDsendNumber (int nbr)
{	
	char nbrString [32];		//créer chaîne de caractères qui contiendra le nombre
	sprintf (nbrString, "%d", nbr); //on envoie le nombre dans la chaîne nbrString.
	return LCDsendString(&nbrString);	//on renvoie la taille de la chaîne
}

void LCDsendFloat(float nbr, uint8_t p)	//p: nombre de décimales
{
	int nbr_ent = 0; int nbr_dec = 0;
	char nbrString [32];
	
	float2ints(nbr,p,&nbr_ent,&nbr_dec);
	sprintf (nbrString, "%d,%d", nbr_ent, nbr_dec);
	LCDsendString(nbrString);
}

void LCDGotoXY(uint8_t x, uint8_t y)	//Cursor to X Y position
{
	register uint8_t DDRAMAddr;
	// remap lines into proper order
	switch(y)
	{
	case 0: DDRAMAddr = LCD_LINE0_DDRAMADDR+x; break;
	case 1: DDRAMAddr = LCD_LINE1_DDRAMADDR+x; break;
	case 2: DDRAMAddr = LCD_LINE2_DDRAMADDR+x; break;
	case 3: DDRAMAddr = LCD_LINE3_DDRAMADDR+x; break;
	default: DDRAMAddr = LCD_LINE0_DDRAMADDR+x;
	}
	// set data address
	LCDsendCommand(1<<LCD_DDRAM | DDRAMAddr);
	
}
//Copies string from flash memory to LCD at x y position
//const uint8_t welcomeln1[] PROGMEM="AVR LCD DEMO\0";
//CopyStringtoLCD(welcomeln1, 3, 1);	
void CopyStringtoLCD(const uint8_t *FlashLoc, uint8_t x, uint8_t y)
{
	uint8_t i;
	LCDGotoXY(x,y);
	for(i=0;(uint8_t)pgm_read_byte(&FlashLoc[i]);i++)
	{
		LCDsendChar((uint8_t)pgm_read_byte(&FlashLoc[i]));
	}
}
//defines char symbol in CGRAM
/*
const uint8_t backslash[] PROGMEM= 
{
0b00000000,//back slash
0b00010000,
0b00001000,
0b00000100,
0b00000010,
0b00000001,
0b00000000,
0b00000000
};
LCDdefinechar(backslash,0);
*/
void LCDdefinechar(const uint8_t *pc,uint8_t char_code){
	uint8_t a, pcc;
	uint16_t i;
	a=(char_code<<3)|0x40;
	for (i=0; i<8; i++){
		pcc=pgm_read_byte(&pc[i]);
		LCDsendCommand(a++);
		LCDsendChar(pcc);
		} 
}

void LCDshiftLeft(uint8_t n)	//Scrol n of characters Right
{
	for (uint8_t i=0;i<n;i++)
	{
		LCDsendCommand(0x1E);
	}
}
void LCDshiftRight(uint8_t n)	//Scrol n of characters Left
{
	for (uint8_t i=0;i<n;i++)
	{
		LCDsendCommand(0x18);
	}
}
void LCDcursorOn(void) //displays LCD cursor
{
	LCDsendCommand(0x0E);
}
void LCDcursorOnBlink(void)	//displays LCD blinking cursor
{
	LCDsendCommand(0x0F);
}
void LCDcursorOFF(void)	//turns OFF cursor
{
	LCDsendCommand(0x0C);
}
void LCDblank(void)		//blanks LCD
{
	LCDsendCommand(0x08);
}
void LCDvisible(void)		//Shows LCD
{
	LCDsendCommand(0x0C);
}
void LCDcursorLeft(uint8_t n)	//Moves cursor by n poisitions left
{
	for (uint8_t i=0;i<n;i++)
	{
		LCDsendCommand(0x10);
	}
}
void LCDcursorRight(uint8_t n)	//Moves cursor by n poisitions left
{
	for (uint8_t i=0;i<n;i++)
	{
		LCDsendCommand(0x14);
	}
}

///////////////////////////////
// Pour envoi nombre float: décomposition en deux entiers avec p décimales
///////////////////////////////
void float2ints(float f, uint8_t p, int *i, int *d)
{
	// f = float, p=decimal precision, i=integer, d=decimal
	int   li;
	long   prec=1;

	for(;p>0;p--)
	{
		prec*=10;
	};  // same as power(10,p)

	li = (int) f;              // get integer part
	*d = (int) ((f-li)*10*prec);  // get decimal part	//*10 pour éviter problème des 9999
	*d = (*d+(5))/10;	//arrondir à valeur supérieure (seulement quand > .5) sans perte de précision pour éviter problème des 2000=1999...
	*i = li;
}
