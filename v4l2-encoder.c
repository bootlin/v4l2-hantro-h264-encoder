/*
 * Copyright (C) 2019-2020 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libudev.h>

#include <linux/videodev2.h>
#include <linux/media.h>
#include <h264-ctrls.h>

#include <media.h>
#include <v4l2.h>
#include <v4l2-encoder.h>
#include <h264.h>
#include <h264-rate-control.h>
#include <bitstream.h>
#include <unit.h>
#include <csc.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

int v4l2_encoder_complete(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *output_buffer;
	unsigned int output_index;
	int ret;

	if (!encoder)
		return -EINVAL;

	output_index = encoder->output_buffers_index;
	output_buffer = &encoder->output_buffers[output_index];

	ret = h264_complete(encoder);
	if (ret)
		return ret;

	/* The reference buffer is always the previous frame. */
	v4l2_buffer_timestamp_get(&output_buffer->buffer,
				  &encoder->reference_timestamp);

	encoder->output_buffers_index++;
	encoder->output_buffers_index %= encoder->output_buffers_count;

	encoder->capture_buffers_index++;
	encoder->capture_buffers_index %= encoder->capture_buffers_count;

	return 0;
}

int v4l2_encoder_prepare(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *output_buffer;
	unsigned int output_index;
	unsigned int width, height;
	int fd;
	int ret;

	if (!encoder)
		return -EINVAL;

	width = encoder->setup.width;
	height = encoder->setup.height;

	output_index = encoder->output_buffers_index;
	output_buffer = &encoder->output_buffers[output_index];

	ret = h264_prepare(encoder);
	if (ret)
		return ret;

#define MANDELBROT

#ifdef MANDELBROT
	draw_mandelbrot_zoom(&encoder->draw_mandelbrot);
	draw_mandelbrot(&encoder->draw_mandelbrot, encoder->draw_buffer);
#endif
#ifdef GRADIENT
	draw_gradient(encoder->draw_buffer);
#endif
#ifdef RECTANGLE
	draw_background(encoder->draw_buffer, 0xff00ffff);
	draw_rectangle(encoder->draw_buffer, encoder->x, height / 3, width / 3,
		       height / 3, 0x00ff0000);

	if (!encoder->direction) {
		if (encoder->x >= 20) {
			encoder->x -= 20;
		} else {
			encoder->x = 0;
			encoder->direction = 1;
		}
	} else {
		if (encoder->x < (2 * width / 3 - 20)) {
			encoder->x += 20;
		} else {
			encoder->x = 2 * width / 3;
			encoder->direction = 0;
		}
	}
#endif
#ifdef PATTERN
	if (!encoder->pattern_drawn) {
		draw_png(encoder->draw_buffer, "test-pattern.png");
		encoder->pattern_drawn = true;
	}
#endif

	if (encoder->setup.format == V4L2_PIX_FMT_YUV420M)
		ret = rgb2yuv420(encoder->draw_buffer,
				 output_buffer->mmap_data[0],
				 output_buffer->mmap_data[1],
				 output_buffer->mmap_data[2]);
	else
		ret = rgb2nv12(encoder->draw_buffer,
			       output_buffer->mmap_data[0],
			       output_buffer->mmap_data[1]);

#ifdef OUTPUT_DUMP
	fd = open("output.yuv",  O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		printf("output open error!\n");
		return -1;
	}

	write(fd, output_buffer->mmap_data[0], width * height);
	write(fd, output_buffer->mmap_data[1], width * height / 4);
	write(fd, output_buffer->mmap_data[2], width * height / 4);

	close(fd);
#endif

	return 0;
}

