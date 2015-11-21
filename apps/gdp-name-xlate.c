/* vim: set ai sw=4 sts=4 ts=4 : */

/*
**  GDP-NAME-XLATE --- show GDP name in various forms
**
**		Given an external name, shows the internal name in base64
**		and as a hex number.
**
**	----- BEGIN LICENSE BLOCK -----
**	Applications for the Global Data Plane
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**	----- END LICENSE BLOCK -----
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
