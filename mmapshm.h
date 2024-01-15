
struct mmapshm {
	int fd;
	unsigned char *ptr;
	unsigned int allocsize,requestsize;
};
void clear_mmapshm(struct mmapshm *m);
void deinit_mmapshm(struct mmapshm *m);
int init_mmapshm(struct mmapshm *m, unsigned int requestsize);
