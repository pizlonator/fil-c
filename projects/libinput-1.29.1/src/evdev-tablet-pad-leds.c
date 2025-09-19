/*
 * Copyright © 2016 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <fcntl.h>
#include <limits.h>

#include "evdev-tablet-pad.h"

#ifdef HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#endif

/* We only have 4 modes on current devices but
 * anything other than MODE_NEXT is used as
 * numeric value anyway so it doesn't matter if a value is
 * missing here
 *
 * Once libwacom 2.15 is commonplace we can use the
 * libwacom enum directly.
 */
enum pad_toggle_button_target_mode {
	MODE_NEXT = -1,
	MODE_0,
	MODE_1,
	MODE_2,
	MODE_3,
};

struct pad_led_group {
	struct libinput_tablet_pad_mode_group base;
	struct list led_list;
	struct list toggle_button_list;
};

struct pad_mode_toggle_button {
	struct list link;
	unsigned int button_index;
	enum pad_toggle_button_target_mode target_mode;
};

struct pad_mode_led {
	struct list link;
	/* /sys/devices/..../input1235/input1235::wacom-led_0.1/brightness */
	int brightness_fd;
	int mode_idx;
};

static inline void
pad_mode_toggle_button_destroy(struct pad_mode_toggle_button *button)
{
	list_remove(&button->link);
	free(button);
}

static inline int
pad_led_group_get_mode(struct pad_led_group *group)
{
	char buf[4] = { 0 };
	int rc;
	unsigned int brightness;
	struct pad_mode_led *led;

	list_for_each(led, &group->led_list, link) {
		rc = lseek(led->brightness_fd, 0, SEEK_SET);
		if (rc == -1)
			return -errno;

		rc = read(led->brightness_fd, buf, sizeof(buf) - 1);
		if (rc == -1)
			return -errno;

		rc = sscanf(buf, "%u\n", &brightness);
		if (rc != 1)
			return -EINVAL;

		/* Assumption: only one LED lit up at any time */
		if (brightness != 0)
			return led->mode_idx;
	}

	/* Wacom PTH-660 doesn't light up any LEDs
	 * until the button is pressed, so let's assume mode 0 */
	return 0;
}

static inline void
pad_led_destroy(struct libinput *libinput, struct pad_mode_led *led)
{
	list_remove(&led->link);
	if (led->brightness_fd != -1)
		close_restricted(libinput, led->brightness_fd);
	free(led);
}

#ifdef HAVE_LIBWACOM
static inline struct pad_mode_led *
pad_led_new(struct libinput *libinput, const char *prefix, int group, int mode)
{
	struct pad_mode_led *led;
	char path[PATH_MAX];
	int rc, fd, save_errno;

	led = zalloc(sizeof *led);
	led->brightness_fd = -1;
	led->mode_idx = mode;
	list_init(&led->link);

	/* /sys/devices/..../input1235/input1235::wacom-0.1/brightness,
	 * where 0 and 1 are group and mode index. */
	rc = snprintf(path, sizeof(path), "%s%d.%d/brightness", prefix, group, mode);
	if (rc == -1)
		goto error;

	fd = open_restricted(libinput, path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		errno = -fd;
		goto error;
	}

	led->brightness_fd = fd;

	return led;

error:
	save_errno = errno;
	pad_led_destroy(libinput, led);
	errno = save_errno;
	return NULL;
}
#endif /* HAVE_LIBWACOM */

static void
pad_led_group_destroy(struct libinput_tablet_pad_mode_group *g)
{
	struct pad_led_group *group = (struct pad_led_group *)g;
	struct pad_mode_toggle_button *button;
	struct pad_mode_led *led;

	list_for_each_safe(button, &group->toggle_button_list, link)
		pad_mode_toggle_button_destroy(button);

	list_for_each_safe(led, &group->led_list, link)
		pad_led_destroy(g->device->seat->libinput, led);

	free(group);
}

