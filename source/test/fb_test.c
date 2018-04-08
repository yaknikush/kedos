#include <bcm2836/fb.h>
#include <bcm2836/timer.h>
#include <test/test.h>

#define FB_TEST_SLEEP_TIME  1000000    // 1 second to watch the colored pixel(s)

#define DBGCALL(func, ...)                       \
({                                               \
    kprint("\"" #func "\" called!\r\n");         \
    fb_error err = func(__VA_ARGS__);            \
    kprint("\"" #func "\" returned %s!\r\n",     \
        (err == FB_OK)? "FB_OK" :                \
        (err == FB_EINVREQ)? "FB_EINVREQ" :      \
        (err == FB_EINVRES)? "FB_EINVRES" :      \
        (err == FB_EINVRES)? "FB_EINVPXL" :      \
        "unknown error code"                     \
    );                                           \
    err;                                         \
})

#define fb_test_run(test_run)                                         \
do {                                                                  \
    kprint("=== RUN " #test_run "\r\n");                              \
    kprint("=== %s "  #test_run "\r\n", test_run()? "PASS" : "FAIL"); \
} while (0)

static inline int fb_verify_errors_equal(fb_error got_error, fb_error expected_error)
{
	int ok = (got_error == expected_error);
	if (!ok) {
		kprint("Error: Got error \"%s\", expected \"%s\".\r\n",
			fb_get_error_message(got_error),
			fb_get_error_message(expected_error));
	}
	return ok;
}

static inline int fb_verify_colors_equal(fb_color got_color, fb_color expected_color)
{
	int ok = (got_color.r == expected_color.r) &&
	         (got_color.g == expected_color.g) &&
	         (got_color.b == expected_color.b);
	if (!ok) {
		kprint("Error: Got error (r: %d, g: %d, b: %d), expected (r: %d, g: %d, b: %d).\r\n",
			got_color.r, got_color.g, got_color.b,
			expected_color.r, expected_color.g, expected_color.b);
	}
	return ok;
}

/* fb_verifying that resolution we get the same as we set before */
static int fb_test_allocate_release()
{
	int ok = 1;
	fb_error expected_error = FB_OK,
	         got_error;

	fb_frame_buffer fb;
	
	got_error = DBGCALL(fb_allocate, &fb);
	ok = fb_verify_errors_equal(got_error, expected_error);
	if (!ok) {
		return ok;
	}

	got_error = DBGCALL(fb_release, &fb);
	ok = fb_verify_errors_equal(got_error, expected_error);
	if (!ok) {
		return ok;
	}

	return ok;
}

static int fb_set_bold_pixel_color(fb_frame_buffer* fb, fb_pixel pixel, fb_color color)
{
	const int radius = 10;

	fb_error err;
	
	int x_max = fb->phys_resol.width,
	    y_max = fb->phys_resol.height;

	for (unsigned int x = pixel.x - radius; x < pixel.x + radius; x++) {
		for (unsigned int y = pixel.y - radius; y < pixel.y + radius; y++) {
			if ((x * x + y * y < radius * radius) && (x < x_max && y < y_max)) {
				fb_pixel p = {x, y};
				err = DBGCALL(fb_set_pixel_color, fb, pixel, color);
				if (err != FB_OK) {
					return err;
				}
			}
		}
	}
}

/* fb_verifying that pixel color we get the same as we set before */
static int fb_test_set_get_pixel()
{
	int ok = 1;
	fb_error expected_error = FB_OK,
	         got_error;
	
	fb_color expected_colors[] = {{0xff, 0x00, 0x00}, {0x00, 0xff, 0x00}, {0x00, 0x00, 0xff}, {0xff, 0xff, 0xff}}, //FB_RED, FB_GREEN, FB_BLUE, FB_BLACK, FB_WHITE},
	         got_color;

	fb_frame_buffer fb;
	fb_pixel pixel;

	DBGCALL(fb_allocate, &fb);
	ok = fb_verify_errors_equal(got_error, expected_error);
	if (!ok) {
		return ok;
	}

	fb_init_pixel(pixel, fb.phys_resol.width / 2, fb.phys_resol.height / 2);

	int n_expected_colors = sizeof(expected_colors) / sizeof(expected_colors[0]);
	for (int n = 0; n < n_expected_colors; n++) {

		fb_color expected_color = expected_colors[n];

		got_error = DBGCALL(fb_set_bold_pixel_color, &fb, pixel, expected_color);
		ok = fb_verify_errors_equal(got_error, expected_error);
		if (!ok) {
			return ok;
		}

		usleep(FB_TEST_SLEEP_TIME);

		got_error = DBGCALL(fb_get_pixel_color, &fb, pixel, &expected_color);
		ok = fb_verify_errors_equal(got_error, expected_error);
		if (!ok) {
			return ok;
		}

		ok = fb_verify_colors_equal(got_color, expected_color);
		if (!ok) {
			return ok;
		}
	}

	DBGCALL(fb_release, &fb);

	return ok;
}

static int fb_test_flush()
{
	int ok;
	fb_error expected_error = FB_OK,
	         got_error;

	fb_color expected_color = FB_BLACK,
	         got_color;

	fb_frame_buffer fb;
	DBGCALL(fb_allocate, &fb);

	fb_color color = FB_RED;
	fb_pixel pixels[] = {
		{0.75 * fb.phys_resol.width, 0.75 * fb.phys_resol.height},
		{0.25 * fb.phys_resol.width, 0.75 * fb.phys_resol.height},
		{0.25 * fb.phys_resol.width, 0.25 * fb.phys_resol.height},
		{0.75 * fb.phys_resol.width, 0.25 * fb.phys_resol.height}
	};

	int n_pixels = sizeof(pixels) / sizeof(pixels[0]);
	for (int n = 0; n < n_pixels; n++) {
		fb_pixel pixel = pixels[n];
		DBGCALL(fb_set_bold_pixel_color, &fb, pixel, color);
	}

	got_error = DBGCALL(fb_flush, &fb);
	ok = fb_verify_errors_equal(got_error, expected_error);
	if (!ok) {
		return ok;
	}

	for (int n = 0; n < n_pixels; n++) {
		fb_pixel pixel = pixels[n];
		DBGCALL(fb_get_pixel_color, &fb, pixel, &got_color);
		ok = fb_verify_colors_equal(got_color, expected_color);
		if (!ok) {
			return ok;
		}
	}

	DBGCALL(fb_release, &fb);

	return ok;
}

int fb_test() {

	usleep(5);

	kprint(" * * * * * FRAME BUFFER TEST * * * * *\r\n");
	fb_test_run(fb_test_allocate_release);
	fb_test_run(fb_test_set_get_pixel);
	fb_test_run(fb_test_flush);
	kprint(" * * * * * * * * * * * * * * * * * * *\r\n");

	return 0;
}

#undef fb_test_run
#undef fb_fb_verify_equal
#undef fb_fb_verify_errors_equal
#undef fb_fb_verify_colors_equal

#undef FB_TEST_SLEEP_TIME