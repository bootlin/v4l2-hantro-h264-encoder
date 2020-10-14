/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <bitstream.h>

struct bitstream *bitstream_create(void)
{
	struct bitstream *bitstream = NULL;

	bitstream = calloc(1, sizeof(*bitstream));
	if (!bitstream)
		goto error;

	bitstream->length = 1024;
	bitstream->buffer = calloc(1, bitstream->length);
	if (!bitstream->buffer)
		goto error;

	return bitstream;

error:
	if (bitstream) {
		if (bitstream->buffer)
			free(bitstream->buffer);

		free(bitstream);
	}

	return NULL;
}

void bitstream_destroy(struct bitstream *bitstream)
{
	if (!bitstream)
		return;

	if (bitstream->buffer)
		free(bitstream->buffer);

	free(bitstream);
}

int bitstream_reset(struct bitstream *bitstream)
{
	if (!bitstream)
		return -EINVAL;

	memset(bitstream->buffer, 0, bitstream->length);

	bitstream->offset_bytes = 0;
	bitstream->offset_bits = 0;

	return 0;
}

int bitstream_append_bits(struct bitstream *bitstream, uint32_t bits,
			  unsigned int bits_count)
{
	uint8_t *buffer;
	unsigned int buffer_bits_left;
	unsigned int bits_left = bits_count;
	unsigned int bits_offset = 0;

	if (!bitstream || bits_count > 32)
		return -EINVAL;

	buffer = bitstream->buffer + bitstream->offset_bytes;

	while (bits_left > 0) {
		uint32_t chunk;
		uint32_t base;
		uint32_t shift;
		uint32_t bits_masked;

		buffer = bitstream->buffer + bitstream->offset_bytes;

		buffer_bits_left = 8 - bitstream->offset_bits;
		chunk = bits_left < buffer_bits_left ? bits_left : buffer_bits_left;

		base = (1 << chunk) - 1;
		shift = bits_count - bits_offset - chunk;
		bits_masked = (bits & (base << shift)) >> shift;
		*buffer |= bits_masked << (8 - bitstream->offset_bits - chunk);

		bitstream->offset_bits += chunk;
		bits_left -= chunk;
		bits_offset += chunk;

		bitstream->offset_bytes += bitstream->offset_bits / 8;
		bitstream->offset_bits = bitstream->offset_bits % 8;
	}

	return 0;
}

int bitstream_append_ue(struct bitstream *bitstream, uint32_t value)
{
	uint32_t value_bit_count = 0;
	uint32_t zero_bit_count;

	if (value >= 0x7fffffff)
		return -EINVAL;

	value++;

	while (value >> value_bit_count)
		value_bit_count++;

	zero_bit_count = value_bit_count - 1;

	bitstream_append_bits(bitstream, 0, zero_bit_count);
	bitstream_append_bits(bitstream, value, value_bit_count);

	return 0;
}

int bitstream_append_se(struct bitstream *bitstream, int32_t value)
{
	uint32_t value_ue;

	if (value > 0)
		value_ue = (uint32_t)(2 * value - 1);
	else
		value_ue = (uint32_t)(-2 * value);

	return bitstream_append_ue(bitstream, value_ue);
}

int bitstream_align(struct bitstream *bitstream)
{
	if (!bitstream->offset_bits)
		return 0;

	bitstream_append_bits(bitstream, 0, 8 - bitstream->offset_bits);

	return 0;
}
