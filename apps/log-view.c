/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <ep/ep.h>
#include <ep/ep_string.h>
#include <gdpd/gdpd_physlog.h>

#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void hexdump(FILE *stream, void *buf, size_t n, int start_label, bool show_ascii)
{
	unsigned char *char_buf = (unsigned char *)buf;
	size_t width = 16;
	size_t end = (n / width) * width;

	for (size_t i = 0; i < end; i += width)
	{
		fprintf(stream, "%08lx", start_label + i);
		for (size_t j = i; j < i + width; ++j)
		{
			fprintf(stream, " %02x", char_buf[j]);
		}
		fprintf(stream, "\n");

		if (show_ascii)
		{
			fprintf(stream, "%-8s", "");
			for (size_t j = i; j < i + width; ++j)
			{
				fprintf(stream, " %c ", char_buf[j]);
			}
			fprintf(stream, "\n");
		}
	}

	fprintf(stream, "%08lx", start_label + end);
	for (size_t i = end; i < n; ++i)
	{
		fprintf(stream, " %02x", char_buf[i]);
	}
	fprintf(stream, "\n");

	if (show_ascii)
	{
		fprintf(stream, "%-8s", "");
		for (size_t i = end; i < n; ++i)
		{
			fprintf(stream, " %c ", char_buf[i]);
		}
		fprintf(stream, "\n");
	}
}

/*
 * log-view is a utility to aid in the debugging of the GDP on-disk storage format
 *
 * Command line arguments
 *
 * l - only list the available GCLs
 * n <GCL name in base64> - print the contents of the specified GCL
 * r - don't print the corresponding raw bytes to each parsed field
 */

