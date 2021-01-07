/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <v4l2-encoder.h>
#include <bitstream.h>
#include <unit.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static void bitstream_sps(struct bitstream *bitstream,
			  struct v4l2_encoder *encoder)
{
	struct v4l2_ctrl_h264_sps *sps = &encoder->sps;
	bool frame_cropping_flag = false;

	bitstream_reset(bitstream);

	/* NALU header */
	bitstream_append_bits(bitstream, 0, 1);
	bitstream_append_bits(bitstream, 3, 2);
	bitstream_append_bits(bitstream, 7, 5);

	bitstream_append_bits(bitstream, sps->profile_idc, 8);
	/* constraint_set0_flag */
	/* fixed by hardware */
	bitstream_append_bits(bitstream, 1, 1);
	/* constraint_set1_flag */
	/* fixed by hardware */
	bitstream_append_bits(bitstream, 1, 1);
	/* constraint_set2_flag */
	bitstream_append_bits(bitstream, 0, 1);
	/* constraint_setn_flag + reserved_zero_2bits */
	bitstream_append_bits(bitstream, 0, 5);
	bitstream_append_bits(bitstream, sps->level_idc, 8);
	bitstream_append_ue(bitstream, sps->seq_parameter_set_id);

	switch (sps->profile_idc) {
	case 100:
	case 110:
	case 122:
	case 244:
	case 44:
	case 83:
	case 86:
	case 118:
	case 128:
	case 138:
	case 139:
	case 134:
	case 135:
		bitstream_append_ue(bitstream, sps->chroma_format_idc);
		bitstream_append_ue(bitstream, sps->bit_depth_luma_minus8);
		bitstream_append_ue(bitstream, sps->bit_depth_chroma_minus8);
		/* qpprime_y_zero_transform_bypass_flag */
		bitstream_append_bits(bitstream, 0, 1);
		/* seq_scaling_matrix_present_flag */
		bitstream_append_bits(bitstream, 0, 1);
		break;
	default:
		break;
	}

	bitstream_append_ue(bitstream, sps->log2_max_frame_num_minus4);
	bitstream_append_ue(bitstream, sps->pic_order_cnt_type);

	if (sps->pic_order_cnt_type == 0) {
		bitstream_append_ue(bitstream, sps->log2_max_pic_order_cnt_lsb_minus4);
	} // XXX: == 1

	bitstream_append_ue(bitstream, sps->max_num_ref_frames);
	/* gaps_in_frame_num_value_allowed_flag */
	bitstream_append_bits(bitstream, 0, 1);
	bitstream_append_ue(bitstream, sps->pic_width_in_mbs_minus1);
	bitstream_append_ue(bitstream, sps->pic_height_in_map_units_minus1);
	/* frame_mbs_only_flag */
	/* XXX: fixed by hardware */
	bitstream_append_bits(bitstream, 1, 1);
	/* direct_8x8_inference_flag */
	bitstream_append_bits(bitstream, !!(sps->flags & V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE), 1);

	if ((encoder->setup.width_mbs * 16) != encoder->setup.width ||
	    (encoder->setup.height_mbs * 16) != encoder->setup.height)
		frame_cropping_flag = true;

	bitstream_append_bits(bitstream, frame_cropping_flag, 1);

	if (frame_cropping_flag) {
		uint32_t crop_right = ((encoder->setup.width_mbs * 16) -
				       encoder->setup.width) >> 1;
		uint32_t crop_bottom = ((encoder->setup.height_mbs * 16) -
				        encoder->setup.height) >> 1;

		/* frame_crop_left_offset */
		bitstream_append_ue(bitstream, 0);
		/* frame_crop_right_offset */
		bitstream_append_ue(bitstream, crop_right);
		/* frame_crop_top_offset */
		bitstream_append_ue(bitstream, 0);
		/* frame_crop_bottom_offset */
		bitstream_append_ue(bitstream, crop_bottom);
	}

	/* vui_parameters_present_flag */
	bitstream_append_bits(bitstream, 0, 1);

	/* rbsp_stop_one_bit */
	bitstream_append_bits(bitstream, 1, 1);
}

