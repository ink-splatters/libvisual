/* Libvisual - The audio visualisation framework.
 *
 * Copyright (C) 2012      Libvisual team
 *               2004-2006 Dennis Smit
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"
#include "lv_mem.h"
#include "lv_common.h"
#include "lv_cpu.h"
#include "lv_bits.h"
#include <stdlib.h>
#include <string.h>

/* Standard C fallbacks */
static void *mem_set16_c (void *dest, int c, visual_size_t n);
static void *mem_set32_c (void *dest, int c, visual_size_t n);
static void *mem_copy_pitch_c (void *dest, const void *src, int pitch1, int pitch2, int width, int rows);

/* Optimal performance functions set by visual_mem_initialize(). */
VisMemCopyFunc visual_mem_copy = memcpy;
VisMemCopyPitchFunc visual_mem_copy_pitch = mem_copy_pitch_c;

VisMemSet8Func visual_mem_set = memset;
VisMemSet16Func visual_mem_set16 = mem_set16_c;
VisMemSet32Func visual_mem_set32 = mem_set32_c;


void visual_mem_initialize ()
{
    /* Nothing to do */
}

void *visual_mem_malloc (visual_size_t nbytes)
{
	void *buf;

	visual_return_val_if_fail (nbytes > 0, NULL);

	buf = malloc (nbytes);

	if (buf == NULL) {
		visual_log (VISUAL_LOG_ERROR, "Cannot get %" VISUAL_SIZE_T_FORMAT " bytes of memory", nbytes);

		return NULL;
	}

	return buf;
}

void *visual_mem_malloc0 (visual_size_t nbytes)
{
	void *buf;

	visual_return_val_if_fail (nbytes > 0, NULL);

	buf = visual_mem_malloc (nbytes);

	visual_mem_set (buf, 0, nbytes);

	return buf;
}

void *visual_mem_realloc (void *ptr, visual_size_t nbytes)
{
	return realloc (ptr, nbytes);
}

void visual_mem_free (void *ptr)
{
	free (ptr);
}

static void *mem_set16_c (void *dest, int c, visual_size_t n)
{
	uint64_t *u64_ptr = dest;

	uint16_t copy	 = c & 0xffff;
	uint32_t copy_x2 = copy | (copy << 16);
	uint64_t copy_x4 = copy_x2 | ((uint64_t)copy_x2 << 32);

	while (n >= 32) {
		u64_ptr[0] = copy_x4;
		u64_ptr[1] = copy_x4;
		u64_ptr[2] = copy_x4;
		u64_ptr[3] = copy_x4;
		u64_ptr[4] = copy_x4;
		u64_ptr[5] = copy_x4;
		u64_ptr[6] = copy_x4;
		u64_ptr[7] = copy_x4;
		n -= 32;
		u64_ptr += 8;
	}

	uint16_t* u16_ptr = (uint16_t *) u64_ptr;
	while (n--) {
		*u16_ptr++ = copy;
	}

	return dest;
}

static void *mem_set32_c (void *dest, int c, visual_size_t n)
{
	uint64_t *u64_ptr = dest;

	uint32_t copy = c;
	uint64_t copy_x2 = ((uint64_t) copy) | ((uint64_t) copy << 32);

	while (n >= 16) {
		u64_ptr[0] = copy_x2;
		u64_ptr[1] = copy_x2;
		u64_ptr[2] = copy_x2;
		u64_ptr[3] = copy_x2;
		u64_ptr[4] = copy_x2;
		u64_ptr[5] = copy_x2;
		u64_ptr[6] = copy_x2;
		u64_ptr[7] = copy_x2;
		n -= 16;
		u64_ptr += 8;
	}

	uint32_t* u32_ptr = (uint32_t *) u64_ptr;
	while (n--) {
		*u32_ptr++ = copy;
	}

	return dest;
}

static void *mem_copy_pitch_c (void *dest, const void *src, int pitch1, int pitch2, int row_bytes, int rows)
{
	uint8_t *d = dest;
	const uint8_t *s = src;
	int i;

	for (i = 0; i < rows; i++) {
		memcpy(d, s, row_bytes);

		d += pitch1;
		s += pitch2;
	}

	return dest;
}
