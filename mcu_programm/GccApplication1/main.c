/*
 * GccApplication1.c
 *
 * Created: 29.12.2015 6:39:11
 * Author : prokopiy
 */ 


#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

uint8_t temp, count, start;
volatile uint8_t c;

#define BAUD_C 102 //Value 102 is equal of baudrate of 9600
#define TxD PB3

#define T_START TCCR0B = (1 << CS01) // F_CPU/8
#define T_STOP	TCCR0B = 0
#define T_RESET TCNT0 = 0

#define T1_START TCCR1 = (1 << CS11) // Timer1 Clocking frequency CLK = F_CPU/2
#define T1_STOP	TCCR1 = 0
#define T1_RESET TCNT1 = 0


#define DEBUG_ON	PORTB |= (1<<PINB4) // Turn ON debug pin PB4
#define DEBUG_OFF	PORTB &= ~(1<<PINB4)// Turn OFF debug pin PB4

#define DEBUG2_ON	PORTB |= (1<<PINB1) // Turn ON debug pin PB5
#define DEBUG2_OFF	PORTB &= ~(1<<PINB1)// Turn OFF debug pin PB5

int STATUS = 0;
#define DATA_ACQUIRED	1
#define DATA_CALCULATED	2
#define DATA_SENT		3

unsigned long LMT01_PULSES;
int STRING_BLINKER = 0;

// External interrupt INT0. Detects an end of each data impulse in data package from LMT01.
ISR(INT0_vect){				

	LMT01_PULSES++;
	T1_RESET;				// Reset timer 1
	T1_START;				// Restart timer 1
	
}

#define SENSOR_TURN_ON  PORTB |=  (1<<PINB0)
#define SENSOR_TURN_OFF PORTB &=  ~(1<<PINB0)

ISR(TIM1_COMPA_vect){		// Timer for LMT01. Detects a pause between data transmission.
	
	T1_STOP;				// Stop timer 1
	STATUS = DATA_ACQUIRED;
	SENSOR_TURN_OFF;		// Turn off LMT01*/
	
}

ISR(TIM0_COMPA_vect){ // Timer for UART
	OCR0A = BAUD_C;
	c = 1;
	T_RESET;
}

void send(uint8_t data) { //Что вообще такое "lov"? 0_о
	if (count >= 8) {
		PORTB |= (1<<TxD);
		start = 0; temp = 0; c = 0; count = 0;
		T_STOP;
		OCR0A = 0;
		return;
	}
	if(c == 1) {
		if (start == 0) {
			temp = 0x80;
			start = 1;
			count--;
		}
		else {
			temp = data;
			temp = temp >> count;
			temp = temp << 7;
		}
		switch(temp) {
			case 0x80 : PORTB &= ~(1 << TxD);	break;
			case 0 : PORTB |= (1 << TxD);	break;
		}
		count++;
		c = 0;
	}
}

