/* Libvisual - The audio visualisation framework.
 * 
 * Copyright (C) 2004, 2005, 2006 Dennis Smit <ds@nerds-incorporated.org>
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 * 	    Sepp Wijnands <sw@nerds-incorporated.org>
 *
 * $Id: lv_bmp.c,v 1.25 2006/01/22 13:23:37 synap Exp $
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

#include <config.h>
#include "lv_bmp.h"
#include "lv_common.h"
#include "lv_bits.h"

#include <stdio.h>
#include <string.h>

#define BI_RGB	0
#define BI_RLE8	1
#define BI_RLE4	2

static int load_uncompressed (FILE *fp, VisVideo *video, int depth);
static int load_rle (FILE *fp, VisVideo *video, int mode);

#if VISUAL_BIG_ENDIAN == 1
static void flip_byte_order (VisVideo *video)
{
	uint8_t *pixel = visual_video_get_pixels (video);
	unsigned int i;
	unsigned int pixel_count = visual_video_get_width (video) * visual_video_get_height (video);

	for (i = 0; i < pixel_count; i++) {
		pixel[0] = pixel[2];
		pixel[2] = pixel[0];
		pixel += 3;
	}
}
#endif /* VISUAL_BIG_ENDIAN */

static int load_uncompressed (FILE *fp, VisVideo *video, int depth)
{
	uint8_t *data;
	int i;
	int pad;

	pad = (4 - (visual_video_get_pitch (video) & 3)) & 3;
	data = (uint8_t *) visual_video_get_pixels (video) + (visual_video_get_height (video) * visual_video_get_pitch (video));

	switch (depth) {
		case 24:
		case 8:
			while (data > (uint8_t *) visual_video_get_pixels (video)) {
				data -= visual_video_get_pitch (video);

				if (fread (data, visual_video_get_pitch (video), 1, fp) != 1)
					goto err;

				if (pad)
					fseek (fp, pad, SEEK_CUR);
			}
			break;

		case 4:
			while (data > (uint8_t *) visual_video_get_pixels (video)) {
				/* Unpack 4 bpp pixels aka 2 pixels per byte */
				uint8_t *col = data - visual_video_get_pitch (video);
				uint8_t *end = (uint8_t *) ((intptr_t)data & ~1);
				data = col;

				while (col < end) {
					uint8_t p = fgetc (fp);
					*col++ = p >> 4;
					*col++ = p & 0xf;
				}

				if (visual_video_get_pitch (video) & 1)
					*col++ = fgetc (fp) >> 4;

				if (pad)
					fseek (fp, pad, SEEK_CUR);
			}
			break;

		case 1:
			while (data > (uint8_t *) visual_video_get_pixels (video)) {
				/* Unpack 1 bpp pixels aka 8 pixels per byte */
				uint8_t *col = data - visual_video_get_pitch (video);
				uint8_t *end = (uint8_t *) ((intptr_t)data & ~7);
				data = col;

				while (col < end) {
					uint8_t p = fgetc (fp);
					for (i=0; i < 8; i++) {
						*col++ = p >> 7;
						p <<= 1;
					}
				}

				if (visual_video_get_pitch (video) & 7) {
					uint8_t p = fgetc (fp);
					uint8_t count = visual_video_get_pitch (video) & 7;
					for (i=0; i < count; i++) {
						*col++ = p >> 7;
						p <<= 1;
					}
				}

				if (pad)
					fseek (fp, pad, SEEK_CUR);
			}
			break;
	}

	return VISUAL_OK;

err:
	visual_log (VISUAL_LOG_ERROR, "Bitmap data is not complete");

	return -VISUAL_ERROR_BMP_CORRUPTED;
}

