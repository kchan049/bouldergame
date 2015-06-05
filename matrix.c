#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <avr/sleep.h>
#include "usart.h"

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}




ISR(TIMER1_COMPA_vect) {
	_avr_timer_cntcurr--;
	if (_avr_timer_cntcurr == 0) {
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}
unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}



typedef struct task {
	unsigned long elapsed_time;
	unsigned int state;
	unsigned long period;
	int (*TickFunction)(int);
} task;

const unsigned char GCDperiod = 1;
unsigned char hiton = 0x00;
unsigned char reset = 0;
unsigned short score = 0;

task tasks[7];


void TimerISR() {
	unsigned char k = 0;

	for( k = 0; k < 7; k++ ) {
		if( tasks[k].elapsed_time > tasks[k].period ) {
			
			if( !hiton ) {
				tasks[k].state = tasks[k].TickFunction(tasks[k].state);
			}
			
			tasks[k].elapsed_time = 0;
		}
		tasks[k].elapsed_time += GCDperiod;
	}
}



static unsigned char matrix[8][8];	//2d array to represent



//creates the random projectile for each row
unsigned char position = 0x00;
void create_projectile() {
	position = rand() % 8;

	matrix[7][position] = 2;
	
	if( tasks[3].period > 75 ) {
		tasks[3].period -= 1;
	}
	
}


//goes down 1 row at a time
void rowdown() {
	for( unsigned char i = 0; i < 8; i++ ) {
		for( unsigned char j = 0; j < 8; j++ ) {
			if( (matrix[i][j] == 2) && (i > 0) ) {
				matrix[i-1][j] += matrix[i][j];
				matrix[i][j] = 0;
				} else if( (matrix[i][j] == 2) && !(i > 0) ) {
				matrix[i][j] = 0;
			}
		}
	}
	
	if( tasks[2].period > 75 ) {
		tasks[2].period -= 1;
	}

	return;
}



void hitornot() {
	for( unsigned char i = 0; i < 8; i++ ) {
		if( matrix[0][i] == 3 ) {
			hiton = 1;
		}
	}

	return;
}


unsigned char posX = 0x04;
unsigned char posY = 0x00;
enum character_States {init, wait, left_button_on, right_button_on, Lbutton_release, Rbutton_release } dotstate;
void move_character() {

	// === Local Variables ===
	
	
	// === Transitions ===
	switch (dotstate) {
		case init:
		dotstate = right_button_on;
		break;
		case wait:
		if( GetBit(~PIND,5) ) {
			dotstate = left_button_on;
			} else if( GetBit(~PIND,6) ) {
			dotstate = right_button_on;
			} else {
			dotstate = wait;
		}
		break;
		case left_button_on:
		dotstate = Lbutton_release;
		break;
		case right_button_on:
		dotstate = Rbutton_release;
		break;
		case Lbutton_release:
		if( !GetBit(~PIND,5) ) {
			dotstate = wait;
			} else {
			dotstate = Lbutton_release;
		}
		break;
		case Rbutton_release:
		if( !GetBit(~PIND,6) ) {
			dotstate = wait;
			} else {
			dotstate = Rbutton_release;
		}
		break;
		default:
		dotstate = init;
		break;
	}
	
	// === Actions ===
	switch (dotstate) {
		case init:   // If illuminated LED in bottom right corner
		matrix[posY][posX] = 1;
		break;
		case wait:
		break;
		case left_button_on:
		if( posX > 0 ) {
			matrix[posY][posX-1] += 1;
			matrix[posY][posX] = 0;
			posX--;
		}
		break;
		case right_button_on:
		if( posX < 7 ) {
			matrix[posY][posX+1] += 1;
			matrix[posY][posX] = 0;
			posX++;
		}
		break;
		case Lbutton_release:
		break;
		case Rbutton_release:
		break;
		default:
		break;
	}

};


