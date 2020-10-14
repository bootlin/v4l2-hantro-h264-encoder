/*
 * Copyright (C) 2019-2020 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020 Bootlin
 */

#ifndef _V4L2_H_
#define _V4L2_H_

#include <stdbool.h>

#include <linux/videodev2.h>

bool v4l2_type_mplane_check(unsigned int type);
bool v4l2_capabilities_check(unsigned int capabilities_probed,
			     unsigned int capabilities);

int v4l2_stream_set(int video_fd, unsigned int type, bool on);
int v4l2_stream_on(int video_fd, unsigned int type);
int v4l2_stream_off(int video_fd, unsigned int type);

int v4l2_ext_controls_set(int video_fd, struct v4l2_ext_controls *ext_controls);
int v4l2_ext_controls_get(int video_fd, struct v4l2_ext_controls *ext_controls);
int v4l2_ext_controls_try(int video_fd, struct v4l2_ext_controls *ext_controls);
void v4l2_ext_controls_request_attach(struct v4l2_ext_controls *ext_controls,
				      int request_fd);
void v4l2_ext_controls_request_detach(struct v4l2_ext_controls *ext_controls);
void v4l2_ext_control_setup_compound(struct v4l2_ext_control *control,
				     void *data, unsigned int size);
void v4l2_ext_control_setup_base(struct v4l2_ext_control *control,
				 unsigned int id);
void v4l2_ext_controls_setup(struct v4l2_ext_controls *ext_controls,
			     struct v4l2_ext_control *controls,
			     unsigned int controls_count);

int v4l2_buffers_create(int video_fd, unsigned int type, unsigned int memory,
			struct v4l2_format *format, unsigned int count,
			unsigned int *index);
int v4l2_buffers_request(int video_fd, unsigned int type, unsigned int memory,
			 unsigned int count);
int v4l2_buffers_destroy(int video_fd, unsigned int type, unsigned int memory);
int v4l2_buffers_capabilities_probe(int video_fd, unsigned int type,
				    unsigned int *capabilities);

int v4l2_buffer_query(int video_fd, struct v4l2_buffer *buffer);
int v4l2_buffer_queue(int video_fd, struct v4l2_buffer *buffer);
int v4l2_buffer_dequeue(int video_fd, struct v4l2_buffer *buffer);
bool v4l2_buffer_error_check(struct v4l2_buffer *buffer);
int v4l2_buffer_plane_offset(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *offset);
int v4l2_buffer_plane_length(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *length);
void v4l2_buffer_request_attach(struct v4l2_buffer *buffer, int request_fd);
void v4l2_buffer_request_detach(struct v4l2_buffer *buffer);
void v4l2_buffer_timestamp_set(struct v4l2_buffer *buffer, uint64_t timestamp);
void v4l2_buffer_timestamp_get(struct v4l2_buffer *buffer, uint64_t *timestamp);
void v4l2_buffer_setup_base(struct v4l2_buffer *buffer, unsigned int type,
			    unsigned int memory, unsigned int index,
			    struct v4l2_plane *planes,
			    unsigned int planes_count);

int v4l2_pixel_format_enum(int video_fd, unsigned int type, unsigned int index,
			   unsigned int *pixel_format, char *description);
bool v4l2_pixel_format_check(int video_fd, unsigned int type,
			     unsigned int pixel_format);

int v4l2_format_try(int video_fd, struct v4l2_format *format);
int v4l2_format_set(int video_fd, struct v4l2_format *format);
int v4l2_format_get(int video_fd, struct v4l2_format *format);
void v4l2_format_setup_pixel(struct v4l2_format *format, unsigned int type,
			     unsigned int width, unsigned int height,
			     unsigned int pixel_format);

int v4l2_capabilities_probe(int video_fd, unsigned int *capabilities,
			    char *driver, char *card);

#endif
