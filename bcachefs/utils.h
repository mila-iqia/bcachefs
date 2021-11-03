/* Include Guard */
#ifndef INCLUDE_UTILS_H
#define INCLUDE_UTILS_H

/**
 * Includes
 */

#include <stdio.h>

/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef __int64_t int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef __uint64_t uint64_t;

uint64_t benz_get_flag_bits(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit);

uint64_t benz_uintXX_as_uint64(const uint8_t *bytes, uint8_t sizeof_uint);

void benz_print_chars(const uint8_t *bytes, uint64_t len);
void benz_print_bytes(const uint8_t *bytes, uint64_t len);
void benz_print_bits(uint64_t bitfield);
void benz_print_hex(const uint8_t *hex, uint64_t len);

/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif