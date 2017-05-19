#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>


static inline bool searchdir(const char* const filename, DIR* const dirp)
{
	const struct dirent* ent;
	while ((ent = readdir(dirp)) != NULL) {
		if (strcmp(filename, ent->d_name) == 0)
			return true;
	}
	return false;
}


static inline int stfind(const char* const root, const char* const file)
{
	return EXIT_SUCCESS;
}


static inline int mtfind(const char* const root, const char* const file)
{
	return EXIT_SUCCESS;
}


int main(const int argc, const char* const* argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [directory] [file]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char* const root = argv[1];
	const char* const file = argv[2];
	const int r1 = stfind(root, file);
	const int r2 = mtfind(root, file);

	return (r1 == EXIT_SUCCESS && r2 == EXIT_SUCCESS)
	       ? EXIT_SUCCESS : EXIT_FAILURE;
}

