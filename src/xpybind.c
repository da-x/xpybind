/* XPyBind
 *
 * Copyright (C) 2017 Dan Aloni <alonid@gmail.com>
 *
 * With a bit of code from Ratpoison (http://ratpoison.sourceforge.net).
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <python2.7/Python.h>
#include <sys/poll.h>
#include <time.h>

#include "xpybind.h"

Display *g_dpy = NULL;

int g_num_screens = 0;
xpb_screen_t *g_screens = NULL;

int g_current_screen = 0;
int g_meta_mod_mask = 0;
int g_alt_mod_mask = 0;
int g_super_mod_mask = 0;
int g_hyper_mod_mask = 0;
int g_num_lock_mask = 0;
int g_scroll_lock_mask = 0;
int g_running = 1;

PyObject *g_pymodule = NULL;

void create_input_window(xpb_screen_t *screen)
{
	char *name = "xpybind";
	XTextProperty tp;
	XSetWindowAttributes swa;

	/* Creates an invisible input window */

	/* TODO: cleanup:
	    screen->window = XCreateWindow(g_dpy, screen->root, 0, 0, 1, 1, 0,
	    CopyFromParent,
	    InputOnly,
	    CopyFromParent,
	    0,
	    NULL);
	*/

	screen->window = XCreateSimpleWindow(g_dpy, screen->root, 1, 1, 1, 1, 1,
					     screen->fg_color, screen->bg_color);

	XSelectInput(g_dpy, screen->window, KeyPressMask | KeyReleaseMask | FocusChangeMask);

	XStringListToTextProperty(&name, 1, &tp);
	XSetWMName(g_dpy, screen->window, &tp);
	XSetWMIconName(g_dpy, screen->window, &tp);

	swa.override_redirect = 1;
	XChangeWindowAttributes(g_dpy, screen->window, CWOverrideRedirect, &swa);

	XSync(g_dpy, False);
	screen->created = 1;
}

void destroy_input_window(xpb_screen_t *screen)
{
	if (screen->window) {
		XDestroyWindow(g_dpy, screen->window);
		screen->window = 0;
	}

	XSync(g_dpy, False);
	screen->created = 0;
}

void init_screen(xpb_screen_t *screen, int screen_num)
{
	XColor color, junk;

	XSync(g_dpy, False);

	screen->num = screen_num;
	screen->root = RootWindow(g_dpy, screen_num);
	screen->window = 0;
	screen->def_cmap = DefaultColormap(g_dpy, screen_num);
	screen->bg_color = BlackPixel(g_dpy, screen_num);
	screen->fg_color = WhitePixel(g_dpy, screen_num);
	XAllocNamedColor(g_dpy, screen->def_cmap, "green", &color, &junk);
	screen->fg_color = color.pixel;
	screen->window_timeout = 0;
	screen->timeout_activated = 0;
}

/*
 * Convert an X11 modifier mask to a caninical modifier mask equivalent, as
 * best it can (the X server may not have a hyper key defined, for
 * instance).
 */
unsigned int convert_x11mask_to_canonical_mask(unsigned int mask)
{
	unsigned int result = 0;

	result |= mask & ControlMask ? XPB_CONTROL:0;
	result |= mask & g_meta_mod_mask ? XPB_META:0;
	result |= mask & g_alt_mod_mask ? XPB_ALT:0;
	result |= mask & g_hyper_mod_mask ? XPB_HYPER:0;
	result |= mask & g_super_mod_mask ? XPB_SUPER:0;

	return result;
}

/*
 * Convert an rp modifier mask to a caninical modifier mask equivalent, as
 * best it can (the X server may not have a hyper key defined, for
 * instance).
 */
