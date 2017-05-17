#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>


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
static char buffer[kBufferSize];
static int buff_idx = 0;


static inline void flushbuffer(void)
{
	buffer[buff_idx] = '\0';
	printf("%s", buffer);
	buff_idx = 0;
}


static inline void catbuffer(const char* const str)
{
	const int len = strlen(str);
	if ((buff_idx + len) >= kBufferSize) {
		printf("%s", buffer);
		buff_idx = 0;
	}

	memcpy(&buffer[buff_idx], str, len);
	buff_idx += len;
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


int main(const int argc, char* const* argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char* dirname = NULL;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			dirname = argv[i];
			break;
		}
	}

	DIR* const dir = opendir(dirname);

	if (dir == NULL) {
		perror("Couldn't open directory");
		return EXIT_FAILURE;
	}

	const int dirnamelen = strlen(dirname);
	const unsigned char opts = get_opts(argc, argv);

	if (opts&kOptDir) {
		catbuffer(dirname);
		catbuffer("\n");
	} else {
		const struct dirent* ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.' && !(opts&kOptAll))
				continue;

			if (opts&kOptLong) {
				struct stat stats;
				const int dnamelen = strlen(ent->d_name);
				const int pathlen = dirnamelen + dnamelen;
				char path[dirnamelen + dnamelen + 2];
				memset(path, 0, pathlen);
				strcat(path, dirname);
				strcat(path, "/");
				strcat(path, ent->d_name);

				if (stat(path, &stats) == -1) {
					fprintf(stderr, "Error while reading %s: %s\n", path, strerror(errno));
					continue;
				}

				const int stmode = stats.st_mode;
				const struct passwd* const passwd = getpwuid(stats.st_uid);
				const struct group* const grp = getgrgid(stats.st_gid);
				char size[20];
				char links[20];
				char date[20];
				sprintf(size, "%ld", stats.st_size);
				sprintf(links, "%ld", stats.st_nlink);
				strftime(date, 20, "%d-%m-%y", localtime(&(stats.st_ctime)));

				catbuffer((S_ISDIR(stmode)) ? "d" : "-");
				catbuffer((stmode&S_IRUSR) ? "r" : "-");
				catbuffer((stmode&S_IWUSR) ? "w" : "-");
				catbuffer((stmode&S_IXUSR) ? "x" : "-");
				catbuffer((stmode&S_IRGRP) ? "r" : "-");
				catbuffer((stmode&S_IWGRP) ? "w" : "-");
				catbuffer((stmode&S_IXGRP) ? "x" : "-");
				catbuffer((stmode&S_IROTH) ? "r" : "-");
				catbuffer((stmode&S_IWOTH) ? "w" : "-");
				catbuffer((stmode&S_IXOTH) ? "x" : "-");
				catbuffer(" ");
				catbuffer(links);
				catbuffer(" ");
				catbuffer(passwd ? passwd->pw_name : " ");
				catbuffer(" ");
				catbuffer(grp ? grp->gr_name : " ");
				catbuffer(" ");
				catbuffer(size);
				catbuffer(" ");
				catbuffer(date);
				catbuffer(" ");
			}
			catbuffer(ent->d_name);
			catbuffer("\n");
		}
	}
	
	flushbuffer();
	closedir(dir);
	return EXIT_SUCCESS;
}

