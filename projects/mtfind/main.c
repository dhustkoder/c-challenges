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


int main(const int argc, const char* const* argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [directory] [file]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char* const dirname = argv[1];
	const char* const filename = argv[2];

	DIR* const dirp = opendir(dirname);
	if (dirp == NULL) {
		perror("Couldn't open directory");
		return EXIT_FAILURE;
	}

	if (searchdir(filename, dirp))
		printf("FOUND\n");
	else
		printf("NOT FOUND\n");

	closedir(dirp);
	return EXIT_SUCCESS;
}

