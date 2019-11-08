/*
*  SmartMesh SDK UART implementation for IoTechnics RFID Solution Board
*/

#include <dn_uart.h>
#include <drivers/uart/adi_uart.h>
#include <drivers/gpio/adi_gpio.h>

#include "../assert.h"

// SmartMesh mote is on UART1
#define MOTE_UART 1

// Timer settings for 115200 baud
#define BAUD_GEN_OSR	3
#define BAUD_GEN_DIV	3
#define BAUD_GEN_DIVM	2
#define BAUD_GEN_DIVN	719

// Global variables used for serial port interface
static uint8_t _uartMemory[ADI_UART_BIDIR_MEMORY_SIZE];
static ADI_UART_HANDLE _uart;

// Receive buffers
static uint8_t _rxBuff1;
static uint8_t _rxBuff2;

// Receive callback
static dn_uart_rxByte_cbt _rxCallback;

// Circular receive buffer
#define RX_BUFFER_SIZE 256
static uint8_t _rxCircBuff[RX_BUFFER_SIZE];
static volatile uint8_t _rxCircGet = 0;
static volatile uint8_t _rxCircPut = 0;

// UART event callback
static void uartCallback(void *pCBParam, uint32_t event, void *pArg) {
	if (event == ADI_UART_EVENT_RX_BUFFER_PROCESSED) {
		// Append to circular receive buffer
		_rxCircBuff[_rxCircPut++] = *((uint8_t*)pArg);
		if (_rxCircGet == _rxCircPut) {
			_rxCircGet++;
		}
		if (_rxCircPut >= RX_BUFFER_SIZE) {
			_rxCircPut = 0;
		}
		if (_rxCircGet >= RX_BUFFER_SIZE) {
			_rxCircGet = 0;
		}

		// Return buffer to driver
		ADI_UART_RESULT eUartResult = adi_uart_SubmitRxBuffer(_uart, pArg, 1, false);
		ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);
	}
}

// Initialise the UART
void dn_uart_init(dn_uart_rxByte_cbt rxByte_cb) {
	ADI_UART_RESULT eUartResult;

	// Save the callback into SmartMesh library for sending received bytes
	_rxCallback = rxByte_cb;
	
	// Set up UART
	eUartResult = adi_uart_Open(MOTE_UART, ADI_UART_DIR_BIDIRECTION, &_uartMemory, ADI_UART_BIDIR_MEMORY_SIZE, &_uart);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Configure UART for correct stopbits, parity, etc
	eUartResult = adi_uart_SetConfiguration(_uart, ADI_UART_NO_PARITY, ADI_UART_ONE_STOPBIT, ADI_UART_WORDLEN_8BITS);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Configure baud rate generator
	eUartResult = adi_uart_ConfigBaudRate(_uart, BAUD_GEN_DIV, BAUD_GEN_DIVM, BAUD_GEN_DIVN, BAUD_GEN_OSR);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Register callback and start callback mode
	eUartResult = adi_uart_RegisterCallback(_uart, &uartCallback, NULL);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Submit receive buffers for interrupt mode
	eUartResult = adi_uart_SubmitRxBuffer(_uart, &_rxBuff1, 1, false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);
	eUartResult = adi_uart_SubmitRxBuffer(_uart, &_rxBuff2, 1, false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);
}

// Transmit a byte over the UART
void dn_uart_txByte(uint8_t byte) {
	ADI_UART_RESULT eUartResult;
	uint8_t *bufferPtr = &byte;

	// Submit tx buffer using interrupt mode
	eUartResult = adi_uart_SubmitTxBuffer(_uart, bufferPtr, sizeof(byte), false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Wait for byte to send
	bool bTxComplete = false;
	while(bTxComplete == false) {
		if(adi_uart_IsTxComplete(_uart, &bTxComplete) != ADI_UART_SUCCESS) {
			break;
		}
	}
}

// Flush the transmit buffer
void dn_uart_txFlush() {
	// No-op
}

// Read and process data from the UART circular receive buffer
uint8_t dn_uart_rxByte() {
	uint16_t bytesRead = 0;
	while (_rxCircGet != _rxCircPut) {
		bytesRead++;
		
		// Call Mesh Library RX Byte callback with received byte
		(*_rxCallback)(_rxCircBuff[_rxCircGet++]);

		if (_rxCircGet >= RX_BUFFER_SIZE) {
			_rxCircGet = 0;
		}
	}

	return bytesRead;
}
