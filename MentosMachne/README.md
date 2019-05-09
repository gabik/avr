
[Datasheet](https://www.sparkfun.com/datasheets/Components/SMD/ATMega328.pdf)

[Parts List](jp://github.com/gabik/avr/blob/master/MentosMachne/parts.md)
# Registers:
## ADC Registers: [c.21 p.250]

### ADMUX: REFS1 REFS0 ADLAR MUX4 MUX3 MUX2 MUX1 MUX0
Reference internal: REFS0=0 REFS1=0  
Left Adjust: ADLAR=1  
Selector bits: MUX0-4 = 0 (ADC0)  

### ADCSRA: ADEN ADSC ADATE ADIF ADIE ADPS2 ADPS1 ADPS0
ADC Enable: ADEN=1  
ADC Start Conversation: ADSC=0 (conversation=13 ADC clocks, will zero this flag. set 1 to start)  
ADC Auto trigger: ADATE=0 (auto trigger on rising edge, no need)  
ADC Interrupt flag: ADIF (flag=1 if conversation copleted)  
ADC prescaler: ADPS0-2=1,1,0 (ADC operates between 50khz-200khz, CPU=1Mhz, PS=8, operation=125Khz)  

### ADLAR = 0:
#### ADCH-ADCL: - - - - - - ADC9 ADC8 ; ADC7 ADC6 ADC5 ADC4 ADC3 ADC2 ADC1 ADC0
### ADLAR = 1:
#### ADCH-ADCL: ADC9 ADC8 ADC7 ADC6 ADC5 ADC4 ADC3 ADC2 ; ADC1 ADC0 - - - - - -
ADC Result, I will use ADLAR=1 and only ADCH  

### SFIOR: ADTS2 ADTS1 ADTS0 - ACME PUD PSR2 PSR10
ADC Auto Trigger Source: ADTS2-0 if ADATE=1 this will decide when to auto start conversation

## PWM Registers: Using Timer2 (8-bit) [c.15, p.144], [15.7.3]

### TCCR2A: COM2A1 COM2A0 COM2B1 COM2B0 – – WGM21 WGM20
COM2A1:0: Compare Match Output A Mode: for inverted PWM mode on match: COM2A0:1=1,1 (+ Fast PWM)  
COM2B1:0: Output B. COM2B1:0=0,0 - normal pin mode  
WGM21:0: (along with WGM22) Waveform Generation Mode. for Fast PWM and OCRA compare: WGM22:0=1,1,1  

### TCCR2B: FOC2A FOC2B – – WGM22 CS22 CS21 CS20
FOC2A:B: Force Output Compare N (for non-PWM, raise compare flag)  
WGM22: set to 1 , see TCCR2A  
CS22:0: Clock select. for 1Mhz and 8 bit = cycle is 3.9Khz. with prescalar of 32, cycle is 122hz CS22:0 = 0,1,1  

### TCNT2: Timer2 counter 8-bit

### OCR2A:B Output Compare Register Timer2 N

### TIMSK2: – – – – – OCIE2B OCIE2A TOIE2
OCIE2A:B: Interrupts enable, set to 1 if need to fire on compare match  
TOIE2: Timer2 overflow interrupt enable  
See TIFR2 for interrupt flags

## Sleep Registers: [7.11.3 p.45]

### PRR – Power Reduction Register: PRTWI PRTIM2 PRTIM0 – PRTIM1 PRSPI PRUSART0 PRADC
Powering down ADC: PRADC=1  
Powering down Timer2: PRTIM2=1

# Diagram :

PD3 -> ^^^330ohm^^^ -> LED (with transistor?) (button led)  
VCC -> AVCC -> 5V  
GND -> 0  
PC0 -> pot (Connected to 0 and 5V) (led bright control)  
PD2 -> \_\_\\\_\_ switch button to 5V INT0 (forcing coin)  
PD5 -> Servo (candy motor, from -90 to 90)  
PB5 -> \_\_\\\_\_GND switch button (get candy)  

![ImageDiagram](https://raw.githubusercontent.com/gabik/avr/master/MentosMachne/images/schemeit-project.png)

Also available on [DigiKey](https://www.digikey.com/schemeit/project/mentos-F8JSJ7O4010G/)

# PWM Tests for Servo
[Servo Datasheet](http://bienonline.magix.net/public/projekte-teedipper/SG90%209%20g%20Micro%20Servo.pdf)

In order for the servo to perform "go left, wait, go right" we need to perform -90&deg; and than +90&deg;.

To perform -90&deg; we need to first set the PWM cycle to be 20ms (50hz). See the registers description above to set it. for testing I am using Login Analyzer.

#### PWM Cycle:
![1.png](https://raw.githubusercontent.com/gabik/avr/master/MentosMachne/images/1.PNG)

#### -90&deg; wave:
![2.png](https://raw.githubusercontent.com/gabik/avr/master/MentosMachne/images/2.PNG)

#### +90&deg; wave:
![3.png](https://raw.githubusercontent.com/gabik/avr/master/MentosMachne/images/3.PNG)

