/*
 * tifs_pke4_random.c SOC specific File
 *
 * This file contains base addresses for PKE
 *
 * Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 *
 */

/*
* Copyright 2024-25 by Cryptography Research, Inc. (Rambus)
* All rights reserved.  Unauthorized use (including, without limitation,
* distribution and copying) is strictly prohibited.  All use
* requires, and is subject to, explicit written authorization and
* nondisclosure agreements with your supplier or Cryptography Research (Rambus). 
*/

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>


#include "pke4_driver.h"
#include "pke4_reg.h"

#include "pke4_random.h"

#include <crypto/rng/rng.h>

extern RNG_Handle     pke_rng_handle;

/**
 * \brief Compute MAU length (in words) and bit-width for a requested byte length.
 *
 * Extracted to reduce path count in cri_pke_get_pseudo_random.
 *
 * \param len       Requested byte length
 * \param p_bits    Output: bit width corresponding to the computed length
 * \return          MAU word length to use
 */
static uint32_t pke_compute_length_and_bits(size_t len, uint32_t *p_bits);
static int32_t pke_issue_rng_command(void *buf, size_t len, int32_t slot, uint32_t slot_len, uint32_t length, uint32_t bits);

static uint32_t pke_compute_length_and_bits(size_t len, uint32_t *p_bits)
{
    uint32_t length;

#ifdef CRI_PKE_32_BIT
    if (len < (MAU_READ_REG(R_MAU_MIN_LEN) * sizeof(uint32_t))) {
        length = MAU_READ_REG(R_MAU_MIN_LEN);
        if (length > MAU_READ_REG(R_MAU_MAX_LEN)) {
            length = MAU_READ_REG(R_MAU_MAX_LEN);
        }
    } else {
        length = ((len - 1U) / sizeof(uint32_t)) + 1U;
    }
    if (length > MAU_READ_REG(R_MAU_MAX_LEN)) {
        length = MAU_READ_REG(R_MAU_MAX_LEN);
    }
    *p_bits = length * 32U;
#else /* 64-bit */
    if (len < (MAU_READ_REG(R_MAU_MIN_LEN) * sizeof(uint64_t))) {
        length = MAU_READ_REG(R_MAU_MIN_LEN);
        if (length > MAU_READ_REG(R_MAU_MAX_LEN)) {
            length = MAU_READ_REG(R_MAU_MAX_LEN);
        }
    } else {
        length = ((len - 1U) / sizeof(uint64_t)) + 1U;
    }
    if (length > MAU_READ_REG(R_MAU_MAX_LEN)) {
        length = MAU_READ_REG(R_MAU_MAX_LEN);
    }
    *p_bits = length * 64U;
#endif

    return length;
}

int32_t cri_pke_get_true_random(void *buf, size_t len)
{
	size_t i;
	uint32_t rand_val[4U];
	int32_t ret = 0;

	for(i = 0; i < len; i = ((len-i) > 16U) ? (i+16U) : len)
	{
		if((len-i)<16U)
		{
			if (RNG_read(pke_rng_handle, &rand_val[0]) != RNG_RETURN_SUCCESS) {
				ret = -1;
			}
			(void)memcpy((uint8_t *)buf + i, (uint8_t *)rand_val, (len-i));
		}
		else
		{
			if (RNG_read(pke_rng_handle, &rand_val[0]) != RNG_RETURN_SUCCESS) {
				ret = -1;
			}
			((uint32_t *)buf + (i/sizeof(uint32_t)))[0U] = rand_val[0U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[1U] = rand_val[1U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[2U] = rand_val[2U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[3U] = rand_val[3U];
		}
	}

	return ret;
}

/* Prevent the compiler from recognizing copy loops below as a memcpy
 * idiom and lowering them to a library call.
 */
__attribute__((no_builtin))
static int32_t pke_issue_rng_command(void *buf, size_t len, int32_t slot, uint32_t slot_len, uint32_t length, uint32_t bits)
{
	int32_t ret;
	uint32_t *src, *dst;
	size_t numWords, remBytes, i;

	ISSUE_MAU_COMMAND(SET_RAM_SLOTS, MAU_SRAM_OFFSET, slot_len);
	if (slot == CRI_PKE_NO_SLOT) {
		ISSUE_MAU_COMMAND(SET_MAND, SLOT(0), length);
	} else {
		ISSUE_MAU_COMMAND(SET_MAND, SLOT(slot), length);
	}
	ISSUE_MAU_COMMAND(COPY, R_MAU_ADDR_RNG, length);

	ret = cri_pke_wait();

	if (ret == 0) {
		if (buf != NULL) {
			
			dst = (uint32_t *)buf;
			numWords = len / sizeof(uint32_t);
			remBytes = len % sizeof(uint32_t);

			if (slot == CRI_PKE_NO_SLOT) {
				src = pke_addr(0, NULL, bits);
			} else {
				src = pke_addr((uint32_t)slot, NULL, bits);
			}

			for (i = 0U; i < numWords; i++) {
				dst[i] = src[i];
			}

			if (remBytes != 0U) {
				uint32_t lastWord = src[numWords];

				(void)memcpy(&dst[numWords], &lastWord, remBytes);
			}
		}
	}

	return ret;
}

int32_t cri_pke_get_pseudo_random(void *buf, size_t len, int32_t slot, uint32_t slot_length)
{
	uint32_t bits = 0U;
	uint32_t slot_len;

	/* Compute MAU word length and bit-width (platform-specific logic isolated in helper) */
	uint32_t length = pke_compute_length_and_bits(len, &bits);

	if (slot_length < MAU_READ_REG(R_MAU_MIN_LEN)) {
		slot_len = MAU_READ_REG(R_MAU_MIN_LEN);
	} else {
		slot_len = slot_length;
	}

	return pke_issue_rng_command(buf, len, slot, slot_len, length, bits);
}
