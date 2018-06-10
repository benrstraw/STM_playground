/*
 * fnv.h
 *
 *  Created on: Jun 10, 2018
 *      Author: Ben
 */

#ifndef FNV_FNV_H_
#define FNV_FNV_H_

#include <stdint.h>
#include <stddef.h>

uint32_t fnv1a_32(const uint8_t* data, size_t len);

#endif /* FNV_FNV_H_ */
