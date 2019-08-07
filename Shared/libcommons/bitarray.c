/*
 * Copyright (C) 2012 Sistemas Operativos - UTN FRBA. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bitarray.h"
#include "Malloc.h"
#include <stddef.h>

/* array index for character containing bit */
static inline uint8_t _bit_in_char(size_t bit, bit_numbering_t mode)
{
    switch (mode)
    {
        case MSB_FIRST:
            // 0x80: 1000 0000 binario
            return (((uint8_t) 0x80) >> (bit % BYTE_BITS));
        case LSB_FIRST:
        default:
            // 0x01: 0000 0001 binario
            return (((uint8_t) 0x01) << (bit % BYTE_BITS));
    }
}

t_bitarray* bitarray_create(uint8_t* bitarray, size_t size)
{
    return bitarray_create_with_mode(bitarray, size, LSB_FIRST);
}

t_bitarray* bitarray_create_with_mode(uint8_t* bitarray, size_t size, bit_numbering_t mode)
{
    t_bitarray* self = Malloc(sizeof(t_bitarray));

    self->bitarray = bitarray;
    self->size = size;
    self->mode = mode;

    return self;
}

bool bitarray_test_bit(t_bitarray* self, size_t bit_index)
{
    return ((self->bitarray[BIT_CHAR(bit_index)] & _bit_in_char(bit_index, self->mode)) != 0);
}

void bitarray_set_bit(t_bitarray* self, size_t bit_index)
{
    self->bitarray[BIT_CHAR(bit_index)] |= _bit_in_char(bit_index, self->mode);
}

void bitarray_clean_bit(t_bitarray* self, size_t bit_index)
{
    /* create a mask to zero out desired bit */
    uint8_t const mask = ~_bit_in_char(bit_index, self->mode);
    self->bitarray[BIT_CHAR(bit_index)] &= mask;
}

size_t bitarray_get_max_bit(t_bitarray* self)
{
    return self->size * BYTE_BITS;
}

void bitarray_destroy(t_bitarray* self)
{
    Free(self);
}
