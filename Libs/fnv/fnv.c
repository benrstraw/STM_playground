/*
 * fnv.c
 *
 *  Created on: Jun 10, 2018
 *      Author: Ben
 */

#include "fnv.h"

const static uint32_t FNV1_PRIME_32 = 0x01000193;
const static uint32_t FNV1_BASIS_32 = 0x811c9dc5;

uint32_t fnv1a_32(const uint8_t* data, size_t len) {
	uint32_t hash = FNV1_BASIS_32;

	for (size_t i = 0; i < len; i++) {
		hash ^= data[i];
		hash *= FNV1_PRIME_32;
	}

	return hash;
}
