/*
 * Copyright (C) 2019-2020 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <v4l2.h>

bool v4l2_type_mplane_check(unsigned int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return true;

	default:
		return false;
	}
}

bool v4l2_capabilities_check(unsigned int capabilities_probed,
			     unsigned int capabilities)
{
	unsigned int capabilities_mask = capabilities_probed & capabilities;

	return capabilities_mask == capabilities;
}

int v4l2_stream_set(int video_fd, unsigned int type, bool on)
{
	int ret;

	ret = ioctl(video_fd, on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF, &type);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_stream_on(int video_fd, unsigned int type)
{
	return v4l2_stream_set(video_fd, type, true);
}

int v4l2_stream_off(int video_fd, unsigned int type)
{
	return v4l2_stream_set(video_fd, type, false);
}

int v4l2_ext_controls_set(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_ext_controls_get(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_ext_controls_try(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_TRY_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_ext_controls_request_attach(struct v4l2_ext_controls *ext_controls,
				      int request_fd)
{
	if (!ext_controls)
		return;

	ext_controls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
	ext_controls->request_fd = request_fd;
}

void v4l2_ext_controls_request_detach(struct v4l2_ext_controls *ext_controls)
{
	if (!ext_controls)
		return;

	if (ext_controls->which == V4L2_CTRL_WHICH_REQUEST_VAL)
		ext_controls->which = 0;

	ext_controls->request_fd = -1;
}

void v4l2_ext_controls_setup(struct v4l2_ext_controls *ext_controls,
			     struct v4l2_ext_control *controls,
			     unsigned int controls_count)
{
	if (!controls)
		return;

	ext_controls->controls = controls;
	ext_controls->count = controls_count;
}

void v4l2_ext_control_setup_compound(struct v4l2_ext_control *control,
				     void *data, unsigned int size)
{
	if (!control)
		return;

	control->ptr = data;
	control->size = size;
}

void v4l2_ext_control_setup_base(struct v4l2_ext_control *control,
				 unsigned int id)
{
	if (!control)
		return;

	memset(control, 0, sizeof(*control));

	control->id = id;
}

int v4l2_buffers_create(int video_fd, unsigned int type, unsigned int memory,
			struct v4l2_format *format, unsigned int count,
			unsigned int *index)
{
	struct v4l2_create_buffers create_buffers = { 0 };
	int ret;

	if (format) {
		create_buffers.format = *format;
	} else {
		ret = ioctl(video_fd, VIDIOC_G_FMT, &create_buffers.format);
		if (ret)
			return -errno;
	}

	create_buffers.format.type = type;
	create_buffers.memory = memory;
	create_buffers.count = count;

	ret = ioctl(video_fd, VIDIOC_CREATE_BUFS, &create_buffers);
	if (ret)
		return -errno;

	if (index)
		*index = create_buffers.index;

	return 0;
}

int v4l2_buffers_request(int video_fd, unsigned int type, unsigned int memory,
			 unsigned int count)
{
	struct v4l2_requestbuffers requestbuffers = { 0 };
	int ret;

	requestbuffers.type = type;
	requestbuffers.memory = memory;
	requestbuffers.count = count;

	ret = ioctl(video_fd, VIDIOC_REQBUFS, &requestbuffers);
	if (ret)
		return -errno;

	return 0;
}


int v4l2_buffers_destroy(int video_fd, unsigned int type, unsigned int memory)
{
	struct v4l2_requestbuffers requestbuffers = { 0 };
	int ret;

	requestbuffers.type = type;
	requestbuffers.memory = memory;
	requestbuffers.count = 0;

	ret = ioctl(video_fd, VIDIOC_REQBUFS, &requestbuffers);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffers_capabilities_probe(int video_fd, unsigned int type,
				    unsigned int *capabilities)
{
	struct v4l2_create_buffers create_buffers = { 0 };
	int ret;

	if (!capabilities)
		return -EINVAL;

	create_buffers.format.type = type;
	create_buffers.memory = V4L2_MEMORY_MMAP;
	create_buffers.count = 0;

	ret = ioctl(video_fd, VIDIOC_CREATE_BUFS, &create_buffers);
	if (ret)
		return -errno;

	if (create_buffers.capabilities)
		*capabilities = create_buffers.capabilities;
	else
		*capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP;

	return 0;
}

int v4l2_buffer_query(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QUERYBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffer_queue(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffer_dequeue(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_DQBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffer_plane_offset(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *offset)
{
	bool mplane_check;

	if (!buffer || !offset)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		*offset = buffer->m.planes[plane_index].m.mem_offset;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		*offset = buffer->m.offset;
	}

	return 0;
}

int v4l2_buffer_plane_length(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *length)
{
	bool mplane_check;

	if (!buffer || !length)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		*length = buffer->m.planes[plane_index].length;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		*length = buffer->length;
	}

	return 0;
}

bool v4l2_buffer_error_check(struct v4l2_buffer *buffer)
{
	if (!buffer)
		return true;

	return buffer->flags & V4L2_BUF_FLAG_ERROR;
}

void v4l2_buffer_request_attach(struct v4l2_buffer *buffer, int request_fd)
{
	if (!buffer)
		return;

	buffer->flags |= V4L2_BUF_FLAG_REQUEST_FD;
	buffer->request_fd = request_fd;
}

void v4l2_buffer_request_detach(struct v4l2_buffer *buffer)
{
	if (!buffer)
		return;

	buffer->flags &= ~V4L2_BUF_FLAG_REQUEST_FD;
	buffer->request_fd = -1;
}

void v4l2_buffer_timestamp_set(struct v4l2_buffer *buffer, uint64_t timestamp)
{
	if (!buffer)
		return;

	buffer->timestamp.tv_sec = timestamp / 1000000000ULL;
	buffer->timestamp.tv_usec = (timestamp % 1000000000ULL) / 1000;
}

void v4l2_buffer_timestamp_get(struct v4l2_buffer *buffer, uint64_t *timestamp)
{
	if (!buffer || !timestamp)
		return;

	*timestamp = v4l2_timeval_to_ns(&buffer->timestamp);
}

void v4l2_buffer_setup_base(struct v4l2_buffer *buffer, unsigned int type,
			    unsigned int memory, unsigned int index,
			    struct v4l2_plane *planes,
			    unsigned int planes_count)
{
	bool mplane_check;

	if (!buffer)
		return;

	memset(buffer, 0, sizeof(*buffer));

	buffer->type = type;
	buffer->memory = memory;
	buffer->index = index;

	mplane_check = v4l2_type_mplane_check(type);
	if (mplane_check && planes) {
		buffer->m.planes = planes;
		buffer->length = planes_count;
	}
}

int v4l2_pixel_format_enum(int video_fd, unsigned int type, unsigned int index,
			   unsigned int *pixel_format, char *description)
{
	struct v4l2_fmtdesc fmtdesc = { 0 };
	int ret;

	if (!pixel_format)
		return -EINVAL;

	fmtdesc.type = type;
	fmtdesc.index = index;

	ret = ioctl(video_fd, VIDIOC_ENUM_FMT, &fmtdesc);
	if (ret)
		return -errno;

	*pixel_format = fmtdesc.pixelformat;

	if (description)
		strncpy(description, fmtdesc.description,
			sizeof(fmtdesc.description));

	return 0;
}

bool v4l2_pixel_format_check(int video_fd, unsigned int type,
			     unsigned int pixel_format)
{
	unsigned int index = 0;
	unsigned int ret;

	do {
		unsigned int pixel_format_enum;

		ret = v4l2_pixel_format_enum(video_fd, type, index,
					     &pixel_format_enum, NULL);
		if (ret)
			break;

		if (pixel_format_enum == pixel_format)
			return true;

		index++;
	} while (ret >= 0);

	return false;
}

int v4l2_format_try(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_TRY_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_format_set(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_format_get(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_format_setup_pixel(struct v4l2_format *format, unsigned int type,
			     unsigned int width, unsigned int height,
			     unsigned int pixel_format)
{
	bool mplane_check;

	if (!format)
		return;

	memset(format, 0, sizeof(*format));

	format->type = type;

	mplane_check = v4l2_type_mplane_check(type);
	if (mplane_check) {
		format->fmt.pix_mp.width = width;
		format->fmt.pix_mp.height = height;
		format->fmt.pix_mp.pixelformat = pixel_format;
	} else {
		format->fmt.pix.width = width;
		format->fmt.pix.height = height;
		format->fmt.pix.pixelformat = pixel_format;
	}
}

int v4l2_capabilities_probe(int video_fd, unsigned int *capabilities,
			    char *driver, char *card)
{
	struct v4l2_capability capability = { 0 };
	int ret;

	if (!capabilities)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QUERYCAP, &capability);
	if (ret < 0)
		return -errno;

	if (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
		*capabilities = capability.device_caps;
	else
		*capabilities = capability.capabilities;

	if (driver)
		strncpy(driver, capability.driver, sizeof(capability.driver));

	if (card)
		strncpy(card, capability.card, sizeof(capability.card));

	return 0;
}
