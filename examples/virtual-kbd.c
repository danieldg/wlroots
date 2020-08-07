/*
 * Copyright © 2019 Josef Gajdusek
 * Copyright © 2020 Daniel De Graaf
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

#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include "virtual-keyboard-unstable-v1-client-protocol.h"

static struct wl_seat *seat = NULL;
static struct zwp_virtual_keyboard_manager_v1 *keyboard_manager = NULL;
static struct {
	xkb_mod_mask_t press, latch, lock, group;
} mod_none, mod_shift;
static uint32_t ts = 0;

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name,
			&wl_seat_interface, version);
	} else if (strcmp(interface,
			zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		keyboard_manager = wl_registry_bind(registry, name,
			&zwp_virtual_keyboard_manager_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares?
}

static void setup_keymap(struct zwp_virtual_keyboard_v1 *keyboard) {
	int fd = memfd_create("keymap", MFD_CLOEXEC);
	struct xkb_rule_names rule_names = {
		.model = "pc104",
		.layout = "",
	};
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &rule_names, 0);
	struct xkb_state *state = xkb_state_new(keymap);
	char* keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	size_t size = strlen(keymap_string);
	write(fd, keymap_string, size);
	free(keymap_string);

	xkb_state_update_key(state, 50 /*lshift*/, XKB_KEY_DOWN);
	mod_shift.press = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
	mod_shift.latch = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
	mod_shift.lock  = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
	mod_shift.group = xkb_state_serialize_mods(state, XKB_STATE_MODS_EFFECTIVE);

	xkb_state_update_key(state, 50 /*lshift*/, XKB_KEY_UP);
	mod_none.press = xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED);
	mod_none.latch = xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED);
	mod_none.lock  = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
	mod_none.group = xkb_state_serialize_mods(state, XKB_STATE_MODS_EFFECTIVE);

	xkb_state_unref(state);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
	close(fd);
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

#define SHIFT 0x100
// from the keyboard map, values already subtracted 8 for zwp use
static const uint32_t charmap[256] = {
	[0x1b] = 1, // esc
	['1'] = 2,	['!'] = SHIFT | 2,
	['2'] = 3,	['@'] = SHIFT | 3,
	['3'] = 4,	['#'] = SHIFT | 4,
	['4'] = 5,	['$'] = SHIFT | 5,
	['5'] = 6,	['%'] = SHIFT | 6,
	['6'] = 7,	['^'] = SHIFT | 7,
	['7'] = 8,	['&'] = SHIFT | 8,
	['8'] = 9,	['*'] = SHIFT | 9,
	['9'] = 10,	['('] = SHIFT | 10,
	['0'] = 11,	[')'] = SHIFT | 11,
	['-'] = 12,	['_'] = SHIFT | 12,
	['='] = 13,	['+'] = SHIFT | 13,
	[0x08] = 14,
	['\t'] = 15,
	['q'] = 16,	['Q'] = SHIFT | 16,
	['w'] = 17,	['W'] = SHIFT | 17,
	['e'] = 18,	['E'] = SHIFT | 18,
	['r'] = 19,	['R'] = SHIFT | 19,
	['t'] = 20,	['T'] = SHIFT | 20,
	['y'] = 21,	['Y'] = SHIFT | 21,
	['u'] = 22,	['U'] = SHIFT | 22,
	['i'] = 23,	['I'] = SHIFT | 23,
	['o'] = 24,	['O'] = SHIFT | 24,
	['p'] = 25,	['P'] = SHIFT | 25,
	['['] = 26,	['{'] = SHIFT | 26,
	[']'] = 27,	['}'] = SHIFT | 27,
	['\n'] = 28,
	// LCtrl = 29,
	['a'] = 30,	['A'] = SHIFT | 30,
	['s'] = 31,	['S'] = SHIFT | 31,
	['d'] = 32,	['D'] = SHIFT | 32,
	['f'] = 33,	['F'] = SHIFT | 33,
	['g'] = 34,	['G'] = SHIFT | 34,
	['h'] = 35,	['H'] = SHIFT | 35,
	['j'] = 36,	['J'] = SHIFT | 36,
	['k'] = 37,	['K'] = SHIFT | 37,
	['l'] = 38,	['L'] = SHIFT | 38,
	[';'] = 39,	[':'] = SHIFT | 39,
	['\''] = 40,	['"'] = SHIFT | 40,
	['`'] = 41,	['~'] = SHIFT | 41,
	// LShift = 42,
	['\\'] = 43,	['|'] = SHIFT | 43,
	['z'] = 44,	['Z'] = SHIFT | 44,
	['x'] = 45,	['X'] = SHIFT | 45,
	['c'] = 46,	['C'] = SHIFT | 46,
	['v'] = 47,	['V'] = SHIFT | 47,
	['b'] = 48,	['B'] = SHIFT | 48,
	['n'] = 49,	['N'] = SHIFT | 49,
	['m'] = 50,	['M'] = SHIFT | 50,
	[','] = 51,	['<'] = SHIFT | 51,
	['.'] = 52,	['>'] = SHIFT | 52,
	['/'] = 53,	['?'] = SHIFT | 53,
	// RShift = 54,
	// kpmu = 55,
	// LAlt = 56,
	[' '] = 57,
	// caps = 58,
	// F1 = 59
	// F10 = 68
	// F11 = 87
	// F12 = 88
};

