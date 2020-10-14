/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <bitstream.h>
#include <unit.h>

uint8_t start_code_prefix[] = { 0x00, 0x00, 0x00, 0x01 };
unsigned int start_code_prefix_size = sizeof(start_code_prefix);

uint8_t forbidden_pattern_mask[] = { 0xff, 0xff, 0xfc };
uint8_t forbidden_pattern_match[] = { 0x00, 0x00, 0x00 };
unsigned int forbidden_pattern_size = sizeof(forbidden_pattern_mask);

struct unit *unit_pack(struct bitstream *bitstream)
{
	struct unit *unit = NULL;
	unsigned int unit_length;
	unsigned int i, j;
	unsigned int bitstream_length;
	uint8_t *bitstream_values;
	uint8_t *unit_values;

	if (!bitstream)
		return NULL;

	bitstream_align(bitstream);

	bitstream_length = bitstream->offset_bytes;
	bitstream_values = bitstream->buffer;
	unit_length = start_code_prefix_size + bitstream_length;

	for (i = 0; i < (bitstream_length - forbidden_pattern_size + 1); i++) {
		bool forbidden_pattern_found = true;

		for (j = 0; j < forbidden_pattern_size; j++) {
			if ((bitstream_values[j] & forbidden_pattern_mask[j]) !=
			    forbidden_pattern_match[j]) {
				forbidden_pattern_found = false;
				break;
			}
		}

		if (forbidden_pattern_found) {
			unit_length++;

			/* Skip the forbidden pattern. */
			i += forbidden_pattern_size - 1;
			bitstream_values += forbidden_pattern_size - 1;
		}

		bitstream_values++;
	}

	bitstream_values = bitstream->buffer + bitstream_length - 1;

	if (!*bitstream_values)
		unit_length++;

	unit = calloc(1, sizeof(*unit));
	if (!unit)
		goto error;

	unit->buffer = calloc(1, unit_length);
	unit->length = unit_length;

	bitstream_values = bitstream->buffer;
	unit_values = unit->buffer;

	/* Add the start code prefix. */
	for (i = 0; i < start_code_prefix_size; i++)
		*unit_values++ = start_code_prefix[i];

	/* Escape any forbidden pattern. */
	for (i = 0; i < (bitstream_length - forbidden_pattern_size + 1); i++) {
		bool forbidden_pattern_found = true;

		for (j = 0; j < forbidden_pattern_size; j++) {
			if ((bitstream_values[j] & forbidden_pattern_mask[j]) !=
			    forbidden_pattern_match[j]) {
				forbidden_pattern_found = false;
				break;
			}
		}

		if (forbidden_pattern_found) {
			for (j = 0; j < (forbidden_pattern_size - 1); j++) {
				*unit_values++ = *bitstream_values++;
				i++;
			}

			*unit_values++ = 0x03;
		}

		*unit_values++ = *bitstream_values++;
	}

	/* Copy leftover data */
	for (; i < bitstream_length; i++)
		*unit_values++ = *bitstream_values++;

	bitstream_values = bitstream->buffer + bitstream_length - 1;

	/* Escape a forbidden final zero byte. */
	if (!*bitstream_values)
		*unit_values++ = 0x03;

	return unit;

error:
	if (unit)
		free(unit);

	return NULL;
}

void unit_destroy(struct unit *unit)
{
	if (!unit)
		return;

	if (unit->buffer)
		free(unit->buffer);

	free(unit);
}
