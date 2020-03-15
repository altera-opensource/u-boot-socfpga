/*
 * Copyright (C) 2017 - This file is part of libecc project
 *
 * Authors:
 *     Ryad BENADJILA <ryadbenadjila@gmail.com>
 *     Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *     Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 * Contributors:
 *     Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *     Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 * https://github.com/ANSSI-FR/libecc/blob/master/src/hash/sha3-384.h
 *
 * This software is licensed under a dual BSD and GPL v2 license.
 *
 * GPLv2 License summary
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * BSD License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SHA384_H__
#define __SHA384_H__

#include "sha2.h"

#define SHA384_STATE_SIZE   8
#define SHA384_BLOCK_SIZE   128
#define SHA384_DIGEST_SIZE  48

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE 0
#endif
#if (MAX_DIGEST_SIZE < SHA384_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE SHA384_DIGEST_SIZE
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < SHA384_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE SHA384_BLOCK_SIZE
#endif

typedef struct {
	/* Number of bytes processed on 128 bits */
	u64 sha384_total[2];
	/* Internal state */
	u64 sha384_state[SHA384_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 sha384_buffer[SHA384_BLOCK_SIZE];
} sha384_context;

void sha384_init(sha384_context *ctx);
void sha384_update(sha384_context *ctx, const u8 *input, u32 ilen);
void sha384_final(sha384_context *ctx, u8 output[SHA384_DIGEST_SIZE]);
void sha384_csum_wd(const unsigned char *input, unsigned int ilen,
		    unsigned char *output, unsigned int chunk_sz);
#endif /* __SHA384_H__ */
