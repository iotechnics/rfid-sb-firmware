/*
*  SmartMesh mote
*/

#ifndef MOTE_H_
#define MOTE_H_

#include <stdint.h>
#include <stdbool.h>

// Result codes
#define RC_OK	0x00
#define RC_INVALID_STATE	0x05
#define RC_INCOMPLETE_JOIN_INTO 0x0D

// Mote State
#define MOTE_STATE_INIT           0x00
#define MOTE_STATE_IDLE           0x01
#define MOTE_STATE_SEARCHING      0x02
#define MOTE_STATE_NEGOTIATING    0x03
#define MOTE_STATE_CONNECTED      0x04
#define MOTE_STATE_OPERATIONAL    0x05

// Mote Send Status
#define MOTE_SEND_SUCCESS 0
#define MOTE_SEND_IN_PROGRESS 1
#define MOTE_SEND_FAILED 2

// Maximum message size
#define MOTE_MAX_DATA_SIZE 90

// Initialise the mote
void mote_init();

// Mote event loop, should be called regularly to process pending mote events
void mote_doEvents();

// Retrieve the current state of the mote
uint32_t mote_getState();

// Convenience method to check if the mote is currently operational
bool mote_isOperational();

// Send data over the SmartMesh
// Parameters:
//   payload: The data to send
//   payloadLen: The length of the payload, in bytes
bool mote_sendData(uint8_t* payload, uint8_t payloadLen);

// Retrieve the status of the message currently being sent
uint32_t mote_getSendStatus();

#endif /* MOTE_H_ */
