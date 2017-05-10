#ifndef __XPYBIND__
#define __XPYBIND__

#define XPB_CONTROL     (1)
#define XPB_ALT         (2)
#define XPB_META        (4)
#define XPB_SUPER       (8)
#define XPB_HYPER       (16)

typedef struct {
	Window root;
	Window window;
	Colormap def_cmap;
	int num;
	unsigned long fg_color;
	unsigned long bg_color;

	int focused;
	int created;

	int timeout_activated;
	double window_timeout;
} xpb_screen_t;

#endif 
