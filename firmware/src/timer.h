/*
*  Timer providing 1ms ticks and delay
*/

#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

// Initialises the timer
void timer_init();

// Sleeps for duration
// Parameters:
//   ms: number of milliseconds to sleep
void timer_sleepMs(uint32_t ms);

// Gets the current value of the interval counter
uint32_t timer_getTicks();

#endif /* TIMER_H_ */
