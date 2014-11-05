// JavaScript GDP access application programs
// 2014-11-04
// Alec Dara-Abrams

// These JS programs should all be run in directory gdp/lang/js/apps/ .
// They access JS and C libraries in gdp/lang/js/gdpjs/ .

// ===================================================================
README.txt
This file.


// ===================================================================
reader-test.js

JS version of reader-test.c . It takes the same command line arguments.

Usage:
node apps/reader-test.js [-D dbgspec] [-f firstrec] [-G gdpd_addr] [-m]
				      [-n nrecs] [-s] <gcl_name>


// ===================================================================
writer-test.js
JS version of writer-test.c . It takes the same command line arguments.

Usage:
node apps/writer-test.js [-a] [-D dbgspec] [-G gdpd_addr] [<gcl_name>]


// ===================================================================
rw_supt_test.js

JS test for functions in gdpjs/rw_supt.js 

Usage: See program.


// ===================================================================
server_running_rw_supt_test.js

Node.js-based minimal HTTP server which runs rw_supt_test.js on the host
machine upon being sent an HTTP GET.

Usage: See program.


// ===================================================================