unsigned int convert_canoncial_mask_to_x11mask (unsigned int mask)
{
	unsigned int result = 0;

	result |= mask & XPB_CONTROL ? ControlMask:0;
	result |= mask & XPB_META ? g_meta_mod_mask:0;
	result |= mask & XPB_ALT ? g_alt_mod_mask:0;
	result |= mask & XPB_HYPER ? g_hyper_mod_mask:0;
	result |= mask & XPB_SUPER ? g_super_mod_mask:0;

	return result;
}


/* Figure out what keysyms are attached to what modifiers */
void update_modifier_map(void)
{
	unsigned int modmasks[] =
		{ Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
	int row, col;	/* The row and column in the modifier table.  */
	XModifierKeymap *mods;

	mods = XGetModifierMapping (g_dpy);

	for (row=3; row < 8; row++)
		for (col=0; col < mods->max_keypermod; col++) {
			KeyCode code = mods->modifiermap[(row * mods->max_keypermod) + col];
			KeySym sym;

			if (code == 0) continue;

			sym = XkbKeycodeToKeysym(g_dpy, code, 0, 0);

			switch (sym) {
			case XK_Meta_L:
			case XK_Meta_R:
				g_meta_mod_mask |= modmasks[row - 3];
				break;

			case XK_Alt_L:
			case XK_Alt_R:
				g_alt_mod_mask |= modmasks[row - 3];
				break;

			case XK_Super_L:
			case XK_Super_R:
				g_super_mod_mask |= modmasks[row - 3];
				break;

			case XK_Hyper_L:
			case XK_Hyper_R:
				g_hyper_mod_mask |= modmasks[row - 3];
				break;

			case XK_Num_Lock:
				g_num_lock_mask |= modmasks[row - 3];
				break;

			case XK_Scroll_Lock:
				g_scroll_lock_mask |= modmasks[row - 3];
				break;
			default:
				break;
			}
		}

	/* 'Stolen' from Emacs 21.0.90 - xterm.c */
	/* If we couldn't find any meta keys, accept any alt keys as meta keys.  */
	if (!g_meta_mod_mask) {
		g_meta_mod_mask = g_alt_mod_mask;
		g_alt_mod_mask = 0;
	}

	/* If some keys are both alt and meta,
	   make them just meta, not alt.  */
	if (g_alt_mod_mask & g_meta_mod_mask) {
		g_alt_mod_mask &= ~g_meta_mod_mask;
	}

	XFreeModifiermap (mods);
}

/* Grab the key while ignoring annoying modifier keys including
   caps lock, num lock, and scroll lock. */
void grab_key(int keycode, unsigned int modifiers, Window grab_window, int grab)
{
	unsigned int mod_list[8];
	int i;

	/* Convert to a modifier mask that X Windows will understand. */
	modifiers = convert_canoncial_mask_to_x11mask (modifiers);

	/* Create a list of all possible combinations of ignored
	   modifiers. Assumes there are only 3 ignored modifiers. */
	mod_list[0] = 0;
	mod_list[1] = LockMask;
	mod_list[2] = g_num_lock_mask;
	mod_list[3] = mod_list[1] | mod_list[2];
	mod_list[4] = g_scroll_lock_mask;
	mod_list[5] = mod_list[1] | mod_list[4];
	mod_list[6] = mod_list[2] | mod_list[4];
	mod_list[7] = mod_list[1] | mod_list[2] | mod_list[4];

	/* Grab every combination of ignored modifiers. */
	for (i=0; i<8; i++){
		if (grab)
			XGrabKey(g_dpy, keycode, modifiers | mod_list[i],
				 grab_window, True, GrabModeAsync, GrabModeAsync);
		else
			XUngrabKey(g_dpy, keycode, modifiers | mod_list[i],  grab_window);
	}
}

int cook_keycode(XKeyEvent *ev, KeySym *keysym, unsigned int *mod, char *keysym_name, int len, int ignore_bad_mods)
{
	int nbytes;

	if (ignore_bad_mods)
		ev->state &= ~(LockMask | g_num_lock_mask  | g_scroll_lock_mask);

	nbytes = XLookupString (ev, keysym_name, len, keysym, NULL);

	*mod = ev->state;
	*mod &= (g_meta_mod_mask | g_alt_mod_mask | g_hyper_mod_mask  | g_super_mod_mask  | ControlMask );

	return nbytes;
}

double gettimeofday_float(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return ((double)tv.tv_sec) + ((double)tv.tv_usec)/1000000;
}

int next_event_timeout(XEvent *event_return, int timeout)
{
	struct pollfd pfd;
	int ret;
	int i;

	for (i=0; i < g_num_screens; i++) {
		xpb_screen_t *screen;

		screen = &g_screens[i];
		if (screen->created) {
			if (screen->timeout_activated) {
				if (screen->window_timeout <= gettimeofday_float()) {
					destroy_input_window(screen);
				}
			}
		}
	}

	if (XPending(g_dpy)) {
		XNextEvent(g_dpy, event_return);
		return 1;
	}

	pfd.fd = g_dpy->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ret = poll(&pfd, 1, timeout);
	if (ret <= 0) {
		if (ret < 0) {
			printf("error from X file descriptor: %d\n", errno);
		}
		return ret;
	}

	if (XPending(g_dpy)) {
		XNextEvent(g_dpy, event_return);
		return 1;
	}

	return 0;
}

int read_key(KeySym *keysym, unsigned int *modifiers, char *keysym_name, int len)
{
	XEvent ev;
	int ret;

	/* Read a key from the keyboard. */
	do {
		/* TODO: cleanup: XMaskEvent(g_dpy, KeyPressMask | FocusChangeMask, &ev); */
		ret = next_event_timeout(&ev, 100);
		if (ret > 0) {
			if (ev.type == KeyPress) {
				*modifiers = ev.xkey.state;
				cook_keycode(&ev.xkey, keysym, modifiers, keysym_name, len, 0);
				if (!IsModifierKey (*keysym))
					break;
			} else if (ev.type == FocusOut) {
				return -1;
			}
		}
	} while (g_running);

	return 0;
}

void event_key(KeySym sym, int modifiers)
{
	PyObject *pDict, *pFunc, *pArgs, *pValue;
	char *name = "_event_key";

	pDict = PyModule_GetDict(g_pymodule);
	/* pDict is a borrowed reference */

	pFunc = PyDict_GetItemString(pDict, name);
	/* pFun is a borrowed reference */

	if (pFunc && PyCallable_Check(pFunc)) {
		pArgs = Py_BuildValue("(ll)", sym, modifiers);
		pValue = PyObject_CallObject(pFunc, pArgs);
		if (pValue) {
			Py_DECREF(pValue);
		}
		Py_DECREF(pArgs);
	} else {
		fprintf(stderr, "Cannot find function \"%s\"\n", name);
	}

	if (PyErr_Occurred()) {
		PyErr_Print();
	}
}

#define FONT_HEIGHT(f) ((f)->max_bounds.ascent + (f)->max_bounds.descent)
#define MAX_FONT_WIDTH(f) ((f)->max_bounds.width)
#define bar_x(x, y) 20
#define bar_y(x, y) 20 + height

void update_input_window(xpb_screen_t *s, const char *prompt,
			 const char *buffer, int length,
			 int window_timeout)
{
	int prompt_width;
	int input_width;
	int total_width;
	GC lgc;
	GC normal_gc;
	XGCValues gv;
	int height;
	struct {
		XFontStruct *font;
		int bar_x_padding;
		int bar_y_padding;
		int input_window_size;
	} defaults;

	// Load an old font from xorg-x11-fonts-misc like it's 2003
	defaults.font = XLoadQueryFont(g_dpy, "9x15bold");
	assert(defaults.font != NULL);

	defaults.bar_x_padding = 0;
	defaults.bar_y_padding = 0;
	defaults.input_window_size = 20;

	gv.foreground = s->fg_color;
	gv.background = s->bg_color;
	gv.function = GXcopy;
	gv.line_width = 1;
	gv.subwindow_mode = IncludeInferiors;
	gv.font = defaults.font->fid;

	normal_gc = XCreateGC(g_dpy, s->root,
			      GCForeground | GCBackground | GCFunction
			      | GCLineWidth | GCSubwindowMode | GCFont,
			      &gv);

	prompt_width = XTextWidth(defaults.font, prompt, strlen(prompt));
	input_width  = XTextWidth(defaults.font, buffer, length);

	total_width = defaults.bar_x_padding * 2 + prompt_width + input_width + MAX_FONT_WIDTH(defaults.font);
	height = (FONT_HEIGHT(defaults.font) + defaults.bar_y_padding * 2);

	if (total_width < defaults.input_window_size + prompt_width) {
		total_width = defaults.input_window_size + prompt_width;
	}

	XMoveResizeWindow(g_dpy, s->window,
			  bar_x(s, total_width), bar_y(s, height), total_width,
			  (FONT_HEIGHT(defaults.font) + defaults.bar_y_padding * 2));

	XClearWindow(g_dpy, s->window);
	XSync(g_dpy, False);

	XDrawString(g_dpy, s->window, normal_gc,
		    defaults.bar_x_padding,
		    defaults.bar_y_padding + defaults.font->max_bounds.ascent,
		    prompt,
		    strlen(prompt));

	XDrawString(g_dpy, s->window, normal_gc,
		    defaults.bar_x_padding + prompt_width,
		    defaults.bar_y_padding + defaults.font->max_bounds.ascent,
		    buffer,
		    length);

	gv.function = GXxor;
	gv.foreground = s->fg_color ^ s->bg_color;
	lgc = XCreateGC(g_dpy, s->window, GCFunction | GCForeground, &gv);

	/* Draw a cheap-o cursor - MkII */

	/*
	XFillRectangle(g_dpy, s->window, lgc,
	defaults.bar_x_padding + prompt_width + XTextWidth (defaults.font, buffer, line->position),
	defaults.bar_y_padding,
	XTextWidth (defaults.font, &buffer[line->position], 1),
	FONT_HEIGHT (defaults.font));
	*/

	XFlush(g_dpy);
	XFreeGC(g_dpy, lgc);
	XFreeGC(g_dpy, normal_gc);
	XFreeFont(g_dpy, defaults.font);

	s->timeout_activated = 0;
	if (window_timeout) {
		s->window_timeout = gettimeofday_float() + window_timeout/1000.0;
		s->timeout_activated = 1;
	}
}

void focus_input_window(Window *fwin)
{
	xpb_screen_t *s = &g_screens[g_current_screen];
	int revert = 0;

	XGetInputFocus(g_dpy, fwin, &revert);

	if (s->created)
		destroy_input_window(s);

	s->focused = 1;
}

void create_and_map_input_window(void)
{
	xpb_screen_t *s = &g_screens[g_current_screen];
	create_input_window(s);

	XMapWindow(g_dpy, s->window);
	XRaiseWindow(g_dpy, s->window);
	XClearWindow(g_dpy, s->window);
	XSync(g_dpy, False);

	XSetInputFocus(g_dpy, s->window, RevertToPointerRoot, CurrentTime);
}

void unfocus_input_window(Window fwin)
{
	xpb_screen_t *s = &g_screens[g_current_screen];

	if (fwin != 0)
		XSetInputFocus(g_dpy, fwin, RevertToPointerRoot, CurrentTime);
	s->focused = 0;

	XSync(g_dpy, False);
}

void destroy_and_unmap_input_window(void)
{
	xpb_screen_t *s = &g_screens[g_current_screen];
	destroy_input_window(s);
}

void event_loop()
{
	XEvent event;
	int ret;

	while (g_running) {
		/* Get the current event from the system queue. */
		ret = next_event_timeout(&event, 100);
		if (ret <= 0)
			continue;

		/* Process the incoming event. */
		switch (event.type) {
		case KeyPress: {
			XKeyEvent *kevent = (XKeyEvent *) & event;
			char keysym_name[0x100];
			KeySym ksym;
			int modifiers;

			XLookupString(kevent, keysym_name, sizeof(keysym_name), &ksym, NULL);

			modifiers = kevent->state;

			modifiers &= ~(LockMask
				       | g_num_lock_mask
				       | g_scroll_lock_mask);

			if (!IsModifierKey(ksym))
				event_key(ksym, convert_x11mask_to_canonical_mask(modifiers));
			break;
		}
		}
	}
}

static PyObject *emb_grab_key(PyObject *self, PyObject *args)
{
	int keysym, modifiers;
	xpb_screen_t *s;

	if (!PyArg_ParseTuple(args,"ii:grab_key", &keysym, &modifiers))
		return NULL;

	s = &g_screens[g_current_screen];

	grab_key(XKeysymToKeycode(g_dpy, keysym), modifiers, s->root, 1);

	return Py_BuildValue("l", 0);
}

static PyObject *emb_ungrab_key(PyObject *self, PyObject *args)
{
	int keysym, modifiers;
	xpb_screen_t *s;

	if (!PyArg_ParseTuple(args, "ii:ungrab_key", &keysym, &modifiers))
		return NULL;

	s = &g_screens[g_current_screen];

	grab_key(XKeysymToKeycode(g_dpy, keysym), modifiers, s->root, 0);

	return Py_BuildValue("l", 0);
}

static PyObject *emb_keysym_to_string(PyObject *self, PyObject *args)
{
	int keysym;
	char *tmp;

	if (!PyArg_ParseTuple(args,"i:keysym_to_string", &keysym))
		return NULL;

	tmp = XKeysymToString(keysym);

	return Py_BuildValue("s", tmp);
}

static PyObject *emb_string_to_keysym(PyObject *self, PyObject *args)
{
	char *buf;
	KeySym tmp;

	if (!PyArg_ParseTuple(args, "s:string_to_keysym", &buf))
		return NULL;

	tmp = XStringToKeysym(buf);

	return Py_BuildValue("l", tmp);
}

static PyObject *emb_get_xpb_modifiers(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":keysym_to_string"))
		return NULL;

	return Py_BuildValue("((lllll), (sssss))",
			     XPB_CONTROL, XPB_ALT, XPB_META, XPB_SUPER, XPB_HYPER,
			     "C", "A", "M", "S", "H"
			     );
}

