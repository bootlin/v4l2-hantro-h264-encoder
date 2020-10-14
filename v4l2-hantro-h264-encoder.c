/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <v4l2.h>
#include <v4l2-encoder.h>

int main(int argc, char *argv[])
{
	struct v4l2_encoder *encoder = NULL;
	unsigned int width = 640;
	unsigned int height = 480;
	unsigned int frames = 10;
	int ret;

	encoder = calloc(1, sizeof(*encoder));
	if (!encoder)
		goto error;

	ret = v4l2_encoder_open(encoder);
	if (ret)
		goto error;

	ret = v4l2_encoder_probe(encoder);
	if (ret)
		goto error;

	ret = v4l2_encoder_setup_defaults(encoder);
	if (ret)
		goto error;

	ret = v4l2_encoder_setup_dimensions(encoder, width, height);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup(encoder);
	if (ret)
		goto error;

	ret = v4l2_encoder_start(encoder);
	if (ret)
		goto error;

	while (frames--) {
		ret = v4l2_encoder_prepare(encoder);
		if (ret)
			goto error;

		ret = v4l2_encoder_run(encoder);
		if (ret)
			goto error;

		ret = v4l2_encoder_complete(encoder);
		if (ret)
			goto error;
	}

	ret = 0;
	goto complete;

error:
	ret = 1;

complete:
	if (encoder) {
		v4l2_encoder_stop(encoder);
		v4l2_encoder_teardown(encoder);
		v4l2_encoder_close(encoder);

		free(encoder);
	}

	return ret;
}
