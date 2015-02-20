/* vim: set ai sw=4 sts=4 ts=4 : */

#include <ep/ep.h>
#include <ep/ep_hexdump.h>
#include <ep/ep_string.h>
#include <ep/ep_time.h>
#include <gdplogd/logd_physlog.h>

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


void
usage(const char *msg)
{
	fprintf(stderr, "Usage error: %s\n", msg);
	fprintf(stderr, "Usage: log-view [-d dir] [-l] [-r] [gcl_name]\n");
	fprintf(stderr, "\t-d dir -- set log database root directory\n");
	fprintf(stderr, "\t-l -- list all local GCLs\n");
	fprintf(stderr, "\t-r -- don't print raw byte hex dumps\n");

	exit(EX_USAGE);
}

/*
**  LOG-VIEW --- display raw on-disk storage
**
**		Not for user consumption.
*/


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
list_gcls(const char *gcl_dir_name)
{
	DIR *dir;
	int subdir;

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
			fprintf(stdout, "%s\n", dent->d_name);
		}
		closedir(dir);
	}

	return EX_OK;
}


int
main(int argc, char *argv[])
{
	int opt;
	bool list_gcl = false;
	bool print_raw = true;
	char *gcl_xname = NULL;
	char *gcl_dir_name = GCL_DIR;

	while ((opt = getopt(argc, argv, "d:lr")) > 0)
	{
		switch (opt)
		{
		case 'd':
			gcl_dir_name = optarg;
			break;

		case 'l':
			list_gcl = true;
			break;

		case 'r':
			print_raw = false;
			break;

		default:
			usage("unknown flag");
		}
	}
	argc -= optind;
	argv += optind;

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
	gdp_pname_t gcl_pname;

	EP_STAT estat = gdp_parse_name(gcl_xname, gcl_name);
	if (!EP_STAT_ISOK(estat))
	{
		usage("unparsable GCL name");
	}

	(void) gdp_printable_name(gcl_name, gcl_pname);

	// Add 5 in the middle for '/_xx/'
	int filename_size = strlen(gcl_dir_name) + 5 + strlen(gcl_pname) +
			strlen(GCL_DATA_SUFFIX) + 1;
	char *data_filename = malloc(filename_size);

	snprintf(data_filename, filename_size,
			"%s/_%02x/%s%s",
			gcl_dir_name, gcl_name[0], gcl_pname, GCL_DATA_SUFFIX);
	fprintf(stdout, "GCL name: %s\n", gcl_pname);
	fprintf(stdout, "Reading %s\n\n", data_filename);

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

	printf("Header: magic = 0x%016" PRIx32
			", version = %" PRIi32
			", type = %" PRIi16 "\n",
			header.magic, header.version, header.log_type);
	printf("\theader size = %" PRId32 " (0x%" PRIx32 ")"
			", metadata entries = %d\n",
			header.header_size, header.header_size,
			header.num_metadata_entries);
	if (print_raw)
	{
		ep_hexdump(&header, sizeof header, stdout, EP_HEXDUMP_HEX, file_offset);
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
		struct mdhdr *metadata_hdrs = malloc(header.num_metadata_entries *
											sizeof *metadata_hdrs);

		if (fread(metadata_hdrs,
					sizeof *metadata_hdrs,
					header.num_metadata_entries,
					data_fp)
			!= header.num_metadata_entries)
		{
			fprintf(stderr,
					"fread() failed while reading metadata headers, ferror = %d\n",
					ferror(data_fp));
			return EX_DATAERR;
		}

		for (i = 0; i < header.num_metadata_entries; ++i)
		{
			printf("\n\tMetadata entry %d: name = 0x%" PRIx32
							", len = %" PRId32 "\n",
					i, metadata_hdrs[i].md_id, metadata_hdrs[i].md_len);
		}

		if (print_raw)
		{
			ep_hexdump(metadata_hdrs,
					header.num_metadata_entries * sizeof *metadata_hdrs,
					stdout, EP_HEXDUMP_ASCII, file_offset);
		}
		file_offset += header.num_metadata_entries * sizeof *metadata_hdrs;
		CHECK_FILE_OFFSET(data_fp, file_offset);

		fprintf(stdout, "\n");

		for (i = 0; i < header.num_metadata_entries; ++i)
		{
			char *metadata_string = malloc(metadata_hdrs[i].md_len + 1);
												// +1 for null-terminator
			if (fread(metadata_string, metadata_hdrs[i].md_len, 1, data_fp) != 1)
			{
				fprintf(stderr, "fread() failed while reading metadata string, ferror = %d\n", ferror(data_fp));
				return EX_DATAERR;
			}
			metadata_string[metadata_hdrs[i].md_len] = '\0';
			fprintf(stdout, "Metadata entry %d: %s", i, metadata_string);
			free(metadata_string);

			if (print_raw)
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
	else
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	fprintf(stdout, "\n");
	fprintf(stdout, "Data records\n");

	while (fread(&record, sizeof record, 1, data_fp) == 1)
	{
		fprintf(stdout, "\n");
		fprintf(stdout, "Offset = %zd (0x%zx)\n", file_offset, file_offset);
		fprintf(stdout, "Record number: %" PRIgdp_recno "\n", record.recno);
		fprintf(stdout, "Human readable timestamp: ");
		ep_time_print(&record.timestamp, stdout, true);
		fprintf(stdout, "\n");
		fprintf(stdout, "Raw timestamp seconds: %" PRIi64 "\n", record.timestamp.tv_sec);
		fprintf(stdout, "Raw Timestamp ns: %" PRIi32 "\n", record.timestamp.tv_nsec);
		fprintf(stdout, "Time accuracy (s): %8f\n", record.timestamp.tv_accuracy);
		fprintf(stdout, "Data length: %" PRIi64 "\n", record.data_length);

		if (print_raw)
		{
			fprintf(stdout, "\n");
			fprintf(stdout, "Raw:\n");
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

		fprintf(stdout, "\n");
		fprintf(stdout, "Data:\n");
		ep_hexdump(data_buffer, record.data_length,
				stdout, EP_HEXDUMP_ASCII, file_offset);
		file_offset += record.data_length;
		CHECK_FILE_OFFSET(data_fp, file_offset);

		free(data_buffer);
	}
	exit(EX_OK);
}
