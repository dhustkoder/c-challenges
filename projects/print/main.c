#include <stdio.h>
#include <stdlib.h>


int main(const int argc, const char* const * const argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage example: %s 3 \"Hello World\"\n", argv[0]);
		return EXIT_FAILURE;
	}

	const int times = (int) strtoll(argv[1], NULL, 0);

	for (int i = 0; i < times; ++i)
		printf("%s\n", argv[2]);

	return EXIT_SUCCESS;
}

