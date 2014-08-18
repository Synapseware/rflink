#include "rftx.h"


volatile char _buff[64];
const uint8_t _len = sizeof(_buff) / sizeof(char);


// --------------------------------------------------------------------------------
void LEDOff(eventState_t state)
{
	PORTB |= (1<<PORTB0);
}

// --------------------------------------------------------------------------------
// toggle the LED pin
void LEDOn(eventState_t state)
{
	PORTB &= ~(1<<PORTB0);
}

// --------------------------------------------------------------------------------
// Prepare the ADC for sampling
void init_adc(void)
{
	// prepare ADC
	ADMUX =	(1<<REFS1) |
			(1<<REFS0) |
			(1<<ADLAR);			// left adjust (ADCH has the most-significant 8 bits)

	// setup
	ADCSRA = (1<<ADEN) |
			(1<<ADSC) |
			(0<<ADATE) |
			(1<<ADIF) |
			(0<<ADIE) |
			(1<<ADPS2) |	// prescale should be ~160 (14.75MHz/125KHz = 118KHz)
			(0<<ADPS1) |
			(1<<ADPS0);

	// setup
	ADCSRB = (0<<ACME) |
			(0<<ADTS2) |
			(0<<ADTS1) |
			(0<<ADTS0);

	// disable digial inputs
	DIDR0 &= ~((1<<ADC1D) | (1<<ADC0D));

	// setup the input channels
	DDRC &= ~((1<<ADC0D) | (1<<ADC1D));
}

// --------------------------------------------------------------------------------
void init(void)
{
	LEDOff(0);
	DDRB |= (1<<PORTB0);

	power_timer1_enable();

	OCR1A	=	(F_CPU / SAMPLE_RATE);
	TCCR1A	=	0;
	TCCR1B	=	(1<<WGM12) |	// CTC
				(1<<CS10);
	TIMSK1	=	(1<<OCIE1A);

	setTimeBase(SAMPLE_RATE);

	DDRD |= (1<<PORTD1);

	init_adc();

	uart_init();

	sei();
}

// --------------------------------------------------------------------------------
// Changes the ADC input channel
void setAdcChannel(uint8_t channel)
{
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
}

// --------------------------------------------------------------------------------
// Reads the ADC value for the specified channel
uint8_t readChannel(uint8_t channel)
{
	setAdcChannel(channel);

	ADCSRA |= (1<<ADSC);
	while (0 != (ADCSRA & (1<<ADSC)));

	return ADCH;
}

// --------------------------------------------------------------------------------
// Read ADC for light-level channel on channel 0
uint8_t lastLightLevel(void)
{
	return readChannel(0);
}

// --------------------------------------------------------------------------------
// Read ADC for battery voltage on channel 1
double lastBatteryLevel(void)
{
	uint8_t adc = readChannel(1);
	double result = 1.1 * adc / 1024.0;

	return result;
}

// --------------------------------------------------------------------------------
void formatMessage(void)
{
/*
	_buff[0]	= 'h';
	_buff[1]	= sizeof(_buff) - 1;
	_buff[2]	= lastLightLevel();
	_buff[3]	= lastBatteryLevel();
	_buff[4]	= '\n';
	_buff[5]	= 0;
*/
	uint8_t light = lastLightLevel();
	double battery = lastBatteryLevel();

	sprintf(_buff, "h %d %.2f ", light, battery);
}

// --------------------------------------------------------------------------------
// Sends the message bytes to the UART
void sendMessageHandler(void)
{
	uint8_t i = 0;
	while (i < sizeof(_buff) / sizeof(char))
	{
		i--;
	}
}
void sendMessage(eventState_t state)
{
	formatMessage();

	//uartBeginSend(sendMessageHandler);
	uartSendBuff(_buff, _len);

	registerOneShot(sendMessage, SAMPLE_RATE, 0);
}

// --------------------------------------------------------------------------------
int main(void)
{
	init();

	// get most recent sample data
	registerEvent(LEDOn, SAMPLE_RATE, 0);
	registerEvent(LEDOff, SAMPLE_RATE * 0.1, 0);

	registerOneShot(sendMessage, SAMPLE_RATE, 0);

	while(1)
	{
		eventsDoEvents();
	}

	return 0;
}

// --------------------------------------------------------------------------------
// Should be running @ 8.000kHz - this is the event sync driver method
ISR(TIMER1_COMPA_vect)
{
	eventSync();
}
