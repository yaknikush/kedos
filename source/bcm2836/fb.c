#include <bcm2836/fb.h>
#include <bcm2836/mbox.h>
#include <bcm2836/cache.h>
#include <bcm2836/videocore.h>
#include <bcm2836/defines.h>
#include <lib/nostdio.h>

/* was taken from gpio.h */
#define __PTR(a) ((uint32_t *)((void *)(a)))

#define FB_WIDTH         1680
#define FB_HEIGHT        1050
#define FB_COLOR_DEPTH   24 /* only this mode now is supported */

/* frame buffer messages to mailbox constants */
#define FB_MAX_MSG_SIZE        144
#define FB_MSG_HEADER_SIZE     8
#define FB_MSG_TAG_HEADER_SIZE 12
#define FB_MSG_TAIL_SIZE       4

typedef struct {
	uint32_t buf[FB_MAX_MSG_SIZE / sizeof(uint32_t)];
	uint8_t cur_index;
	uint8_t size;
} fb_message;

typedef struct {
	uint32_t id;
	uint32_t param_cap;
	uint32_t param_len;
	uint32_t param0;
	uint32_t param1;
} fb_message_tag;

#define fb_init_message(msg)                                \
do {                                                        \
	(msg).cur_index = FB_MSG_HEADER_SIZE;                   \
	(msg).size = FB_MSG_HEADER_SIZE + FB_MSG_TAIL_SIZE;     \
} while(0);

static inline void fb_message_push_tag(fb_message* msg, fb_message_tag tag)
{
	int tag_size = FB_MSG_TAG_HEADER_SIZE + tag.param_len;
	if (msg->size + tag_size > FB_MAX_MSG_SIZE) {
		return;
	}

	/* init tag header */
	msg->buf[msg->cur_index++] = tag.id;
	msg->buf[msg->cur_index++] = tag.param_cap;
	msg->buf[msg->cur_index++] = tag.param_len;

	/* init tag params */
	if (tag.param_len > 0) {
		msg->buf[msg->cur_index++] = tag.param0;
		if (tag.param_len > sizeof(uint32_t)) {
			msg->buf[msg->cur_index++] = tag.param1;
		}
	}	

	msg->size += tag_size;
}

static inline void fb_message_pop_tag(fb_message *msg, fb_message_tag *tag)
{
	if (msg->cur_index * sizeof(uint32_t)
		+ FB_MSG_TAG_HEADER_SIZE
		+ FB_MSG_TAIL_SIZE > msg->size) {
		return;
	}

	tag->id = msg->buf[msg->cur_index++];
	tag->param_cap = msg->buf[msg->cur_index++];
	tag->param_len = msg->buf[msg->cur_index++];

	if (msg->cur_index * sizeof(uint32_t)
		+ tag->param_len
		+ FB_MSG_TAIL_SIZE > msg->size) {
		return;
	}

	if (tag->param_len > 0) {
		tag->param0 = msg->buf[msg->cur_index++];
		if (tag->param_len > sizeof(uint32_t)) {
			tag->param1 = msg->buf[msg->cur_index++];
		}
	}
}

static void fb_dump_message_header(fb_message* msg)
{
	kprint("message header:\n"
	       "   size: %d == 0x%08x\n"
	       "   code: %d == 0x%08x\n",
	       msg->buf[msg->cur_index + 0], msg->buf[msg->cur_index + 0],
	       msg->buf[msg->cur_index + 1], msg->buf[msg->cur_index + 1]
	);
	msg->cur_index += 2;
}

static void fb_dump_message_tag(fb_message* msg, int n_tag) {
	int n_params = 0, param_cap = 0, param_len = 0;
	kprint("message tag #%d:\r\n"
		   "    tag ID:     %d == 0x%08x\r\n"
		   "    param cap:  %d == 0x%08x\r\n"
		   "    param len:  %d == 0x%08x\r\n",
		   n_tag,
		   param_cap = msg->buf[msg->cur_index + 0], msg->buf[msg->cur_index + 0],
		   param_cap = msg->buf[msg->cur_index + 1], msg->buf[msg->cur_index + 1],
		   param_len = msg->buf[msg->cur_index + 2], msg->buf[msg->cur_index + 2]
	);
	msg->cur_index += 3;
	n_params = param_cap / sizeof(uint32_t);
	kprint("    params:     (");
	while (n_params-- > 0) {
		kprint("  %d == 0x%08x",
			msg->buf[msg->cur_index + 0], msg->buf[msg->cur_index] + 0);
		msg->cur_index += 1;
	}
	kprint("  )\r\n");
}

static void fb_dump_message(fb_message* msg) {
	int index = msg->cur_index;
	msg->cur_index = 0;
	fb_dump_message_header(msg);
	int n_tag = 0;
	while (msg->buf[msg->cur_index] != 0 &&
		msg->cur_index * sizeof(uint32_t) < FB_MAX_MSG_SIZE) {
		fb_dump_message_tag(msg, ++n_tag);
	}
	msg->cur_index = index;
}


static inline fb_error fb_message_call_mbox(fb_message *msg)
{
	/* every msg ends with end tag >> TAIL */
	msg->buf[msg->cur_index++] = 0;

	/* every msg starts with msg size and request code >> HEAD */
	msg->buf[0] = msg->size;
	msg->buf[1] = 0;

	kprint("sending message...\n");
	fb_dump_message(msg);		

#if defined (RPI2)
	clean_data_cache();
#endif
	dsb();

    mbox_flush();
	mbox_write(MBOX_PROP, GPU_MEM_BASE + (uint32_t)msg);
	mbox_read(MBOX_PROP);

#if defined (RPI2)
	invalidate_data_cache();
#endif
	dsb();

	if (msg->buf[1] != MBOX_SUCCESS) {
		kprint("recieving message...\n");
		fb_dump_message(msg);
		return FB_EINVREQ;
	}

	msg->cur_index = FB_MSG_HEADER_SIZE;

	return FB_OK;
}

