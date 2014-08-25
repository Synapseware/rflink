#ifndef _ADC_H_
#define _ADC_H_


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>


extern volatile uint16_t _lastAdcValue;


void init_adc(void);
void setAdcChannelAndVRef(uint8_t channel, uint8_t vref);
uint16_t readChannel(void);
uint16_t getAdcAverage(uint8_t samples);
double scaleAdcReading(uint16_t value, double scale);

#endif