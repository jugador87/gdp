#include "ep.h"

#include <inttypes.h>
#include <sysexits.h>

/*
**  Utility to convert integer EP_STAT codes to text
*/

int
main(int argc, char **argv)
{
	int32_t i;
	EP_STAT estat;
	char ebuf[100];

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s int\n"
			"\tint is the integer version of a status code\n",
			argv[0]);
		exit(EX_USAGE);
	}

	i = strtol(argv[1], NULL, 0);
	printf("EP_STAT as integer is %d = 0x%X\n", i, i);
	ep_lib_init(0);
	estat = EP_STAT_FROM_INT(i);
	printf("%s\n", ep_stat_tostr(estat, ebuf, sizeof ebuf));
	exit(EX_OK);
}
