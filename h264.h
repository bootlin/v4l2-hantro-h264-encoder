/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _H264_H_
#define _H264_H_

#include <v4l2-encoder.h>

int h264_complete(struct v4l2_encoder *encoder);
int h264_prepare(struct v4l2_encoder *encoder);
int h264_setup(struct v4l2_encoder *encoder);
int h264_teardown(struct v4l2_encoder *encoder);

#endif
