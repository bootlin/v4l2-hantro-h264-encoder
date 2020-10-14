/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <v4l2-encoder.h>
#include <h264-rate-control.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static uint32_t hantro_qp_estimation[2][11] = {
	/* Bitrate-related estimation factors. */
	{ 27, 44, 72, 119, 192, 314, 453, 653, 952, 1395, 0xffffffff },
	/* Corresponding QP values. */
	{ 51, 47, 43, 39, 35, 31, 27, 23, 19, 15, 11}
};

static unsigned int rlc_upscale = 256;

static unsigned int hantro_qp_inital_estimate(struct v4l2_encoder_setup *setup,
					      struct h264_rate_control *rc)
{
	uint32_t estimation;
	uint32_t pixels;
	uint32_t pixels_down;
	uint64_t upscale = 8000;
	unsigned int qp;
	unsigned int i;

	/* Very high bitrates are capped by minimal QP. */
	if (rc->bits_per_frame > 1000000)
		return setup->qp_min;

	pixels = 16 * 16 * setup->width_mbs * setup->height_mbs;
	pixels_down = pixels >> 8;

	/* Calculate a bitrate-related estimation value, taking in account the
	   number of pixels to encode. This seems highly hardware-specific. */
	estimation = rc->bits_per_frame >> 5;
	estimation *= pixels_down + 250;
	estimation /= 350 + 3 * pixels_down / 4;
	estimation = upscale * estimation / (pixels_down << 6);

	for (i = 0; hantro_qp_estimation[0][i] < estimation; i++)
		continue;

	qp = hantro_qp_estimation[1][i];

	if (qp > setup->qp_max)
		return setup->qp_max;
	else if (qp < setup->qp_min)
		return setup->qp_min;
	else
		return qp;
}

static void hantro_checkpoints_prepare(struct v4l2_encoder_setup *setup,
				       struct h264_rate_control *rc,
				       bool gop_start)
{
	unsigned int macroblocks;
	unsigned int rlc_target;
	unsigned int rlc_max;
	unsigned int error_base;
	unsigned int i;

	/* Don't apply checkpoints without statistics or at GOP start (intra
	 * frames), for which bitrate is best unconstrained. Also go easy on
	 * last GOP frames with insufficient leftover bits. */
	if (!rc->bits_per_rlc_upscaled || gop_start ||
	    (!rc->gop_left && rc->bits_target < rc->bits_per_frame)) {
		rc->cp_enabled = false;
		return;
	}

	macroblocks = setup->width_mbs * setup->height_mbs;

	/* H.264 has a maximum of 24 * 16 coefficients per macroblock. */
	rlc_max = setup->width_mbs * setup->height_mbs * 24 * 16;

	/* Calculate target number of coefficients based on target bits. */
	rlc_target = rc->bits_target * rlc_upscale / rc->bits_per_rlc_upscaled;

	if (rlc_target > rlc_max)
		rlc_target = rlc_max;

	/* Evenly spread target coefficients count across checkpoints. */
	for (i = 0; i < rc->cp_count; i++) {
		/* RLC target is / 32 to match hardware register expectations. */
		rc->cp_target[i] = ((i + 1) * rlc_target * rc->cp_distance_mbs /
				   macroblocks + 31) / 32;
	}

	/* Base error unit for QP delta ladder, set to a quarter of the RLC
	 * count interval between two checkpoints. */
	error_base = rlc_target * rc->cp_distance_mbs / macroblocks / 4;

	/* Target error is / 4 to match hardware register expectations. */
	/* Decrease QP (increase quality) for negative error. */
	rc->cp_qp_delta[0] = -3;
	rc->cp_target_error[0] = -error_base * 3 / 4;
	rc->cp_qp_delta[1] = -2;
	rc->cp_target_error[1] = -error_base * 2 / 4;
	rc->cp_qp_delta[2] = -1;
	rc->cp_target_error[2] = -error_base * 1 / 4;
	/* Keep QP for nearly no error. */
	rc->cp_qp_delta[3] = 0;
	rc->cp_target_error[3] = error_base * 1 / 4;
	/* Increase QP (decrease quality) for positive error. */
	rc->cp_qp_delta[4] = 1;
	rc->cp_target_error[4] = error_base * 2 / 4;
	rc->cp_qp_delta[5] = 2;
	rc->cp_target_error[5] = error_base * 3 / 4;
	rc->cp_qp_delta[6] = 3;

	rc->cp_enabled = true;
}

