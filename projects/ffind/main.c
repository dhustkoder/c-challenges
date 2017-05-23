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


/*
static inline void* th_stfind(void* const th_args)
{
	char* const rootdir = *((void**)th_args);
	const char* const target = *(((void**)th_args) + 1);

	volatile bool* sign = *(((void**)th_args) + 2);
	char buff[strlen(rootdir) + 1];
	strcpy(buff, rootdir);

	*sign = true;

	char* path[] = { buff, NULL };

	errno = 0;
	FTS* const ftsp = fts_open(path, FTS_NOSTAT, &compare);

	if (errno != 0) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n", rootdir, strerror(errno));
		goto funexit;
	}

	const FTSENT* parent;

	while ((parent = fts_read(ftsp)) != NULL) {
		if (parent->fts_info != FTS_D)
			continue;
		//printf("THREAD 2 SCANNING %s\n", parent->fts_path);
		const FTSENT* child = fts_children(ftsp, 0);
		for ( ; child != NULL; child = child->fts_link)
			if (strcmp(child->fts_name, target) == 0)
				printf("THREAD 2 FOUND: %s/%s\n", parent->fts_path, child->fts_name);
	}

funexit:
	fts_close(ftsp);
	*sign = false;
	return NULL;
}



static inline int mtfind(char* const rootdir, const char* const target)
{
	char* path[] = { rootdir, NULL };

	errno = 0;
	FTS* const ftsp = fts_open(path, FTS_NOCHDIR, &compare);

	if (errno != 0) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n", rootdir, strerror(errno));
		fts_close(ftsp);
		return EXIT_FAILURE;
	}


	pthread_t t1;
	const FTSENT *parent, *child;

	volatile bool sign1 = false;
	void* th_args[] = { NULL, (void*) target, (void*) &sign1 };
	bool first = true;
	char ppath[256];
	ppath[0] = '\0';

	while ((parent = fts_read(ftsp)) != NULL) {
		if (parent->fts_info != FTS_D || (ppath[0] != '\0' && strcmp(ppath, parent->fts_parent->fts_name) == 0)) {
			continue;
		} else if (sign1 == false && first == false) {
			//printf("THREAD 1 PASSING %s to new thread\n", parent->fts_path);
			th_args[0] = parent->fts_path;
			pthread_create(&t1, NULL, &th_stfind, th_args);
			strcpy(ppath, parent->fts_name);
			while (sign1 == false) {
				// wait sync ...
			}
			continue;
		}

		//printf("THREAD 1 SCANNING %s\n", parent->fts_path);
		for (child = fts_children(ftsp, 0); child != NULL; child = child->fts_link)
			if (strcmp(child->fts_name, target) == 0)
				printf("THREAD 1 FOUND: %s/%s\n", parent->fts_path, child->fts_name);

		first = false;
	}

	if (sign1 == true)
		pthread_join(t1, NULL);

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

