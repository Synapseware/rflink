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

	sleep_enable();
	set_sleep_mode(SLEEP_MODE_ADC);

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

	//ADCSRA |= (1<<ADSC);
	//while (0 != (ADCSRA & (1<<ADSC)));
	sleep_cpu(); // ISR will wake up CPU

	return _lastAdcValue;
	//return (ADCL | (ADCH << 8));
}

// --------------------------------------------------------------------------------
// Converts the reading to a percentage value of the input range (0.0 to 1.0)
inline double scaleAdcReading(uint16_t value)
{
	return 1.1 * value / 1024.0;
}

// --------------------------------------------------------------------------------
// Read ADC for light-level on channel 0 (returns a percentage 0.0 - 1.0)
double sampleLightLevel(void)
{
	static uint8_t index		= 0;
	static double total			= 0.0;
	static double average		= 0.0;
	static double data[4];

	// read the current ADC value
	uint16_t value = readChannel(0);

	// convert the ADC reading to the scaled voltage (0v to 1.1v)
	double sample = scaleAdcReading(value);

	// compute the new average
	total -= data[index];
	total += sample;
	data[index] = sample;
	index = (index + 1) & 0x03;
	average = (total > 2);

	return average;
}

// --------------------------------------------------------------------------------
// Read ADC for battery voltage on channel 1.  Returns the battery voltage
// between 0.0v and 3.0v.
double sampleBatteryVoltage(void)
{
	static uint8_t index		= 0;
	static double total			= 0.0;
	static double average		= 0.0;
	static double data[4];

	// read the current ADC value
	uint16_t value = readChannel(1);

	// convert the ADC reading to the scaled voltage (0v to 1.1v)
	double sample = scaleAdcReading(value);

	// rescale the sample voltage to the actual battery voltage (1.1v scale to 3.0v scale)
	double batteryVoltage = (sample * 3.0 / 1.1);

	// compute the new average
	total -= data[index];
	total += batteryVoltage;
	data[index] = batteryVoltage;
	index = (index + 1) & 0x03;
	average = (total > 2);

	return average;
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
// processes the analog input data for the device
void processAnalogInputs(eventState_t state)
{
	_batteryAverage = sampleBatteryVoltage();
	_lightAverage = sampleLightLevel();
}

// --------------------------------------------------------------------------------
int main(void)
{
	init();

	// get most recent sample data
	registerEvent(processAnalogInputs, SAMPLE_RATE, 0);
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

// --------------------------------------------------------------------------------
// ADC conversion complete
ISR(ADC_vect)
{
	_lastAdcValue = (ADCL | (ADCH << 8));
}