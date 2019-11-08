#include "adi_initialize.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <sys/platform.h>
#include <drivers/general/adi_drivers_general.h>
#include <drivers/pwr/adi_pwr.h>
#include <drivers/gpio/adi_gpio.h>
#include <hashset.h>

#include "led.h"
#include "timer.h"
#include "rfid.h"
#include "mote.h"
#include "assert.h"

// TID_SIZE sets the expected size (in bytes) of the unique tag id.
// This can be set to zero for when using unique EPCs
#define TID_SIZE 0 // bytes

// EPC_SIZE sets the expected maximum size (in bytes) of the EPC data
#define EPC_SIZE 12 // bytes

// Total tag data size (in bytes)
#define TAG_DATA_SIZE (EPC_SIZE + TID_SIZE)

// Maximum number of items to store in the hashset at any one time.
#define HASHSET_ITEMS 200 // items

// Size of the rfid_tag_update header struct (in bytes)
#define RFID_TAG_UPDATE_SIZE 5 // bytes

// Duration to read tags
#define RFID_READ_TIMEOUT 1000 // milliseconds
// Duration to wait between reads
#define RFID_READ_INTERVAL 1 // milliseconds

// Maximum amount of tags that can be included in a single message
#define TRANSMIT_TAG_MAX_ITEMS ((MOTE_MAX_DATA_SIZE - RFID_TAG_UPDATE_SIZE) / TAG_DATA_SIZE)
// Amount of time to delay in between sending tags
#define TRANSMIT_TAG_UPDATE_INTERVAL 10 // milliseconds

// GPIO peripheral memory
static uint8_t _gpioMemory[ADI_GPIO_MEMORY_SIZE];

// Amount of time for an LED to toggle on/off when blinking
#define BLINK_INTERVAL 500
// Stores the timeout for the next blink state change 
static volatile uint32_t _blinkTimeout = 0;
// Stores the current blink state
static volatile bool _blinkState = OFF;

// App states
#define APP_STATE_PENDING_MESH 0
#define APP_STATE_PENDING_READ 1
#define APP_STATE_READING_TAGS 2
#define APP_STATE_PENDING_TRANSMIT 3
#define APP_STATE_TRANSMITTING_TAGS 4
static volatile uint8_t _appState = APP_STATE_PENDING_MESH;

// Timeout, used for setting delays when processing app states
static volatile uint32_t _nextTimeout = 0;
// Hashset, for eliminating duplicate RFID tag reads
static hashset _hashset;
// Hashset iterator for iterating the current unique entries in the hashset
static hashset_iterator _hashsetIterator;

// Buffer to store the data for the SmartMesh message currently being sent
static uint8_t _transmitBuffer[TRANSMIT_TAG_MAX_ITEMS * TAG_DATA_SIZE];
// Stores whether the last SmartMesh message transmitted successfully
static volatile bool _lastTransmitOk = true;
// Unique SmartMesh message id, to assist in de-duplication and ordering at the manager
static uint8_t _transmitMsgId = 0;
// Stores the number of tags currently being transmitted
static uint8_t _transmitTagCount = 0;

// SmartMesh RFID protocol
#define RFID_MSG_TYPE_NOTIF 0x01
#define RFID_NOTIF_TYPE_TAG_UPDATE 0x01
struct rfid_tag_update {
	uint8_t msgId;
	uint8_t msgType;
	uint8_t notifType;
	uint8_t itemSize;
	uint8_t itemCount;
};
// Buffer to store the SmartMessage payload
uint8_t _sendBuffer[MOTE_MAX_DATA_SIZE];