static struct pad_led_group *
pad_group_new(struct pad_dispatch *pad, unsigned int group_index, int num_modes)
{
	struct pad_led_group *group;

	group = zalloc(sizeof *group);
	group->base.device = &pad->device->base;
	group->base.refcount = 1;
	group->base.index = group_index;
	group->base.current_mode = 0;
	group->base.num_modes = num_modes;
	group->base.destroy = pad_led_group_destroy;
	list_init(&group->toggle_button_list);
	list_init(&group->led_list);

	return group;
}

static inline struct libinput_tablet_pad_mode_group *
pad_get_mode_group(struct pad_dispatch *pad, unsigned int index)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (group->index == index)
			return group;
	}

	return NULL;
}

#ifdef HAVE_LIBWACOM
static inline bool
is_litest_device(struct evdev_device *device)
{
	return !!udev_device_get_property_value(device->udev_device,
						"LIBINPUT_TEST_DEVICE");
}

static inline struct pad_mode_toggle_button *
pad_mode_toggle_button_new(unsigned int button_index)
{
	struct pad_mode_toggle_button *button;

	button = zalloc(sizeof *button);
	button->button_index = button_index;
	button->target_mode = MODE_NEXT;

	return button;
}

static int
pad_led_group_add_toggle_button(struct pad_led_group *group,
				int button_index,
				enum pad_toggle_button_target_mode mode)
{
	struct pad_mode_toggle_button *button;

	button = pad_mode_toggle_button_new(button_index);
	if (!button)
		return -ENOMEM;

	button->target_mode = mode;
	list_append(&group->toggle_button_list, &button->link);
	group->base.toggle_button_mask |= bit(button_index);
	group->base.button_mask |= bit(button_index);

	return 0;
}

static inline bool
pad_led_get_sysfs_base_path(struct evdev_device *device,
			    char *path_out,
			    size_t path_out_sz)
{
	struct udev_device *parent, *udev_device;
	const char *test_path;
	int rc;

	udev_device = device->udev_device;

	/* For testing purposes only allow for a base path set through a
	 * udev rule. We still expect the normal directory hierarchy inside */
	test_path =
		udev_device_get_property_value(udev_device,
					       "LIBINPUT_TEST_TABLET_PAD_SYSFS_PATH");
	if (test_path) {
		rc = snprintf(path_out, path_out_sz, "%s", test_path);
		return rc != -1;
	}

	parent = udev_device_get_parent_with_subsystem_devtype(udev_device,
							       "input",
							       NULL);
	if (!parent)
		return false;

	rc = snprintf(path_out,
		      path_out_sz,
		      "%s/%s::wacom-",
		      udev_device_get_syspath(parent),
		      udev_device_get_sysname(parent));

	return rc != -1;
}

static int
pad_add_mode_group(struct pad_dispatch *pad,
		   struct evdev_device *device,
		   unsigned int group_index,
		   int nmodes,
		   int button_index,
		   enum pad_toggle_button_target_mode mode,
		   uint32_t ring_mask,
		   uint32_t strip_mask,
		   uint32_t dial_mask,
		   bool create_leds)
{
	struct libinput *li = pad_libinput_context(pad);
	struct pad_led_group *group = NULL;
	int rc = -ENOMEM;
	char syspath[PATH_MAX];

	/* syspath is /sys/class/leds/input1234/input12345::wacom-" and
	   only needs the group + mode appended */
	if (!pad_led_get_sysfs_base_path(device, syspath, sizeof(syspath)))
		return -ENOMEM;

	group = pad_group_new(pad, group_index, nmodes);
	if (!group)
		goto out;
	group->base.ring_mask = ring_mask;
	group->base.strip_mask = strip_mask;
	group->base.dial_mask = dial_mask;
	group->base.button_mask |= bit(button_index);

	rc = pad_led_group_add_toggle_button(group, button_index, mode);
	if (rc < 0)
		goto out;

	for (int mode = 0; create_leds && mode < nmodes; mode++) {
		struct pad_mode_led *led;

		led = pad_led_new(li, syspath, group_index, mode);
		if (!led) {
			rc = -errno;
			goto out;
		}
		list_append(&group->led_list, &led->link);
	}

	if (create_leds) {
		rc = pad_led_group_get_mode(group);
		if (rc < 0) {
			goto out;
		}
	}

	list_insert(&pad->modes.mode_group_list, &group->base.link);
	rc = 0;
out:
	if (rc)
		pad_led_group_destroy(&group->base);

	return rc;
}

