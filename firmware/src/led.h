/*
*  Debug LEDs
*/

#ifndef LED_H_
#define LED_H_

#include <drivers/gpio/adi_gpio.h>

// Debug LED GPIO port
#define DBG_LED_PORT		ADI_GPIO_PORT0

// Aliases LED GPIO pins
typedef enum {
	DBG_LED_RED 			= ADI_GPIO_PIN_0,
	DBG_LED_AMBER 		= ADI_GPIO_PIN_1,
	DBG_LED_GREEN 		= ADI_GPIO_PIN_2,
} DBG_LED;

#define OFF	false
#define ON	true

// Initialise debug LEDs
void led_setup();

// Turn all the debug LEDs on/off
void led_setAll(const bool enabled);

// Toggle LEDs state
void led_toggle(const DBG_LED led);

// Turn a debug LED on/off
void led_set(const DBG_LED led, const bool enabled);

#endif /* LED_H_ */