// Send an RFID tag update SmartMesh notification to the manager
// Parameters:
//   msgId: Unique if for the message
//   itemSize: Size (in bytes) of each tag
//   itemCount: Total number of tags in data
// Returns: true if message is successfully queued for send, false otherwise
// Notes: mote_getSendStatus() should be called to determine whether 
// the message was sent successfully
static bool sendRfidTagUpdate(uint8_t msgId, uint16_t itemSize, uint8_t itemCount, uint8_t *data) {
	// Create message header
	struct rfid_tag_update *msg = (struct rfid_tag_update*)_sendBuffer;
	msg->msgId = msgId;
	msg->msgType = RFID_MSG_TYPE_NOTIF;
	msg->notifType = RFID_NOTIF_TYPE_TAG_UPDATE;
	msg->itemSize = itemSize;
	msg->itemCount = itemCount;

	// Add RFID tag data to message payload
	uint8_t *payload = &_sendBuffer[RFID_TAG_UPDATE_SIZE];
	memcpy((void*)payload, (void*)data, itemSize * itemCount);
	uint8_t len = (itemSize * itemCount) + RFID_TAG_UPDATE_SIZE;

	// Send the message across the SmartMesh
	return mote_sendData(_sendBuffer, len);
}

// Transition from one app state to another
// Parameters:
//   newState: The app state to transition to
static void setAppState(uint8_t newState) {
	// No app state change required
	if (_appState == newState) {
		return;
	}

	// Transition away from state...

	if (_appState == APP_STATE_READING_TAGS) {
		// Stop reading tags
		rfid_stopRead();
	}

	// Transition to state...

	if (newState == APP_STATE_PENDING_READ) {
		// Schedule a read
		_nextTimeout = timer_getTicks() + RFID_READ_INTERVAL;
	} else if (newState == APP_STATE_READING_TAGS) {
		// Start reading tags
		_nextTimeout = timer_getTicks() + RFID_READ_TIMEOUT;
		hashset_reset(&_hashset);
		rfid_startRead();
	} else if (newState == APP_STATE_TRANSMITTING_TAGS) {
		// Initialise hashset iterator
		hashset_initIterator(&_hashset, &_hashsetIterator);
		// Set timeout to now
		_nextTimeout = timer_getTicks();
		// Initialise transmit variables
		_lastTransmitOk = true;
	}

	// Set new app state
	_appState = newState;
}

// Toggles LEDs depending on the current app/mote states
// Parameters:
//   appState: The current app state
//   moteState: The current mote state
//   currentTimestamp: The current timestamp
static void setStateLeds(uint8_t appState, uint32_t moteState, uint32_t currentTimestamp) {
	// Turn all LEDs off
	led_setAll(OFF);

	// Determine if blinking LEDs are on/off
	if (_blinkTimeout <= currentTimestamp) {
		_blinkState = !_blinkState;
		_blinkTimeout = currentTimestamp + BLINK_INTERVAL;
	}

	if (appState == APP_STATE_PENDING_MESH) {
		// Waiting for SmartMesh, set LEDs based on mote state
		switch (moteState) {
			case MOTE_STATE_IDLE:
				led_set(DBG_LED_RED, _blinkState);
				break;
			case MOTE_STATE_SEARCHING:
				led_set(DBG_LED_RED, ON);
				led_set(DBG_LED_AMBER, _blinkState);
				break;
			case MOTE_STATE_NEGOTIATING:
				led_set(DBG_LED_AMBER, ON);
				led_set(DBG_LED_GREEN, _blinkState);
				break;
			case MOTE_STATE_CONNECTED:
				led_set(DBG_LED_GREEN, _blinkState);
				break;
			case MOTE_STATE_OPERATIONAL:
				led_set(DBG_LED_GREEN, ON);
				break;
			default:
				led_set(DBG_LED_RED, ON);
				break;
		}
	} else {
		// SmartMesh connected, set LEDs based on app state
		led_set(DBG_LED_GREEN, ON);

		switch (_appState) {
			case APP_STATE_READING_TAGS:
				led_set(DBG_LED_RED, ON);
				break;
			case APP_STATE_TRANSMITTING_TAGS:
				led_set(DBG_LED_AMBER, ON);
				break;
		}
	}
}