static void bitstream_pps(struct bitstream *bitstream,
			  struct v4l2_encoder *encoder)
{
	struct v4l2_ctrl_h264_pps *pps = &encoder->pps;

	bitstream_reset(bitstream);

	/* NALU header */
	bitstream_append_bits(bitstream, 0, 1);
	bitstream_append_bits(bitstream, 3, 2);
	bitstream_append_bits(bitstream, 8, 5);

	bitstream_append_ue(bitstream, pps->pic_parameter_set_id);
	bitstream_append_ue(bitstream, pps->seq_parameter_set_id);
	/* entropy_coding_mode_flag */
	bitstream_append_bits(bitstream, !!(pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE), 1);
	/* bottom_field_pic_order_in_frame_present_flag */
	bitstream_append_bits(bitstream, 0, 1);
	bitstream_append_ue(bitstream, pps->num_slice_groups_minus1);
	bitstream_append_ue(bitstream, pps->num_ref_idx_l0_default_active_minus1);
	bitstream_append_ue(bitstream, pps->num_ref_idx_l1_default_active_minus1);
	/* weighted_pred_flag */
	bitstream_append_bits(bitstream, 0, 1);
	bitstream_append_bits(bitstream, pps->weighted_bipred_idc, 2);
	bitstream_append_se(bitstream, pps->pic_init_qp_minus26);
	bitstream_append_se(bitstream, pps->pic_init_qs_minus26);
	bitstream_append_se(bitstream, pps->chroma_qp_index_offset);
	/* deblocking_filter_control_present_flag */
	/* XXX: fixed by hardware */
	bitstream_append_bits(bitstream, 1, 1);
	/* constrained_intra_pred_flag */
	bitstream_append_bits(bitstream, 0, 1);
	/* XXX: fixed by hardware */
	/* redundant_pic_cnt_present_flag */
	bitstream_append_bits(bitstream, 0, 1);

	/* rbsp_stop_one_bit */
	bitstream_append_bits(bitstream, 1, 1);
}

int h264_complete(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *capture_buffer;
	struct v4l2_ctrl_h264_encode_feedback *encode_feedback;
	unsigned int bytes_used;
	unsigned int capture_index;

	capture_index = encoder->capture_buffers_index;
	capture_buffer = &encoder->capture_buffers[capture_index];

	/* Feedback */

	encode_feedback = &encoder->h264_dst_controls.encode_feedback;
	bytes_used = capture_buffer->buffer.m.planes[0].bytesused;

	h264_rate_control_feedback(encoder, bytes_used,
				   encode_feedback->rlc_count,
				   encode_feedback->qp_sum);

	/* Slice */

	if (encoder->bitstream_fd >= 0)
		write(encoder->bitstream_fd, capture_buffer->mmap_data[0],
		      capture_buffer->buffer.m.planes[0].bytesused);

	/* GOP */

	encoder->gop_index++;
	encoder->gop_index %= encoder->setup.gop_size;

	return 0;
}

