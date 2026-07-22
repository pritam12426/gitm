#include "process.h"

ProcessResult git_exec(const char *cwd, char *const argv[])
{
	(void) cwd;
	(void) argv;
	ProcessResult r = { 0 };
	return r;
}
void process_result_free(ProcessResult *r)
{
	(void) r;
}
