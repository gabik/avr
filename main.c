
//
// Datasheet: https://www.sparkfun.com/datasheets/Components/SMD/ATMega328.pdf
//
// ADC Registers: [c.21 p.250]
//
// ADMUX: REFS1 REFS0 ADLAR MUX4 MUX3 MUX2 MUX1 MUX0
// Reference internal: REFS0=0 REFS1=0
// Left Adjust: ADLAR=1
// Selector bits: MUX0-4 = 0 (ADC0)
//
// ADCSRA: ADEN ADSC ADATE ADIF ADIE ADPS2 ADPS1 ADPS0
// ADC Enable: ADEN=1
// ADC Start Conversation: ADSC=0 (conversation=13 ADC clocks, will zero this flag. set 1 to start)
// ADC Auto trigger: ADATE=0 (auto trigger on rising edge, no need)
// ADC Interrupt flag: ADIF (flag=1 if conversation copleted)
// ADC prescaler: ADPS0-2=1,1,0 (ADC operates between 50khz-200khz, CPU=1Mhz, PS=8, operation=125Khz)
//
// ADLAR = 0:
// ADCH-ADCL: - - - - - - ADC9 ADC8 ; ADC7 ADC6 ADC5 ADC4 ADC3 ADC2 ADC1 ADC0 
// ADLAR = 1:
// ADCH-ADCL: ADC9 ADC8 ADC7 ADC6 ADC5 ADC4 ADC3 ADC2 ; ADC1 ADC0 - - - - - -
// ADC Result, I will use ADLAR=1 and only ADCH
//
// SFIOR: ADTS2 ADTS1 ADTS0 - ACME PUD PSR2 PSR10
// ADC Auto Trigger Source: ADTS2-0 if ADATE=1 this will decide when to auto start conversation
// 
// --------------------------------------------------------------------------------------------
//
// PWM Registers: Using Timer2 (8-bit) [c.15, p.144], [15.7.3]
//
// TCCR2A: COM2A1 COM2A0 COM2B1 COM2B0 – – WGM21 WGM20
// COM2A1:0: Compare Match Output A Mode: for inverted PWM mode on match: COM2A0:1=1,1 (+ Fast PWM)
// COM2B1:0: Output B. COM2B1:0=0,0 - normal pin mode
// WGM21:0: (along with WGM22) Waveform Generation Mode. for Fast PWM and OCRA compare: WGM22:0=1,1,1
//
// TCCR2B: FOC2A FOC2B – – WGM22 CS22 CS21 CS20
// FOC2A:B: Force Output Compare N (for non-PWM, raise compare flag)
// WGM22: set to 1 , see TCCR2A
// CS22:0: Clock select. for 1Mhz and 8 bit = cycle is 3.9Khz. with prescalar of 32, cycle is 122hz CS22:0 = 0,1,1
//
// TCNT2: Timer2 counter 8-bit
//
// OCR2A:B Output Compare Register Timer2 N
//
// TIMSK2: – – – – – OCIE2B OCIE2A TOIE2
// OCIE2A:B: Interrupts enable, set to 1 if need to fire on compare match
// TOIE2: Timer2 overflow interrupt enable
// See TIFR2 for interrupt flags
//
// --------------------------------------------------------------------------------------------
//
// Sleep Registers: [7.11.3 p.45]
//
// PRR – Power Reduction Register: PRTWI PRTIM2 PRTIM0 – PRTIM1 PRSPI PRUSART0 PRADC
// Powering down ADC: PRADC=1
// Powering down Timer2: PRTIM2=1
//
// -------------------------------------------------------------------------------------------
//
// Diagram :
//
// PD3 -> ^^^330ohm^^^ -> LED (with transistor?)
// VCC -> AVCC -> 5V
// GND -> 0
// PC0 -> pot (Connected to 0 and 5V)
// PD2 -> ____\___ switch button to 5V INT0
//
// -------------------------------------------------------------------------------------------

#define F_CPU 1000000L
#define POT_PIN PC0
#define LED_PIN PD3
#define LED_PIN 3

uint8_t counter_timer_overflow;

void adc_init() {
    // ADREF = AVcc [REFS0=1, REFS1=0]
    // ADLAR = 1
    ADMUX = (1 << 6) | (1 << 5); // 01100000
    ADCSRA = (1 << 7) | (1 << 1) | (1 << 0); // 10000011
}