static inline fb_error fb_message_flush(fb_message *msg)
{
	for(int index = 0; index < msg->size; index++) {
		msg->buf[index] = 0;
	}
	msg->cur_index = msg->size = 0;

	return FB_OK;
}

static inline fb_error fb_get_pixel_offset(fb_frame_buffer* fb, fb_pixel pixel, uint32_t* offset)
{
	uint32_t off;

	off = (pixel.y * fb->pitch + pixel.x) * fb->depth;
	if (off > fb->size) {
		return FB_EINVPXL;
	}

	return FB_OK;
}

fb_error fb_allocate(fb_frame_buffer* fb)
{
	fb_error err;

	fb_message msg __attribute__((aligned(16)));
	fb_message_tag tag;

	int width, height;

	fb_init_message(msg);

	tag.id = BCM2835_VC_TAG_GET_PHYS_WH;
	tag.param_cap = 2;
	tag.param_len = 0;
	tag.param0 = 0;
	tag.param1 = 0;
	fb_message_push_tag(&msg, tag);
	
	err = fb_message_call_mbox(&msg);
	if (err != FB_OK) {
		return err;
	}

	fb_message_pop_tag(&msg, &tag);
	kprint("width: %d, height: %d\r\n", tag.param0, tag.param1);
	fb->phys_resol.width = fb->virt_resol.width = (tag.param0? tag.param0 : FB_WIDTH);
	fb->phys_resol.height = fb->virt_resol.height = (tag.param1? tag.param1 : FB_HEIGHT);

	fb_message_flush(&msg);
	
	tag.id = BCM2835_VC_TAG_GET_PITCH;
	tag.param_cap = 2;
	tag.param_len = 0;
	fb_message_push_tag(&msg, tag);

	tag.id = BCM2835_VC_TAG_GET_PITCH;
	tag.param_cap = 1;
	tag.param_len = 1;
	tag.param0 = FB_COLOR_DEPTH;
	fb_message_push_tag(&msg, tag);

	tag.id = BCM2835_VC_TAG_SET_PHYS_WH;
	tag.param_cap = 2;
	tag.param_len = 2;
	tag.param0 = fb->phys_resol.width;
	tag.param1 = fb->phys_resol.height;
	fb_message_push_tag(&msg, tag);
	

	tag.id = BCM2835_VC_TAG_SET_VIRT_WH;
	tag.param_cap = 2;
	tag.param_len = 2;
	tag.param0 = fb->virt_resol.width;
	tag.param1 = fb->virt_resol.height;
	fb_message_push_tag(&msg, tag);
	
	err = fb_message_call_mbox(&msg);
	if (err != FB_OK) {
		return err;
	}

	fb_message_pop_tag(&msg, &tag);
	fb->pitch = tag.param0;
	kprint("pitch: %d\r\n", tag.param0);

	fb_message_flush(&msg);

	tag.id = BCM2835_VC_TAG_ALLOCATE_BUFFER;
	tag.param_cap = 2;
	tag.param_len = 0;
	fb_message_push_tag(&msg, tag);

	err = fb_message_call_mbox(&msg);
	if (err != FB_OK) {
		return err;
	}
	
	fb_message_pop_tag(&msg, &tag);
	fb->ptr = tag.param0;
	fb->size = tag.param1;

	kprint("ptr: 0x%08x, size: %d\r\n", tag.param0, tag.param1);

	return FB_OK;
}

fb_error fb_set_pixel_color(fb_frame_buffer* fb, fb_pixel pixel, fb_color color)
{
	fb_error err;
	uint32_t off;

	err = fb_get_pixel_offset(fb, pixel, &off);
	if (err != FB_OK) {
		return err;
	}

	__PTR(fb->ptr)[off] = (color.r << 0) | (color.g << 8) | (color.b << 16);

	return FB_OK;
}

fb_error fb_get_pixel_color(fb_frame_buffer* fb, fb_pixel pixel, fb_color* color)
{
	fb_error err;
	uint32_t off, val;
	const uint32_t FIRST_BYTE = 0xff;
	
	err = fb_get_pixel_offset(fb, pixel, &off);
	if (err != FB_OK) {
		return err;
	}

	val = __PTR(fb->ptr)[off];
	fb_init_color(*color, (val >> 0) & FIRST_BYTE, (val >> 8) & FIRST_BYTE, (val >> 16) & FIRST_BYTE);

	return FB_OK;
}

fb_error fb_flush(fb_frame_buffer* fb)
{
	fb_message msg;
	fb_message_tag tag;
	fb_init_message(msg);
	
	tag.id = BCM2835_VC_TAG_BLANK_SCREEN;
	tag.param_cap = 0;
	tag.param_len = 0;
	fb_message_push_tag(&msg, tag);
	
	return fb_message_call_mbox(&msg);
}


fb_error fb_release(fb_frame_buffer* fb)
{
	fb_message msg;
	fb_message_tag tag;
	fb_init_message(msg);

	tag.id = BCM2835_VC_TAG_RELEASE_BUFFER;
	tag.param_cap = 0;
	tag.param_len = 0;
	fb_message_push_tag(&msg, tag);
	
	return fb_message_call_mbox(&msg);
}

const char* fb_get_error_message(fb_error err)
{
	switch (err) {
	case FB_OK:
		return "success";
	case FB_EINVREQ:
		return "invalid request to mailbox";
	case FB_EINVRES:
		return "invalid response from mailbox";
	case FB_EINVPXL:
		return "invalid pixel of frame buffer";
	default:
		return "unknown error";
	}
}