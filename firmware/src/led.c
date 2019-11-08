#include "led.h"

#include <drivers/gpio/adi_gpio.h>

#include "assert.h"

// Turn a debug LED on/off
void led_set(const DBG_LED led, const bool enabled) {
	if (enabled) {
		adi_gpio_SetHigh(DBG_LED_PORT, led);
	} else {
		adi_gpio_SetLow(DBG_LED_PORT, led);
	}
}

// Toggle LEDs state
void led_toggle(const DBG_LED led) {
	adi_gpio_Toggle(DBG_LED_PORT, led);
}

// Turn all the debug LEDs on/off
void led_setAll(const bool enabled) {
	led_set(DBG_LED_RED, enabled);
	led_set(DBG_LED_AMBER, enabled);
	led_set(DBG_LED_GREEN, enabled);
}

// Initialise debug LEDs
void led_setup() {
	ADI_GPIO_RESULT eGpioResult;

	// Set up GPIO pins for User/Debug LED's
	eGpioResult = adi_gpio_OutputEnable(DBG_LED_PORT, DBG_LED_GREEN, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	eGpioResult = adi_gpio_OutputEnable(DBG_LED_PORT, DBG_LED_AMBER, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	eGpioResult = adi_gpio_OutputEnable(DBG_LED_PORT, DBG_LED_RED, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
}
