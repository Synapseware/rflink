#include "rftx.h"


static char _buff[64];
const uint8_t _len = sizeof(_buff) / sizeof(char);
static double _batteryAverage = 0.0;
static double _lightAverage = 0.0;
volatile uint16_t _lastAdcValue = 0;


// --------------------------------------------------------------------------------
// turn the LED off
void LEDOff(eventState_t state)
{
	// a logical 1 turns off the LED
	PORTB |= (1<<PORTB0);
}

// --------------------------------------------------------------------------------
// turn the LED on
void LEDOn(eventState_t state)
{
	// a logical 0 turns on the LED
	PORTB &= ~(1<<PORTB0);
}

// --------------------------------------------------------------------------------
// Prepare the ADC for sampling
void init_adc(void)
{
	// prepare ADC
	ADMUX =	(1<<REFS1) |
			(1<<REFS0) |
			(0<<ADLAR);		// right adjust result

	// setup
	ADCSRA = (1<<ADEN) |
			(1<<ADSC) |
			(0<<ADATE) |
			(1<<ADIF) |
			(1<<ADIE) |
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

#ifdef SLEEP_FOR_ADC
	sleep_enable();
	set_sleep_mode(SLEEP_MODE_ADC);
#endif

	sei();
}

// --------------------------------------------------------------------------------
// Changes the ADC input channel
void setAdcChannel(uint8_t channel)
{
	// set the ADMUX bits for the specified channel
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
}


// --------------------------------------------------------------------------------
// Sets the ADC v-ref to the analog VREF pin, external cap on VREF recommended
void setAdcVRefToARef(void)
{
	ADMUX = (ADMUX & 0x3F) | (0<<REFS1) | (0<<REFS0);
}

// --------------------------------------------------------------------------------
// Sets the ADC v-ref to AVCC
void setAdcVRefToAVCC(void)
{
	ADMUX = (ADMUX & 0x3F) | (0<<REFS1) | (1<<REFS0);
}

// --------------------------------------------------------------------------------
// Sets the ADC v-ref to the internal 1.1v
void setAdcVRefTo1v1(void)
{
	ADMUX = (ADMUX & 0x3F) | (1<<REFS1) | (1<<REFS0);
}

// --------------------------------------------------------------------------------
// Reads the ADC value for the specified channel
uint16_t readChannel(uint8_t channel)
{
	setAdcChannel(channel);

#ifdef SLEEP_FOR_ADC
	sleep_cpu(); // ISR will wake up CPU
	return _lastAdcValue;
#else
	ADCSRA |= (1<<ADSC);
	while (0 != (ADCSRA & (1<<ADSC)));

	return (ADCL | (ADCH << 8));
#endif
}

// --------------------------------------------------------------------------------
// Reads a fixed number of samples from the ADC and averages the result
uint16_t getAdcAverage(uint8_t channel)
{
	uint16_t	total		= 0;
	uint8_t		index		= 0;

	while (index < ADC_AVERAGE_OVER)
	{
		total += readChannel(channel);
	}

	total >>= ADC_AVERAGE_OVER;

	return total;
}

// --------------------------------------------------------------------------------
// Converts the reading to a percentage value of the input range (0.0 to 1.0)
inline double scaleAdcReading(uint16_t value, double scale)
{
	// re-scale the ADC reading
	return scale * value / 1024.0;
}

// --------------------------------------------------------------------------------
// formats the response message for serial transmission
void formatMessage(void)
{
	sprintf_P(_buff, PSTR("h %.2f %.2f "),
		_lightAverage,
		_batteryAverage
	);
}

// --------------------------------------------------------------------------------
// Sends the message bytes to the UART
void sendMessage(eventState_t state)
{
	formatMessage();

	//uartBeginSend(sendMessageHandler);
	uartSendBuff(_buff, _len);

	registerOneShot(sendMessage, SAMPLE_RATE, 0);
}

// --------------------------------------------------------------------------------
// Processes the inputs using a state machine
void processAnalogInputs(eventState_t t)
{
	static uint8_t state	= STATE_VBATT_SET;

	uint16_t	reading		= 0;

	switch (state)
	{
		case STATE_VBATT_SET:
			setAdcVRefTo1v1();
			readChannel(CHANNEL_VBATT);
			state = STATE_VBATT_READ;
			break;

		case STATE_VBATT_READ:
			reading = getAdcAverage(CHANNEL_VBATT);
			_batteryAverage	= scaleAdcReading(reading, SCALE_VBATT);
			state = STATE_LIGHT_SET;
			break;

		case STATE_LIGHT_SET:
			setAdcVRefToAVCC();
			readChannel(CHANNEL_LIGHT);
			state = STATE_LIGHT_READ;
			break;

		case STATE_LIGHT_READ:
			reading = getAdcAverage(CHANNEL_LIGHT);
			_lightAverage = scaleAdcReading(reading, SCALE_LIGHT);
			state = STATE_VBATT_SET;
			break;
	}
}

// --------------------------------------------------------------------------------
int main(void)
{
	init();

	// get most recent sample data
	registerEvent(processAnalogInputs, SAMPLE_RATE / 16, 0);
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
	// event tick signal
	eventSync();
}

// --------------------------------------------------------------------------------
// ADC conversion complete
ISR(ADC_vect)
{
	// save the ADC reading in a global var
	_lastAdcValue = (ADCL | (ADCH << 8));
}