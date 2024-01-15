
struct buffer_display {
	struct wl_buffer *wl_buffer;
	struct buffer_display *next;
};

struct display {
	struct wl_display *wl_display;
	struct wl_compositor *wl_compositor;
	struct wl_registry *wl_registry;
	struct wl_shm *wl_shm;
	struct wl_shm_pool *wl_shm_pool;
	struct wl_surface *wl_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct xdg_wm_base *xdg_wm_base;
	struct wl_callback *cb;
	struct wl_seat *wl_seat;
	struct wl_keyboard *wl_keyboard;
	int wl_fd; // don't free it
	unsigned int width,height,stride;
	int iskeyboard;
	int isquit,isconfigured;
	struct {
		struct buffer_display *first_inuse;
		struct buffer_display *first_unused;
	} buffers;
	struct {
		int ispredrawn; // bytes have been copied in w/o commit
		int isready; // frame is ready
	} drawstatus;
	struct mmapshm mmapshm;
	unsigned int keycode;
};

void clear_display(struct display *display);
int init_display(struct display *display, char *title, unsigned int width, unsigned int height, char *hint);
void deinit_display(struct display *s);
int checkforkey_display(unsigned int *keycode_out, struct display *display);
int waitforkey_display(unsigned int *keycode_out, struct display *display, int seconds);
int pollforevent_display(struct display *display, unsigned int msecs);
int drawchar_display(unsigned int *width_out, struct display *display, struct font *font,
		unsigned int x, unsigned int y, unsigned char uch);
