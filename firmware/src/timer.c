#include "timer.h"

#include <stdint.h>

#include <ADuCM4050.h>

// SysTick interval counter
static volatile uint32_t _msTicks;

// SysTick Interrupt Handler
void SysTick_Handler (void) {
	_msTicks++;
}

// Sleeps for duration
// Parameters:
//   ms: number of milliseconds to sleep
void timer_sleepMs(uint32_t ms)  {
  uint32_t wakeTarget = _msTicks + ms;
  while (wakeTarget > _msTicks) {
	  // Power-Down until next Event/Interrupt
	  __WFE();
  }
}

// Gets the current value of the interval counter
uint32_t timer_getTicks() {
	return _msTicks;
}

// Initialises the timer
void timer_init() {
	// CMSIS clock update
	SystemCoreClockUpdate();

	// Set SysTick timer to 1ms intervals
	SysTick_Config(SystemCoreClock / 1000);
}
