#include "adc.h"

volatile uint16_t _lastAdcValue = 0;


// --------------------------------------------------------------------------------
// Sets the ADC's channel and VREF source
void setAdcChannelAndVRef(uint8_t channel, uint8_t vref)
{
	// 0x30 = 0b00110000, we don't want to keep the REFS0:1 or MUX3:0 bits
	ADMUX = (ADMUX & 0x30) |
			(channel & 0x0F) |
			((vref & 0x03) << 6);

	// read the ADC (discards the result after a channel & vref change)
	readAdcSample();
}

// --------------------------------------------------------------------------------
// Reads the ADC value for the specified channel
uint16_t readAdcSample(void)
{
	sleep_cpu(); // ISR will wake up CPU
	return _lastAdcValue;

/*
	//#ifdef SLEEP_FOR_ADC

	sleep_cpu(); // ISR will wake up CPU
	return _lastAdcValue;

	//#else

	//ADCSRA |= (1<<ADSC);
	//while (0 != (ADCSRA & (1<<ADSC)));

	//return (ADCL | (ADCH << 8));

	//#endif
*/
}

// --------------------------------------------------------------------------------
// Reads a fixed number of samples from the ADC and averages the result
uint16_t getAdcAverage(uint8_t samples)
{
	uint16_t	total		= 0;
	uint8_t		count		= samples;

	while (count > 0)
	{
		total += readAdcSample();
		count--;
	}

	total = total / samples;

	return total;
}

// --------------------------------------------------------------------------------
// Converts the reading to a percentage value of the input range (0.0 to 1.0)
inline double scaleAdcReading(uint16_t value, double scale)
{
	// re-scale the ADC reading
	return scale * (value / 1024.0);
}

// --------------------------------------------------------------------------------
// Prepare the ADC for sampling
void init_adc(void)
{
	power_adc_enable();

	sleep_enable();
	set_sleep_mode(SLEEP_MODE_ADC);

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
// ADC conversion complete
ISR(ADC_vect)
{
	// save the ADC reading in a global var
	_lastAdcValue = (ADCL | (ADCH << 8));
}
