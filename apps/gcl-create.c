/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <gdp/gdp.h>

#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>

#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sysexits.h>
#include <sys/stat.h>

/*
**  WRITER-TEST --- writes records to a GCL
**
**		This reads the records one line at a time from standard input
**		and assumes they are text, but there is no text requirement
**		implied by the GDP.
*/


void
usage(void)
{
	fprintf(stderr, "Usage: %s [-D dbgspec] [-G gdpd_addr]\n"
			"\t[-k] [-K keyfile] [logd_name]\n"
			"\t[<mdid>=<metadata>...] [gcl_name]\n"
			"  * if -K specifies a directory, a .pem file is written there\n"
			"    with the name of the GCL\n",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	gdp_name_t gcliname;			// internal name of GCL
	char *gclxname = NULL;			// external name of GCL
	gdp_name_t logdiname;			// internal name of log daemon
	char *logdxname;				// external name of log daemon
	gdp_gclmd_t *gmd = NULL;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	char buf[200];
	bool show_usage = false;
	bool make_new_key = false;
	char *keyfile = NULL;
	RSA *key = NULL;
	char *p;

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "D:G:kK:")) > 0)
	{
		switch (opt)
		{
		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'k':
			make_new_key = true;
			break;

		 case 'K':
			keyfile = optarg;
			break;

		 default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage || argc < 1)
		usage();

	argc--;
	logdxname = *argv++;

	// collect any metadata
	while (argc > 0 && (p = strchr(argv[0], '=')) != NULL)
	{
		gdp_gclmd_id_t mdid = 0;
		int i;

		if (gmd == NULL)
			gmd = gdp_gclmd_new();

		p++;
		for (i = 0; i < 4; i++)
		{
			if (argv[0][i] == '=')
				break;
			mdid = (mdid << 8) | (unsigned) argv[0][i];
		}

		gdp_gclmd_add(gmd, mdid, strlen(p), p);

		argc--;
		argv++;
	}

	// name is optional ; if omitted one will be created
	if (argc-- > 0)
		gclxname = *argv++;

	if (show_usage || argc > 0)
		usage();

	// see if we have an existing key
	bool keyfile_is_directory = false;
	if (keyfile == NULL)
	{
		// nope -- create a key in current directory if requested
		if (make_new_key)
		{
			keyfile_is_directory = true;
			keyfile = ".";
		}
	}
	else
	{
		// might be a directory
		struct stat st;
		FILE *key_fp;

		if (stat(keyfile, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
		{
			keyfile_is_directory = true;
		}
		else if ((key_fp = fopen(keyfile, "r")) == NULL)
		{
			// no existing key file; OK if we are creating it
			if (!make_new_key)
			{
				ep_app_error("Could not read private key file %s", keyfile);
				exit(EX_NOINPUT);
			}
		}
		else
		{
			key = PEM_read_RSAPrivateKey(key_fp, NULL, NULL, NULL);
			if (key == NULL)
			{
				ep_app_error("Could not read private key %s", keyfile);
				exit(EX_DATAERR);
			}
			else
			{
				make_new_key = false;
			}
			fclose(key_fp);
		}
	}

	// if creating new key, go ahead and do it
	if (make_new_key)
	{
		key = RSA_generate_key(2048, 3, NULL, NULL);
		if (key == NULL)
		{
			ep_app_error("Could not create new key");
			exit(EX_UNAVAILABLE);
		}
	}

	// at this point, key is initialized but not necessarily on disk
	if (key != NULL)
	{
		// add the public key to the metadata
		uint8_t der_buf[_GDP_MAX_DER_LEN];
		uint8_t *derp = der_buf;

		if (gmd == NULL)
			gmd = gdp_gclmd_new();
		if (i2d_RSAPublicKey(key, &derp) < 0)
		{
			ep_app_error("Could not create DER format public key");
			exit(EX_SOFTWARE);
		}

		// already too late to test for this
		if ((derp - der_buf) > sizeof der_buf)
			ep_app_abort("DANGER: i2d_RSAPublicKey overflowed buffer");

		gdp_gclmd_add(gmd, GDP_GCLMD_PUBKEY, derp - der_buf, der_buf);
	}

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	gdp_parse_name(logdxname, logdiname);

	if (gclxname == NULL)
	{
		// create a new GCL handle with a new name
		estat = gdp_gcl_create(NULL, logdiname, gmd);
	}
	else
	{
		// open or create a GCL with the provided name
		gdp_parse_name(gclxname, gcliname);

		// save the external name as metadata
		if (gmd == NULL)
			gmd = gdp_gclmd_new();
		gdp_gclmd_add(gmd, GDP_GCLMD_XID, strlen(gclxname), gclxname);
		estat = gdp_gcl_create(gcliname, logdiname, gmd);
	}
	EP_STAT_CHECK(estat, goto fail1);

	// now would be a good time to write the private key to disk
	if (make_new_key)
	{
		char *keyfilebuf = NULL;

		// it might be a directory, in which case we append the GCL name
		if (keyfile_is_directory)
		{
			size_t len;
			gdp_pname_t pbuf;

			gdp_printable_name(gcliname, pbuf);
			len = strlen(keyfile) + sizeof pbuf + 6;
			keyfilebuf = ep_mem_malloc(len);
			snprintf(keyfilebuf, len, "%s/%s.pem", keyfile, pbuf);
			keyfile = keyfilebuf;
		}

		int fd;
		FILE *fp;

		if ((fd = open(keyfile, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0 ||
				(fp = fdopen(fd, "w")) == NULL)
		{
			ep_app_error("Cannot create %s", keyfile);
			exit(EX_CANTCREAT);
		}

		// third arg is cipher, which should be settable
		PEM_write_RSAPrivateKey(fp, key, NULL, NULL, 0, NULL, NULL);
		fclose(fp);

		// abandon keyfilebuf; we may need the string later
//		if (keyfilebuf != NULL)
//		{
//			ep_mem_free(keyfilebuf);
//			keyfile = NULL;
//		}
	}

fail1:
	// free metadata, if set
	if (gmd != NULL)
		gdp_gclmd_free(gmd);

fail0:
	// OK status can have values; hide that from the user
	if (EP_STAT_ISOK(estat))
		estat = EP_STAT_OK;
	fprintf(stderr, "exiting with status %s\n",
			ep_stat_tostr(estat, buf, sizeof buf));
	return !EP_STAT_ISOK(estat);
}
