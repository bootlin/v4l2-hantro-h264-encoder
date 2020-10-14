/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <draw.h>
#include <csc.h>

int rgb2yuv420(struct draw_buffer *buffer, void *buffer_y, void *buffer_u,
	       void *buffer_v)
{
	unsigned int width, height, stride;
	unsigned int x, y;
	void *data;
	
	if (!buffer)
		return -EINVAL;

	data = buffer->data;
	width = buffer->width;
	height = buffer->height;
	stride = buffer->stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint8_t *crgb;
			uint8_t *cy;
			float value;

			crgb = data + stride * y + x * 4;
			value = crgb[2] * 0.299 + crgb[1] * 0.587 + crgb[0] * 0.114;

			cy = buffer_y + width * y + x;
			*cy = byte_range(value);

			if ((x % 2) == 0 && (y % 2) == 0) {
				uint8_t *cu;
				uint8_t *cv;

				value = crgb[2] * -0.14713 + crgb[1] * -0.28886 + crgb[0] * 0.436 + 128.;

				cu = buffer_u + width / 2 * y / 2 + x / 2;
				*cu = byte_range(value);

				value = crgb[2] * 0.615 + crgb[1] * -0.51499 + crgb[0] * -0.10001 + 128.;

				cv = buffer_v + width / 2 * y / 2 + x / 2;
				*cv = byte_range(value);
			}
		}
	}

	return 0;
}

int rgb2nv12(struct draw_buffer *buffer, void *buffer_y, void *buffer_uv)
{
	unsigned int width, height, stride;
	unsigned int x, y;
	void *data;

	if (!buffer)
		return -EINVAL;

	data = buffer->data;
	width = buffer->width;
	height = buffer->height;
	stride = buffer->stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint8_t *crgb;
			uint8_t *cy;
			float value;

			crgb = data + stride * y + x * 4;
			value = crgb[2] * 0.299 + crgb[1] * 0.587 + crgb[0] * 0.114;

			cy = buffer_y + width * y + x;
			*cy = byte_range(value);

			if ((x % 2) == 0 && (y % 2) == 0) {
				uint8_t *cu;
				uint8_t *cv;

				value = crgb[2] * -0.14713 + crgb[1] * -0.28886 + crgb[0] * 0.436 + 128.;

				cu = buffer_uv + width * y / 2 + x;
				*cu = byte_range(value);

				value = crgb[2] * 0.615 + crgb[1] * -0.51499 + crgb[0] * -0.10001 + 128.;

				cv = buffer_uv + width * y / 2 + x + 1;
				*cv = byte_range(value);
			}
		}
	}

	return 0;
}
