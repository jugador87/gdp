/* vim: set ai sw=4 sts=4 ts=4 :*/

#include <gdp/gdp.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_log.h>
#include <ep/ep_string.h>

#include <mosquitto.h>

#include <errno.h>
#include <getopt.h>
#include <sysexits.h>


static EP_DBG	Dbg = EP_DBG_INIT("mqtt-gdp-gateway", "MTQQ to GDP gateway");


/*
**  Called by Mosquitto to do logging
*/

void
log_cb(struct mosquitto *mosq,
				void *udata,
				int level,
				const char *str)
{
	const char *level_str = NULL;

	switch (level)
	{
	case MOSQ_LOG_DEBUG:
		//level_str = "debug";
		break;

	case MOSQ_LOG_INFO:
		level_str = "info";
		break;

	case MOSQ_LOG_WARNING:
		level_str = "warning";
		break;

	case MOSQ_LOG_ERR:
		level_str = "error";
		break;

	case MOSQ_LOG_NOTICE:
		level_str = "notice";
		break;

	default:
		fprintf(stderr, "unknown log level %d\n", level);
		level_str = "???";
		break;
	}
	if (level_str != NULL)
		fprintf(stderr, "MOSQ(%s): %s\n", level_str, str);
}


/*
**  Called by Mosquitto when a message comes in.
*/

void
message_cb(struct mosquitto *mosq,
				void *udata,
				const struct mosquitto_message *msg)
{
	gdp_gcl_t *gcl = udata;
	const char *payload;

	ep_dbg_cprintf(Dbg, 5, "message_cb: %s @%d => %s\n",
			msg->topic, msg->qos, msg->payload);
	//printf("%s @%d => %s\n", msg->topic, msg->qos, msg->payload);

	if (msg->payloadlen <= 0)
		payload = "\"none\"";
	else
		payload = msg->payload;
	if (gcl != NULL)
	{
		EP_STAT estat;
		gdp_datum_t *datum = gdp_datum_new();

		gdp_buf_printf(gdp_datum_getbuf(datum),
				"{topic:\"%s\", qos:%d, len:%d, payload:%s}\n",
				msg->topic, msg->qos, msg->payloadlen, msg->payload);
		estat = gdp_gcl_append(gcl, datum);
		if (!EP_STAT_ISOK(estat))
		{
			ep_log(estat, "cannot log MQTT message");
		}
		gdp_datum_free(datum);
	}
	else
	{
		printf("{topic:\"%s\", qos:%d, len:%d, payload:%s}\n",
				msg->topic, msg->qos, msg->payloadlen, msg->payload);
	}
}


/*
**  Run Mosquitto against a designated broker.
*/

EP_STAT
mosquitto_run(const char *mqtt_broker,
			int mqtt_port,
			const char *subscr_pat,
			int subscr_qos,
			void *udata)
{
	struct mosquitto *mosq;
	int mid;				// message id --- do we need this?
	const char *phase;
	int istat;

	mosquitto_lib_init();

	// get a mosquitto context
	mosq = mosquitto_new(NULL, true, udata);
	if (mosq == NULL)
		goto fail1;

	// can't just pass in NULL for udata, since mosquitto_new automatically
	// maps NULL to mosq
	mosquitto_user_data_set(mosq, udata);

	// set up callbacks
	mosquitto_log_callback_set(mosq, &log_cb);
	mosquitto_message_callback_set(mosq, &message_cb);

	// connect to the broker
	phase = "connect";
	istat = mosquitto_connect(mosq, mqtt_broker, mqtt_port, 60);
	if (istat != 0)
		goto fail2;

	phase = "subscribe";
	istat = mosquitto_subscribe(mosq, &mid, subscr_pat, subscr_qos);
	if (istat != 0)
		goto fail2;

	phase = "loop";
	istat = mosquitto_loop_forever(mosq, -1, 1);

	// should never get here

fail2:
	if (istat == MOSQ_ERR_ERRNO)
		fprintf(stderr, "mosquitto error in %s: %s\n",
				phase, strerror(errno));
	else
		fprintf(stderr, "mosquitto error in %s: %s\n",
				phase, mosquitto_strerror(istat));
	(void) mosquitto_disconnect(mosq);	// ignore possible MOSQ_ERR_NO_CONN
										// "not connected" error
	mosquitto_destroy(mosq);
	return EP_STAT_ABORT;

fail1:
	fprintf(stderr, "cannot create mosquitto context: %s\n",
			strerror(errno));
	return EP_STAT_ABORT;
}