static void do_type(struct zwp_virtual_keyboard_v1 *keyboard, uint32_t c) {
	uint32_t map = charmap[c & 0xFF];
	int key = map & 0xFF;
	if (map & SHIFT) {
		zwp_virtual_keyboard_v1_modifiers(keyboard, mod_shift.press, mod_shift.latch, mod_shift.lock, mod_shift.group);
		zwp_virtual_keyboard_v1_key(keyboard, ts, 42 /* lshift */, WL_KEYBOARD_KEY_STATE_PRESSED);
		ts += 10;
	}
	if (key) {
		zwp_virtual_keyboard_v1_key(keyboard, ts, key, WL_KEYBOARD_KEY_STATE_PRESSED);
		ts += 10;
		zwp_virtual_keyboard_v1_key(keyboard, ts, key, WL_KEYBOARD_KEY_STATE_RELEASED);
		ts += 10;
	}
	if (map & SHIFT) {
		zwp_virtual_keyboard_v1_modifiers(keyboard, mod_none.press, mod_none.latch, mod_none.lock, mod_none.group);
		zwp_virtual_keyboard_v1_key(keyboard, ts, 42 /* lshift */, WL_KEYBOARD_KEY_STATE_RELEASED);
		ts += 10;
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: ./virtual-keyboard <subcommand>\n");
		return EXIT_FAILURE;
	}
	struct wl_display * display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return EXIT_FAILURE;
	}

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (keyboard_manager == NULL) {
		fprintf(stderr, "compositor does not support wp-virtual-keyboard-unstable-v1\n");
		return EXIT_FAILURE;
	}

	struct zwp_virtual_keyboard_v1 *keyboard =
		zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
			keyboard_manager, seat);

	setup_keymap(keyboard);

	const char *cmd = argv[1];
	if (strcmp(cmd, "type") == 0) {
		if (argc < 3) {
			fprintf(stderr, "Usage: ./virtual-kbd type <text>\n");
			return EXIT_FAILURE;
		}
		const char *text = argv[2];
		while (*text) {
			do_type(keyboard, (uint8_t)*text);
			text++;
		}
	} else if (strcmp(cmd, "pipe") == 0) {
		uint8_t buf[100];
		while (1) {
			int rv = read(0, buf, 100);
			if (rv <= 0)
				break;
			for(int i=0; i < rv; i++)
				do_type(keyboard, buf[i]);
		}
	} else {
		// TODO support more advanced xdotool-style commands:
		//  - distinct key names (F*, insert/del, ...)
		//  - sending modifiers with keys (other than shift)
		//  - sending key-down and key-up independently
		// Other features:
		//  - Non-QWERTY keyboard layouts
		fprintf(stderr, "Invalid subcommand\n");
		return EXIT_FAILURE;
	}

	zwp_virtual_keyboard_v1_destroy(keyboard);
	wl_display_roundtrip(display);

	return EXIT_SUCCESS;
}
