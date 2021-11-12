#include <stdio.h>

#include "utils.h"


inline uint64_t benz_get_flag_bits(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit)
{
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

uint64_t benz_uintXX_as_uint64(const uint8_t *bytes, uint8_t sizeof_uint)
{
    switch (sizeof_uint)
    {
    case 64:
        return *(const uint64_t*)(const void*)bytes;
    case 32:
        return *(const uint32_t*)(const void*)bytes;
    case 16:
        return *(const uint16_t*)(const void*)bytes;
    case 8:
        return *(const uint8_t*)bytes;
    }
    return (uint64_t)-1;
}

void benz_print_chars(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        printf("%c", bytes[i]);
    }
}

void benz_print_bytes(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        if (i && i % 4 == 0)
        {
            printf(" ");
        }
        if (i && i % 32 == 0)
        {
            printf("\n");
        }
        benz_print_hex(bytes + i, 1);
    }
}

void benz_print_bits(uint64_t bitfield)
{
    uint8_t* bytes = (uint8_t*)&bitfield;
    for (int i = 0, e = sizeof(bitfield) / sizeof(uint8_t); i < e; ++i)
    {
        for (int j = sizeof(uint8_t) * 8; j > 0; --j)
        {
            if (bytes[i] & 128)
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
            bytes[i] <<= 1;
        }
        printf(" ");
    }
}

void benz_print_hex(const uint8_t *hex, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        printf("%02x", hex[i]);
    }
}
