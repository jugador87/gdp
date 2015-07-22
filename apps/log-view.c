/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>
#include <gdp/gdp.h>

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

// following are actually private definitions
#include <gdp/gdp_priv.h>
#include <gdplogd/logd_physlog.h>


/*
**  LOG-VIEW --- display raw on-disk storage
**
**		Not for user consumption.
**		This does peek into private header files.
*/

static EP_DBG	Dbg = EP_DBG_INIT("log-view", "Dump GDP logs for debugging");


#define CHECK_FILE_OFFSET	check_file_offset

void
check_file_offset(FILE *fp, long offset)
{
	if (ftell(fp) != offset)
	{
		printf("%sWARNING: file offset error (%ld != %ld)%s\n",
				EpVid->vidfgred, ftell(fp), offset, EpVid->vidnorm);
	}
}


int
show_gcl(const char *gcl_dir_name, gdp_name_t gcl_name, int plev)
{
	gdp_pname_t gcl_pname;

	(void) gdp_printable_name(gcl_name, gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_DATA_SUFFIX) + 1;
	char *data_filename = alloca(filename_size);

	snprintf(data_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_DATA_SUFFIX);
	fprintf(stdout, "%s\n", gcl_pname);
	ep_dbg_cprintf(Dbg, 6, "Reading %s\n\n", data_filename);

	FILE *data_fp = fopen(data_filename, "r");
	size_t file_offset = 0;
	gcl_log_header header;
	gcl_log_record record;

	if (data_fp == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n", data_filename, errno);
		return EX_NOINPUT;
	}

	if (fread(&header, sizeof header, 1, data_fp) != 1)
	{
		fprintf(stderr, "fread() failed while reading header, ferror = %d\n",
				ferror(data_fp));
		return EX_DATAERR;
	}

	if (plev >= GDP_PR_BASIC)
	{
		printf("Header: magic = 0x%016" PRIx32
				", version = %" PRIi32
				", type = %" PRIi16 "\n",
				header.magic, header.version, header.log_type);
		printf("\theader size = %" PRId32 " (0x%" PRIx32 ")"
				", metadata entries = %d\n",
				header.header_size, header.header_size,
				header.num_metadata_entries);
		if (plev >= GDP_PR_DETAILED)
		{
			ep_hexdump(&header, sizeof header, stdout,
					EP_HEXDUMP_HEX, file_offset);
		}
	}
	file_offset += sizeof header;
	CHECK_FILE_OFFSET(data_fp, file_offset);

	if (header.num_metadata_entries > 0)
	{
		int i;
		struct mdhdr
		{
			uint32_t md_id;
			uint32_t md_len;
		};
		struct mdhdr *metadata_hdrs = alloca(header.num_metadata_entries *
											sizeof *metadata_hdrs);

		if (fread(metadata_hdrs,
					sizeof *metadata_hdrs,
					header.num_metadata_entries,
					data_fp)
			!= header.num_metadata_entries)
		{
			fprintf(stderr,
					"fread() failed while reading metadata headers,"
					" ferror = %d\n",
					ferror(data_fp));
			return EX_DATAERR;
		}

		if (plev >= GDP_PR_DETAILED)
		{
			ep_hexdump(metadata_hdrs,
					header.num_metadata_entries * sizeof *metadata_hdrs,
					stdout, EP_HEXDUMP_ASCII, file_offset);
		}

		file_offset += header.num_metadata_entries * sizeof *metadata_hdrs;
		CHECK_FILE_OFFSET(data_fp, file_offset);

		for (i = 0; i < header.num_metadata_entries; ++i)
		{
			char *metadata_string = malloc(metadata_hdrs[i].md_len + 1);
												// +1 for null-terminator
			if (fread(metadata_string, metadata_hdrs[i].md_len, 1, data_fp) != 1)
			{
				fprintf(stderr,
						"fread() failed while reading metadata string,"
						" ferror = %d\n",
						ferror(data_fp));
				return EX_DATAERR;
			}
			metadata_string[metadata_hdrs[i].md_len] = '\0';
			if (plev >= GDP_PR_BASIC)
			{
				fprintf(stdout,
						"\tMetadata entry %d: name = 0x%08" PRIx32
						", len = %" PRId32 "\n\t\t%s\n",
						i, metadata_hdrs[i].md_id, metadata_hdrs[i].md_len,
						metadata_string);
			}
			else if (metadata_hdrs[i].md_id == GDP_GCLMD_XID)
			{
				fprintf(stdout,
						"\tExternal name: %s\n", metadata_string);
			}
			free(metadata_string);

			if (plev >= GDP_PR_DETAILED)
			{
				fprintf(stdout, "\n");
				fprintf(stdout, "Raw:\n");
				ep_hexdump(metadata_string, metadata_hdrs[i].md_len,
						stdout, EP_HEXDUMP_ASCII, file_offset);
			}
			file_offset += metadata_hdrs[i].md_len;
			CHECK_FILE_OFFSET(data_fp, file_offset);
		}
	}
	else if (plev >= GDP_PR_BASIC)
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	if (plev < GDP_PR_BASIC)
		return EX_OK;

	fprintf(stdout, "\n");
	fprintf(stdout, "Data records\n");

	while (fread(&record, sizeof record, 1, data_fp) == 1)
	{
		fprintf(stdout, "\nRecord number: %" PRIgdp_recno "\n", record.recno);
		fprintf(stdout, "\tOffset = %zd (0x%zx)\n", file_offset, file_offset);
		fprintf(stdout, "\tHuman readable timestamp: ");
		ep_time_print(&record.timestamp, stdout, true);
		fprintf(stdout, "\n\tRaw timestamp seconds: %" PRIi64 "\n", record.timestamp.tv_sec);
		fprintf(stdout, "\tRaw Timestamp ns: %" PRIi32 "\n", record.timestamp.tv_nsec);
		fprintf(stdout, "\tTime accuracy (s): %8f\n", record.timestamp.tv_accuracy);
		fprintf(stdout, "\tData length: %" PRIi64 "\n", record.data_length);

		if (plev >= GDP_PR_DETAILED)
		{
			fprintf(stdout, "\tRaw:\n");
			ep_hexdump(&record, sizeof record, stdout,
					EP_HEXDUMP_HEX, file_offset);
		}
		file_offset += sizeof record;
		CHECK_FILE_OFFSET(data_fp, file_offset);

		char *data_buffer = malloc(record.data_length);
		if (fread(data_buffer, record.data_length, 1, data_fp) != 1)
		{
			fprintf(stderr, "fread() failed while reading data, ferror = %d\n", ferror(data_fp));
			return EX_DATAERR;
		}

		if (plev >= GDP_PR_BASIC + 1)
		{
			fprintf(stdout, "\tData:\n");
			ep_hexdump(data_buffer, record.data_length,
					stdout, EP_HEXDUMP_ASCII,
					plev < GDP_PR_DETAILED ? 0 : file_offset);
		}
		file_offset += record.data_length;
		CHECK_FILE_OFFSET(data_fp, file_offset);

		free(data_buffer);
	}
	
	return EX_OK;
}