int v4l2_encoder_run(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *output_buffer;
	unsigned int output_index;
	struct v4l2_encoder_buffer *capture_buffer;
	unsigned int capture_index;
	struct timeval timeout = { 0, 300000 };
	int ret;

	if (!encoder)
		return -EINVAL;

	output_index = encoder->output_buffers_index;
	output_buffer = &encoder->output_buffers[output_index];

	v4l2_buffer_request_attach(&output_buffer->buffer,
				   output_buffer->request_fd);

	ret = v4l2_buffer_queue(encoder->video_fd, &output_buffer->buffer);
	if (ret)
		return ret;

	v4l2_buffer_request_detach(&output_buffer->buffer);

	capture_index = encoder->capture_buffers_index;
	capture_buffer = &encoder->capture_buffers[capture_index];

	ret = v4l2_buffer_queue(encoder->video_fd, &capture_buffer->buffer);
	if (ret)
		return ret;

	v4l2_ext_controls_request_attach(&encoder->h264_src_controls.ext_controls,
					 output_buffer->request_fd);

	ret = v4l2_ext_controls_set(encoder->video_fd,
				    &encoder->h264_src_controls.ext_controls);
	if (ret)
		return ret;

	v4l2_ext_controls_request_detach(&encoder->h264_src_controls.ext_controls);

	ret = media_request_queue(output_buffer->request_fd);
	if (ret)
		return ret;

	ret = media_request_poll(output_buffer->request_fd, &timeout);
	if (ret <= 0)
		return ret;

	v4l2_ext_controls_request_attach(&encoder->h264_dst_controls.ext_controls,
					 output_buffer->request_fd);

	ret = v4l2_ext_controls_get(encoder->video_fd,
				    &encoder->h264_dst_controls.ext_controls);
	if (ret)
		return ret;

	v4l2_ext_controls_request_detach(&encoder->h264_dst_controls.ext_controls);

	do {
		ret = v4l2_buffer_dequeue(encoder->video_fd, &output_buffer->buffer);
		if (ret && ret != -EAGAIN)
			return ret;
	} while (ret == -EAGAIN);

	do {
		ret = v4l2_buffer_dequeue(encoder->video_fd, &capture_buffer->buffer);
		if (ret && ret != -EAGAIN)
			return ret;
	} while (ret == -EAGAIN);

	ret = media_request_reinit(output_buffer->request_fd);
	if (ret)
		return ret;

	return 0;
}

int v4l2_encoder_start(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder || encoder->started)
		return -EINVAL;

	ret = v4l2_stream_on(encoder->video_fd, encoder->output_type);
	if (ret)
		return ret;

	ret = v4l2_stream_on(encoder->video_fd, encoder->capture_type);
	if (ret)
		return ret;

	encoder->started = true;

	return 0;
}

int v4l2_encoder_stop(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder || !encoder->started)
		return -EINVAL;

	ret = v4l2_stream_off(encoder->video_fd, encoder->output_type);
	if (ret)
		return ret;

	ret = v4l2_stream_off(encoder->video_fd, encoder->capture_type);
	if (ret)
		return ret;

	encoder->started = false;

	return 0;
}

int v4l2_encoder_intra_request(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder || !encoder->started)
		return -EINVAL;

	encoder->gop_index = 0;

	ret = h264_rate_control_intra_request(encoder);
	if (ret)
		return ret;

	return 0;
}

int v4l2_encoder_buffer_setup(struct v4l2_encoder_buffer *buffer,
			     unsigned int type, unsigned int index)
{
	struct v4l2_encoder *encoder;
	int ret;

	if (!buffer || !buffer->encoder)
		return -EINVAL;

	encoder = buffer->encoder;

	v4l2_buffer_setup_base(&buffer->buffer, type, encoder->memory, index,
			       buffer->planes, buffer->planes_count);

	ret = v4l2_buffer_query(encoder->video_fd, &buffer->buffer);
	if (ret) {
		fprintf(stderr, "Failed to query buffer\n");
		goto complete;
	}

	if(encoder->memory == V4L2_MEMORY_MMAP) {
		unsigned int i;

		for (i = 0; i < buffer->planes_count; i++) {
			unsigned int offset;
			unsigned int length;

			ret = v4l2_buffer_plane_offset(&buffer->buffer, i,
						       &offset);
			if (ret)
				goto complete;

			ret = v4l2_buffer_plane_length(&buffer->buffer, i,
						       &length);
			if (ret)
				goto complete;

			buffer->mmap_data[i] =
				mmap(NULL, length, PROT_READ | PROT_WRITE,
				     MAP_SHARED, encoder->video_fd, offset);
			if (buffer->mmap_data[i] == MAP_FAILED) {
				ret = -errno;
				goto complete;
			}
		}
	}

	if (type == encoder->output_type) {
		buffer->request_fd = media_request_alloc(encoder->media_fd);
		if (buffer->request_fd < 0) {
			ret = -EINVAL;
			goto complete;
		}

		v4l2_buffer_timestamp_set(&buffer->buffer, index * 1000);
	} else {
		buffer->request_fd = -1;
	}

	ret = 0;

complete:
	return ret;
}

