/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _UNIT_H_
#define _UNIT_H_

struct bitstream;

struct unit {
	void *buffer;
	unsigned int length;
};

extern uint8_t start_code_prefix[];
extern unsigned int start_code_prefix_size;

struct unit *unit_pack(struct bitstream *bitstream);
void unit_destroy(struct unit *unit);

#endif
