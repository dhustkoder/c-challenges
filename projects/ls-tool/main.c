#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>


int main(const int argc, const char* const* argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
		return EXIT_FAILURE;
	}

	DIR* const dir = opendir(argv[1]);

	if (dir == NULL) {
		perror("Couldn't open directory");
		return EXIT_FAILURE;
	}

	const struct dirent* ent;
	while ((ent = readdir(dir)) != NULL)
		printf("NAME: %-13s| TYPE: %d\n", ent->d_name, ent->d_type); 

	closedir(dir);
	return EXIT_SUCCESS;
}

