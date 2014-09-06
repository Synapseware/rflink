#include "rftx.h"


static char _buff[64];
const uint8_t _len = sizeof(_buff) / sizeof(char);
static double _batteryAverage = 0.0;
static double _lightAverage = 0.0;


// --------------------------------------------------------------------------------
void LedOn(eventState_t state)
{
	// LED is active low
	PORTB &= ~(1<<LED_PIN);
}
// --------------------------------------------------------------------------------
void LedOff(eventState_t state)
{
	// bring the output pin high to turn off the LED
	PORTB |= (1<<LED_PIN);
}

void ToggleLed(eventState_t state)
{
	PINB |= (1<<LED_PIN);
}

// --------------------------------------------------------------------------------
// formats the response message for serial transmission
void formatMessage(void)
{
	sprintf_P(_buff, PSTR("Light: %.0f%% Battery: %.2fv \r\n"),
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
			state = STATE_VBATT_READ;
			break;

		case STATE_VBATT_READ:
			reading = getAdcAverage(SAMPLES_TO_AVERAGE);
			_batteryAverage	= scaleAdcReading(reading, SCALE_VBATT);
			state = STATE_LIGHT_SET;
			break;

		case STATE_LIGHT_SET:
			setAdcChannelAndVRef(CHANNEL_LIGHT, VREF_AREF);
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
	registerHighPriorityEvent(LedOn, SAMPLE_RATE, 0);
	registerHighPriorityEvent(LedOff, SAMPLE_RATE + (SAMPLE_RATE * 0.05), 0);
	//registerHighPriorityEvent(ToggleLed, SAMPLE_RATE / 2, 0);

	registerEvent(processAnalogInputs, SAMPLE_RATE / 4, 0);
	registerEvent(sendMessage, SAMPLE_RATE, 0);	
}

// --------------------------------------------------------------------------------
void init(void)
{
	LedOff(0);
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
