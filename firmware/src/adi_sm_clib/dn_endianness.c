/*
*  SmartMesh SDK endianness implementation for IoTechnics RFID Solution Board
*/

#include <dn_endianness.h>

// Little-endian platform

// Encode a 16-bit unsigned integer
void dn_write_uint16_t(uint8_t* ptr, uint16_t val) {
   ptr[0] = (val>>8)  & 0xff;
   ptr[1] = (val>>0)  & 0xff;
}

// Encode a 32-bit unsigned integer
void dn_write_uint32_t(uint8_t* ptr, uint32_t val) {
   ptr[0] = (val>>24) & 0xff;
   ptr[1] = (val>>16) & 0xff;
   ptr[2] = (val>>8)  & 0xff;
   ptr[3] = (val>>0)  & 0xff;
}

// Decode a 16-bit unsigned integer
void dn_read_uint16_t(uint16_t* to, uint8_t* from) {
   *to = 0;
   *to |= (from[1]<<0);
   *to |= (from[0]<<8);
}

// Decode a 32-bit unsigned integer
void dn_read_uint32_t(uint32_t* to, uint8_t* from) {
   *to = 0;
   *to |= (((uint32_t)from[3])<<0);
   *to |= (((uint32_t)from[2])<<8);
   *to |= (((uint32_t)from[1])<<16);
   *to |= (((uint32_t)from[0])<<24);
}
