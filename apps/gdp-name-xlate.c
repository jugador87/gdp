/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-NAME-XLATE --- show GDP name in various forms
**
**		Given an external name, shows the internal name in base64
**		and as a hex number.
*/

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_dbg.h>

#include <getopt.h>
#include <sysexits.h>

void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbgspec] gdp_name\n"
			"    -D  set debugging flags\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int opt;
	int i;
	bool show_usage = false;
	gdp_name_t gdpiname;
	gdp_pname_t gdppname;

	while ((opt = getopt(argc, argv, "D:")) > 0)
	{
		switch (opt)
		{
			case 'D':
				ep_dbg_set(optarg);
				break;

			default:
				show_usage = true;
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc != 1)
		usage();

	// don't really need to initialize the GDP library for this
	gdp_parse_name(argv[0], gdpiname);
	gdp_printable_name(gdpiname, gdppname);
	fprintf(stdout,
			"external:  %s\n"
			"printable: %s\n"
			"hex:       ",
			argv[0], gdppname);
	for (i = 0; i < sizeof gdpiname; i++)
		fprintf(stdout, "%02x", gdpiname[i]);
	fprintf(stdout, "\n");
	exit(EX_OK);
}