static int load_rle (FILE *fp, VisVideo *video, int mode)
{
	uint8_t *col, *end;
	uint8_t p;
	int c, y, k, pad;
	int processing = 1;

	end = (uint8_t *)visual_video_get_pixels (video) + (visual_video_get_height (video) * visual_video_get_pitch (video));
	col = end - visual_video_get_pitch (video);
	y = visual_video_get_height (video) - 1;

	do {
		if ((c = fgetc (fp)) == EOF)
			goto err;

		if (c) {
			if (y < 0)
				goto err;

			/* Encoded mode */
			p = fgetc (fp); /* Color */
			if (mode == BI_RLE8) {
				while (c-- && col < end)
					*col++ = p;
			} else {
				k = c >> 1; /* Even count */
				while (k-- && col < end - 1) {
					*col++ = p >> 4;
					*col++ = p & 0xf;
				}

				if (c & 1 && col < end)
					*col++ = p >> 4;
			}
			continue;
		}

		/* Escape sequence */
		c = fgetc (fp);
		switch (c) {
			case EOF:
				goto err;

			case 0: /* End of line */
				y--;
				col = (uint8_t *) visual_video_get_pixels (video) + visual_video_get_pitch (video) * y;

				/* Normally we would error here if y < 0.
				 * However, some encoders apparently emit an
				 * End-Of-Line sequence at the very end of a bitmap.
				 */
				break;

			case 1: /* End of bitmap */
				processing = 0;
				break;

			case 2: /* Delta */
				/* X Delta */
				col += (uint8_t) fgetc (fp);

				/* Y Delta */
				c = (uint8_t) fgetc (fp);
				col -= c * visual_video_get_pitch (video);
				y -= c;

				if (col < (uint8_t *)visual_video_get_pixels (video))
					goto err;

				break;

			default: /* Absolute mode: 3 - 255 */
				if (mode == BI_RLE8) {
					pad = c & 1;
					while (c-- && col < end)
						*col++ = fgetc (fp);
				} else {
					pad = ((c + 1) >> 1) & 1;
					k = c >> 1; /* Even count */
					while (k-- && col < end - 1) {
						p = fgetc (fp);
						*col++ = p >> 4;
						*col++ = p & 0xf;
					}

					if (c & 1 && col < end)
						*col++ = fgetc (fp) >> 4;
				}

				if (pad)
					fgetc (fp);
				break;

		}
	} while (processing);

	return VISUAL_OK;

err:
	visual_log (VISUAL_LOG_ERROR, "Bitmap data is not complete");

	return -VISUAL_ERROR_BMP_CORRUPTED;
}


