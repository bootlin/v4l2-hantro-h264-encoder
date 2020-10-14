/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _DRAW_H_
#define _DRAW_H_

struct draw_buffer {
	void *data;
	unsigned int size;

	unsigned int width;
	unsigned int height;
	unsigned int stride;
};

struct draw_mandelbrot {
	float center_x;
	float center_y;
	float view_width;
	float view_height;

	float zoom;

	float bounds_x[2];
	float bounds_y[2];
	float iterations_zoom;
	unsigned int iterations;
};

static inline uint32_t *draw_buffer_pixel(struct draw_buffer *buffer,
					  unsigned int x, unsigned int y)
{
	unsigned int offset = y * buffer->stride + x * sizeof(uint32_t);

	return (uint32_t *)(buffer->data + offset);
}

struct draw_buffer *draw_buffer_create(unsigned int width, unsigned int height);
void draw_buffer_destroy(struct draw_buffer *buffer);
void draw_png(struct draw_buffer *buffer, char *path);
void draw_gradient(struct draw_buffer *buffer);
void draw_background(struct draw_buffer *buffer, uint32_t color);
void draw_rectangle(struct draw_buffer *buffer, unsigned int x_start,
		    unsigned int y_start, unsigned int width,
		    unsigned int height, uint32_t color);
void draw_mandelbrot(struct draw_mandelbrot *mandelbrot,
		     struct draw_buffer *buffer);
void draw_mandelbrot_zoom(struct draw_mandelbrot *mandelbrot);
void draw_mandelbrot_init(struct draw_mandelbrot *mandelbrot);

#endif
