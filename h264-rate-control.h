/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _H264_RATE_CONTROL_H_
#define _H264_RATE_CONTROL_H_

#include <stdbool.h>

struct v4l2_encoder;

struct h264_rate_control {
	unsigned int bits_per_frame;
	unsigned int bits_per_gop;

	unsigned int bits_target;
	unsigned int bits_left;
	unsigned int gop_left;

	unsigned int bits_per_rlc_upscaled;

	bool cp_enabled;
	unsigned int cp_count;
	unsigned int cp_distance_mbs;

	unsigned int cp_target[10];
	int cp_target_error[6];
	int cp_qp_delta[7];

	unsigned int qp;
	unsigned int qp_sum;
	bool qp_intra_privilege;

	bool intra_request;
};


void h264_rate_control_feedback(struct v4l2_encoder *encoder,
				unsigned int bytes_used, unsigned int rlc_count,
				unsigned int qp_sum);
void h264_rate_control_step(struct v4l2_encoder *encoder);
int h264_rate_control_intra_request(struct v4l2_encoder *encoder);
int h264_rate_control_setup(struct v4l2_encoder *encoder);

#endif
