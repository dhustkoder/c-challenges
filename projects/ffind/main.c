#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef unsigned short u_short;
#include <fts.h>
#include <pthread.h>
#include <utils/debug.h>


static inline int strcomp(const char* a, const char* b)
{
	for (; *a == *b; ++a, ++b)
		if (*a == '\0' || *b == '\0')
			break;
	return *a != *b ? 1 : 0;
}


static inline int compare(const FTSENT** const a, const FTSENT** const b)
{
	return strcomp((*a)->fts_name, (*b)->fts_name);
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
			if (strcomp(child->fts_name, target) == 0)
				printf("%s/%s\n", parent->fts_path, child->fts_name);
	}

	fts_close(ftsp);
	return EXIT_SUCCESS;
}



static inline void parsen(const char* const target, const FTSENT* child, const int begin, const int end)
{
	int i;
	for (i = 0; i < begin; ++i)
		child = child->fts_link;

	for (; i < end; ++i) {
		if (strcomp(target, child->fts_name) == 0)
			printf("FOUND: %s%s\n", child->fts_parent->fts_path, child->fts_name);
		child = child->fts_link;
	}
}


static inline void* th_parsen(void* p)
{
	const int b = *((int*) *((void**)p));
	const int e = *((int*) *(((void**)p) + 1));
	const char* const target = *(((void**)p) + 2);
	const FTSENT* const child = *(((void**)p) + 3);
	parsen(target, child, b, e);
	return NULL;
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
	pthread_t t1;
	int b, e;
	void* th_args[] = { &b, &e, (void*) target, NULL };

	while ((parent = fts_read(ftsp)) != NULL) {
		if (parent->fts_info != FTS_D)
			continue;

		dprintf("SCANNING %s\n LINKS %u\n", parent->fts_path, parent->fts_statp->st_nlink);
		const FTSENT* const child = fts_children(ftsp, FTS_NAMEONLY);
		
		int i = 0;
		for (const FTSENT* p = child; p != NULL; p = p->fts_link)
		       ++i;
		
		if (i > 150) {
			b = i / 2;
			e = i;
			th_args[3] = (void*) child;
			pthread_create(&t1, NULL, &th_parsen, th_args);
			parsen(target, child, 0, i / 2);
			pthread_join(t1, NULL);
		} else {
			parsen(target, child, 0, i);
		}
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

	return stfind(argv[1], argv[2]);
}

