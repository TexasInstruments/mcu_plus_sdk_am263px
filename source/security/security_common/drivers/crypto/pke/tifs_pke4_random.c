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

int32_t cri_pke_get_true_random(void *buf, size_t len)
{
	size_t i;
	uint32_t rand_val[4U];

	for(i = 0; i < len; i=i+16U)
	{
		if((len-i)<16U)
		{
			(void)RNG_read(pke_rng_handle, &rand_val[0]);
			if((len-i)%4U == 0U)
			{
				(void)memcpy((uint8_t *)buf + i, rand_val, (len-i));
			}
			else
			{
				if((len-i)/4U > 0U)
				{
					((uint32_t *)buf + (i/sizeof(uint32_t)))[0U] = rand_val[0U];
				}
				if((len-i)/4U > 1U)
				{
					((uint32_t *)buf + (i/sizeof(uint32_t)))[1U] = rand_val[1U];
				}
				if((len-i)/4U > 2U)
				{
					((uint32_t *)buf + (i/sizeof(uint32_t)))[2U] = rand_val[2U];
				}
				if((len-i)/4U > 3U)
				{
					((uint32_t *)buf + (i/sizeof(uint32_t)))[3U] = rand_val[3U];
				}
			}
		}
		else
		{
			(void)RNG_read(pke_rng_handle, &rand_val[0]);
			((uint32_t *)buf + (i/sizeof(uint32_t)))[0U] = rand_val[0U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[1U] = rand_val[1U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[2U] = rand_val[2U];
			((uint32_t *)buf + (i/sizeof(uint32_t)))[3U] = rand_val[3U];
		}
	}
		
	return 0;
}

int32_t cri_pke_get_pseudo_random(void *buf, size_t len, int32_t slot, uint32_t slot_length)
{
	int32_t ret = 0;  

	uint32_t length = 0;
	uint32_t bits;
	uint32_t slot_len;

#ifdef CRI_PKE_32_BIT
	if (len < MAU_READ_REG(R_MAU_MIN_LEN) * sizeof(uint32_t)) {
		length = MAU_READ_REG(R_MAU_MIN_LEN);
	} else {
		length = ((len - 1) / sizeof(uint32_t)) + 1;
	}
	bits = length * 32;
#else /* 64-bit */
	if (len < MAU_READ_REG(R_MAU_MIN_LEN) * sizeof(uint64_t)) {
		length = MAU_READ_REG(R_MAU_MIN_LEN);
	} else {
		length = ((len - 1U) / sizeof(uint64_t)) + 1U;
	}
	bits = length * 64U;
#endif

	if (slot_length < MAU_READ_REG(R_MAU_MIN_LEN)) {
		slot_len = MAU_READ_REG(R_MAU_MIN_LEN);
	} else {
		slot_len = slot_length;
	}

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
			if (slot == CRI_PKE_NO_SLOT) {
				(void)memcpy(buf, pke_addr(0, NULL, bits), len);
			} else {
				(void)memcpy(buf, pke_addr(slot, NULL, bits), len);
			}
		}
	}

	return ret;
}