static PyObject *emb_read_key(PyObject *self, PyObject *args)
{
	KeySym keysym;
	unsigned int modifiers;
	char klen[0x100];
	int nret;

	if (!PyArg_ParseTuple(args,":read_key"))
		return NULL;

	nret = read_key(&keysym, &modifiers, klen, sizeof(klen));
	if (nret != 0)
		return Py_BuildValue("s", "focus lost");

	return Py_BuildValue("(ll)", keysym, convert_x11mask_to_canonical_mask(modifiers));
}

static PyObject *emb_focus_input_window(PyObject *self, PyObject *args)
{
	Window fwin = 0;

	if (!PyArg_ParseTuple(args,":focus_input_window"))
		return NULL;

	focus_input_window(&fwin);

	return Py_BuildValue("(l)", fwin);
}

static PyObject *emb_unfocus_input_window(PyObject *self, PyObject *args)
{
	Window fwin = 0;

	if (!PyArg_ParseTuple(args,"l:unfocus_input_window", &fwin))
		return NULL;

	unfocus_input_window(fwin);

	return Py_BuildValue("l", 0);
}

static PyObject *emb_create_input_window(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":create_input_window"))
		return NULL;

	create_and_map_input_window();

	return Py_BuildValue("l", 0);
}

static PyObject *emb_destory_input_window(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":destroy_input_window"))
		return NULL;

	destroy_and_unmap_input_window();

	return Py_BuildValue("l", 0);
}


