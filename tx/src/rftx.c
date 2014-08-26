#include "rftx.h"


static char _buff[32];
const uint8_t _len = sizeof(_buff) / sizeof(char);
static double _batteryAverage = 0.0;
static double _lightAverage = 0.0;


// --------------------------------------------------------------------------------
// Toggles the LED
void ToggleLED(eventState_t state)
{
	// or'ing the PINB port pin actually toggles the output state
	PINB |= (1<<LED_PIN);
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

	uartSendBuff(_buff, _len);
}

// --------------------------------------------------------------------------------
// Processes the inputs using a state machine
void processAnalogInputs(eventState_t t)
{
	static uint8_t	state		= STATE_VBATT_SET;
	uint16_t		reading		= 0;

	switch (state)
	{
		case STATE_VBATT_SET:
			setAdcChannelAndVRef(CHANNEL_VBATT, VREF_11);
			readChannel();
			state = STATE_VBATT_READ;
			break;

		case STATE_VBATT_READ:
			reading = getAdcAverage(SAMPLES_TO_AVERAGE);
			_batteryAverage	= scaleAdcReading(reading, SCALE_VBATT);
			state = STATE_LIGHT_SET;
			break;

		case STATE_LIGHT_SET:
			setAdcChannelAndVRef(CHANNEL_LIGHT, VREF_AREF);
			readChannel();
			state = STATE_LIGHT_READ;
			break;

		case STATE_LIGHT_READ:
			reading = getAdcAverage(SAMPLES_TO_AVERAGE);
			_lightAverage = scaleAdcReading(reading, SCALE_LIGHT);
			state = STATE_VBATT_SET;
			break;
	}
}

// --------------------------------------------------------------------------------
// Registers all the periodic events of interest
void registerEvents(void)
{
	// get most recent sample data
	registerHighPriorityEvent(ToggleLED, SAMPLE_RATE, 0);
	registerHighPriorityEvent(ToggleLED, SAMPLE_RATE * 1.25, 0);

	registerEvent(processAnalogInputs, SAMPLE_RATE / 16, 0);
	registerEvent(sendMessage, SAMPLE_RATE, 0);	
}

// --------------------------------------------------------------------------------
void init(void)
{
	PORTB |= (1<<LED_PIN);
	DDRB |= (1<<LED_PIN);

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

	registerEvents();

	sei();
}

// --------------------------------------------------------------------------------
int main(void)
{
	init();

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