static int
pad_fetch_group_index(struct pad_dispatch *pad, WacomDevice *wacom, int button_index)
{
	char btn = 'A' + button_index;
	WacomButtonFlags flags = libwacom_get_button_flag(wacom, btn);
	int led_group = libwacom_get_button_led_group(wacom, btn);

	if ((flags & WACOM_BUTTON_MODESWITCH) == 0) {
		evdev_log_bug_libinput(
			pad->device,
			"Cannot fetch group index for non-mode toggle button %c\n",
			btn);
		return -1;
	}

	if (led_group >= 0)
		return led_group;

	/* Note on group_index: libwacom gives us the led_group
	 * but this is really just the index of the entry in
	 * the StatusLEDs line. This is effectively always
	 * 0 for the first ring or strip and 1 for the second
	 * ring or strip. The few devices that matter
	 * have either a pair of rings or strips, not mixed.
	 *
	 * Which means for devices where we don't have StatusLEDs
	 * we can hardcode this behavior, if we ever get a ring+strip
	 * devcice we need to update libwacom for that anyway.
	 */

	int group_index = -1;
	switch (flags & WACOM_BUTTON_MODESWITCH) {
	case WACOM_BUTTON_RING_MODESWITCH:
		group_index = 0;
		break;
	case WACOM_BUTTON_RING2_MODESWITCH:
		group_index = 1;
		break;
	case WACOM_BUTTON_TOUCHSTRIP_MODESWITCH:
		group_index = 0;
		break;
	case WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH:
		group_index = 1;
		break;
#ifdef HAVE_LIBWACOM_BUTTON_DIAL_MODESWITCH
	case WACOM_BUTTON_DIAL_MODESWITCH:
		group_index = 0;
		break;
	case WACOM_BUTTON_DIAL2_MODESWITCH:
		group_index = 1;
		break;
#endif
	}

	return group_index;
}

static inline int
pad_find_button_group(struct pad_dispatch *pad,
		      WacomDevice *wacom,
		      int button_index,
		      WacomButtonFlags button_flags)
{
	int i;
	WacomButtonFlags flags;

	for (i = 0; i < libwacom_get_num_buttons(wacom); i++) {
		if (i == button_index)
			continue;

		flags = libwacom_get_button_flag(wacom, 'A' + i);
		if ((flags & WACOM_BUTTON_MODESWITCH) == 0)
			continue;

		if ((flags & WACOM_BUTTON_DIRECTION) ==
		    (button_flags & WACOM_BUTTON_DIRECTION))
			return pad_fetch_group_index(pad, wacom, i);
	}

	return -1;
}

static enum pad_toggle_button_target_mode
pad_button_target_mode(WacomDevice *wacom, char button)
{
#ifdef HAVE_LIBWACOM_BUTTON_MODESWITCH_MODE
	switch (libwacom_get_button_modeswitch_mode(wacom, button)) {
	case WACOM_MODE_SWITCH_NEXT:
		return MODE_NEXT;
	case WACOM_MODE_SWITCH_0:
		return MODE_0;
	case WACOM_MODE_SWITCH_1:
		return MODE_1;
	case WACOM_MODE_SWITCH_2:
		return MODE_2;
	case WACOM_MODE_SWITCH_3:
		return MODE_3;
	}
#endif
	return MODE_NEXT;
}