int v4l2_encoder_buffer_teardown(struct v4l2_encoder_buffer *buffer)
{
	struct v4l2_encoder *encoder;

	if (!buffer || !buffer->encoder)
		return -EINVAL;

	encoder = buffer->encoder;

	if(encoder->memory == V4L2_MEMORY_MMAP) {
		unsigned int i;

		for (i = 0; i < buffer->planes_count; i++) {
			unsigned int length;

			if (!buffer->mmap_data[i] ||
			    buffer->mmap_data[i] == MAP_FAILED)
					continue;

			v4l2_buffer_plane_length(&buffer->buffer, i, &length);
			munmap(buffer->mmap_data[i], length);
		}
	}

	if (buffer->request_fd >= 0)
		close(buffer->request_fd);

	memset(buffer, 0, sizeof(*buffer));
	buffer->request_fd = -1;

	return 0;
}

int v4l2_encoder_h264_src_controls_setup(struct v4l2_encoder_h264_src_controls *h264_src_controls)
{
	unsigned int controls_count;
	unsigned int index = 0;

	if (!h264_src_controls)
		return -EINVAL;

	h264_src_controls->encode_params_control =
		&h264_src_controls->controls[index];
	v4l2_ext_control_setup_base(h264_src_controls->encode_params_control,
				    V4L2_CID_MPEG_VIDEO_H264_ENCODE_PARAMS);
	v4l2_ext_control_setup_compound(h264_src_controls->encode_params_control,
					&h264_src_controls->encode_params,
					sizeof(h264_src_controls->encode_params));
	index++;

	h264_src_controls->encode_rc_control =
		&h264_src_controls->controls[index];
	v4l2_ext_control_setup_base(h264_src_controls->encode_rc_control,
				    V4L2_CID_MPEG_VIDEO_H264_ENCODE_RC);
	v4l2_ext_control_setup_compound(h264_src_controls->encode_rc_control,
					&h264_src_controls->encode_rc,
					sizeof(h264_src_controls->encode_rc));
	index++;

	controls_count = index;

	v4l2_ext_controls_setup(&h264_src_controls->ext_controls,
				h264_src_controls->controls, controls_count);

	h264_src_controls->controls_count = controls_count;

	return 0;
}

int v4l2_encoder_h264_dst_controls_setup(struct v4l2_encoder_h264_dst_controls *h264_dst_controls)
{
	unsigned int controls_count;
	unsigned int index = 0;

	if (!h264_dst_controls)
		return -EINVAL;

	h264_dst_controls->encode_feedback_control =
		&h264_dst_controls->controls[index];
	v4l2_ext_control_setup_base(h264_dst_controls->encode_feedback_control,
				    V4L2_CID_MPEG_VIDEO_H264_ENCODE_FEEDBACK);
	v4l2_ext_control_setup_compound(h264_dst_controls->encode_feedback_control,
					&h264_dst_controls->encode_feedback,
					sizeof(h264_dst_controls->encode_feedback));
	index++;

	controls_count = index;

	v4l2_ext_controls_setup(&h264_dst_controls->ext_controls,
				h264_dst_controls->controls, controls_count);

	h264_dst_controls->controls_count = controls_count;

	return 0;
}

int v4l2_encoder_setup_defaults(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	ret = v4l2_encoder_setup_dimensions(encoder, 1280, 720);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_format(encoder, V4L2_PIX_FMT_NV12M);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_fps(encoder, 25);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_bitrate(encoder, 500000);
	if (ret)
		return ret;

	encoder->setup.gop_size = 10;

	encoder->setup.qp_intra_delta = 2;
	encoder->setup.qp_min = 11;
	encoder->setup.qp_max = 51;

	return 0;
}

int v4l2_encoder_setup_dimensions(struct v4l2_encoder *encoder,
				  unsigned int width, unsigned int height)
{
	if (!encoder || !width || !height)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	encoder->setup.width = width;
	encoder->setup.width_mbs = (width + 15) / 16;

	encoder->setup.height = height;
	encoder->setup.height_mbs = (height + 15) / 16;

	return 0;
}

int v4l2_encoder_setup_format(struct v4l2_encoder *encoder, uint32_t format)
{
	if (!encoder)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	encoder->setup.format = format;

	return 0;
}

int v4l2_encoder_setup_fps(struct v4l2_encoder *encoder, float fps)
{
	if (!encoder || !fps)
		return -EINVAL;

	encoder->setup.fps_den = 1000;
	encoder->setup.fps_num = fps * encoder->setup.fps_den;

	if (encoder->up)
		h264_rate_control_setup(encoder);

	return 0;
}

