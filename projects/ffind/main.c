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

	FTS* const ftsp = fts_open(path, FTS_NOSTAT, &compare);

	if (ftsp == NULL) {
		fprintf(stderr, "Couldn't open \"%s\": %s", rootdir, strerror(errno));
		return EXIT_FAILURE;
	}

	const FTSENT* parent;

	while ((parent = fts_read(ftsp)) != NULL) {
		for (const FTSENT* child = fts_children(ftsp, 0); child != NULL; child = child->fts_link)
			if (strcmp(child->fts_name, target) == 0)
				printf("%s/%s\n", parent->fts_name, child->fts_name);
	}

	fts_close(ftsp);
	return EXIT_SUCCESS;
}


static void* th_search(void* p)
{
	FTS* const ftsp = *((void**)p);	
	const char* const target = *(((void**)p) + 1);

	const FTSENT *parent, *child;
	
	while ((parent = fts_read(ftsp)) != NULL) {
		for (child = fts_children(ftsp, 0); child != NULL; child = child->fts_link) {
			if (strcmp(child->fts_name, target) == 0)
				printf("%s/%s\n", parent->fts_path, child->fts_name);
		}
	}

	return NULL;
}


static inline int mtfind(char* const rootdir, const char* const target)
{
	char* path[] = { rootdir, NULL };

	FTS* const ftsp = fts_open(path, 0, &compare);

	if (ftsp == NULL) {
		fprintf(stderr, "Couldn't open \"%s\": %s", rootdir, strerror(errno));
		return EXIT_FAILURE;
	}

	void* th_args[] = { ftsp, (void*) target };

	th_search(th_args);

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