static int
pad_init_leds_from_libwacom(struct pad_dispatch *pad,
			    struct evdev_device *device,
			    WacomDevice *wacom)
{
	int rc = -EINVAL;

	if (!wacom)
		return -ENOENT;

	for (int b = 0; b < libwacom_get_num_buttons(wacom); b++) {
		char btn = 'A' + b;
		WacomButtonFlags flags = libwacom_get_button_flag(wacom, btn);
		int nmodes = 0;
		uint32_t ring_mask = 0;
		uint32_t strip_mask = 0;
		uint32_t dial_mask = 0;
		bool have_status_led = false;

		if ((flags & WACOM_BUTTON_MODESWITCH) == 0)
			continue;

		enum pad_toggle_button_target_mode target_mode =
			pad_button_target_mode(wacom, btn);

		int group_index = pad_fetch_group_index(pad, wacom, b);
		switch (flags & WACOM_BUTTON_MODESWITCH) {
		case WACOM_BUTTON_RING_MODESWITCH:
			nmodes = libwacom_get_ring_num_modes(wacom);
			ring_mask = 0x1;
			break;
		case WACOM_BUTTON_RING2_MODESWITCH:
			nmodes = libwacom_get_ring2_num_modes(wacom);
			ring_mask = 0x2;
			break;
		case WACOM_BUTTON_TOUCHSTRIP_MODESWITCH:
			nmodes = libwacom_get_strips_num_modes(wacom);
			strip_mask = 0x1;
			break;
		case WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH:
			/* there is no get_strips2_... */
			nmodes = libwacom_get_strips_num_modes(wacom);
			strip_mask = 0x2;
			break;
#ifdef HAVE_LIBWACOM_BUTTON_DIAL_MODESWITCH
		case WACOM_BUTTON_DIAL_MODESWITCH:
			nmodes = libwacom_get_dial_num_modes(wacom);
			dial_mask = 0x1;
			break;
		case WACOM_BUTTON_DIAL2_MODESWITCH:
			nmodes = libwacom_get_dial2_num_modes(wacom);
			dial_mask = 0x2;
			break;
#endif
		default:
			evdev_log_error(
				pad->device,
				"unable to init pad mode group: button %c has multiple modeswitch flags 0x%x\n",
				btn,
				flags);
			goto out;
		}
		have_status_led = libwacom_get_button_led_group(wacom, btn) >= 0;
		if (nmodes > 1) {
			struct libinput_tablet_pad_mode_group *group =
				pad_get_mode_group(pad, group_index);
			if (!group) {
				rc = pad_add_mode_group(pad,
							device,
							group_index,
							nmodes,
							b,
							target_mode,
							ring_mask,
							strip_mask,
							dial_mask,
							have_status_led);
			} else {
				struct pad_led_group *led_group =
					(struct pad_led_group *)group;
				/* Multiple toggle buttons (Wacom MobileStudio Pro 16)
				 */
				rc = pad_led_group_add_toggle_button(led_group,
								     b,
								     target_mode);
				if (rc < 0)
					goto out;
			}
		}
	}

	if (list_empty(&pad->modes.mode_group_list)) {
		rc = 1;
		goto out;
	}

	/* Now loop again to match the other buttons with the existing
	 * mode groups */
	for (int b = 0; b < libwacom_get_num_buttons(wacom); b++) {
		char btn = 'A' + b;
		WacomButtonFlags flags = libwacom_get_button_flag(wacom, btn);

		if (flags & WACOM_BUTTON_MODESWITCH)
			continue;

		int group_index = pad_find_button_group(pad, wacom, b, flags);
		if (group_index == -1) {
			evdev_log_bug_libinput(pad->device,
					       "unhandled position for button %i\n",
					       b);
			rc = -EINVAL;
			goto out;
		}

		struct libinput_tablet_pad_mode_group *group =
			pad_get_mode_group(pad, group_index);
		if (!group) {
			evdev_log_bug_libinput(
				pad->device,
				"Failed to find group %d for button %i\n",
				group_index,
				b);
			rc = -EINVAL;
			goto out;
		}
		group->button_mask |= bit(b);
	}

	rc = 0;
out:
	if (rc != 0) {
		if (rc == -ENOENT && is_litest_device(pad->device)) {
			evdev_log_error(pad->device,
					"unable to init pad mode group: %s\n",
					strerror(-rc));
		}
		pad_destroy_leds(pad);
	}

	return rc;
}

