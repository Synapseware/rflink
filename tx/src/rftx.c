#include "rftx.h"


static char _buff[64];
const uint8_t _len = sizeof(_buff) / sizeof(char);
static double _batteryLevels[4];
static double _lightLevels[4];


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
	// set the ADMUX bits for the specified channel
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
}

// --------------------------------------------------------------------------------
// Reads the ADC value for the specified channel
uint16_t readChannel(uint8_t channel)
{
	setAdcChannel(channel);

	ADCSRA |= (1<<ADSC);
	while (0 != (ADCSRA & (1<<ADSC)));

	return (ADCL | (ADCH << 8));
}

// --------------------------------------------------------------------------------
// returns the moving-average of all the sampled battery voltages
double getLightLevel(void)
{
	double value = 0.0;
	uint8_t count = 0;

	for (uint8_t index = 0; index < sizeof(_lightLevels) / sizeof(double); index++)
	{
		if (_lightLevels[index] != 0.0)
		{
			value += _lightLevels[index];
			count++;
		}
	}

	// only compute the average if we have enough data
	if (count == 8)
		return value / 8;
	
	// return the most recent sample if we have data but not enough for the average
	if (count > 0)
		return _lightLevels[count - 1];

	// just return 0 since no values have been sampled yet
	return 0.0;
}

// --------------------------------------------------------------------------------
// Saves the most recent light level
void saveLightLevel(double value)
{
	static uint8_t index = 0;
	_lightLevels[index] = value;
	index += (index & 0x03);
}

// --------------------------------------------------------------------------------
// Read ADC for light-level on channel 0 (returns a percentage 0.0 - 1.0)
double sampleLightLevel(void)
{
	uint16_t value = readChannel(0);

	return value / 1024.0;
}

// --------------------------------------------------------------------------------
// returns the moving-average of all the sampled battery voltages
double getBatteryLevel(void)
{
	double value = 0.0;
	uint8_t count = 0;

	for (uint8_t index = 0; index < sizeof(_batteryLevels) / sizeof(double); index++)
	{
		if (_batteryLevels[index] != 0.0)
		{
			value += _batteryLevels[index];
			count++;
		}
	}

	// only compute the average if we have enough data
	if (count == 8)
		return value / 8;
	
	// return the most recent sample if we have data but not enough for the average
	if (count > 0)
		return _batteryLevels[count - 1];

	// just return 0 since no values have been sampled yet
	return 0.0;
}

// --------------------------------------------------------------------------------
// Saves the most recent battery voltage
void saveBatteryLevel(double value)
{
	static uint8_t index = 0;
	_batteryLevels[index] = value;
	index += (index & 0x03);
}

// --------------------------------------------------------------------------------
// Read ADC for battery voltage on channel 1.  Returns the battery voltage
// between 0.0v and 3.0v.
double sampleBatteryVoltage(void)
{
	uint16_t value = readChannel(1);

	// convert the ADC reading to the scaled voltage (0v to 1.1v)
	double sampleVoltage = 1.1 * value / 1024.0;

	// rescale the sample voltage to the actual battery voltage (1.1v scale to 3.0v scale)
	double batteryVoltage = (sampleVoltage * 3.0 / 1.1);

	return batteryVoltage;
}

// --------------------------------------------------------------------------------
// Starts the light level sampling cycle
void processLightLevel(eventState_t state)
{
	double result = sampleLightLevel();

	saveLightLevel(result);
}

// --------------------------------------------------------------------------------
// Starts the battery level sampling cycle
void processBatteryLevel(eventState_t state)
{
	double result = sampleBatteryVoltage();

	saveBatteryLevel(result);
}

// --------------------------------------------------------------------------------
void formatMessage(void)
{
/*
	_buff[0]	= 'h';
	_buff[1]	= sizeof(_buff) - 1;
	_buff[2]	= lastLightLevel();
	_buff[3]	= sampleBatteryVoltage();
	_buff[4]	= '\n';
	_buff[5]	= 0;
*/
	double light = getLightLevel();
	double battery = getBatteryLevel();

	sprintf_P(_buff, PSTR("h %.2f %.2f "), light, battery);
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

	registerEvent(processBatteryLevel, SAMPLE_RATE, 0);
	registerEvent(processLightLevel, SAMPLE_RATE, 0);

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