int main(int argc, char *argv[]) {
	int opt;
	bool list_gcl = false;
	bool print_raw = true;
	char *gcl_name = NULL;

	if (argc == 1)
	{
		fprintf(stdout, "Usage:\n");
		fprintf(stdout, "\tlog-view -l -- list all local GCLs\n");
		fprintf(stdout, "\tlog-view -n gcl_name [-r] -- print contents of GCL\n");
		fprintf(stdout, "Options:\n");
		fprintf(stdout, "\t-r -- don't print raw byte hex dumps\n");

		return 0;
	}

	while ((opt = getopt(argc, argv, "l::n:r::")) > 0)
	{
		switch (opt)
		{
		case 'l':
			list_gcl = true;
			break;
		case 'n':
			gcl_name = strdup(optarg);
			break;
		case 'r':
			print_raw = false;
			break;
		}
	}

	if (list_gcl)
	{
		struct dirent *dir_entry;
		DIR *gcl_dir = opendir(GCL_DIR);

		if (gcl_dir == NULL)
		{
			fprintf(stderr, "Could not open %s, errno = %d\n", GCL_DIR, errno);
			return 1;
		}

		while ((dir_entry = readdir(gcl_dir)) != NULL)
		{
			char *dot = strrchr(dir_entry->d_name, '.');
			if (dot && !strcmp(dot, GCL_DATA_SUFFIX))
			{
				fprintf(stdout, "%.*s\n", (int)(dot - dir_entry->d_name), dir_entry->d_name);
			}
		}
		return 0;
	}

	// Add 1 in the middle for '/'
	int filename_size = strlen(GCL_DIR) + 1 + strlen(gcl_name) + strlen(GCL_DATA_SUFFIX) + 1;
	char *data_filename = malloc(filename_size);

	data_filename[0] = '\0';
	strlcat(data_filename, GCL_DIR, filename_size);
	strlcat(data_filename, "/", filename_size);
	strlcat(data_filename, gcl_name, filename_size);
	strlcat(data_filename, GCL_DATA_SUFFIX, filename_size);
	fprintf(stdout, "GCL name: %s\n", gcl_name);
	fprintf(stdout, "Reading %s\n\n", data_filename);

	FILE *data_fp = fopen(data_filename, "r");
	size_t file_offset = 0;
	gcl_log_header header;
	gcl_log_record record;

	if (data_fp == NULL)
	{
		fprintf(stderr, "Could not open %s, errno = %d\n", data_filename, errno);
		return 1;
	}

	if (fread(&header, sizeof(header), 1, data_fp) != 1)
	{
		fprintf(stderr, "fread() failed while reading header, ferror = %d\n", ferror(data_fp));
		return 1;
	}

	fprintf(stdout, "Magic: 0x%016" PRIx64 "\n", header.magic);
	fprintf(stdout, "Version: %" PRIi64 "\n", header.version);
	fprintf(stdout, "Header size: %" PRIi16 "\n", header.header_size);
	fprintf(stdout, "Log type: %" PRIi16 "\n", header.log_type);
	fprintf(stdout, "Number of metadata entries: %" PRIi16 "\n", header.num_metadata_entries);

	if (print_raw)
	{
		fprintf(stdout, "\n");
		fprintf(stdout, "Raw:\n");
		hexdump(stdout, &header, sizeof(header), file_offset, false);
	}
	file_offset += sizeof(header);

	int16_t *metadata_lengths = malloc(header.num_metadata_entries * sizeof(int16_t));

	if (header.num_metadata_entries > 0)
	{
		fprintf(stdout, "\n");
		fprintf(stdout, "Metadata\n\n");

		if (fread(metadata_lengths, sizeof(int16_t), header.num_metadata_entries, data_fp)
			!= header.num_metadata_entries)
		{
			fprintf(stderr, "fread() failed while reading metadata lengths, ferror = %d\n", ferror(data_fp));
			return 1;
		}

		for (int i = 0; i < header.num_metadata_entries; ++i)
		{
			fprintf(stdout, "Length of metadata entry %d: %" PRIi16 "\n", i, metadata_lengths[i]);
		}

		if (print_raw)
		{
			fprintf(stdout, "\n");
			fprintf(stdout, "Raw:\n");
			hexdump(stdout, metadata_lengths, header.num_metadata_entries * sizeof(int16_t), file_offset, false);
		}
		file_offset += header.num_metadata_entries * sizeof(int16_t);

		fprintf(stdout, "\n");

		for (int i = 0; i < header.num_metadata_entries; ++i)
		{
			char *metadata_string = malloc(metadata_lengths[i] + 1); // +1 for null-terminator
			if (fread(metadata_string, metadata_lengths[i], 1, data_fp) != 1)
			{
				fprintf(stderr, "fread() failed while reading metadata string, ferror = %d\n", ferror(data_fp));
				return 1;
			}
			metadata_string[metadata_lengths[i]] = '\0';
			fprintf(stdout, "Metadata entry %d: %s", i, metadata_string);
			free(metadata_string);

			if (print_raw)
			{
				fprintf(stdout, "\n");
				fprintf(stdout, "Raw:\n");
				hexdump(stdout, metadata_string, metadata_lengths[i], file_offset, true);
			}
			file_offset += metadata_lengths[i];
		}

		return 0;
	}
	else
	{
		fprintf(stdout, "\n<No metadata>\n");
	}

	fprintf(stdout, "\n");
	fprintf(stdout, "Data records\n");

	while (fread(&record, sizeof(record), 1, data_fp) == 1)
	{
		fprintf(stdout, "\n");
		fprintf(stdout, "Record number: %" PRIgdp_recno "\n", record.recno);
		fprintf(stdout, "Human readable timestamp: %s", ctime(&record.timestamp.stamp.tv_sec));
		fprintf(stdout, "Raw timestamp seconds: %" PRIi64 "\n", record.timestamp.stamp.tv_sec);
		fprintf(stdout, "Raw Timestamp ns: %" PRIi32 "\n", record.timestamp.stamp.tv_nsec);
		fprintf(stdout, "Time accuracy (ns): %" PRIu32 "\n", record.timestamp.accuracy);
		fprintf(stdout, "Data length: %" PRIi64 "\n", record.data_length);

		if (print_raw)
		{
			fprintf(stdout, "\n");
			fprintf(stdout, "Raw:\n");
			hexdump(stdout, &record, sizeof(record), file_offset, false);
		}
		file_offset += sizeof(record);

		char *data_buffer = malloc(record.data_length);
		if (fread(data_buffer, record.data_length, 1, data_fp) != 1)
		{
			fprintf(stderr, "fread() failed while reading data, ferror = %d\n", ferror(data_fp));
			return 1;
		}

		fprintf(stdout, "\n");
		fprintf(stdout, "Data:\n");
		hexdump(stdout, data_buffer, record.data_length, file_offset, true);
		file_offset += record.data_length;

		free(data_buffer);
	}
}