#endif /* HAVE_LIBWACOM */

static int
pad_init_fallback_group(struct pad_dispatch *pad)
{
	struct pad_led_group *group;

	group = pad_group_new(pad, 0, 1);
	if (!group)
		return 1;

	/* If we only have one group, all buttons/strips/rings are part of
	 * that group. We rely on the other layers to filter out invalid
	 * indices */
	group->base.button_mask = -1;
	group->base.strip_mask = -1;
	group->base.ring_mask = -1;
	group->base.dial_mask = -1;
	group->base.toggle_button_mask = 0;

	list_insert(&pad->modes.mode_group_list, &group->base.link);

	return 0;
}

int
pad_init_leds(struct pad_dispatch *pad, struct evdev_device *device, WacomDevice *wacom)
{
	int rc = 1;

	list_init(&pad->modes.mode_group_list);

	if (pad->nbuttons > 32) {
		evdev_log_bug_libinput(pad->device,
				       "Too many pad buttons for modes %d\n",
				       pad->nbuttons);
		return rc;
	}

	/* If libwacom fails, we init one fallback group anyway */
#ifdef HAVE_LIBWACOM
	rc = pad_init_leds_from_libwacom(pad, device, wacom);
#endif
	if (rc != 0)
		rc = pad_init_fallback_group(pad);

	return rc;
}

void
pad_destroy_leds(struct pad_dispatch *pad)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each_safe(group, &pad->modes.mode_group_list, link)
		libinput_tablet_pad_mode_group_unref(group);
}

void
pad_button_update_mode(struct libinput_tablet_pad_mode_group *g,
		       unsigned int button_index,
		       enum libinput_button_state state)
{
	struct pad_led_group *group = (struct pad_led_group *)g;
	int rc = -ENODEV;

	if (state != LIBINPUT_BUTTON_STATE_PRESSED)
		return;

	if (!libinput_tablet_pad_mode_group_button_is_toggle(g, button_index))
		return;

	if (list_empty(&group->led_list)) {
		struct pad_mode_toggle_button *button;
		list_for_each(button, &group->toggle_button_list, link) {
			if (button->button_index == button_index) {
				if (button->target_mode == MODE_NEXT) {
					unsigned int nmodes = group->base.num_modes;
					rc = (group->base.current_mode + 1) % nmodes;
				} else {
					rc = button->target_mode;
				}
				break;
			}
		}
	} else {
		rc = pad_led_group_get_mode(group);
	}
	if (rc >= 0)
		group->base.current_mode = rc;
}

int
evdev_device_tablet_pad_get_num_mode_groups(struct evdev_device *device)
{
	struct pad_dispatch *pad = (struct pad_dispatch *)device->dispatch;
	struct libinput_tablet_pad_mode_group *group;
	int num_groups = 0;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	list_for_each(group, &pad->modes.mode_group_list, link)
		num_groups++;

	return num_groups;
}

struct libinput_tablet_pad_mode_group *
evdev_device_tablet_pad_get_mode_group(struct evdev_device *device, unsigned int index)
{
	struct pad_dispatch *pad = (struct pad_dispatch *)device->dispatch;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return NULL;

	if (index >= (unsigned int)evdev_device_tablet_pad_get_num_mode_groups(device))
		return NULL;

	return pad_get_mode_group(pad, index);
}
