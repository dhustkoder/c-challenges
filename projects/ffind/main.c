#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>


static inline bool searchdir(const char* const dirname, const char* const filename)
{
	DIR* const dirp = opendir(dirname);
	if (dirp == NULL) {
		fprintf(stderr, "Couldn't open directory \"%s\": %s\n", dirname, strerror(errno));
		return false;
	}

	const struct dirent* ent;
	bool ret = false;

	while ((ent = readdir(dirp)) != NULL) {
		if (strcmp(filename, ent->d_name) == 0) {
			ret = true;
			break;
		}
	}

	if (!ret) {
		rewinddir(dirp);
		int len = 0;
		char* fullpath = NULL;
		while ((ent = readdir(dirp)) != NULL) {
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;

			const int newlen = strlen(dirname) + strlen(ent->d_name);
			if (newlen > len) {
				free(fullpath);
				fullpath = malloc(newlen + 2);
				len = newlen;
			}

			sprintf(fullpath, "%s/%s", dirname, ent->d_name);
			const bool found = searchdir(fullpath, filename);

			if (found) {
				ret = true;
				break;
			}
		}
		free(fullpath);
	}


	closedir(dirp);
	return ret;
}


static inline int stfind(const char* const root, const char* const file)
{
	if (!searchdir(root, file))
		return EXIT_FAILURE;

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

	if (r1 == EXIT_FAILURE) {
		puts("NOT FOUND");
		return EXIT_FAILURE;
	}

	puts("FOUND");
	return EXIT_SUCCESS;
}