int h264_prepare(struct v4l2_encoder *encoder)
{
	struct v4l2_ctrl_h264_encode_params *encode_params =
		&encoder->h264_src_controls.encode_params;
	struct v4l2_ctrl_h264_encode_rc *encode_rc =
		&encoder->h264_src_controls.encode_rc;
	struct v4l2_ctrl_h264_sps *sps = &encoder->sps;
	struct v4l2_ctrl_h264_pps *pps = &encoder->pps;
	unsigned int i;

	/* Encode */

	if (!encoder->gop_index) {
		encode_params->slice_type = V4L2_H264_SLICE_TYPE_I;
		encode_params->idr_pic_id++;
		encode_params->frame_num = 0;
	} else {
		encode_params->slice_type = V4L2_H264_SLICE_TYPE_P;
		encode_params->reference_ts = encoder->reference_timestamp;
		encode_params->frame_num++;
		encode_params->frame_num %= (1 << (sps->log2_max_frame_num_minus4 + 4));
	}

	encode_params->pic_parameter_set_id = 0;
	encode_params->cabac_init_idc = 0;

	encode_params->pic_init_qp_minus26 = pps->pic_init_qp_minus26;
	encode_params->chroma_qp_index_offset = pps->chroma_qp_index_offset;

	if (pps->flags & V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE)
		encode_params->flags |= V4L2_H264_ENCODE_FLAG_ENTROPY_CODING_MODE;
	if (pps->flags & V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE)
		encode_params->flags |= V4L2_H264_ENCODE_FLAG_TRANSFORM_8X8_MODE;
	if (pps->flags & V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED)
		encode_params->flags |= V4L2_H264_ENCODE_FLAG_CONSTRAINED_INTRA_PRED;

	/* Rate Control */

	h264_rate_control_step(encoder);

	encode_rc->qp = encoder->rc.qp;
	encode_rc->qp_min = encoder->setup.qp_min;
	encode_rc->qp_max = encoder->setup.qp_max;

	/* Checkpoints */

	/* Only apply checkpoints to intra frames. */
	if (encoder->rc.cp_enabled) {
		encode_rc->cp_distance_mbs = encoder->rc.cp_distance_mbs;

		for (i = 0; i < encoder->rc.cp_count; i++)
			encode_rc->cp_target[i] = encoder->rc.cp_target[i];

		for (i = 0; i < ARRAY_SIZE(encode_rc->cp_target_error); i++)
			encode_rc->cp_target_error[i] =
				encoder->rc.cp_target_error[i];

		for (i = 0; i < ARRAY_SIZE(encode_rc->cp_qp_delta); i++)
			encode_rc->cp_qp_delta[i] = encoder->rc.cp_qp_delta[i];
	} else {
		encode_rc->cp_distance_mbs = 0;

		for (i = 0; i < encoder->rc.cp_count; i++)
			encode_rc->cp_target[i] = 0;

		for (i = 0; i < ARRAY_SIZE(encode_rc->cp_target_error); i++)
			encode_rc->cp_target_error[i] = 0;

		for (i = 0; i < ARRAY_SIZE(encode_rc->cp_qp_delta); i++)
			encode_rc->cp_qp_delta[i] = 0;
	}

	/* MAD */

	encode_rc->mad_threshold = 0;
	encode_rc->mad_qp_delta = 0;

	return 0;
}

int h264_setup(struct v4l2_encoder *encoder)
{
	struct v4l2_ctrl_h264_sps *sps = &encoder->sps;
	struct v4l2_ctrl_h264_pps *pps = &encoder->pps;
	struct bitstream *bitstream;
	struct unit *unit;

	/* SPS */

	sps->profile_idc = 100;
	sps->level_idc = 31;
	sps->seq_parameter_set_id = 0;
	sps->chroma_format_idc = 1; /* YUV 4:2:0 */

	sps->pic_width_in_mbs_minus1 = encoder->setup.width_mbs - 1;
	sps->pic_height_in_map_units_minus1 = encoder->setup.height_mbs - 1;

	sps->max_num_ref_frames = 1;

	// XXX: fixed by hardware
	sps->pic_order_cnt_type = 2;

	// XXX: fixed by hardware FOSHO
	sps->log2_max_frame_num_minus4 = 12;

	// XXX: fixed by hardware (at least constant in MPP)
	sps->flags |= V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE;

	/* PPS */

	pps->pic_parameter_set_id = 0;
	pps->seq_parameter_set_id = 0;

	pps->flags |= V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE;

	/* XXX: fixed by hardware */
	pps->weighted_bipred_idc = 0;

	pps->chroma_qp_index_offset = 4;
	pps->pic_init_qp_minus26 = 20;

	/* Bitstream */

	bitstream = bitstream_create();

	/* Bitstream SPS */

	bitstream_sps(bitstream, encoder);

	unit = unit_pack(bitstream);

	if (encoder->bitstream_fd >= 0)
		write(encoder->bitstream_fd, unit->buffer, unit->length);

	unit_destroy(unit);

	/* Bitstream PPS */

	bitstream_pps(bitstream, encoder);

	unit = unit_pack(bitstream);

	if (encoder->bitstream_fd >= 0)
		write(encoder->bitstream_fd, unit->buffer, unit->length);

	unit_destroy(unit);

	bitstream_destroy(bitstream);

	/* Rate control */

	h264_rate_control_setup(encoder);

	return 0;
}

int h264_teardown(struct v4l2_encoder *encoder)
{
	return 0;
}
