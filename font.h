
struct char_font {
	unsigned int left,top;
	unsigned int width,height;
	unsigned int zrlelen;
	char *zrle;
};
struct font {
	unsigned int width,height;
	struct char_font *chars[256];
};
extern struct font mono_global;