int v4l2_encoder_setup_bitrate(struct v4l2_encoder *encoder, uint64_t bitrate)
{
	if (!encoder || !bitrate)
		return -EINVAL;

	encoder->setup.bitrate = bitrate;

	if (encoder->up)
		h264_rate_control_setup(encoder);

	return 0;
}

int v4l2_encoder_setup(struct v4l2_encoder *encoder)
{
	unsigned int width, height;
	unsigned int buffers_count;
	unsigned int capture_size;
	uint32_t format;
	unsigned int i;
	int ret;

	if (!encoder || encoder->up)
		return -EINVAL;

	capture_size = 512 * 1024;
	width = encoder->setup.width;
	height = encoder->setup.height;
	format = encoder->setup.format;

	/* Capture format */

	v4l2_format_setup_pixel(&encoder->capture_format, encoder->capture_type,
				width, height, V4L2_PIX_FMT_H264_SLICE);

	if (v4l2_type_mplane_check(encoder->capture_type))
		encoder->capture_format.fmt.pix_mp.plane_fmt[0].sizeimage =
			capture_size;
	else
		encoder->capture_format.fmt.pix.sizeimage = capture_size;

	ret = v4l2_format_try(encoder->video_fd, &encoder->capture_format);
	if (ret) {
		fprintf(stderr, "Failed to try capture format\n");
		goto complete;
	}

	ret = v4l2_format_set(encoder->video_fd, &encoder->capture_format);
	if (ret) {
		fprintf(stderr, "Failed to set capture format\n");
		goto complete;
	}

	/* Output format */

	v4l2_format_setup_pixel(&encoder->output_format, encoder->output_type,
				width, height, format);

	ret = v4l2_format_try(encoder->video_fd, &encoder->output_format);
	if (ret) {
		fprintf(stderr, "Failed to try output format\n");
		goto complete;
	}

	ret = v4l2_format_set(encoder->video_fd, &encoder->output_format);
	if (ret) {
		fprintf(stderr, "Failed to set output format\n");
		goto complete;
	}

	/* Capture buffers */

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	ret = v4l2_buffers_request(encoder->video_fd, encoder->capture_type,
				   encoder->memory, buffers_count);
	if (ret) {
		fprintf(stderr, "Failed to allocate capture buffers\n");
		goto error;
	}

	for (i = 0; i < buffers_count; i++) {
		struct v4l2_encoder_buffer *buffer = &encoder->capture_buffers[i];

		buffer->encoder = encoder;
		buffer->planes_count =
			encoder->capture_format.fmt.pix_mp.num_planes;

		ret = v4l2_encoder_buffer_setup(buffer, encoder->capture_type, i);
		if (ret) {
			fprintf(stderr, "Failed to setup capture buffer\n");
			goto error;
		}
	}

	encoder->capture_buffers_count = buffers_count;

	/* Output buffers */

	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	ret = v4l2_buffers_request(encoder->video_fd, encoder->output_type,
				   encoder->memory, buffers_count);
	if (ret) {
		fprintf(stderr, "Failed to allocate output buffers\n");
		goto complete;
	}

	for (i = 0; i < buffers_count; i++) {
		struct v4l2_encoder_buffer *buffer = &encoder->output_buffers[i];

		buffer->encoder = encoder;
		buffer->planes_count =
			encoder->output_format.fmt.pix_mp.num_planes;

		ret = v4l2_encoder_buffer_setup(buffer, encoder->output_type, i);
		if (ret) {
			fprintf(stderr, "Failed to setup output buffer\n");
			goto error;
		}
	}

	encoder->output_buffers_count = buffers_count;

	/* Source controls */

	ret = v4l2_encoder_h264_src_controls_setup(&encoder->h264_src_controls);
	if (ret) {
		fprintf(stderr, "Failed to setup source controls\n");
		goto error;
	}

	/* Destination controls */

	ret = v4l2_encoder_h264_dst_controls_setup(&encoder->h264_dst_controls);
	if (ret) {
		fprintf(stderr, "Failed to setup destination controls\n");
		goto error;
	}

	/* H.264 */

	ret = h264_setup(encoder);
	if (ret) {
		fprintf(stderr, "Failed to setup H.264 parameters\n");
		goto error;
	}

	/* Draw buffer */

	encoder->draw_buffer = draw_buffer_create(width, height);
	if (!encoder->draw_buffer) {
		fprintf(stderr, "Failed to create draw buffer\n");
		goto error;
	}

	/* Mandelbrot */

	draw_mandelbrot_init(&encoder->draw_mandelbrot);

	encoder->up = true;

	ret = 0;
	goto complete;

error:
	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_teardown(&encoder->output_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->output_type,
			     encoder->memory);

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_teardown(&encoder->capture_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->capture_type,
			     encoder->memory);

complete:
	return ret;
}

