/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _BITSTREAM_H_
#define _BITSTREAM_H_

struct bitstream {
	void *buffer;
	unsigned int length;

	unsigned int offset_bytes;
	unsigned int offset_bits;
};

struct bitstream *bitstream_create(void);
void bitstream_destroy(struct bitstream *bitstream);
int bitstream_reset(struct bitstream *bitstream);
int bitstream_append_bits(struct bitstream *bitstream, uint32_t bits,
			  unsigned int bits_count);
int bitstream_append_ue(struct bitstream *bitstream, uint32_t value);
int bitstream_append_se(struct bitstream *bitstream, int32_t value);
int bitstream_align(struct bitstream *bitstream);

#endif
