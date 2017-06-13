#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "network.h"


extern int chat(const ConnectionInfo* cinfo);


int main(const int argc, const char* const * const argv)
{
	if (argc > 1) {
		const ConnectionInfo* cinfo = NULL;
		if (strcmp(argv[1], "client") == 0) {
			if ((cinfo = initialize_connection(CONMODE_CLIENT)) == NULL)
				return EXIT_FAILURE;
		} else if (strcmp(argv[1], "host") == 0) {
			if ((cinfo = initialize_connection(CONMODE_HOST)) == NULL)
				return EXIT_FAILURE;
		}

		if (cinfo != NULL) {
			const int ret = chat(cinfo);
			terminate_connection(cinfo);
			return ret;
		}
	}

	fprintf(stderr, "Usage: %s [type: host, client]\n", argv[0]);
	return EXIT_FAILURE;
}

