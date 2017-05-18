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
	const char* name;
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

#define BUFFERSIZE ((4096))
static char buffer[BUFFERSIZE + 1];
static int buff_idx = 0;


static inline void flushbuffer(void)
{
	write(STDOUT_FILENO, buffer, buff_idx);
	buff_idx = 0;
}


static inline void catbuffer(const char* const fmt, ...)
{
	if (buff_idx >= (int)(BUFFERSIZE * 0.80f))
		flushbuffer();

	va_list argptr;
	va_start(argptr, fmt);
	buff_idx += vsprintf(&buffer[buff_idx], fmt, argptr);
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


static inline struct FileList* mkfilelist(DIR* const dir, const char* const basepath)
{
	struct FileList* const fl = calloc(1, sizeof(struct FileList));
	const int baselen = strlen(basepath);
	struct File** p = &fl->head;
	const struct dirent* ent;
	
	while ((ent = readdir(dir)) != NULL) {
		(*p) = calloc(1, sizeof(struct File));
		(*p)->name = ent->d_name;

		char path[baselen + strlen(ent->d_name) + 2];
		sprintf(path, "%s/%s", basepath, ent->d_name);
		stat(path, &(*p)->stat);

		p = &(*p)->next;
		++fl->size;
	}

	return fl;
}


static inline void rmfilelist(const struct FileList* const fl)
{
	struct File* p = fl->head;
	while (p != NULL) {
		struct File* const rm = p;
		p = p->next;
		free(rm);
	}

	free((void*)fl);
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
		int aux = strlen(p->name);
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
	while ((ent = readdir(dir)) != NULL) {
		if (!all && ent->d_name[0] == '.')
			continue;
		catbuffer("%s\n", ent->d_name);
	}
	return EXIT_SUCCESS;
}


static inline int lslong(DIR* const dir, const char* const basepath, const bool all)
{
	// format: permissions - links - user - user group - size - last modified date - file name
	const struct FileList* const fl = mkfilelist(dir, basepath);
	const struct Paddings pad = getpaddings(fl->head);
	char perm[11];
	char date[21];
	perm[10] = '\0';

	for (struct File* p = fl->head; p != NULL; p = p->next) {
		if (!all && p->name[0] == '.')
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
		strftime(date, 20, "%b %d %H:%M", localtime(&p->stat.st_ctime));
		catbuffer("%s %*d %*s %*s %*ld %s %-*s\n",
		          perm, pad.links, p->stat.st_nlink,
			  pad.usr, getpwuid(p->stat.st_uid)->pw_name,
			  pad.ugrp, getgrgid(p->stat.st_gid)->gr_name,
			  pad.size, p->stat.st_size,
			  date, pad.name, p->name);
	}

	rmfilelist(fl);
	return EXIT_SUCCESS;
}


static inline int ls(const char* const dirname, const unsigned char opts)
{
	DIR* const dir = opendir(dirname);
	if (dir == NULL) {
		fprintf(stderr, "Couldn't open directory: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	int ret;
	if (opts&kOptDir) {
		catbuffer("%s\n", dirname);
		ret = EXIT_SUCCESS;	
	} else if (opts&kOptLong) {
		ret = lslong(dir, dirname, (opts&kOptAll) != 0);
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

	return ls(dirname, get_opts(argc, argv));
}