void outputmatrix( unsigned char d_counter ) {
	
	unsigned char X_coordinate = 0x00;
	unsigned char Y_coordinate = 0x01 << d_counter;
	
	for( unsigned char col = 0x00; col < 8; ++col ) {
		X_coordinate = (X_coordinate << 1) | !( matrix[d_counter][col] == 2 || matrix[d_counter][col] == 3 );
	}
	
	PORTA = 0xFF;
	PORTB = Y_coordinate;
	PORTC = X_coordinate;
	
}

void dotcolor() {
	
	unsigned char X_coordinate = 0x00;
	
	for( unsigned char col = 0x00; col < 8; ++col ) {
		X_coordinate = (X_coordinate << 1) | !( matrix[0][col] == 1 );
	}
	
	PORTC = 0xFF;
	PORTB = 0x01;
	PORTA = X_coordinate;
	
}


void count_score() {
	for( unsigned i = 0; i < 8; i++ ) {
		if( matrix[0][i] != 0 ) {
			score++;
		}
	}
	
	if( tasks[4].period > 75 ) {
		tasks[4].period -= 1;
	}
}

void clear_matrix() {
	for( unsigned char i = 0; i < 8; i++ ) {
		for( unsigned char j = 0; j < 8; j++ ) {
			matrix[i][j] = 0;
		}
	}
}


enum send_score_states { check_send_rdy, has_transmitted, send } send_score_state;
int send_score( int state ) {
	
	switch( state ) {
		
		case check_send_rdy:
		if( USART_IsSendReady(0) ) {
			state = send;
			} else {
			state = check_send_rdy;
		}
		break;
		
		case has_transmitted:
		if( USART_HasTransmitted(0) ) {
			state = check_send_rdy;
			} else {
			state = has_transmitted;
		}
		break;
		
		case send:
		state = has_transmitted;
		break;
		
		default:
		state = check_send_rdy;
		break;
		
	}
	
	switch( state ) {
		
		case check_send_rdy:
		break;
		
		case has_transmitted:
		break;
		
		case send:
		USART_Send(score/5, 0);
		break;
		
		default:
		break;
		
	}
	
	return state;
}



int main(void)
{

	TimerOn();
	TimerSet(GCDperiod);
	
	initUSART(0);
	USART_Flush(0);
	
	DDRA = 0xFF;	PORTA = 0x00;
	DDRB = 0xFF;	PORTB = 0x00;
	DDRC = 0xFF;	PORTC = 0x00;
	DDRD = 0x02;	PORTD = 0xFD;
	
	unsigned char i = 0;
	
	tasks[i].state = 0;
	tasks[i].period = 50;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &hitornot;
	i++;

	tasks[i].state = 0;
	tasks[i].period = 50;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &move_character;
	i++;
	
	tasks[i].state = 0;
	tasks[i].period = 200;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &rowdown;
	i++;

	tasks[i].state = 0;
	tasks[i].period = 200;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &create_projectile;
	i++;
	
	tasks[i].state = 0;
	tasks[i].period = 200;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &count_score;
	i++;
	
	tasks[i].state = 0;
	tasks[i].period = 1;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &dotcolor;
	i++;
	
	tasks[i].state = check_send_rdy;
	tasks[i].period = 1;
	tasks[i].elapsed_time = 0;
	tasks[i].TickFunction = &send_score;
	i++;
	
	while(1) {
		clear_matrix();
		matrix[0][4] = 1;
		posX = 0x04;
		posY = 0x00;
		
		
		dotstate = init;
		hiton = 0;
		reset = 0;
		score = 0;
		
		unsigned char scorecount = 0x00;
		
		tasks[2].period = 200;
		tasks[3].period = 200;
		tasks[4].period = 200;

		while(!reset) {
			if( scorecount > 7 ) {

				scorecount = 0x00;

			}

			outputmatrix(scorecount);

			scorecount++;
			
			if( GetBit(~PIND,7) ) {
				reset = 1;
				hiton = 0;
			}
			
		
			
			sleep_mode();
			
		}
		
	}
	
	
}

