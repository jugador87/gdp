#include <errno.h>
#include <getopt.h>
#include <sysexits.h>

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <gdp/gdp.h>
#include <jansson.h>

/*
 * Note: Much of the following code is borrowed from `apps/gdp-reader.c`,
 * `apps/gdp-writer.c`.
 */

void usage(void) {
  fprintf(stderr,
          "Usage: %s [-h] motion_log light_log\n"
          "    -h  Print usage\n"
          "    -G  IP host to contact for gdp_router\n",
          ep_app_getprogname());
  exit(EX_USAGE);
}

void motion_callback(gdp_event_t *gev) {
  // Check the data item and detect motion.
  if (gdp_event_gettype(gev) != GDP_EVENT_DATA) return;

  gdp_datum_t* datum = gdp_event_getdatum(gev);
  gdp_datum_print(datum, stdout, GDP_DATUM_PRQUIET);

  gdp_buf_t *b = gdp_datum_getbuf(datum);
  size_t l = gdp_buf_getlength(b);
  json_t *json = json_loadb((const char *)gdp_buf_getptr(b, l), l, 0, NULL);
  if (json == NULL || !json_is_object(json)) {
    ep_app_error("Cannot load JSON data");
    exit(EX_DATAERR);
  }

  json_t *jval = json_object_get(json, "value");
  if (jval == NULL || !json_is_boolean(jval)) {
    ep_app_error("Invalid JSON data");
    exit(EX_DATAERR);
  }

  gdp_gcl_t *o_gcl = (gdp_gcl_t *) gdp_event_getudata(gev);

  // Motion detected if true!
  if (json_is_true(jval)) {
    // We append the same datum (which is a JSON with value true.
    gdp_gcl_append(o_gcl, datum);
  }

  json_decref(jval);
  json_decref(json);
  gdp_event_free(gev);
}

int main(int argc, char** argv) {
  EP_STAT estat;
  int opt;
  char *gdpd_addr = NULL;
  gdp_name_t gclname;
  gdp_pname_t gclpname;
  gdp_gcl_t *i_gcl;
  gdp_gcl_t *o_gcl;

  while ((opt = getopt(argc, argv, "h:G")) > 0) {
    switch (opt) {
      case 'G':
        // set the port for connecting to the GDP daemon
        gdpd_addr = optarg;
        break;
      case 'h':
      default:
        usage();
        break;
    }
  }
  if (argc <= 2) { usage(); }

  // Step 1: Initialize the GDP library
  estat = gdp_init(gdpd_addr);
  if (!EP_STAT_ISOK(estat)) {
    ep_app_error("GDP initialization failed:\n\tconnecting to %s", gdpd_addr);
    exit(EX_DATAERR);
  }

  // Step 2: Parse the input GLC name.
  estat = gdp_parse_name(argv[0], gclname);
  if (!EP_STAT_ISOK(estat)) {
    ep_app_fatal("Illegal GCL name syntax:\n\t%s", argv[0]);
    exit(EX_USAGE);
  }

  // Step 3: Open the input GCL in Read-only mode.
  gdp_printable_name(gclname, gclpname);
  fprintf(stderr, "Reading GCL %s\n", gclpname);
  estat = gdp_gcl_open(gclname, GDP_MODE_RO, NULL, &i_gcl);
  if (!EP_STAT_ISOK(estat)) {
    char sbuf[100];
    ep_app_error("Cannot open GCL:\n\t%s",
                 ep_stat_tostr(estat, sbuf, sizeof sbuf));
    exit(EX_SOFTWARE);
  }

  // Step 4: Parse the output GLC name.
  estat = gdp_parse_name(argv[1], gclname);
  if (!EP_STAT_ISOK(estat)) {
    ep_app_fatal("Illegal GCL name syntax:\n\t%s", argv[1]);
    exit(EX_USAGE);
  }

  // Step 5: Open the output GCL in append-only mode.
  gdp_printable_name(gclname, gclpname);
  fprintf(stderr, "Reading GCL %s\n", gclpname);
  estat = gdp_gcl_open(gclname, GDP_MODE_AO, NULL, &o_gcl);
  if (!EP_STAT_ISOK(estat)) {
    char sbuf[100];
    ep_app_error("Cannot open GCL:\n\t%s",
                 ep_stat_tostr(estat, sbuf, sizeof sbuf));
    exit(EX_SOFTWARE);
  }

  // Step 6: Subscribe to the log with a callback function, with the output GCL
  // as data.
  estat = gdp_gcl_subscribe(i_gcl, 1, -1, NULL, motion_callback, (void *)o_gcl);
  if (!EP_STAT_ISOK(estat)) {
    char sbuf[100];
    ep_app_error("Cannot subscribe:\n\t%s",
                 ep_stat_tostr(estat, sbuf, sizeof sbuf));
    exit(EX_SOFTWARE);
  }

  gdp_gcl_close(i_gcl);
  gdp_gcl_close(o_gcl);

  return !EP_STAT_ISOK(estat);
}
