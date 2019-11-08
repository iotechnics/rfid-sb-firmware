/*
*  Impinj ITK UART platform implementation for IoTechnics RFID Solution Board
*/

#include <stdint.h>
#include <stdio.h>

#include <drivers/tmr/adi_tmr.h>
#include <drivers/uart/adi_uart.h>

#include "platform.h"
#include "iri.h"
#include "../timer.h"
#include "../assert.h"

// RFID module is on UART0
#define RFID_UART 0

// Impinj SDK constants
#define IPJ_SUCCESS   1
#define IPJ_FAILED 0
#define BAUDRATE_LOOKUP_FAILED 0xFF

// UART port handle
static ADI_UART_HANDLE _uart;
// UART device memory
static uint8_t _uartMemory[ADI_UART_BIDIR_MEMORY_SIZE];
// Receive buffers
static uint8_t _rxBuff1;
static uint8_t _rxBuff2;

// Circular receive buffer
#define RX_BUFFER_SIZE 256
static uint8_t _rxCircBuff[RX_BUFFER_SIZE];
static volatile uint8_t _rxCircGet = 0;
static volatile uint8_t _rxCircPut = 0;

// Baud rate configuration struct
struct UartBaudSt {
	uint32_t desiredBaud;
	uint16_t osr;
	uint16_t div;
	uint16_t divm;
	uint16_t divn;
};

// Baud rate lookup for ADUCM4050 at 26MHz
struct UartBaudSt _uartBaudLookup[] = {
	{9600, 3, 28, 3, 46},
	{19200, 3, 14, 3, 46},
	{38400, 3, 7, 3, 46},
	{57600, 3, 14, 1, 15},
	{115200, 3, 3, 2, 719},
	{230400, 3, 3, 1, 359},
	{460800, 3, 1, 1, 1563},
	{921600, 2, 1, 1, 1563},
	{0, 0, 0, 0, 0}
};

// Lookup baud rate configuration for desired baud rate
static uint8_t baudrate_lookup(uint32_t desiredBaud) {
	uint8_t i;

	i = 0;
	while (_uartBaudLookup[i].desiredBaud > 0) {
		if (_uartBaudLookup[i].desiredBaud == desiredBaud) {
			return i;
		}
		i++;
	}

	return BAUDRATE_LOOKUP_FAILED;

}

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

// Open UART
uint32_t platform_open_port_handler(IPJ_READER_CONTEXT* readerCtx, IPJ_READER_IDENTIFIER readerIdent, ipj_connection_type connType, ipj_connection_params* params) {
	ADI_UART_RESULT eUartResult;

	// Assert connection type
	ASSERT_RESULT(connType, E_IPJ_CONNECTION_TYPE_SERIAL);

	// Open specified UART
	eUartResult = adi_uart_Open(RFID_UART, ADI_UART_DIR_BIDIRECTION, _uartMemory, ADI_UART_BIDIR_MEMORY_SIZE, &_uart);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Configure UART for correct stop bits, parity, etc
	eUartResult = adi_uart_SetConfiguration(_uart, ADI_UART_NO_PARITY, ADI_UART_ONE_STOPBIT, ADI_UART_WORDLEN_8BITS);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Lookup baud rate
	uint8_t baudIndex = baudrate_lookup(params->serial.baudrate);
	ASSERT_RESULT(baudIndex == BAUDRATE_LOOKUP_FAILED, false);

	// Configure baud rate generator
	eUartResult = adi_uart_ConfigBaudRate(_uart, _uartBaudLookup[baudIndex].div, _uartBaudLookup[baudIndex].divm, _uartBaudLookup[baudIndex].divn, _uartBaudLookup[baudIndex].osr);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Register callback and start callback mode
	eUartResult = adi_uart_RegisterCallback(_uart, &uartCallback, NULL);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	adi_uart_SetRxFifoTriggerLevel(_uart, ADI_UART_RX_FIFO_TRIG_LEVEL_1BYTE);

	// Submit a receive buffer for interrupt mode
	eUartResult = adi_uart_SubmitRxBuffer(_uart, &_rxBuff1, 1, false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);
	eUartResult = adi_uart_SubmitRxBuffer(_uart, &_rxBuff2, 1, false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Set Impinj reader context to UART handle
	*readerCtx = (IPJ_READER_CONTEXT*)_uart;

	return IPJ_SUCCESS;
}

// Close UART
uint32_t platform_close_port_handler(IPJ_READER_CONTEXT readerCtx) {
	ADI_UART_RESULT eUartResult;
	eUartResult = adi_uart_Close(_uart);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	return IPJ_SUCCESS;
}

// Transmit data
uint32_t platform_transmit_handler(IPJ_READER_CONTEXT readerCtx, uint8_t* buffer, uint16_t bufferSize, uint16_t* bytesTransmitted) {
	ADI_UART_RESULT eUartResult;

	// Submit tx buffer using interrupt mode
	eUartResult = adi_uart_SubmitTxBuffer(_uart, (void*)buffer, bufferSize, false);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	// Wait for data to transmit
	bool bTxComplete = false;
	while(bTxComplete == false) {
		if(adi_uart_IsTxComplete(_uart, &bTxComplete) != ADI_UART_SUCCESS) {
			break;
		}
	}

	// Transmit complete
	*bytesTransmitted = bufferSize;

    return IPJ_SUCCESS;
}

// Receive data
uint32_t platform_receive_handler(IPJ_READER_CONTEXT readerCtx, uint8_t* buffer, uint16_t bufferSize, uint16_t* bytesReceived, uint16_t timeoutMs) {
	// Read any pending data in circular receive buffer
	uint16_t bytesRead = 0;
	while (_rxCircGet != _rxCircPut) {
		*buffer++ = _rxCircBuff[_rxCircGet++];
		if (_rxCircGet >= RX_BUFFER_SIZE) {
			_rxCircGet = 0;
		}
		if (++bytesRead >= bufferSize) {
			break;
		}
	}

	if (bytesRead >= 0) {
		*bytesReceived = bytesRead;
		return IPJ_SUCCESS;
	}

	return IPJ_FAILED;
}

// Provide timestamp
uint32_t platform_timestamp_ms_handler() {
    return timer_getTicks();
}

// Busy sleep
void platform_sleep_ms_handler(uint32_t milliseconds) {
	timer_sleepMs(milliseconds);
}

// Modify connection configuration
// NOTE: Only baud rate update implemented
uint32_t platform_modify_connection_handler(IPJ_READER_CONTEXT readerCtx, ipj_connection_type connType, ipj_connection_params* params) {
	ADI_UART_RESULT eUartResult;

	uint8_t baudIndex = baudrate_lookup(params->serial.baudrate);
	ASSERT_RESULT(baudIndex == BAUDRATE_LOOKUP_FAILED, false);

	eUartResult = adi_uart_ConfigBaudRate(_uart, _uartBaudLookup[baudIndex].div, _uartBaudLookup[baudIndex].divm, _uartBaudLookup[baudIndex].divn, _uartBaudLookup[baudIndex].osr);
	ASSERT_RESULT(eUartResult, ADI_UART_SUCCESS);

	return IPJ_SUCCESS;
}

// No-ops
uint32_t platform_flush_port_handler(IPJ_READER_CONTEXT readerCtx) { return IPJ_SUCCESS; }
uint32_t platform_reset_pin_handler(IPJ_READER_CONTEXT readerCtx, bool enable) { return IPJ_SUCCESS; }
uint32_t platform_wakeup_pin_handler(IPJ_READER_CONTEXT readerCtx, bool enable) { return IPJ_SUCCESS; }
