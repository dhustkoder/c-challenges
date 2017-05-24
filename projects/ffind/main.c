#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <pthread.h>


static inline int compare(const FTSENT** a, const FTSENT** b)
{
	return strcmp((*a)->fts_name, (*b)->fts_name);
}


static inline int stfind(char* const rootdir, const char* const target)
{
	char* path[] = { rootdir, NULL };

	errno = 0;
	FTS* const ftsp = fts_open(path, FTS_NOSTAT|FTS_PHYSICAL, &compare);

	if (errno != 0) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n", rootdir, strerror(errno));
		fts_close(ftsp);
		return EXIT_FAILURE;
	}

	const FTSENT* parent;

	while ((parent = fts_read(ftsp)) != NULL) {
		if (parent->fts_info != FTS_D)
			continue;

		const FTSENT* child = fts_children(ftsp, 0);
		for ( ; child != NULL; child = child->fts_link)
			if (strcmp(child->fts_name, target) == 0)
				printf("%s/%s\n", parent->fts_path, child->fts_name);
	}

	fts_close(ftsp);
	return EXIT_SUCCESS;
}



static inline void th_parse(const char* const target, const FTSENT* child, const int begin, const int end)
{
	for (int i = 0; i < begin; ++i)
		child = child->fts_link;

	for (int i = 0; i < end; ++i) {
		if (strcmp(target, child->fts_name) == 0)
			printf("FOUND: %s%s\n", child->fts_parent->fts_path, child->fts_name);
		child = child->fts_link;
	}
}


static inline int mtfind(char* const rootdir, const char* const target)
{
	char* path[] = { rootdir, NULL };

	errno = 0;
	FTS* const ftsp = fts_open(path, FTS_NOCHDIR|FTS_PHYSICAL, &compare);

	if (errno != 0) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n", rootdir, strerror(errno));
		fts_close(ftsp);
		return EXIT_FAILURE;
	}

	const FTSENT* parent;

	while ((parent = fts_read(ftsp)) != NULL) {
		if (parent->fts_info != FTS_D)
			continue;

		printf("SCANNING %s\n LINKS %lu\n", parent->fts_path, parent->fts_statp->st_nlink);
		const FTSENT* const child = fts_children(ftsp, FTS_NAMEONLY);
		int i = 0;
		for (const FTSENT* p = child; p != NULL; p = p->fts_link)
		       ++i;	
		th_parse(target, child, 0, i);
	}

	fts_close(ftsp);
	return EXIT_SUCCESS;
}


int main(const int argc, char* const* argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [directory] [file]\n", argv[0]);
		return EXIT_FAILURE;
	}

	return mtfind(argv[1], argv[2]);
}