static PyObject *emb_exit(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args,":exit"))
		return NULL;

	g_running = 0;

	return Py_BuildValue("l", 0);
}

static PyObject *emb_update_input_window(PyObject *self, PyObject *args)
{
	xpb_screen_t *s = &g_screens[g_current_screen];
	const char *str;
	int size;
	int timeout = 0;

	if (!PyArg_ParseTuple(args,"s#i:update_input_window", &str, &size, &timeout))
		return NULL;

	update_input_window(s, "xpybind:", str, size, timeout);

	return Py_BuildValue("l", 0);
}

static PyMethodDef g_python_methods_to_script[] = {
	{"keysym_to_string", emb_keysym_to_string, METH_VARARGS, NULL},
	{"string_to_keysym", emb_string_to_keysym, METH_VARARGS, NULL},
	{"get_xpb_modifiers", emb_get_xpb_modifiers, METH_VARARGS, NULL},
	{"focus_input_window", emb_focus_input_window, METH_VARARGS, NULL},
	{"unfocus_input_window", emb_unfocus_input_window, METH_VARARGS, NULL},
	{"create_input_window", emb_create_input_window, METH_VARARGS, NULL},
	{"destroy_input_window", emb_destory_input_window, METH_VARARGS, NULL},
	{"read_key", emb_read_key, METH_VARARGS, NULL},
	{"grab_key", emb_grab_key, METH_VARARGS, NULL},
	{"ungrab_key", emb_ungrab_key, METH_VARARGS, NULL},
	{"uodate_input_window", emb_update_input_window, METH_VARARGS, NULL},
	{"exit", emb_exit, METH_VARARGS, NULL},
	{NULL, },
};

