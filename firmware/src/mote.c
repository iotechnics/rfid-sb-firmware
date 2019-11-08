#include "mote.h"

#include <stdint.h>
#include <stdbool.h>

#include <drivers/gpio/adi_gpio.h>
#include <dn_ipmt.h>
#include <dn_uart.h>

#include "assert.h"
#include "timer.h"

// Buffer to hold reply message data
static uint8_t _replyBuf[MAX_FRAME_LENGTH];
// Buffer to hold notification message data
static uint8_t _notifBuf[MAX_FRAME_LENGTH];

// Method (declared in dn_uart.c) to read a byte from the UART
uint8_t dn_uart_rxByte();

// Forward declaration for SmartMesh SDK notification callback
static void dn_ipmt_notif_cb(uint8_t cmdId, uint8_t subCmdId);
// Forward declaration for SmartMesh SDK reply callback
static void dn_ipmt_reply_cb(uint8_t cmdId);
// Stores whether we're waiting on a reply from the mote
static volatile bool _replyPending = false;
// Stores the current state of the mote
static volatile uint8_t _moteState = MOTE_STATE_INIT;

// Typedef of mote command methods
typedef void (*moteCmd)(void);
// Stores the next command to be executed
static volatile moteCmd _moteCmd = 0;
// Timestamp for when to send the next command
static volatile uint32_t _moteCmdScheduled = 0;
// Timestamp for when a command times out
static volatile uint32_t _moteCmdTimeout = 0;

// Typedef of mote reply handlers
typedef void (*moteReplyHandler)(void);
// Stores the reply handler for the current command
static volatile moteReplyHandler _moteReplyHandler = 0;

