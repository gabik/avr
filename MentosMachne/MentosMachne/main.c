/*
 * Created: 5/2/2019 19:30
 * Author : Gabi Kazav
 */ 

/* AVRDude commands:
* Verify ISP: avrdude.exe -p m328p -c usbtiny
* Copy file: copy c:\Users\Kazav\avr\MentosMachne\MentosMachne\Debug\MentosMachne.hex .
* Write file to chip: avrdude.exe -p m328p -c usbtiny -U flash:w:MentosMachne.hex
*/

#include <avr/io.h>
#include <avr/interrupt.h>

enum timer_todo {
	GO_SLEEP,
	MOTOR_BACK,
	MOTOR_DROP,
	};

#define F_CPU 1000000L
#define POT_ADC_PIN 0
#define LED_PIN PD3
#define LED_SECONDS 30
#define MOTOR_SECONDS 3
#define MOTOR_P90_MS 20
#define MOTOR_M90_MS 10

volatile uint8_t timer1_end_todo;
volatile uint8_t counter_timer_overflow;
volatile uint8_t wip;

void adc_init() {
	// Wake ADC up from sleep
	PRR &= ~(1 << 0); // PRADC=0
	// ADREF = AVcc [REFS0=1, REFS1=0]
	// ADLAR = 1
	ADMUX = (1 << 6) | (1 << 5); // 01100000
	ADCSRA = (1 << 7) | (1 << 1) | (1 << 0); // 10000011
}

uint8_t adc_read(uint8_t ch) {
	adc_init();
	// Masking channel to be 0-3 (PC0-PC3)
	ch &= 3;
	// Setting the multiplex to be the "ch", clearing the 2 bits first
	ADMUX = (ADMUX & 0xFC) | ch; //FC = 11111100, ch is 00|01|10|11
	// Start conversation
	ADCSRA |= (1 << 6);
	// Waiting for the conversation to finish with rechecking the ADCS
	while (ADCSRA & (1 << 6));
	// Once the conversation over I read the result and empty the flag
	ADCSRA |= (1 << 4); // ADIF
	return ADCH;
}

void set_pins_direction() {
	// Setting ADC0=PC0 as input
	DDRC = 0;
	// Setting bus D as input, for INT0-PD2 interrupts
	DDRD = 0;
	// Setting bus B as input, we will use PB1 as input
	DDRB = 0;
	// Setting PD3, PD5 as output (PWM - OC2B, OC0B)
	DDRD = (1 << PD3) | (1 << PD5);
}

void setup() {
	// Disabling all digital input buffers on ADC pins [21.9.5, p.266]
	DIDR0 = 0x3F;
	DIDR1 = 0x03; // 20.3.3 p.249
	// Disabling Analog comperator for reducing power
	ACSR |= (1 << 7); // [20.3.2 p.247]
	// Disable Timers [12.9.2, p.109]
	TCCR0A = 0;
	TCCR1A = 0;
	TCCR2A = 0;
	TCCR0B = 0;
	TCCR1B = 0;
	TCCR2B = 0;
	// Disable Watchdog timer [p.50]
	MCUSR &= ~(1 << 3); // WDRF clear (not mendatory)
	WDTCSR = (1 << 4); // WDCE (change enabler)
	WDTCSR = 0; // clear WDE and all register
	// Disabling USART, Disabling SPI, Disabling TWI and Reducing power [7.11.3 p.45]
	PRR = 0xF7;
	wip = 0;
}

void pwm_led_start(uint8_t val) {
	// PRTIM2 to 0, canceling power reduce, OC2A as output
	PRR &= ~(1 << 6); // Enabling timer2 on PRR
	TCNT2 = 0; // clearing counter
	// Setting Fast PWM non-inverted mode, TOP = OCRA, with prescaler of 32
	TCCR2A = (1 << 5) | (1 << 1) | (1 << 0);
	TCCR2B = (1 << 3) | (1 << 1) | (1 << 0);
	OCR2A = 82; // TOP Value, with 122hz cycle, TOP=82 will give 100hz cycle
	if (val > 100) val = 100; // limiting val to 100%
	OCR2B = OCR2A * val / 100; //OCR2B set to TOPxN%
}

void stop_counter_timer1() {
	TCCR1A = 0;
	TCCR1B = 0;
}

void start_timer_seconds(uint8_t seconds) {
	// Using Timer1 in CTC mode with prescaler:
	// prescaler 1024: 1M/1024 = 976hz ticks. OCR0A of 195 will give TOV every 195x(1/976)=0.2.
	// counting 4 times TOV = 1 second.
	PRR &= ~(1 << 3); // Enabling timer1 on PRR
	counter_timer_overflow = seconds * 5; // starting from TOP and reducing to 0 every TOV interrupt
	TCNT1 = 0;
	TCCR1A = 0; // Normal port operation, CTC mode (WGM11=1) [13.11.1 p.134]
	TCCR1B = (1 << 3) | (1 << 2) | (1 << 0); // WGM12, CS10:2 = 1,0,1 = prescaler 1024
	OCR1A = 195; // 0.2 Second cycles
	TIMSK1 = (1 << 1); // OCIE1A - compare interrupt [13.11.8 p.139]
	SREG |= (1 << 7); // Enabling interrupt [4.3.1 p.10]
}

