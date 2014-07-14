/* vim: set ai sw=4 sts=4 : */

#include <gdp/gdp.h>

#include <ep/ep_app.h>
#include <ep/ep_b64.h>
#include <ep/ep_dbg.h>
#include <ep/ep_string.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

int
random_in_range(unsigned int min, unsigned int max)
{
	int base_random = rand(); /* in [0, RAND_MAX] */
	if (RAND_MAX == base_random)
		return random_in_range(min, max);
	/* now guaranteed to be in [0, RAND_MAX) */
	int range = max - min, remainder = RAND_MAX % range, bucket = RAND_MAX
	        / range;
	/* There are range buckets, plus one smaller interval
	 within remainder of RAND_MAX */
	if (base_random < RAND_MAX - remainder)
	{
		return min + base_random / bucket;
	}
	else
	{
		return random_in_range(min, max);
	}
}

int
main(int argc, char *argv[])
{
	gcl_handle_t *gclh_write;
	gcl_handle_t *gclh_read;
	int opt;
	int num_records = 1000;
	int min_length = 1023;
	int max_length = 2047;
	int trials = 1;
	EP_STAT estat;
	char buf[200];

	while ((opt = getopt(argc, argv, "D:n:t:m:M:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			ep_dbg_set(optarg);
			break;
		case 'n':
			num_records = atoi(optarg);
			break;
		case 't':
			trials = atoi(optarg);
			break;
		case 'm':
			min_length = atoi(optarg);
			break;
		case 'M':
			max_length = atoi(optarg);
			break;
		}
	}

	estat = gdp_init(true);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	fprintf(stdout, "\nRunning trials\n\n");

	struct timespec start_time;
	struct timespec end_time;
	char *data;
	size_t record_size;
	size_t max_record_size = max_length + 1;
	size_t data_size = num_records * (max_record_size);
	char *cur_record;
	char *cur_record_b64;
	gdp_msg_t msg;
	gcl_name_t internal_name;
	gcl_pname_t printable_name;
	struct evbuffer *evb = evbuffer_new();

	data = malloc(data_size);
	cur_record = malloc(max_record_size);
	cur_record_b64 = malloc((2 * max_length) + 1);

	int t;
	int i;

	for (t = 0; t < trials; ++t)
	{
		fprintf(stdout, "Trial %d\n", t);
		fprintf(stdout, "Generating %d records of length [%d, %d]\n",
		        num_records, min_length, max_length);
		for (i = 0; i < num_records; ++i)
		{
			evutil_secure_rng_get_bytes(cur_record, max_length);
			ep_b64_encode(cur_record, max_length, cur_record_b64,
			        (2 * max_length) + 1, EP_B64_ENC_URL);
			record_size = random_in_range(min_length, max_length + 1);
			memcpy(data + (i * max_record_size), cur_record_b64, record_size);
			data[(i * max_record_size) + record_size] = '\0';
			fprintf(stdout, "Msgno = %d\n", i + 1);
			fprintf(stdout, "%s\n", &data[(i * max_record_size)]);
			fprintf(stdout, "record length: %lu\n", strlen(&data[(i * max_record_size)]));
		}

		estat = gdp_gcl_create(NULL, NULL, &gclh_write);

		EP_STAT_CHECK(estat, goto fail0);
		gdp_gcl_print(gclh_write, stdout, 0, 0);

		clock_gettime(CLOCK_REALTIME, &start_time);
		fprintf(stdout, "Writing data (start_time = %lu:%lu)\n", start_time.tv_sec, start_time.tv_nsec);
		for (i = 0; i < num_records; ++i) {
			memset(&msg, '\0', sizeof msg);
			msg.data = &data[(i * max_record_size)];
			msg.len = strlen(msg.data);
			msg.msgno = i + 1;

			estat = gdp_gcl_append(gclh_write, &msg);
			EP_STAT_CHECK(estat, goto fail1);
		}
		clock_gettime(CLOCK_REALTIME, &end_time);
		fprintf(stdout, "Finished writing data (end_time = %lu:%lu)\n", end_time.tv_sec, end_time.tv_nsec);
		fprintf(stdout, "Elapsed time = %lu s (%lu ns)\n", end_time.tv_sec - start_time.tv_sec,
			end_time.tv_nsec - start_time.tv_nsec);
		memcpy(internal_name, gdp_gcl_getname(gclh_write), sizeof internal_name);
		gdp_gcl_printable_name(internal_name, printable_name);
		gdp_gcl_close(gclh_write);
		estat = gdp_gcl_open(internal_name, GDP_MODE_RO, &gclh_read);
		clock_gettime(CLOCK_REALTIME, &start_time);
		fprintf(stdout, "Reading data (start_time = %lu:%lu)\n", start_time.tv_sec, start_time.tv_nsec);
		for (i = 0; i < num_records; ++i) {
			estat = gdp_gcl_read(gclh_read, i + 1, &msg, evb);
			EP_STAT_CHECK(estat, goto fail2);
			msg.len = evbuffer_remove(evb, cur_record, max_record_size);
			msg.data = cur_record;
			gdp_gcl_msg_print(&msg, stdout);

			evbuffer_drain(evb, UINT_MAX);
		}
		clock_gettime(CLOCK_REALTIME, &end_time);
		fprintf(stdout, "Finished reading data (end_time = %lu:%lu)\n", end_time.tv_sec, end_time.tv_nsec);
		fprintf(stdout, "Elapsed time = %lu s (%lu ns)\n", end_time.tv_sec - start_time.tv_sec,
			end_time.tv_nsec - start_time.tv_nsec);
	}

	free(cur_record_b64);
	free(cur_record);
	free(data);

	goto done;

fail2:
	gdp_gcl_close(gclh_read);

fail1:
	gdp_gcl_close(gclh_write);

fail0:
done:
	fprintf(stderr, "exiting with status %s\n",
	ep_stat_tostr(estat, buf, sizeof buf));

	return !EP_STAT_ISOK(estat);
}