int
list_gcls(const char *gcl_dir_name)
{
	DIR *dir;
	int subdir;
	gdp_name_t gcl_iname;

	dir = opendir(gcl_dir_name);
	if (dir == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n",
				gcl_dir_name, errno);
		return EX_NOINPUT;
	}
	closedir(dir);

	for (subdir = 0; subdir < 0x100; subdir++)
	{
		char dbuf[400];

		snprintf(dbuf, sizeof dbuf, "%s/_%02x", gcl_dir_name, subdir);
		dir = opendir(dbuf);
		if (dir == NULL)
			continue;

		for (;;)
		{
			struct dirent dentbuf;
			struct dirent *dent;

			// read the next directory entry
			int i = readdir_r(dir, &dentbuf, &dent);
			if (i != 0)
			{
				ep_log(ep_stat_from_errno(i),
						"list_gcls: readdir_r failed");
				break;
			}
			if (dent == NULL)
				break;

			// we're only interested in .data files
			char *p = strrchr(dent->d_name, '.');
			if (p == NULL || strcmp(p, GCL_DATA_SUFFIX) != 0)
				continue;

			// strip off the ".data"
			*p = '\0';

			// print the name
			gdp_parse_name(dent->d_name, gcl_iname);
			show_gcl(gcl_dir_name, gcl_iname, GDP_PR_PRETTY);
		}
		closedir(dir);
	}

	return EX_OK;
}


void
usage(const char *msg)
{
	fprintf(stderr,
			"Usage error: %s\n"
			"Usage: log-view [-d dir] [-l] [-r] [gcl_name]\n"
			"\t-d dir -- set log database root directory\n"
			"\t-D spec -- set debug flags\n"
			"\t-l -- list all local GCLs\n"
			"\t-r -- print raw byte hex dumps\n"
			"\t-v -- print verbose information\n",
				msg);

	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	int opt;
	int plev = GDP_PR_BASIC;
	bool list_gcl = false;
	char *gcl_xname = NULL;
	const char *gcl_dir_name = NULL;

	ep_lib_init(0);

	while ((opt = getopt(argc, argv, "d:D:lrv")) > 0)
	{
		switch (opt)
		{
		case 'd':
			gcl_dir_name = optarg;
			break;

		case 'D':
			ep_dbg_set(optarg);
			break;

		case 'l':
			list_gcl = true;
			break;

		case 'r':
			plev = GDP_PR_DETAILED;
			break;

		case 'v':
			plev = GDP_PR_BASIC + 1;
			break;

		default:
			usage("unknown flag");
		}
	}
	argc -= optind;
	argv += optind;

	// arrange to get runtime parameters
	ep_adm_readparams("gdp");
	ep_adm_readparams("gdplogd");

	// set up GCL directory path
	if (gcl_dir_name == NULL)
		gcl_dir_name = ep_adm_getstrparam("swarm.gdplogd.gcl.dir", GCL_DIR);

	if (list_gcl)
	{
		if (argc > 0)
			usage("cannot use a GCL name with -l");
		return list_gcls(gcl_dir_name);
	}

	if (argc > 0)
	{
		gcl_xname = argv[0];
		argc--;
		argv++;
		if (argc > 0)
			usage("extra arguments");
	}
	else
	{
		usage("GCL name required");
	}

	gdp_name_t gcl_name;

	EP_STAT estat = gdp_parse_name(gcl_xname, gcl_name);
	if (!EP_STAT_ISOK(estat))
	{
		usage("unparsable GCL name");
	}

	exit(show_gcl(gcl_dir_name, gcl_name, plev));
}