VisVideo *visual_bitmap_load (const char *filename)
{
	/* The win32 BMP header */
	char magic[2];
	uint32_t bf_size = 0;
	uint32_t bf_bits = 0;

	/* The win32 BITMAPINFOHEADER */
	int32_t bi_size = 0;
	int32_t bi_width = 0;
	int32_t bi_height = 0;
	int16_t bi_bitcount = 0;
	uint32_t bi_compression;
	uint32_t bi_clrused;

	/* File read vars */
	FILE *fp;

	/* Worker vars */
	uint8_t depth = 24;
	int32_t error = 0;
	int i;

	VisVideo *video = NULL;
	VisPalette *palette = NULL;

	fp = fopen (filename, "rb");
	if (fp == NULL) {
		visual_log (VISUAL_LOG_WARNING, "Bitmap file not found: %s", filename);
		return NULL;
	}

	/* Read the magic string */
	fread (magic, 2, 1, fp);
	if (strncmp (magic, "BM", 2) != 0) {
		visual_log (VISUAL_LOG_WARNING, "Not a bitmap file");
		fclose (fp);
		return NULL;
	}

	/* Read the file size */
	fread (&bf_size, 4, 1, fp);
	bf_size = VISUAL_ENDIAN_LEI32 (bf_size);

	/* Skip past the reserved bits */
	fseek (fp, 4, SEEK_CUR);

	/* Read the offset bits */
	fread (&bf_bits, 4, 1, fp);
	bf_bits = VISUAL_ENDIAN_LEI32 (bf_bits);

	/* Read the info structure size */
	fread (&bi_size, 4, 1, fp);
	bi_size = VISUAL_ENDIAN_LEI32 (bi_size);

	if (bi_size == 12) {
		/* And read the width, height */
		fread (&bi_width, 2, 1, fp);
		fread (&bi_height, 2, 1, fp);
		bi_width = VISUAL_ENDIAN_LEI16 (bi_width);
		bi_height = VISUAL_ENDIAN_LEI16 (bi_height);

		/* Skip over the planet */
		fseek (fp, 2, SEEK_CUR);

		/* Read the bits per pixel */
		fread (&bi_bitcount, 2, 1, fp);
		bi_bitcount = VISUAL_ENDIAN_LEI16 (bi_bitcount);
		bi_compression = BI_RGB;
	} else {
		/* And read the width, height */
		fread (&bi_width, 4, 1, fp);
		fread (&bi_height, 4, 1, fp);
		bi_width = VISUAL_ENDIAN_LEI32 (bi_width);
		bi_height = VISUAL_ENDIAN_LEI32 (bi_height);

		/* Skip over the planet */
		fseek (fp, 2, SEEK_CUR);

		/* Read the bits per pixel */
		fread (&bi_bitcount, 2, 1, fp);
		bi_bitcount = VISUAL_ENDIAN_LEI16 (bi_bitcount);

		/* Read the compression flag */
		fread (&bi_compression, 4, 1, fp);
		bi_compression = VISUAL_ENDIAN_LEI32 (bi_compression);

		/* Skip over the nonsense we don't want to know */
		fseek (fp, 12, SEEK_CUR);

		/* Number of colors in palette */
		fread (&bi_clrused, 4, 1, fp);
		bi_clrused = VISUAL_ENDIAN_LEI32 (bi_clrused);

		/* Skip over the other nonsense */
		fseek (fp, 4, SEEK_CUR);
	}

	/* Check if we can handle it */
	if (bi_bitcount != 1 && bi_bitcount != 4 && bi_bitcount != 8 && bi_bitcount != 24) {
		visual_log (VISUAL_LOG_ERROR, "Only bitmaps with 1, 4, 8 or 24 bits per pixel are supported");
		fclose (fp);
		return NULL;
	}

	if (bi_compression > 3) {
		visual_log (VISUAL_LOG_ERROR, "Bitmap uses an invalid or unsupported compression scheme");
		fclose (fp);
		return NULL;
	}

	/* Load the palette */
	if (bi_bitcount < 24) {
		if (bi_clrused == 0) {
			/* When the colors used variable is zero, use the
			 * maximum number of palette colors allowed for the specified depth. */
			bi_clrused = 1 << bi_bitcount;
		}

		/* Always allocate 256 palette entries.
		 * Depth transformation depends on this */
		palette = visual_palette_new (256);

		VisColor *colors = visual_palette_get_colors (visual_video_get_palette (video));

		if (bi_size == 12) {
			for (i = 0; i < bi_clrused; i++) {
				colors[i].b = fgetc (fp);
				colors[i].g = fgetc (fp);
				colors[i].r = fgetc (fp);
			}
		} else {
			for (i = 0; i < bi_clrused; i++) {
				colors[i].b = fgetc (fp);
				colors[i].g = fgetc (fp);
				colors[i].r = fgetc (fp);
				fseek (fp, 1, SEEK_CUR);
			}
		}
	}

	/* Use 8 bpp for all bit depths under 24 bits */
	if (bi_bitcount < 24)
		depth = 8;

	/* Make the target VisVideo ready for use */
	video = visual_video_new_with_buffer (bi_width, bi_height, visual_video_depth_enum_from_value (depth));

	if (palette)
		visual_video_set_palette (video, palette);

	/* Set to the beginning of image data, note that MickeySoft likes stuff upside down .. */
	fseek (fp, bf_bits, SEEK_SET);

	/* Load image data */
	switch (bi_compression) {
		case BI_RGB:
			error = load_uncompressed (fp, video, bi_bitcount);
#if VISUAL_BIG_ENDIAN == 1
			if (error == VISUAL_OK)
				flip_byte_order (video);
#endif
			break;

		case BI_RLE4:
			error = load_rle (fp, video, BI_RLE4);
			break;

		case BI_RLE8:
			error = load_rle (fp, video, BI_RLE8);
			break;
	}

	fclose (fp);

	if (error != VISUAL_OK) {
		visual_video_unref (video);
		return NULL;
	}

	return video;
}
