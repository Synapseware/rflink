#ifndef __RFTX_H_
#define __RFTX_H_


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/sleep.h>

#include <utils.h>
#include <events/events.h>
#include <uart/uart.h>

#include "adc.h"


#define STATE_VBATT_SET		1
#define STATE_VBATT_READ	2
#define STATE_LIGHT_SET		3
#define STATE_LIGHT_READ	4

#define CHANNEL_LIGHT		0
#define CHANNEL_VBATT		1

static const double	SCALE_VBATT			= 3.0;
static const double	SCALE_LIGHT			= 100.0;


#define LED_PIN			PB0




#endif