int v4l2_encoder_teardown(struct v4l2_encoder *encoder)
{
	unsigned int buffers_count;
	unsigned int i;

	if (!encoder || !encoder->up)
		return -EINVAL;

	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_teardown(&encoder->output_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->output_type,
			     encoder->memory);

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_teardown(&encoder->capture_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->capture_type,
			     encoder->memory);

	h264_teardown(encoder);

	encoder->up = false;

	return 0;
}

int v4l2_encoder_probe(struct v4l2_encoder *encoder)
{
	bool check, mplane_check;
	int ret;

	if (!encoder || encoder->video_fd < 0)
		return -EINVAL;

	ret = v4l2_capabilities_probe(encoder->video_fd, &encoder->capabilities,
				      (char *)&encoder->driver,
				      (char *)&encoder->card);
	if (ret) {
		fprintf(stderr, "Failed to probe V4L2 capabilities\n");
		return ret;
	}

	printf("Probed driver %s card %s\n", encoder->driver, encoder->card);

	mplane_check = v4l2_capabilities_check(encoder->capabilities,
					       V4L2_CAP_VIDEO_M2M_MPLANE);
	check = v4l2_capabilities_check(encoder->capabilities,
					V4L2_CAP_VIDEO_M2M);
	if (mplane_check) {
		encoder->output_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		encoder->capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	} else if (check) {
		encoder->output_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		encoder->capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	} else {
		fprintf(stderr, "Missing V4L2 M2M support\n");
		return -1;
	}

	ret = v4l2_buffers_capabilities_probe(encoder->video_fd,
					      encoder->output_type,
					      &encoder->output_capabilities);
	if (ret)
		return ret;

	check = v4l2_capabilities_check(encoder->output_capabilities,
					V4L2_BUF_CAP_SUPPORTS_REQUESTS);
	if (!check) {
		fprintf(stderr, "Missing output requests support\n");
		return -EINVAL;
	}

	ret = v4l2_buffers_capabilities_probe(encoder->video_fd,
					      encoder->capture_type,
					      &encoder->capture_capabilities);
	if (ret)
		return ret;

	encoder->memory = V4L2_MEMORY_MMAP;

	check = v4l2_pixel_format_check(encoder->video_fd, encoder->capture_type,
					V4L2_PIX_FMT_H264_SLICE);
	if (!check) {
		fprintf(stderr, "Missing H.264 slice pixel format\n");
		return -EINVAL;
	}

	return 0;
}

static int media_device_probe(struct v4l2_encoder *encoder, struct udev *udev,
			      struct udev_device *device)
{
	const char *path = udev_device_get_devnode(device);
	struct media_device_info device_info = { 0 };
	struct media_v2_topology topology = { 0 };
	struct media_v2_interface *interfaces = NULL;
	struct media_v2_entity *entities = NULL;
	struct media_v2_pad *pads = NULL;
	struct media_v2_link *links = NULL;
	struct media_v2_entity *encoder_entity;
	struct media_v2_interface *encoder_interface;
	struct media_v2_pad *sink_pad;
	struct media_v2_link *sink_link;
	struct media_v2_pad *source_pad;
	struct media_v2_link *source_link;
	const char *driver = "hantro-vpu";
	int media_fd = -1;
	int video_fd = -1;
	dev_t devnum;
	int ret;

	media_fd = open(path, O_RDWR);
	if (media_fd < 0)
		return -errno;

	ret = media_device_info(media_fd, &device_info);
	if (ret)
		goto error;

	ret = strncmp(device_info.driver, driver, strlen(driver));
	if (ret)
		goto error;

	ret = media_topology_get(media_fd, &topology);
	if (ret)
		goto error;

	if (!topology.num_interfaces || !topology.num_entities ||
	    !topology.num_pads || !topology.num_links) {
		ret = -ENODEV;
		goto error;
	}

	interfaces = calloc(1, topology.num_interfaces * sizeof(*interfaces));
	if (!interfaces) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_interfaces = (__u64)interfaces;

	entities = calloc(1, topology.num_entities * sizeof(*entities));
	if (!entities) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_entities = (__u64)entities;

	pads = calloc(1, topology.num_pads * sizeof(*pads));
	if (!pads) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_pads = (__u64)pads;

