#include "rfid.h"

#include <stdint.h>
#include <string.h>

#include <iri.h>
#include <platform.h>
#include <drivers/gpio/adi_gpio.h>
#include <hashset.h>

#include "timer.h"
#include "assert.h"

// RFID enable pin
#define RFID_ENABLE_PORT	ADI_GPIO_PORT1
#define RFID_ENABLE_PIN		ADI_GPIO_PIN_8

// Interval to delay between reset toggles
#define RFID_RESET_TIME 150 // milliseconds

// RFID device memory
static ipj_iri_device iri_device = { 0 };
// Flag for monitoring if RFID is reading
static uint32_t ipj_stopped_flag;

// Expected EPC size
static uint16_t expectedEpcSize = 0;
// Expected TID size
static uint16_t expectedTidSize = 0;
// Hashset for storing results
static hashset *resultHashset = 0;
// Buffer for storing tag data
static uint8_t tagBuffer[128];

// Impinj SDK Platform handlers
struct ipj_handler {
	ipj_handler_type type;
	void* handler;
};
static struct ipj_handler event_handlers[] =
{
	{ E_IPJ_HANDLER_TYPE_PLATFORM_OPEN_PORT,        &platform_open_port_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_CLOSE_PORT,       &platform_close_port_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_TRANSMIT,         &platform_transmit_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_RECEIVE,          &platform_receive_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_TIMESTAMP,        &platform_timestamp_ms_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_SLEEP_MS,         &platform_sleep_ms_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_MODIFY_CONNECTION,&platform_modify_connection_handler },
	{ E_IPJ_HANDLER_TYPE_PLATFORM_FLUSH_PORT,       &platform_flush_port_handler },
};

// Impinj SDK tag report handler
ipj_error ipj_util_tag_operation_report_handler(ipj_iri_device* iri_device, ipj_tag_operation_report* tag_operation_report) {
	// Check for error
	if (tag_operation_report->has_error && tag_operation_report->error > 0) {
		return tag_operation_report->error;
	}

	// If hashset isn't initialised then ignore tag read
	if (resultHashset == 0) {
		return E_IPJ_ERROR_SUCCESS;
	}

	bool hasEpc = false;
	bool hasTid = false;
	uint8_t addResult;

	// Check if tag has EPC
	if (tag_operation_report->tag.has_epc && tag_operation_report->tag.epc.size == expectedEpcSize) {
		hasEpc = true;
	}

	// Check if tag has TID
	if (expectedTidSize > 0 && tag_operation_report->has_tag_operation_type && tag_operation_report->tag_operation_type == E_IPJ_TAG_OPERATION_TYPE_READ) {
		if (tag_operation_report->has_tag_operation_data && tag_operation_report->tag_operation_data.size == expectedTidSize) {
			hasTid = true;
		}
	}

	if (hasEpc && (expectedTidSize == 0 || hasTid)) {
		if (expectedTidSize == 0) {
			// EPC only
			addResult = hashset_add(resultHashset, tag_operation_report->tag.epc.bytes);
			ASSERT_RESULT(addResult != HASHSET_TABLE_FULL, true);
		} else {
			// Combined EPC/TID
			memcpy(tagBuffer, tag_operation_report->tag.epc.bytes, tag_operation_report->tag.epc.size);
			memcpy(tagBuffer + tag_operation_report->tag.epc.size, tag_operation_report->tag_operation_data.bytes, tag_operation_report->tag_operation_data.size);

			addResult = hashset_add(resultHashset, tagBuffer);
			ASSERT_RESULT(addResult != HASHSET_TABLE_FULL, true);
		}
	}

	return E_IPJ_ERROR_SUCCESS;
}

// Impinj SDK stop report handler
ipj_error ipj_util_stop_report_handler(ipj_iri_device* iri_device, ipj_stop_report* ipj_stop_report) {
	ipj_stopped_flag = 1;
	return ipj_stop_report->error;
}

