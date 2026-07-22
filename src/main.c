#include <stdio.h>

#include "command_line.h"
#include "log.h"

int main(int argc, char *argv[])
{
	if (!command_line_parse(argc, argv)) return 1;
	log_init(G_args.log_file, G_args.log_level);

	if (LOG_LEVEL_IS_ENABLED(LOG_LEVEL_DEBUG)) {
		LOG_CUSTOM(LOG_LEVEL_DEBUG, false, "Command-line args: [");
		for (int i = 0; i < argc; i++) {
			fprintf(log_get_file(), "\"%s\"", argv[i]);
			if (i != argc - 1) fputs(", ", log_get_file());
		}
		fputs("]\n", log_get_file());
	}

	return 0;
}
