/*
*  Impinj RFID Reader
*/

#ifndef RFID_H_
#define RFID_H_

#include <stdint.h>

#include <hashset.h>
#include <iri.h>

// RFID region
#define RFID_REGION 		E_IPJ_REGION_ETSI_EN_302_208_V1_4_1
// RFID transmit power
#define RFID_TX_POWER		2300

// Setup RFID module
// Parameters:
//   epcSize: Expected size, in bytes, of the EPC
//   tidSize: Expected size, in bytes, of the TID
void rfid_setup(uint16_t epcSize, uint16_t tidSize);

// Start scanning for RFID tags
void rfid_startRead();

// Read the next RFID tag into a hashset
// Parameters:
//   h: Hashset to add the tag data to
void rfid_readNext(hashset *h);

// Stop scanning for RFID tags
void rfid_stopRead();

#endif /* RFID_H_ */