void init_python(int argc, char *argv[])
{
	PyObject *pDict, *pFunc, *pArgs, *pValue;
	PyObject *pName;
	char *name = "_init";

	Py_Initialize();
	Py_InitModule("xpybind_c", g_python_methods_to_script);

	pName = PyString_FromString("xpybind");
	g_pymodule = PyImport_Import(pName);
	Py_DECREF(pName);

	if (g_pymodule == NULL) {
		PyErr_Print();
		return;
	}

	pDict = PyModule_GetDict(g_pymodule);
	/* pDict is a borrowed reference */

	pFunc = PyDict_GetItemString(pDict, name);
	/* pFunc is a borrowed reference */
	/* pFunc gets xpybind.init */

	if (pFunc && PyCallable_Check(pFunc)) {
		if (argc >= 2)
			pArgs = Py_BuildValue("(s)", argv[1]);
		else
			pArgs = Py_BuildValue("()");
		pValue = PyObject_CallObject(pFunc, pArgs);
		if (pValue) {
			Py_DECREF(pValue);
		}
		Py_DECREF(pArgs);
	} else {
		fprintf(stderr, "xpybind: error, cannot find function \"%s\" in xpybind module\n", name);
	}

	if (PyErr_Occurred()) {
		PyErr_Print();
	}
}

void fini_python()
{
	Py_Finalize();
}

int main(int argc, char *argv[])
{
	int i;

	if (!(g_dpy = XOpenDisplay (NULL))) {
		fprintf(stderr, "xpybind: can't open display\n");
		return 1;
	}

	g_num_screens = ScreenCount(g_dpy);
	g_screens = malloc(sizeof(xpb_screen_t) * g_num_screens);

	if (!g_screens) {
		fprintf(stderr, "xpybind: error allocating memory\n");
		return 1;
	}

	for (i=0; i < g_num_screens; i++)
		init_screen(&g_screens[i], i);

	update_modifier_map();

	init_python(argc, argv);
	event_loop();
	fini_python();

	free(g_screens);

	return 0;
}