// SmartMesh port number to send data
#define MOTE_APP_PORT 0xf0b8
// IPv6 address of the SmartMesh manager
static uint8_t _managerIpv6[] = {0xff, 0x02,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
// Handle to port of SmartMesh mote
static uint8_t _socketId;

// Stores the current mote send status
static volatile uint32_t _sendStatus = MOTE_SEND_SUCCESS;
// Unique packet id when sending SmartMesh messages
static volatile uint16_t _packetId = 0;

// Buffer to store message data during transmit
uint8_t _sendBuffer[MOTE_MAX_DATA_SIZE];

// Forward declarations
void mote_scheduleCmd(moteCmd cmdFn);
void mote_setReplyHandler(moteReplyHandler handlerFn);
void mote_bindSocketReplyHandler();
void mote_bindSocket();
void mote_openSocketReplyHandler();
void mote_openSocket();
void mote_joinNetworkReplyHandler();
void mote_joinNetwork();
void mote_setDutyCycleReplyHandler();
void mote_setDutyCycle();
void mote_sendDataReplyHandler();

// Notification handler, as defined by the SmartMesh SDK
static void dn_ipmt_notif_cb(uint8_t cmdId, uint8_t subCmdId) {
	if (cmdId == CMDID_EVENTS) {
		// Event notification
		dn_ipmt_events_nt* notif = (dn_ipmt_events_nt*)_notifBuf;

		if (notif->state == MOTE_STATE_IDLE) {
			// In case of reset event
			_sendStatus = MOTE_SEND_FAILED;
			_replyPending = false;
			dn_ipmt_cancelTx();

			// Start config/join process if entered idle state
			if (_moteState != notif->state) {
				mote_setDutyCycle();
			}
		}

		_moteState = notif->state;
	} else if (cmdId == CMDID_TXDONE) {
		// Transmit completed notification
		dn_ipmt_txDone_nt* notif = (dn_ipmt_txDone_nt*)_notifBuf;
		if (notif->packetId != _packetId) {
			// Mismatched packet id
			_sendStatus = MOTE_SEND_FAILED;
		} else {
			// Check if packet dropped
			if (notif->status == 0x01) {
				_sendStatus = MOTE_SEND_FAILED;
			} else {
				_sendStatus = MOTE_SEND_SUCCESS;
			}
		}
	}
}

// Reply handler, as defined by the SmartMesh SDK
static void dn_ipmt_reply_cb(uint8_t cmdId) {
	// Reset command timeout
	_moteCmdTimeout = 0;
	// Indicate we've received a reply
	_replyPending = true;
	// Call current the reply handler
	_moteReplyHandler();
}

// Schedules a mote command to be executed
void mote_scheduleCmd(moteCmd cmdFn) {
	// Set when the command should be executed
	_moteCmdScheduled = timer_getTicks() + 1000; // 1s
	// Set the current command
	_moteCmd = cmdFn;
}

// Runs any scheduled mote commands
void mote_runCmd() {
	// Check if we have a mote command queued and it's due to be sent
	if (_moteCmd != 0 && _moteCmdScheduled != 0 && _moteCmdScheduled > timer_getTicks()) {
		moteCmd cmd = _moteCmd;
		// Reset state variables
		_moteCmd = 0;
		_replyPending = false;
		_moteCmdScheduled = 0;
		_moteCmdTimeout = timer_getTicks() + 1000; // 1s
		// Run the command
		cmd();
	}
}

// Sets the handler which processes the next reply message
void mote_setReplyHandler(moteReplyHandler handlerFn) {
	_moteCmdTimeout = 0;
	_moteReplyHandler = handlerFn;
}

// Opens a socket on the mote
void mote_openSocket() {
	mote_setReplyHandler(mote_openSocketReplyHandler);
	dn_err_t eResult = dn_ipmt_openSocket(0x00, (dn_ipmt_openSocket_rpt*)(_replyBuf));
	ASSERT_RESULT(eResult, DN_ERR_NONE);
}

void mote_openSocketReplyHandler() {
	dn_ipmt_openSocket_rpt* reply = (dn_ipmt_openSocket_rpt*)_replyBuf;
	ASSERT_RESULT(reply->RC, RC_OK);
	// Store socket id
	_socketId = reply->socketId;

	// Bind the socket
	mote_scheduleCmd(mote_bindSocket);
}

// Binds an open socket on the mote
void mote_bindSocket() {
	mote_setReplyHandler(mote_bindSocketReplyHandler);
	dn_err_t eResult = dn_ipmt_bindSocket(_socketId, MOTE_APP_PORT, (dn_ipmt_bindSocket_rpt*)(_replyBuf));
	ASSERT_RESULT(eResult, DN_ERR_NONE);
}

void mote_bindSocketReplyHandler() {
	dn_ipmt_bindSocket_rpt* reply = (dn_ipmt_bindSocket_rpt*)_replyBuf;
	ASSERT_RESULT(reply->RC, RC_OK);

	// Join the mesh network
	mote_scheduleCmd(mote_joinNetwork);
}

// Join the mesh network
void mote_joinNetwork() {
	mote_setReplyHandler(mote_joinNetworkReplyHandler);
	dn_err_t eResult = dn_ipmt_join((dn_ipmt_join_rpt*)(_replyBuf));
	ASSERT_RESULT(eResult, DN_ERR_NONE);
}

void mote_joinNetworkReplyHandler() {
	dn_ipmt_join_rpt* reply = (dn_ipmt_join_rpt*)(_replyBuf);
	ASSERT_RESULT(reply->RC, RC_OK);
}

// Set duty cycle to maximum (speeds up connection time, increases power consumption)
void mote_setDutyCycle() {
	mote_setReplyHandler(mote_setDutyCycleReplyHandler);
	dn_err_t eResult = dn_ipmt_setParameter_joinDutyCycle(255, (dn_ipmt_setParameter_joinDutyCycle_rpt*)(_replyBuf));
	ASSERT_RESULT(eResult, DN_ERR_NONE);
}

void mote_setDutyCycleReplyHandler() {
	dn_ipmt_setParameter_joinDutyCycle_rpt* reply = (dn_ipmt_setParameter_joinDutyCycle_rpt*)(_replyBuf);
	ASSERT_RESULT(reply->RC, RC_OK);

	mote_scheduleCmd(mote_openSocket);
}

// Hard reset the mote by toggling reset pin
void mote_hardReset() {
	ADI_GPIO_RESULT eGpioResult = adi_gpio_SetHigh(ADI_GPIO_PORT2, ADI_GPIO_PIN_9);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	timer_sleepMs(500);

	eGpioResult = adi_gpio_SetLow(ADI_GPIO_PORT2, ADI_GPIO_PIN_9);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	timer_sleepMs(500);

	eGpioResult = adi_gpio_SetHigh(ADI_GPIO_PORT2, ADI_GPIO_PIN_9);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	timer_sleepMs(500);
}

// Initialise the mote
void mote_init() {
	// Enable RF reset
	ADI_GPIO_RESULT eGpioResult = adi_gpio_OutputEnable(ADI_GPIO_PORT2, ADI_GPIO_PIN_9, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);

	// Perform hard reset on mote
	mote_hardReset();

	// Disable flow control
	eGpioResult = adi_gpio_OutputEnable(ADI_GPIO_PORT0, ADI_GPIO_PIN_3, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	eGpioResult = adi_gpio_SetLow(ADI_GPIO_PORT0, ADI_GPIO_PIN_3);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);

	// Time sync packet
	eGpioResult = adi_gpio_OutputEnable(ADI_GPIO_PORT1, ADI_GPIO_PIN_11, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	eGpioResult = adi_gpio_SetHigh(ADI_GPIO_PORT1, ADI_GPIO_PIN_11);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);

	// Initialise mote
	dn_ipmt_init(dn_ipmt_notif_cb, _notifBuf, sizeof(_notifBuf), dn_ipmt_reply_cb);

	// Skip reset notification
	while (dn_uart_rxByte()) {};
}

// Mote event loop, should be called regularly to process pending mote events
void mote_doEvents() {
	// Read available bytes from UART
	while (dn_uart_rxByte()) {};

	// Execute any scheduled mote commands
	mote_runCmd();

	// Monitor for command timeouts
	if (_moteCmdTimeout != 0 && _moteCmdTimeout > timer_getTicks()) {
		_moteState = MOTE_STATE_IDLE;
	}
}

// Retrieve the current state of the mote
uint32_t mote_getState() {
	return _moteState;
}

// Convenience method to check if the mote is currently operational
bool mote_isOperational() {
	return _moteState == MOTE_STATE_OPERATIONAL;
}

// Send data over the SmartMesh
// Parameters:
//   payload: The data to send
//   payloadLen: The length of the payload, in bytes
bool mote_sendData(uint8_t* payload, uint8_t payloadLen) {
	// Don't send if mote not operational or send is already in progress
	if (_moteState != MOTE_STATE_OPERATIONAL || _sendStatus == MOTE_SEND_IN_PROGRESS) {
		return false;
	}

	_sendStatus = MOTE_SEND_IN_PROGRESS;
	_packetId = (_packetId + 1) % 255;

	mote_setReplyHandler(mote_sendDataReplyHandler);
	dn_err_t eResult = dn_ipmt_sendTo(_socketId, _managerIpv6, MOTE_APP_PORT, 0x00, 0x01, _packetId, payload, payloadLen, (dn_ipmt_sendTo_rpt*)(_replyBuf));
	if (eResult != DN_ERR_NONE) {
		_sendStatus = MOTE_SEND_FAILED;
		return false;
	}

	return true;
}

void mote_sendDataReplyHandler() {
	dn_ipmt_sendTo_rpt* reply = (dn_ipmt_sendTo_rpt*)(_replyBuf);
	if (reply->RC != RC_OK) {
		// Mote failed to send data
		_sendStatus = MOTE_SEND_FAILED;
	}
}

// Retrieve the status of the message currently being sent
uint32_t mote_getSendStatus() {
	return _sendStatus;
}
