/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_crypto.h>
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


// minimum secure key length
#ifndef GDP_MIN_KEY_LEN
# define GDP_MIN_KEY_LEN		1024
#endif // GDP_MIN_KEY_LEN

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-D dbgspec] [-G gdpd_addr]\n"
			"\t[-k] [-K keyfile] [-t keytype] [-b keybits] [logd_name]\n"
			"\t[<mdid>=<metadata>...] [gcl_name]\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for GDP router\n"
			"    -k  create a public/secret key pair\n"
			"    -K  use indicated public key/place to write secret key\n"
			"\tIf -K specifies a directory, a .pem file is written there\n"
			"\twith the name of the GCL (defaults to \"KEYS\" or \".\")\n"
			"    -t  type of key; valid key types are \"rsa\" and \"dsa\"\n"
			"    -b  set size of key in bits\n"
			"    logd_name is the name of the log server to host this log\n"
			"    gcl_name is the name of the log to create\n"
			"    metadata ids are (by convention) four letters or digits\n",
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
	gdp_gcl_t *gcl = NULL;
	gdp_gclmd_t *gmd = NULL;
	int opt;
	EP_STAT estat;
	char *gdpd_addr = NULL;
	char buf[200];
	bool show_usage = false;
	bool make_new_key = false;
	int md_alg_id = -1;
	int keytype = EP_CRYPTO_KEYTYPE_UNKNOWN;
	int keylen = 0;
	int exponent = 0;
	const char *keyfile = NULL;
	int keyform = EP_CRYPTO_KEYFORM_PEM;
	EP_CRYPTO_KEY *key = NULL;
	char *p;

	// collect command-line arguments
	while ((opt = getopt(argc, argv, "b:D:G:h:kK:t:")) > 0)
	{
		switch (opt)
		{
		 case 'b':
			keylen = atoi(optarg);
			break;

		 case 'D':
			ep_dbg_set(optarg);
			break;

		 case 'G':
			gdpd_addr = optarg;
			break;

		 case 'h':
			md_alg_id = ep_crypto_md_alg_byname(optarg);
			if (md_alg_id < 0)
			{
				ep_app_error("unknown digest hash algorithm %s", optarg);
				show_usage = true;
			}
			break;

		 case 'k':
			make_new_key = true;
			break;

		 case 'K':
			keyfile = optarg;
			break;

		 case 't':
			make_new_key = true;
			keytype = ep_crypto_keytype_byname(optarg);
			if (keytype == EP_CRYPTO_KEYTYPE_UNKNOWN)
			{
				ep_app_error("unknown key type %s", optarg);
				show_usage = true;
			}
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
			gmd = gdp_gclmd_new(0);

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

	// initialize the GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	if (md_alg_id < 0)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.hash.alg",
								"sha256");
		md_alg_id = ep_crypto_md_alg_byname(p);
		if (md_alg_id < 0)
		{
			ep_app_error("unknown digest hash algorithm %s", p);
			exit(EX_CONFIG);
		}
	}
	if (keytype <= 0)
	{
		const char *p = ep_adm_getstrparam("swarm.gdp.crypto.sign.alg", "rsa");
		keytype = ep_crypto_keytype_byname(p);
		if (keytype <= 0)
		{
			ep_app_error("unknown keytype %s", p);
			exit(EX_CONFIG);
		}
	}
	switch (keytype)
	{
	case EP_CRYPTO_KEYTYPE_RSA:
		if (keylen <= 0)
			keylen = ep_adm_getintparam("swarm.gdp.crypto.rsa.keylen", 2048);
		exponent = ep_adm_getintparam("swarm.gdp.crypto.rsa.keyexp", 3);
		break;

	case EP_CRYPTO_KEYTYPE_DSA:
		if (keylen <= 0)
			keylen = ep_adm_getintparam("swarm.gdp.crypto.dsa.keylen", 2048);
		break;
	}

	// see if we have an existing key
	bool keyfile_is_directory = false;
	if (keyfile == NULL)
	{
		// nope -- create a key in current directory if requested
		if (make_new_key)
		{
			struct stat st;

			keyfile_is_directory = true;
			keyfile = ep_adm_getstrparam("swarm.gdp.crypto.keydir", "KEYS");
			if (stat(keyfile, &st) != 0 || (st.st_mode & S_IFMT) != S_IFDIR)
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
			key = ep_crypto_key_read_fp(key_fp, keyfile, keytype,
						keyform, EP_CRYPTO_F_SECRET);
			if (key == NULL)
			{
				ep_app_error("Could not read secret key %s", keyfile);
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
		if (keylen < GDP_MIN_KEY_LEN)
		{
			ep_app_error("Insecure key length %d; %d min",
					keylen, GDP_MIN_KEY_LEN);
			exit(EX_UNAVAILABLE);
		}
		key = ep_crypto_key_create(keytype, keylen, exponent, NULL);
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
		uint8_t der_buf[EP_CRYPTO_MAX_DER + 4];
		uint8_t *derp = der_buf + 4;

		if (gmd == NULL)
			gmd = gdp_gclmd_new(0);
		der_buf[0] = md_alg_id;
		der_buf[1] = keytype;
		der_buf[2] = (keylen >> 8) & 0xff;
		der_buf[3] = keylen & 0xff;
		estat = ep_crypto_key_write_mem(key, derp, EP_CRYPTO_MAX_DER,
						EP_CRYPTO_KEYFORM_DER, EP_CRYPTO_CIPHER_NONE,
						EP_CRYPTO_F_PUBLIC);
		if (!EP_STAT_ISOK(estat))
		{
			ep_app_error("Could not create DER format public key");
			exit(EX_SOFTWARE);
		}

		gdp_gclmd_add(gmd, GDP_GCLMD_PUBKEY,
				EP_STAT_TO_INT(estat) + 4, der_buf);
	}

	gdp_parse_name(logdxname, logdiname);

	/**************************************************************
	**  Hello sailor, this is where the actual creation happens  **
	**************************************************************/
	if (gclxname == NULL)
	{
		// create a new GCL handle with a new name
		estat = gdp_gcl_create(NULL, logdiname, gmd, &gcl);
	}
	else
	{
		// open or create a GCL with the provided name
		gdp_parse_name(gclxname, gcliname);

		// save the external name as metadata
		if (gmd == NULL)
			gmd = gdp_gclmd_new(0);
		gdp_gclmd_add(gmd, GDP_GCLMD_XID, strlen(gclxname), gclxname);

		// make sure it doesn't already exist
		estat = gdp_gcl_open(gcliname, GDP_MODE_RO, NULL, &gcl);
		if (EP_STAT_ISOK(estat))
		{
			// oops, we shouldn't be able to open it
			(void) gdp_gcl_close(gcl);
			ep_app_fatal("Cannot create %s: already exists", gclxname);
		}

		// OK, we're cool, go ahead and create it...
		estat = gdp_gcl_create(gcliname, logdiname, gmd, &gcl);
	}
	EP_STAT_CHECK(estat, goto fail1);

	// just for a lark, let the user know the (internal) name
	{
		gdp_pname_t pname;

		printf("Created new GCL %s\n\ton log server %s\n",
				gdp_printable_name(*gdp_gcl_getname(gcl), pname), logdxname);
	}

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
		estat = ep_crypto_key_write_fp(key, fp, EP_CRYPTO_KEYFORM_PEM,
						EP_CRYPTO_CIPHER_NONE, EP_CRYPTO_F_SECRET);
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