	links = calloc(1, topology.num_links * sizeof(*links));
	if (!links) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_links = (__u64)links;

	ret = media_topology_get(media_fd, &topology);
	if (ret)
		goto error;

	encoder_entity = media_topology_entity_find_by_function(&topology,
								MEDIA_ENT_F_PROC_VIDEO_ENCODER);
	if (!encoder_entity) {
		ret = -ENODEV;
		goto error;
	}

	sink_pad = media_topology_pad_find_by_entity(&topology,
						     encoder_entity->id,
						     MEDIA_PAD_FL_SINK);
	if (!sink_pad) {
		ret = -ENODEV;
		goto error;
	}

	sink_link = media_topology_link_find_by_pad(&topology, sink_pad->id,
						    sink_pad->flags);
	if (!sink_link) {
		ret = -ENODEV;
		goto error;
	}

	source_pad = media_topology_pad_find_by_id(&topology,
						   sink_link->source_id);
	if (!source_pad) {
		ret = -ENODEV;
		goto error;
	}

	source_link = media_topology_link_find_by_entity(&topology,
							 source_pad->entity_id,
							 MEDIA_PAD_FL_SINK);
	if (!source_link) {
		ret = -ENODEV;
		goto error;
	}

	encoder_interface = media_topology_interface_find_by_id(&topology,
								source_link->source_id);
	if (!encoder_interface) {
		ret = -ENODEV;
		goto error;
	}

	devnum = makedev(encoder_interface->devnode.major,
			 encoder_interface->devnode.minor);

	device = udev_device_new_from_devnum(udev, 'c', devnum);
	if (!device) {
		ret = -ENODEV;
		goto error;
	}

	path = udev_device_get_devnode(device);

	video_fd = open(path, O_RDWR | O_NONBLOCK);
	if (video_fd < 0) {
		ret = -errno;
		goto error;
	}

	encoder->media_fd = media_fd;
	encoder->video_fd = video_fd;

	ret = 0;
	goto complete;

error:
	if (media_fd >= 0)
		close(media_fd);

	if (video_fd >= 0)
		close(video_fd);

complete:
	if (links)
		free(links);

	if (pads)
		free(pads);

	if (entities)
		free(entities);

	if (interfaces)
		free(interfaces);

	return ret;
}

int v4l2_encoder_open(struct v4l2_encoder *encoder)
{
	struct udev *udev = NULL;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices;
	struct udev_list_entry *entry;
	int ret;

	if (!encoder)
		return -EINVAL;

	encoder->media_fd = -1;
	encoder->video_fd = -1;

	udev = udev_new();
	if (!udev)
		goto error;

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		goto error;

	udev_enumerate_add_match_subsystem(enumerate, "media");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(entry, devices) {
		struct udev_device *device;
		const char *path;

		path = udev_list_entry_get_name(entry);
		if (!path)
			continue;

		device = udev_device_new_from_syspath(udev, path);
		if (!device)
			continue;

		ret = media_device_probe(encoder, udev, device);

		udev_device_unref(device);

		if (!ret)
			break;
	}

	if (encoder->media_fd < 0) {
		fprintf(stderr, "Failed to open encoder media device\n");
		goto error;
	}

	if (encoder->video_fd < 0) {
		fprintf(stderr, "Failed to open encoder video device\n");
		goto error;
	}

	encoder->bitstream_fd = open("capture.h264",
				     O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (encoder->bitstream_fd < 0) {
		fprintf(stderr, "Failed to open bitstream file\n");
		ret = -errno;
		goto error;
	}

	ret = 0;
	goto complete;

error:
	if (encoder->media_fd) {
		close(encoder->media_fd);
		encoder->media_fd = -1;
	}

	if (encoder->video_fd) {
		close(encoder->video_fd);
		encoder->video_fd = -1;
	}

	ret = -1;

complete:
	if (enumerate)
		udev_enumerate_unref(enumerate);

	if (udev)
		udev_unref(udev);

	return ret;
}

void v4l2_encoder_close(struct v4l2_encoder *encoder)
{
	if (!encoder)
		return;

	if (encoder->bitstream_fd > 0) {
		close(encoder->bitstream_fd);
		encoder->bitstream_fd = -1;
	}

	if (encoder->media_fd > 0) {
		close(encoder->media_fd);
		encoder->media_fd = -1;
	}

	if (encoder->video_fd > 0) {
		close(encoder->video_fd);
		encoder->video_fd = -1;
	}
}
