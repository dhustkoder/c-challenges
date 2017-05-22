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
	FTS* const ftsp = fts_open(path, FTS_NOSTAT, &compare);

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

/*
static inline int mtfind(char* const rootdir, const char* const target)
{
	char* path[] = { rootdir, NULL };

	errno = 0;
	FTS* const ftsp = fts_open(path, FTS_NOSTAT, &compare);

	if (errno != 0) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n", rootdir, strerror(errno));
		fts_close(ftsp);
		return EXIT_FAILURE;
	}

	const FTSENT *parent, *child;
	
	while ((parent = fts_read(ftsp)) != NULL) {
		const unsigned short info = parent->fts_info;
		if (info != FTS_D)
			continue;
		printf("PARENT [%s] CHILDREN", parent->fts_path);
		for (child = fts_children(ftsp, 0); child != NULL; child = child->fts_link)
			printf(" [%s]", child->fts_name);
		putchar('\n');
	}

	fts_close(ftsp);
	return EXIT_SUCCESS;
}
*/

int main(const int argc, char* const* argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [directory] [file]\n", argv[0]);
		return EXIT_FAILURE;
	}

	return stfind(argv[1], argv[2]);
}

