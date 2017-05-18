#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>


struct File {
	struct dirent ent;
	struct stat stat;
	struct File* next;
};


struct FileList {
	struct File* head;
	int size;
};


struct Paddings {
	int links;
	int usr;
	int ugrp;
	int size;
	int name;
};


static const unsigned char kOptAll  = 0x04;
static const unsigned char kOptLong = 0x02;
static const unsigned char kOptDir  = 0x01;

static const char* const short_opts = "lad";
static const struct option long_opts[] = {
	{"long", no_argument, NULL, 'l'},
	{"all", no_argument, NULL, 'a'},
	{"directory", no_argument, NULL, 'd'},
	{NULL, 0, NULL, 0}
};

#define kBufferSize ((2048))
static char buffer[kBufferSize + 1];
static int buff_idx = 0;


static inline void flushbuffer(void)
{
	buffer[buff_idx] = '\0';
	printf("%s", buffer);
	buff_idx = 0;
}


static inline void catbuffer(const char* const fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	int writen = vsnprintf(&buffer[buff_idx], kBufferSize - buff_idx, fmt, argptr);
	buff_idx += writen;

	if (buff_idx >= kBufferSize) {
		buff_idx = kBufferSize;
		flushbuffer();
		writen = vsnprintf(&buffer[buff_idx], kBufferSize, &fmt[writen], argptr);
		buff_idx += writen;
	}

	va_end(argptr);
}


static inline unsigned char get_opts(const int argc, char* const* argv)
{
	unsigned char r = 0;
	int c;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
			default:
			case -1: /*fall*/
			case 0: break;
			case 'a': r |= kOptAll; break;
			case 'l': r |= kOptLong; break;
			case 'd': r |= kOptDir; break;
		}
	}
	return r;
}


static inline struct FileList* mkfilelist(DIR* const dir)
{
	struct FileList* const fl = calloc(1, sizeof(struct FileList));
	const struct dirent* ent;
	struct File** p = &fl->head;

	while ((ent = readdir(dir)) != NULL) {
		(*p) = calloc(1, sizeof(struct File));
		memcpy(&(*p)->ent, ent, sizeof(struct dirent));
		stat(ent->d_name, &(*p)->stat);
		p = &(*p)->next;
		++fl->size;
	}

	return fl;
}


static inline void rmfilelist(struct FileList* const fl)
{
	struct File* p = fl->head;
	struct File* rm;

	while (p != NULL) {
		rm = p;
		p = p->next;
		free(rm);
	}

	free(fl);
}


static inline int intlen(int n)
{
	int len = 1;
	while (n != 0) {
		++len;
		n /= 10;
	}
	return len;
}


static inline struct Paddings getpaddings(const struct File* p)
{
	struct Paddings pad = { 0, 0, 0, 0, 0 };

	for (; p != NULL; p = p->next) {
		int aux = strlen(p->ent.d_name);
		if (aux > pad.name)
			pad.name = aux;

		aux = intlen(p->stat.st_nlink);
		if (aux > pad.links)
			pad.links = aux;

		aux = intlen(p->stat.st_size);
		if (aux > pad.size)
			pad.size = aux;

		aux = strlen(getpwuid(p->stat.st_uid)->pw_name);
		if (aux > pad.usr)
			pad.usr = aux;

		aux = strlen(getgrgid(p->stat.st_gid)->gr_name);
		if (aux > pad.ugrp)
			pad.ugrp = aux;
	}

	return pad;
}


static inline int lsshort(DIR* const dir, const bool all)
{
	const struct dirent* ent;
	int pad = 0;
	while ((ent = readdir(dir)) != NULL) {
		int len = strlen(ent->d_name);
		if (len > pad)
			pad = len;
	}

	rewinddir(dir);

	while ((ent = readdir(dir)) != NULL) {
		if (!all && ent->d_name[0] == '.')
			continue;
		catbuffer("%-*s ", pad, ent->d_name);
	}

	catbuffer("\n");
	return EXIT_SUCCESS;
}


static inline int lslong(DIR* const dir, const bool all)
{
	// permissions - links - user - user group - size - last modified date - name
	struct FileList* const fl = mkfilelist(dir);
	const struct Paddings pad = getpaddings(fl->head);
	char perm[11];
	char date[15];
	perm[10] = '\0';

	for (struct File* p = fl->head; p != NULL; p = p->next) {
		if (!all && p->ent.d_name[0] == '.')
			continue;
		perm[0] = S_ISDIR(p->stat.st_mode) ? 'd' : '-';
		perm[1] = (p->stat.st_mode&S_IRUSR) ? 'r' : '-';
		perm[2] = (p->stat.st_mode&S_IWUSR) ? 'w' : '-';
		perm[3] = (p->stat.st_mode&S_IXUSR) ? 'x' : '-';
		perm[4] = (p->stat.st_mode&S_IRGRP) ? 'r' : '-';
		perm[5] = (p->stat.st_mode&S_IWGRP) ? 'w' : '-';
		perm[6] = (p->stat.st_mode&S_IXGRP) ? 'x' : '-';
		perm[7] = (p->stat.st_mode&S_IROTH) ? 'r' : '-';
		perm[8] = (p->stat.st_mode&S_IWOTH) ? 'w' : '-';
		perm[9] = (p->stat.st_mode&S_IXOTH) ? 'x' : '-';
		strftime(date, 20, "%B %d %H:%M", localtime(&p->stat.st_ctime));
		catbuffer("%s %*d %*s %*s %*d %s %-*s\n",
		          perm, pad.links, p->stat.st_nlink,
			  pad.usr, getpwuid(p->stat.st_uid)->pw_name,
			  pad.ugrp, getgrgid(p->stat.st_gid)->gr_name,
			  pad.size, p->stat.st_size,
			  date, pad.name, p->ent.d_name);
	}

	rmfilelist(fl);
	return EXIT_SUCCESS;
}


static inline int ls(const char* const dirname, const unsigned char opts)
{
	DIR* const dir = opendir(dirname);
	if (dir == NULL) {
		fprintf(stderr, "Couldn't open directory: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	int ret;
	if (opts&kOptDir) {
		catbuffer(dirname);
		catbuffer("\n");
		ret = EXIT_SUCCESS;	
	} else if (opts&kOptLong) {
		ret = lslong(dir, (opts&kOptAll) != 0);
	} else {
		ret = lsshort(dir, (opts&kOptAll) != 0);
	}
	
	flushbuffer();
	closedir(dir);
	return ret;
}


int main(const int argc, char* const* argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [directory] [options]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char* dirname = NULL;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			dirname = argv[i];
			break;
		}
	}

	if (dirname == NULL) {
		fprintf(stderr, "Missing directory path\n");
		return EXIT_FAILURE;
	}

	ls(dirname, get_opts(argc, argv));

	return EXIT_SUCCESS;
}