void pwm_servo_start() {
	wip = 1; // Working in progress, logic disable for INT0
	// PRTIM0 to 0, canceling power reduce, OC0B as output
	PRR &= ~(1 << 5); // Enabling timer0 on PRR
	TCNT0 = 0; // clearing counter
	// Setting Fast PWM non-inverted mode, TOP = OCRA, with prescaler of 256
	// Servo need 50hz cycles (20ms), so 1Mhz = 0.000001s x 256 presaler = 0.000256s. 20ms = 0.02s. 0.02 / 0.000256 = 78. TOP=78
	TCCR0A = (1 << 5) | (1 << 1) | (1 << 0); // set COM0B1, WGM01, WGM02 
	TCCR0B = (1 << 3) | (1 << 2); // set WGM02, CS02
	OCR0A = 78; // TOP Value, with 3906hz cycle, TOP=78 will give 50hz cycle
	OCR0B = 1; //OCR0B set to -90c TOP=20ms, we need 0.3ms == 0.3/20*78 = 1
	timer1_end_todo = MOTOR_DROP;
	start_timer_seconds(MOTOR_SECONDS);
}

void motor_drop() {
	OCR0B = 10; // 2.5/20*78 = 10
	timer1_end_todo = MOTOR_BACK;
	start_timer_seconds(MOTOR_SECONDS);
}

void motor_back() {
	OCR0B = 1;
	timer1_end_todo = GO_SLEEP;
	start_timer_seconds(MOTOR_SECONDS);
	wip = 0;
}

void sleep_power() {
	// Reducing power [7.10 p.42]
	// Disabling ADC and ADC clock and Timer0:2
	PRR |= (1 << 0) | (1 << 6) | (1 << 5) | (1 << 3); // PRADC=1, PRTIM2=1, PRTIM0=1, PRTIM1=1
	ADCSRA &= (1 << 7); // ADEN=0
	// Disable Timer0 for the motor
	//TCCR0A = 0;
	//TCCR0B = 0;
	// Disable Timer2 for the led
	//TCCR2A = 0;
	//TCCR2B = 0;
	// Disable Timer1 for counting
	//TCCR1A = 0;
	//TCCR1B = 0;
	// Set all pins as input
	DDRB = 0;
	DDRC = 0;
	DDRD = 0;
	// Interrupts
	EICRA = (1 << 1) | (1 << 0); // Enable INT0 rising edge interrupt. ISC01:0=1,1 [10.2.1 p.71]
	EIMSK = (1 << 0); // INT0 interrupt enable [10.2.2 p.72]
	SREG |= (1 << 7); // Enabling interrupt [4.3.1 p.10]
	// Set sleep mode
	SMCR = (1 << 2); // Power-Down mode [7.11.1 p.44]
	// Disable BOD on sleep [p.44] less than 3 cycles before sleep
	MCUCR = (1 << 6) | (1 << 5);
	MCUCR &= ~(1<<BODSE);  // [7.11.2 p.44]
	// Go To sleep
	SMCR |= (1 << 0); // SE=1 (Sleep enable)
}

void led_power_on(uint8_t adc_ch) {
	uint8_t level = adc_read(adc_ch);
	pwm_led_start(level);
	timer1_end_todo = GO_SLEEP;
	PORTB |= (1 << PB1); // pull up resistor on input button
	start_timer_seconds(LED_SECONDS);
}

int main() {
	setup();
	set_pins_direction();
	led_power_on(POT_ADC_PIN);
	while (1);
}

ISR(INT0_vect) {
	if (!wip) {
		EIFR = (1 << 0); // clear flag [10.2.3 p.72]
		EIMSK &= ~(1 << 0); // Disabling interrupt [10.2.2 p.72]
		SREG &= ~(1 << 7); // Disabling interrupt [4.3.1 p.10]
		SMCR = 0x0F; // clearing SE [7.11.1 p.44]
		PRR &= ~((1 << 0) | (1 << 6)); // Enabling ADC and Timer2
		set_pins_direction();
		led_power_on(POT_ADC_PIN);
	}
}

ISR(TIMER1_COMPA_vect) {
	TIFR1 = (1 << 1); // clear flag [13.11.9 p.140]
	counter_timer_overflow -= 1;
	if ((!(DDRB & (1 << PB1))) && (!(PINB & (1 << PB1)))) { // pin is input and button is pressed
		stop_counter_timer1();
		counter_timer_overflow = 0;
		// Disabling the button (as output + clear pin)
		DDRB |= (1 << PB1);
		PORTB &= ~(1 << PB1);
		// Shut button led and stop PWM on Timer2
		OCR2B = 0;
		TCCR2A = 0;
		TCCR2B = 0;
		pwm_servo_start();
	}
	else if (counter_timer_overflow == 0) {
		if (timer1_end_todo == GO_SLEEP)
			sleep_power();
		else if (timer1_end_todo == MOTOR_BACK)
			motor_back();
		else if (timer1_end_todo == MOTOR_DROP)
			motor_drop();
	}
}
