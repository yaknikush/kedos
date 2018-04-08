#ifndef __FRAME_BUFFER__
#define __FRAME_BUFFER__

#include <bcm2836/defines.h>

typedef enum {
	FB_OK = 0,
	FB_EINVREQ = 1,
	FB_EINVRES = 2,
	FB_EINVPXL = 3
} fb_error;

const char * fb_get_error_message(fb_error err);

typedef struct {
	uint32_t width;
	uint32_t height;
} fb_resolution;

/* v_ - means value */
#define fb_init_resolution(res, v_width, v_height) \
do {                                               \
    (res).width = (v_width);                       \
    (res).height = (v_height);                     \
} while(0)

typedef struct {
	fb_resolution phys_resol;
	fb_resolution virt_resol;
	uint32_t pitch;
	uint32_t depth;
	uint32_t x_off;
	uint32_t y_off;
	uint32_t ptr;
	uint32_t size;
} fb_frame_buffer;

#define fb_init_frame_buffer(fb, v_phys_resol, v_virt_resol, v_pitch, v_depth, v_x_off, v_y_off, v_ptr, v_size) \
do {                                                                                                            \
    (fb).phys_resol = (v_phys_resol);                                                                           \
    (fb).virt_resol = (v_virt_resol);                                                                           \
    (fb).pitch = (v_pitch);                                                                                     \
    (fb).depth = (v_depth);                                                                                     \
    (fb).x_off = (v_x_off);                                                                                     \
    (fb).y_off = (v_y_off);                                                                                     \
    (fb).ptr = (v_ptr);                                                                                         \
    (fb).size = (v_size);                                                                                       \
} while(0)


typedef struct {
	uint32_t x;
	uint32_t y;
} fb_pixel;

#define fb_init_pixel(pixel, v_x, v_y)   \
do {                                     \
    (pixel).x = (v_x);                   \
    (pixel).y = (v_y);                   \
} while(0)

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} fb_color;

#define fb_init_color(color, v_r, v_g, v_b) \
do {                                        \
    (color).r = (v_r);                      \
    (color).g = (v_g);                      \
    (color).b = (v_b);                      \
} while(0)

#define FB_RED   ({fb_color color = {0xff, 0x00, 0x00}; color; })
#define FB_GREEN ({fb_color color = {0x00, 0xff, 0x00}; color; })
#define FB_BLUE  ({fb_color color = {0x00, 0x00, 0xff}; color; })
#define FB_WHITE ({fb_color color = {0xff, 0xff, 0xff}; color; })
#define FB_BLACK ({fb_color color = {0x00, 0x00, 0x00}; color; })

fb_error fb_allocate(fb_frame_buffer* fb);
fb_error fb_get_pixel_color(fb_frame_buffer *fb, fb_pixel pixel, fb_color* color);
fb_error fb_set_pixel_color(fb_frame_buffer *fb, fb_pixel pixel, fb_color color);
fb_error fb_flush(fb_frame_buffer* fb);
fb_error fb_release(fb_frame_buffer *fb);

int fb_test();

#endif