EP_STAT
mqtt_to_log(
		const char *mqtt_addr,
		int mqtt_port,
		const char *mqtt_topic,
		int mqtt_qos,
		const char *gdp_log_name,
		const char *gdpd_addr,
		const char *signing_key_file)
{
	EP_STAT estat;
	gdp_name_t gcliname;
	gdp_gcl_open_info_t *info;

	ep_dbg_cprintf(Dbg, 1, "mqtt_to_log\n");

	// initialize GDP library
	estat = gdp_init(gdpd_addr);
	if (!EP_STAT_ISOK(estat))
	{
		ep_app_error("GDP Initialization failed");
		goto fail0;
	}

	// allow thread to settle to avoid interspersed debug output
	ep_time_nanosleep(INT64_C(100000000));

	// set up any open information
	info = gdp_gcl_open_info_new();

	if (signing_key_file != NULL)
	{
		FILE *fp;
		EP_CRYPTO_KEY *skey;

		fp = fopen(signing_key_file, "r");
		if (fp == NULL)
		{
			ep_app_error("cannot open signing key file %s", signing_key_file);
			goto fail1;
		}

		skey = ep_crypto_key_read_fp(fp, signing_key_file,
				EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_F_SECRET);
		if (skey == NULL)
		{
			ep_app_error("cannot read signing key file %s", signing_key_file);
			goto fail1;
		}

		estat = gdp_gcl_open_info_set_signing_key(info, skey);
		EP_STAT_CHECK(estat, goto fail1);
	}

	// open a GCL with the provided name
	gdp_gcl_t *gcl = NULL;

	gdp_parse_name(gdp_log_name, gcliname);
	estat = gdp_gcl_open(gcliname, GDP_MODE_AO, info, &gcl);
	EP_STAT_CHECK(estat, goto fail1);

	mosquitto_run(mqtt_addr, mqtt_port, mqtt_topic, mqtt_qos, gcl);
	gdp_gcl_close(gcl);

fail1:
	if (info != NULL)
		gdp_gcl_open_info_free(info);

fail0:
	return estat;
}


EP_STAT
mqtt_to_stdout(
		const char *mqtt_addr,
		int mqtt_port,
		const char *mqtt_topic,
		int mqtt_qos)
{
	ep_dbg_cprintf(Dbg, 1, "mqtt_to_stdout\n");

	mosquitto_run(mqtt_addr, mqtt_port, mqtt_topic, mqtt_qos, NULL);
	return EP_STAT_ABORT;
}


void
usage(void)
{
	fprintf(stderr,
			"Usage: %s [-D dbgspec] [-G router_addr] [-K signing_key_file]\n"
			"\t[-M broker_addr] [-q qos] mqtt_topic gdp_log\n"
			"    -D  set debugging flags\n"
			"    -G  IP host to contact for gdp_router\n"
			"    -K  key file to use to sign GDP log writes\n"
			"    -M  IP host to contact for MQTT broker\n"
			"    -q  MQTT Quality of Service to request\n"
			"",
			ep_app_getprogname());
	exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
	EP_STAT estat;
	bool show_usage = false;
	char *mqtt_addr = "127.0.0.1";
	int mqtt_port = 1883;
	int mqtt_qos = 2;
	char *mqtt_topic = NULL;
	char *gdpd_addr = NULL;
	char *signing_key_file = NULL;
	char *gdp_log_name = NULL;
	int opt;
	char *p;

	while ((opt = getopt(argc, argv, "D:G:K:M:q:")) > 0)
	{
		switch (opt)
		{
		case 'D':
			ep_dbg_set(optarg);
			break;

		case 'G':
			gdpd_addr = optarg;
			break;

		case 'K':
			signing_key_file = optarg;
			break;

		case 'M':
			mqtt_addr = optarg;
			break;

		case 'q':
			mqtt_qos = atoi(optarg);
			break;

		default:
			show_usage = true;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (show_usage)
		usage();
	if (argc != 2)
	{
		fprintf(stderr, "missing required argument (argc = %d)\n", argc);
		usage();
	}

	mqtt_topic = argv[0];
	gdp_log_name = argv[1];

	// parse the broker information
	p = strrchr(mqtt_addr, ':');
	if (p != NULL && strchr(p, ']') == NULL)
	{
		*p++ = '\0';
		mqtt_port = atol(p);
	}

	if (strcmp(gdp_log_name, "-") != 0)
		estat = mqtt_to_log(mqtt_addr, mqtt_port, mqtt_topic, mqtt_qos,
						gdp_log_name, gdpd_addr, signing_key_file);
	else
		estat = mqtt_to_stdout(mqtt_addr, mqtt_port, mqtt_topic, mqtt_qos);

	ep_log(estat, "mosquitto_run returned");
	exit(EX_SOFTWARE);
}