void send_ch(uint8_t data){
	uint8_t f;
	data = ~data;
	T_START;
	for(f = 0; f < 10; f++){
		while(c == 0);
		send(data);
	}
}
void send_str(char *text){
	while(*text) {
		send_ch(*text++);
	}
}
void itoa(uint16_t n, char s[]) {
	uint8_t i = 0;
	do { s[i++] = n % 10 + '0'; }
	while ((n /= 10) > 0);
	s[i] = '\0';
	// Reversing
	uint8_t j;
	char c;
	for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

void send_num(char *text, uint16_t n){
	char s[6];
	itoa((uint16_t)n, s);
	send_str(text);
	send_str(s);
}

int16_t TEMP = 0;

int main(void)
{
	// Setup IO for debug
	DDRB |= (1 << PINB4);
	DEBUG_OFF;
	DDRB |= (1 << PINB1);
	DEBUG2_OFF;
	
	// Setup IO for powering LMT01
	DDRB |= (1 << PINB0);
	SENSOR_TURN_OFF; // Turn off the LMT01
	
	// Setup IO for UART transmission
	DDRB |= (1 << TxD);
	PORTB |= (1 << TxD);
		
	// Setup external interrupt INT0 on a rising edge
	MCUCR |= (1 << ISC01);
	MCUCR |= (1 << ISC00);
	//GIMSK |= (1 << PCIE);
	
	// Timer 1 Setup - Is used to detect a pause between data packages from LMT01.
	// The minimum duration of one data pulse from LMT01 is 12.2uSec.
	// Let's settle, that pause is detected, when there is no data signal from LMT01 in period longer than 3 data pulses (36.6uSec).
	// Timer1 is clocked by Fclk/2 = 8/2 = 4 MHz
	// To count 36.6uSec with 4MHz clock we need: 36.6*4 = 146.4 cycles.
	OCR1A = 147;
		
	// Timer 0 Setup - Is used for UART
	TIMSK |= (1 << OCIE0A);
	
	// Vaariables initialization
	LMT01_PULSES = 0;
	
	// Debug
	char sign = ' ';
	char temp_string[3];
	char sens_string[5];
	
	// Global interrupt enable
	sei();
	
	send_str("=  LMT01 demo programm for ATtiny25 MCU  =");
	_delay_ms(10);
	send_ch(0x0D);			// Carriage return
	send_ch(0x0A);			// Go to new line
	_delay_ms(100);
	
	SENSOR_TURN_ON;			// Turn on LMT01
	_delay_ms(10);			// wait until LMT01 will completely powered
	
	GIMSK |= (1 << INT0);	// Enable external interrupt INT0, which detects rising edge of data package from LMT01
	TIMSK |= (1 << OCIE1A);	// Enable Timer1 interrupt, which is used to detect a pause between data transmitting from LMT01
		
	while(1){
		if(STATUS == DATA_ACQUIRED){
				
			unsigned long data = ((LMT01_PULSES*256)/4096)-50;
			
			TEMP = data;
			
			if(TEMP > 0){sign = "+";}
			else if(TEMP < 0){TEMP *= -1; sign = "-";}
			else {sign = ' ';}
			
			itoa(TEMP, temp_string);		// Convert the format of TEMP
			/*
			//If necessary, the number of received pulses can be sent to UART
			itoa(LMT01_PULSES, sens_string);// Convert the format of LMT01_PULSES
			send_str("Pulses:");
			send_str(sens_string);		// Send pulses value
			send_str(" ");*/
						
			LMT01_PULSES = 0;			// Reset previous
			
			STATUS = DATA_CALCULATED;	// Change status
			
			// Depending on STRING_BLINKER value we send three or two points to UART, so we can see values are changing and program is executing
			if(STRING_BLINKER == 0){
				send_str("...");
				STRING_BLINKER = 1;
			}
			else{
				send_str("..");
				STRING_BLINKER = 0;
			}
			
			send_ch(0x0D);			// Carriage return
			send_ch(0x0A);			// Go to new line
		}
		//send_num(" Habr:", 4242);
		//send_ch(0x0D);
		
		if(STATUS == DATA_CALCULATED){
			
			send_str("LMT01 temp:");
			send_str(sign);			// Send sign
			send_str(temp_string);	// Send temperature value
			send_str(" C");
			send_ch(0x0D);			// Carriage return
			send_ch(0x0A);			// Go to new line
			STATUS = DATA_SENT;
			
			GIMSK &= ~(1 << INT0);	// Disable external interrupt INT0, which detects rising edge of data package from LMT01 to power LMT01 whithout interrupt on rising edge
			SENSOR_TURN_ON;
			_delay_ms(10);
			GIFR |= (1<<INTF0);		// Reset possible flag of INT0
			GIMSK |= (1 << INT0);	// Enable external interrupt INT0, which detects rising edge of data package from LMT01
		}
		_delay_ms(500);
	}
}

