/*
 * Copyright (C) 2019-2020 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020 Bootlin
 */

#ifndef _V4L2_ENCODER_H_
#define _V4L2_ENCODER_H_

#include <linux/videodev2.h>

#include <h264-ctrls.h>
#include <h264-rate-control.h>
#include <draw.h>

struct v4l2_encoder;

struct v4l2_encoder_buffer {
	struct v4l2_encoder *encoder;

	struct v4l2_buffer buffer;

	struct v4l2_plane planes[4];
	void *mmap_data[4];
	unsigned int planes_count;

	int request_fd;
};

struct v4l2_encoder_h264_src_controls {
	struct v4l2_ext_controls ext_controls;
	struct v4l2_ext_control controls[2];
	unsigned int controls_count;

	struct v4l2_ctrl_h264_encode_params encode_params;
	struct v4l2_ext_control *encode_params_control;

	struct v4l2_ctrl_h264_encode_rc encode_rc;
	struct v4l2_ext_control *encode_rc_control;
};

struct v4l2_encoder_h264_dst_controls {
	struct v4l2_ext_controls ext_controls;
	struct v4l2_ext_control controls[1];
	unsigned int controls_count;

	struct v4l2_ext_control *encode_feedback_control;
	struct v4l2_ctrl_h264_encode_feedback encode_feedback;
};

struct v4l2_encoder_setup {
	/* Dimensions */
	unsigned int width;
	unsigned int width_mbs;
	unsigned int height;
	unsigned int height_mbs;

	/* Format */
	uint32_t format;

	/* Framerate */
	unsigned int fps_num;
	unsigned int fps_den;

	/* Bitrate */
	uint64_t bitrate;
	unsigned int gop_size;

	/* Quality */
	unsigned int qp_intra_delta;
	unsigned int qp_min;
	unsigned int qp_max;
	
};

struct v4l2_encoder {
	int video_fd;
	int media_fd;

	char driver[32];
	char card[32];

	unsigned int capabilities;
	unsigned int memory;

	bool up;
	bool started;

	struct v4l2_encoder_setup setup;

	unsigned int output_type;
	unsigned int output_capabilities;
	struct v4l2_format output_format;
	struct v4l2_encoder_buffer output_buffers[3];
	unsigned int output_buffers_count;
	unsigned int output_buffers_index;

	unsigned int capture_type;
	unsigned int capture_capabilities;
	struct v4l2_format capture_format;
	struct v4l2_encoder_buffer capture_buffers[3];
	unsigned int capture_buffers_count;
	unsigned int capture_buffers_index;

	struct v4l2_encoder_h264_src_controls h264_src_controls;
	struct v4l2_encoder_h264_dst_controls h264_dst_controls;

	struct v4l2_ctrl_h264_sps sps;
	struct v4l2_ctrl_h264_pps pps;

	struct h264_rate_control rc;
	uint64_t reference_timestamp;
	unsigned int gop_index;

	struct draw_mandelbrot draw_mandelbrot;
	struct draw_buffer *draw_buffer;

	unsigned int x, y;
	bool pattern_drawn;
	bool direction;

	int bitstream_fd;
};

int v4l2_encoder_prepare(struct v4l2_encoder *encoder);
int v4l2_encoder_complete(struct v4l2_encoder *encoder);
int v4l2_encoder_run(struct v4l2_encoder *encoder);
int v4l2_encoder_start(struct v4l2_encoder *encoder);
int v4l2_encoder_stop(struct v4l2_encoder *encoder);
int v4l2_encoder_intra_request(struct v4l2_encoder *encoder);
int v4l2_encoder_buffer_setup(struct v4l2_encoder_buffer *buffer,
			     unsigned int type, unsigned int index);
int v4l2_encoder_buffer_teardown(struct v4l2_encoder_buffer *buffer);
int v4l2_encoder_setup_defaults(struct v4l2_encoder *encoder);
int v4l2_encoder_setup_dimensions(struct v4l2_encoder *encoder,
				  unsigned int width, unsigned int height);
int v4l2_encoder_setup_format(struct v4l2_encoder *encoder, uint32_t format);
int v4l2_encoder_setup_fps(struct v4l2_encoder *encoder, float fps);
int v4l2_encoder_setup_bitrate(struct v4l2_encoder *encoder, uint64_t bitrate);
int v4l2_encoder_setup(struct v4l2_encoder *encoder);
int v4l2_encoder_teardown(struct v4l2_encoder *encoder);
int v4l2_encoder_probe(struct v4l2_encoder *encoder);
int v4l2_encoder_open(struct v4l2_encoder *encoder);
void v4l2_encoder_close(struct v4l2_encoder *encoder);

#endif