uint8_t adc_read(uint8_t ch) {
    // Masking channel to be 0-3 (PC0-PC3)
    ch &= 3;
    // Setting the multiplex to be the "ch", clearing the 2 bits first
    ADMUX = (ADMUX & 0xFC) | ch; //FC = 11111100, ch is 00|01|10|11
    // Wake ADC up from sleep
    PRR &= ~(1 << 0); // PRADC=0
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
    // Setting PD3 as output (PWM - OC2B)
    DDRD = (1 << 3);
}

void setup() {
    // Disabling all digital input buffers on ADC pins [21.9.5, p.266]
    DIDR0 = 0x3F;
    DIDR1 = 0x03; // 20.3.3 p.249
    // Disabling Analog comperator for reducing power
    ACSR |= (1 << 7); // [20.3.2 p.247]
    // Disable Timers [12.9.2, p.109]
    TCCR0B = 0;
    TCCR1B = 0;
    TCCR2B = 0;
    // Disable Watchdog timer [p.50]
    MCUSR &= ~(1 << 3); // WDRF clear (not mendatory)
    WDTCSR = (1 << 4); // WDCE (change enabler)
    WDTCSR = 0; // clear WDE and all register
    // Disabling USART, Disabling SPI, Disabling TWI and Reducing power [7.11.3 p.45]
    PRR = 0xF7;
}

void pwm_start(uint8_t val) {
    // PRTIM2 to 0, canceling power reduce, OC2A as output
    DDRB |= (1 << 3); // PB3 (OC2A) as output
    PRR &= ~(1 << 6); // Enabling timer2 on PRR
    TCNT2 = 0; // clearing counter
    // Setting Fast PWM inverted mode, TOP = OCRA, with prescaler of 32
    TCCR2A = (1 << 7) | (1 << 6) | (1 << 1) | (1 << 0);
    TCCR2B = (1 << 3) | (1 << 1) | (1 << 0);
    OCR2A = 82; // TOP Value, with 122hz cycle, TOP=82 will give 100hz cycle
    if (val > 100) val = 100; // limiting val to 100%
    OCR2B = OCR2A * val / 100; //OCR2B set to TOPxN%
}

void sleep_power() {
    // Reducing power [7.10 p.42]
    // Disabling ADC and ADC clock and Timer2 and Timer0
    PRR |= (1 << 0) | (1 << 6) | (1 << 5); // PRADC=1, PRTIM2=1, PRTIM0=1
    ADCSRA &= (1 << 7); // ADEN=0
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
    uint8_t level = ac_read(adc_ch);
    pwn_start(level);
    start_timer_seconds(LED_SECONDS);
}

void main() {
    setup();
    set_pins_direction();
    adc_init();
    sleep_power();
    while (1) ;
}

void start_timer_seconds(uint8_t seconds) {
    // Using Timer0 in CTC mode with prescaler:
    // prescaler 1024: 1M/1024 = 976hz ticks. OCR0A of 244 will give TOV every 244x(1/976)=0.25.
    // counting 4 times TOV = 1 second.
    PRR &= ~(1 << 5); // Enabling timer0 on PRR
    counter_timer_overflow = seconds * 4; // starting from TOP and reducing to 0 every TOV interrupt
    TCCR0A = (1 << 1); // Normal port operation, CTC mode (WGM01=1) [12.9.1 p.106]
    TCCR0B = (1 << 2) | (1 << 0); // CS00:2 = 1,0,1 = prescaler 1024
    OCR0A = 244;
    TIMSK0 = (1 << 1); // OCIE0A - compare interrupt [12.9.6 p.111]
    SREG |= (1 << 7); // Enabling interrupt [4.3.1 p.10]
}

ISR(INT0_vect) {
    EIFR = (1 << 0); // clear flag [10.2.3 p.72]
    EIMSK &= ~(1 << 0); // Disabling interrupt [10.2.2 p.72]
    SREG &= ~(1 << 7); // Disabling interrupt [4.3.1 p.10]
    SMCR = 0x0F; // clearing SE [7.11.1 p.44]
    PRR &= ~((1 << 0) | (1 << 6)); // Enabling ADC and Timer2
    set_pins_direction();
    led_power_on(POT_PIN);
}

ISR(TIMER0_COMPA_vect) {
    TIFR0 = (1 << 1); // clear flag [12.9.7 p.111]
    counter_timer_overflow -= 1;
    if (counter_timer_overflow <= 0)
        sleep_power();
}