// Impinj SDK error report handler
ipj_error ipj_util_report_handler(ipj_iri_device* iri_device, ipj_report_id report_id, void* report) {
	ipj_error error = E_IPJ_ERROR_SUCCESS;

	switch (report_id)
	{
		case E_IPJ_REPORT_ID_TAG_OPERATION_REPORT:
			error = ipj_util_tag_operation_report_handler(iri_device, (ipj_tag_operation_report*) report);
			break;
		case E_IPJ_REPORT_ID_STOP_REPORT:
			error = ipj_util_stop_report_handler(iri_device, (ipj_stop_report*) report);
			break;
		default:
			error = E_IPJ_ERROR_GENERAL_ERROR;
			break;
	}

	return error;
}

// Setup RFID module
// Parameters:
//   epcSize: Expected size, in bytes, of the EPC
//   tidSize: Expected size, in bytes, of the TID
void rfid_setup(uint16_t epcSize, uint16_t tidSize) {
	ipj_error eIpjError;
	ADI_GPIO_RESULT eGpioResult;

	ASSERT_RESULT(epcSize <= 64 && tidSize <= 64, true);

	expectedEpcSize = epcSize;
	expectedTidSize = tidSize;

	// Initialise RFID GPIO
	eGpioResult = adi_gpio_OutputEnable(RFID_ENABLE_PORT, RFID_ENABLE_PIN, true);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);

	// Reset RFID module
	eGpioResult = adi_gpio_SetLow(RFID_ENABLE_PORT, RFID_ENABLE_PIN);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	timer_sleepMs(RFID_RESET_TIME);
	eGpioResult = adi_gpio_SetHigh(RFID_ENABLE_PORT, RFID_ENABLE_PIN);
	ASSERT_RESULT(eGpioResult, ADI_GPIO_SUCCESS);
	timer_sleepMs(RFID_RESET_TIME);

	// Setup Impinj SDK
	eIpjError = ipj_initialize_iri_device(&iri_device);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);

	// Register Impinj SDK platform handlers
	unsigned int i;
	for (i = 0; i < (sizeof(event_handlers) / sizeof(event_handlers[0])); i++)
	{
		eIpjError = ipj_register_handler(
				&iri_device,
				event_handlers[i].type,
				event_handlers[i].handler);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	}

	// Register non-platform event handlers
	eIpjError = ipj_register_handler(&iri_device, E_IPJ_HANDLER_TYPE_REPORT, (void*)&ipj_util_report_handler);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);

	// Open UART and initialise Impinj SDK
	eIpjError = ipj_connect(&iri_device, NULL, E_IPJ_CONNECTION_TYPE_SERIAL, NULL);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);

	// Configure module region
	eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_REGION_ID, RFID_REGION);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);

	// Configure module transmit power
	eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_ANTENNA_TX_POWER, RFID_TX_POWER);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);

	// Configure read mode (Dense Reader Mode profile for ETSI operation)
	eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_RF_MODE, 2);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	
	if (tidSize > 0) {
		// Configure reader to read TID memory bank
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_TAG_OPERATION_ENABLE, true);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_TAG_OPERATION, E_IPJ_TAG_OPERATION_TYPE_READ);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_READ_MEM_BANK, E_IPJ_MEM_BANK_TID);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_READ_WORD_POINTER, 0x00);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_READ_WORD_COUNT, tidSize / 2);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	} else  {
		// Configure reader to ignore TID memory bank
		eIpjError = ipj_set_value(&iri_device, E_IPJ_KEY_TAG_OPERATION_ENABLE, false);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	}
}

// Start scanning for RFID tags
void rfid_startRead() {
	resultHashset = 0;

	// Clear the stopped flag
	ipj_stopped_flag = 0;

	ipj_error eIpjError = ipj_start(&iri_device, E_IPJ_ACTION_INVENTORY);
	ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
}

// Read the next RFID tag into a hashset
// Parameters:
//   h: Hashset to add the tag data to
void rfid_readNext(hashset *h) {
	if (!ipj_stopped_flag) {
		resultHashset = h;

		// Call ipj_receive to process tag reports
		ipj_error eIpjError = ipj_receive(&iri_device);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	}

	resultHashset = 0;
}

// Stop scanning for RFID tags
void rfid_stopRead() {
	// Stop inventory if it is still running
	if (!ipj_stopped_flag) {
		ipj_error eIpjError = ipj_stop(&iri_device, E_IPJ_ACTION_INVENTORY);
		ASSERT_RESULT(eIpjError, E_IPJ_ERROR_SUCCESS);
	}

	resultHashset = 0;
}