int main(int argc, char *argv[])
{
	ADI_PWR_RESULT ePwrResult;
	ADI_GPIO_RESULT eGpioResult;

	// Initialise ADI components
	adi_initComponents();
	
	// Initialise power driver and set clock dividers
	ePwrResult = adi_pwr_Init();
	ASSERT_RESULT(ePwrResult, ADI_PWR_SUCCESS);

	// Set core clock divider to "1" which sets it to 26Mhz
	ePwrResult = adi_pwr_SetClockDivider(ADI_CLOCK_HCLK, 1);
	ASSERT_RESULT(ePwrResult, ADI_PWR_SUCCESS);

	// Set peripheral clock  divider to "1" which sets it to 26Mhz
	ePwrResult = adi_pwr_SetClockDivider(ADI_CLOCK_PCLK, 1);
	ASSERT_RESULT(ePwrResult, ADI_PWR_SUCCESS);

	// Initialise GPIO
	eGpioResult = adi_gpio_Init(_gpioMemory, ADI_GPIO_MEMORY_SIZE);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);

	// Initialise timer
	timer_init();

	// Initialise debug LEDs
	led_setup();

	// Initialise hashset
	hashset_init(&_hashset, HASHSET_ITEMS, TAG_DATA_SIZE);

	// Initialise RFID reader
	rfid_setup(EPC_SIZE, TID_SIZE);

	// Initialise mote
	mote_init();

	// Mote states
	int lastMoteState = MOTE_STATE_INIT;
	int moteState = MOTE_STATE_INIT;

	// Caches current timestamp
	uint32_t currentTimestamp;

	// Event loop
	while (1) {
		// Process mote events
		mote_doEvents();

		// Fetch the current mote state
		moteState = mote_getState();

		// Process mote state changes
		if (moteState != lastMoteState) {
			if (moteState == MOTE_STATE_OPERATIONAL) {
				setAppState(APP_STATE_PENDING_READ);
			} else {
				setAppState(APP_STATE_PENDING_MESH);
			}
		}
		lastMoteState = moteState;

		// Fetch current timestamp
		currentTimestamp = timer_getTicks();

		// Set LEDs
		setStateLeds(_appState, moteState, currentTimestamp);

		// Process app states
		if (_appState == APP_STATE_PENDING_READ && _nextTimeout < currentTimestamp) {
			setAppState(APP_STATE_READING_TAGS);
		} else if (_appState == APP_STATE_READING_TAGS) {
			if (_nextTimeout < currentTimestamp) {
				// Timeout reached, start transmit
				setAppState(APP_STATE_TRANSMITTING_TAGS);
			} else {
				// Read next tag
				rfid_readNext(&_hashset);
			}
		} else if (_appState == APP_STATE_TRANSMITTING_TAGS && _nextTimeout < currentTimestamp) {
			if (_lastTransmitOk && mote_getSendStatus() == MOTE_SEND_SUCCESS) {
				// Prepare next transmit
				_transmitTagCount = 0;
				while (hashset_iterate(&_hashsetIterator)) {
					memcpy((void*)&_transmitBuffer[_transmitTagCount++ * TAG_DATA_SIZE], (void*)_hashsetIterator.item, TAG_DATA_SIZE);
					if (_transmitTagCount >= TRANSMIT_TAG_MAX_ITEMS) {
						break;
					}
				}
				if (_transmitTagCount > 0) {
					// Transmit
					_transmitMsgId = (_transmitMsgId + 1) % 256;
					_lastTransmitOk = sendRfidTagUpdate(_transmitMsgId, TAG_DATA_SIZE, _transmitTagCount, _transmitBuffer);

					// Schedule the next send
					_nextTimeout = timer_getTicks() + TRANSMIT_TAG_UPDATE_INTERVAL;
				} else {
					setAppState(APP_STATE_PENDING_READ);
				}
			} else {
				if (mote_getSendStatus() != MOTE_SEND_IN_PROGRESS) {
					// Last transmit failed, try again
					_lastTransmitOk = sendRfidTagUpdate(_transmitMsgId, TAG_DATA_SIZE, _transmitTagCount, _transmitBuffer);

					// Schedule the next send
					_nextTimeout = timer_getTicks() + TRANSMIT_TAG_UPDATE_INTERVAL;
				}
			}
		}
	}

	return 0;
}

