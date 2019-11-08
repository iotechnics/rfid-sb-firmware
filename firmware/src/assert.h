/*
*  Assert macro, for use in debugging
*/

#ifndef ASSERT_H_
#define ASSERT_H_

#include <stdlib.h>

#include "led.h"

// Assert will set a breakpoint when built for debug, halts otherwise
#ifdef _DEBUG
#define ASSERT_HALT() asm ("BKPT 0");
#else
#define ASSERT_HALT() while(1) {};
#endif

// Asserts a result, halts if unexpected value
#define ASSERT_RESULT(result, expected_value) \
	do { \
		if ((result) != (expected_value)) { \
			led_setAll(ON); \
			ASSERT_HALT(); \
		} \
	} while (0)

#endif /* ASSERT_H_ */