void h264_rate_control_feedback(struct v4l2_encoder *encoder,
				unsigned int bytes_used, unsigned int rlc_count,
				unsigned int qp_sum)
{
	struct v4l2_encoder_setup *setup;
	struct h264_rate_control *rc;
	unsigned int bits_used = bytes_used * 8;
	unsigned int macroblocks;
	unsigned int qp_average;

	setup = &encoder->setup;
	rc = &encoder->rc;

	macroblocks = setup->width_mbs * setup->height_mbs;
	qp_average = qp_sum / macroblocks;

	/* Collect statistics. */

	rc->qp_sum += qp_average;

	/* Calculate how many bits are used per non-zero coefficient, with an
	 * upscaling factor for precision. */
	rc->bits_per_rlc_upscaled = bits_used * rlc_upscale / rlc_count;

	/* For (privileged) intra frames, remove privilege and don't
	 * check for intra bit target error. */
	if (rc->qp_intra_privilege) {
		rc->qp += setup->qp_intra_delta;
		rc->qp_intra_privilege = false;
	}

	if (!rc->bits_left || bits_used >= rc->bits_left) {
		rc->bits_left = 0;

		/* Drastically increase QP for each over-bitrate frame in
		 * remaining GOP. */
		rc->qp += 2;
	} else if (bits_used < (7 * rc->bits_target / 8) && rc->qp) {
		rc->qp--;
	} else if (bits_used > (9 * rc->bits_target / 8)) {
		rc->qp++;
	}

	if (rc->qp < setup->qp_min)
		rc->qp = setup->qp_min;
	else if (rc->qp > setup->qp_max)
		rc->qp = setup->qp_max;

	if (rc->bits_left)
		rc->bits_left -= bits_used;
}

void h264_rate_control_step(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_setup *setup;
	struct h264_rate_control *rc;
	bool gop_start;

	if (!encoder)
		return;

	setup = &encoder->setup;
	rc = &encoder->rc;
	gop_start = !encoder->gop_index || rc->intra_request;

	if (gop_start) {
		/* Starting a new GOP. */
		rc->gop_left = setup->gop_size;

		/* Start from the previous GOP average QP. Otherwise, initial
		 * QP estimation is used or current QP for intra request. */
		if (rc->qp_sum && !rc->intra_request)
			rc->qp = rc->qp_sum / setup->gop_size;

		rc->qp_sum = 0;

		/* Apply intra QP delta privilege. */
		if (rc->qp > setup->qp_intra_delta)
			rc->qp -= setup->qp_intra_delta;
		else
			rc->qp = 0;

		rc->qp_intra_privilege = true;

		/* Keep the benefit of previous under-bitrate GOP. */
		rc->bits_left += rc->bits_per_gop;
		rc->bits_target = rc->bits_per_frame;
	} else if (!rc->bits_left) {
		/* Already out of bits to match bitrate. */
		rc->bits_target = 0;
	} else {
		/* Evenly split remaining bits for GOP inter frames. */
		rc->bits_target = rc->bits_left / rc->gop_left;

		/* Limit to 1.5x the average bits per frame. */
		if (rc->bits_target >  (2 * rc->bits_per_frame / 3))
			rc->bits_target = rc->bits_per_frame;
	}

	/* Checkpoint algorithm needs to care about last GOP frame. */
	rc->gop_left--;

	hantro_checkpoints_prepare(setup, rc, gop_start);

	if (rc->intra_request)
		rc->intra_request = false;
}

int h264_rate_control_intra_request(struct v4l2_encoder *encoder)
{
	struct h264_rate_control *rc;

	if (!encoder)
		return -EINVAL;

	rc = &encoder->rc;
	rc->intra_request = true;

	return 0;
}

int h264_rate_control_setup(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_setup *setup;
	struct h264_rate_control *rc;
	unsigned int cp_count;

	if (!encoder)
		return -EINVAL;

	setup = &encoder->setup;
	rc = &encoder->rc;

	memset(rc, 0, sizeof(*rc));

	/* Start with intra request to ensure GOP start. */
	rc->intra_request = true;

	rc->bits_per_frame = setup->bitrate * setup->fps_den / setup->fps_num;
	rc->bits_per_gop = rc->bits_per_frame * setup->gop_size;

	rc->qp = hantro_qp_inital_estimate(setup, rc);

	/* Checkpoints */

	cp_count = setup->height_mbs - 1;
	if (cp_count > ARRAY_SIZE(rc->cp_target))
		cp_count = ARRAY_SIZE(rc->cp_target);

	rc->cp_count = cp_count;
	rc->cp_distance_mbs = setup->width_mbs * setup->height_mbs /
			      (cp_count + 1);

	return 0;
}
