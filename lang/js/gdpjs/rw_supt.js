/* vim: set ai sw=4 sts=4 ts=4 : */

// Node.js Javascript support routines for reading and writing gcl's
//
// Used by:
//    writer-test.js -- Node.js Javascript version of gdp/apps/writer-test.c
//    reader-test.js -- Node.js Javascript version of gdp/apps/reader-test.c


// ========================================================================
// Example set ups for calls to write_gcl_records()
//
// A: write to gcl from stdin
// recsrc = -1;  // read the gcl records to be written from stdin with
//               // prompts to and echoing for the user on stdout
// recarray = [ ];
//
// B: write to gcl from JS Array
// recsrc =  0;  // read the gcl records from the Array recarray
// recarray = [ "Item 01 - from recarray", "Item 02", "Item 03" ];
//
// C: write to gcl N records with integers as contents
// recsrc >  0;  // write recsrc records with automatically generated
//               // content: the integers starting at 1 and going up to
//               // recsrc, inclusive.
// recsrc   =  7;
// recarray = [ ];
//
// Finally, call the function:
// recsrc   = -1;   // read the gcl records to be written from stdin...
// recarray = [ ];  // not used for recsrc = -1
// write_gcl_records( gdpd_addr, gcl_name, gcl_append, recsrc, recarray );


/*  TBD: return something like { error_code: EP_STAT, gcl_name: String } */
function write_gcl_records( gdpd_addr, gcl_name, gcl_append, recsrc, recarray )

// gdpd_addr    gdp daemon's <host:port>; if null, use default "127.0.0.1:2468"
// gcl_name     if gcl_append is true, name of existing GCL; 
//              if gcl_append is false, ignored.  A new GCL will be created.
// gcl_append   Boolean: append to an existing GCL
// recsrc = -1  read the gcl records to be written from stdin with
//              prompts to and echoing for the user on stdout
// recsrc =  0  read the gcl records from the Array recarray
// recsrc >  0  write recsrc records with automatically generated
//              content: the integers starting at 1 and going up to
//              recsrc, inclusive.
// Note, there still may be undesired output via console.log() and
// console.error(). TBD
{	
	// internal variables for historical reasons
	var xname   = gcl_name;
	var append  = gcl_append;
	var numrecs = recsrc;

	// Local working variables
	var gcl_Ptr; // gclh
	var estat;   // EP_STAT
	var gcliname = ref.alloc(gcl_name_t);
	// Note, buf is re-allocated below for each item string read from stdin
	var buf      = new buf_t(10);


	estat = gdp_init_js( /* String */ gdpd_addr );
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

	// allow thread to settle to avoid interspersed debug output
	sleep.sleep(1); // needed only for stdout and stderr

	if ( xname == null )
	{
		// TBD: check signature
		var gclcreaterv = gdp_gcl_create_js( null );
		estat   = gclcreaterv.error_code;
		gcl_Ptr = gclcreaterv.gclH;
	}
	else
	{
		gdp_gcl_parse_name_js( xname, gcliname );
	
		if (append)
		{
			// TBD: check signature
			var gclopenrv = gdp_gcl_open_js(gcliname, GDP_MODE_AO );
			estat   = gclopenrv.error_code;
			gcl_Ptr = gclopenrv.gclH;
		}
		else
		{
			// TBD: check signature
			var gclcreaterv = gdp_gcl_create_js( gcliname );
			estat   = gclcreaterv.error_code;
			gcl_Ptr = gclcreaterv.gclH;
		}
	}
    // TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

	// TBD: don't send to stdout, return gcl string name to caller.
	gdp_gcl_print_stdout_js( gcl_Ptr, 0, 0 );

	var datum;
	datum = gdp_datum_new_js();
		
	if ( numrecs < 0 )
	{
		// read records from stdin, prompting & echoing to user on stdout

		console.log( "\nStarting to read input - ^D to end" );
		var rvget;  /* String */
		// really a dummy for gets's parameter; we ignore its value.
		// Just trying to avoid possible buffer overruns inside gets().
		var strbuf = new Array( 200 + 1 ).join( " " );  // long enough??

		while ( (rvgets = libc.gets( strbuf ) ) != null )
		{
			var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
			for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
			buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
			console.log( "Got input %s%s%s", "<<", rvgets, ">>" );

			estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
			// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

			gdp_datum_print_stdout_js( datum );

		} /* end while */
	}
	else if ( numrecs > 0 )
	{
		// generate numrecs records with contents = integers 1 to numrecs.

		for ( var crec = 1; crec <= numrecs; crec++ )
		{
			var rvgets;  /* String */
			rvgets = crec.toString();

			var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
			for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
			buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
			console.log( "Got input %s%s%s", "<<", rvgets, ">>" );

			estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
			// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

			gdp_datum_print_stdout_js( datum );
		}
	}
	else
	{
		// write contents of recarray to the gcl
		
		for ( var crec = 0; crec < recarray.length; crec++ )
		{
			var rvgets;  /* String */
			rvgets = recarray[crec].toString();

			var buf   = new buf_t(rvgets.length+1);  // we'll tack on a \0
			for ( var i = 0; i < rvgets.length; i++ )
			{  buf[i] = rvgets.charCodeAt(i);  // not sure if really necessary
			}
			buf[rvgets.length] = 0;  // Hopefully, interpreted in C as \0
			console.log( "Got input %s%s%s", "<<", rvgets, ">>" );

			estat = gdp_gcl_publish_buf_js( gcl_Ptr, datum, buf );
			// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

			gdp_datum_print_stdout_js( datum );
		}
	}

	gdp_datum_free_js( datum );

	estat = gdp_gcl_close_js( gcl_Ptr );

	// TBD: fix this error return - see corresponding location in writer-test.js
	// string.repeat not available for us here in ECMASscript<6
	var str = new Array( 200 + 1 ).join( " " );  // long enough??
	console.error( "exiting with status %s",
					ep_stat_tostr_js(estat, str, str.length) );
	return ( ! ep_stat_isok_js(estat) );

} /* end function write_gcl_records() */



// ========================================================================
// Example set ups for calls to read_gcl_records()
//
// A: read gcl and write to stdout
// recdest = -1;  // writes the gcl records to stdout with
//                // readable formatting
//
// B: read the gcl into a JS Array and return the Array
// recdest =  0;  // read the gcl records into the Array {... records: }
//               TBD: Not Yet Implemented
//
// Finally, call the function:
// Returns:
// { error_code: EP_STAT, records: Array of Strings }
// recdest = -1;  // writes the gcl records to stdout...
// read_gcl_records( gdpd_addr, gcl_name,
//                   gcl_firstrec, gcl_numrecs,
//                   gcl_subscribe, gcl_multiread, recdest
//                 );
//
// Note, read_gcl_records() calls:
//   do_multiread(gcl_Ptr, gcl_firstrec, gcl_numrecs, gcl_subscribe);
//   do_simpleread(gcl_Ptr, gcl_firstrec, gcl_numrecs);
// TBD: These functions are currently only in reader-test.js .

/* TBD: { error_code: EP_STAT, records: Array of Strings } */
function read_gcl_records( gdpd_addr, gcl_name,
                           gcl_firstrec, gcl_numrecs,
                           gcl_subscribe, gcl_multiread, recdest
                         )

// gdpd_addr     gdp daemon's <host:port>; if null, use default "127.0.0.1:2468"
// gcl_name      name of existing GCL 
// gcl_firstrec, gcl_numrecs, gcl_subscribe, gcl_multiread
//               as for reader-test.js -f, -n, -s and -m cmd line options
// recdest = -1  writes the gcl records to stdout with readable formatting
// recdest =  0  read the gcl records into the return value's Array { records: }
//               TBD: Not Yet Implemented
// Note, there still may be undesired output via console.log() and
// console.error(). TBD
{
		// Local working variables
		var gcl_Ptr; // gclh
		var estat;   // EP_STAT
		var gclname  = new gcl_name_t(32);
		var gclpname = new gcl_pname_t(GDP_GCL_PNAME_LEN);


		estat = gdp_init_js( /* String */ gdpd_addr );
		// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

		// allow thread to settle to avoid interspersed debug output
		sleep.sleep(1); // needed only for stdout and stderr

		estat = gdp_gcl_parse_name_js( gcl_name, gclname );
		// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

		var rv_str = gdp_gcl_printable_name_js( gclname, gclpname );
		console.log( "Reading GCL %s", array_to_String(gclpname) );

		// TBD: is this ref.alloc() necessary?
        var gclPtrPtr = ref.alloc( gdp_gcl_tPtrPtr );
		// TBD: check signature
		var gclopenrv = gdp_gcl_open_js(gclname, GDP_MODE_RO, gclPtrPtr);
		estat   = gclopenrv.error_code;
		gcl_Ptr = gclopenrv.gclH;
		// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

		if (gcl_subscribe || gcl_multiread)
		{
			estat = do_multiread(gcl_Ptr, gcl_firstrec, gcl_numrecs, gcl_subscribe);
		}
		else
		{
			estat = do_simpleread(gcl_Ptr, gcl_firstrec, gcl_numrecs);
		}
		// TBD: check for errors:  if ( ! ep_stat_isok_js(estat) )

		gdp_gcl_close_js( gcl_Ptr );

		// TBD: fix this error return - see corresponding location in reader-test.js
		// string.repeat not available for us here in ECMASscript<6
		var str = new Array( 200 + 1 ).join( " " );  // long enough??
		console.error( "exiting with status %s",
			           ep_stat_tostr_js(estat, str, str.length) );
		fflush_all_js();  // sometimes Node.js may not empty buffers
		return ( ! ep_stat_isok_js(estat) );

} /* end read_gcl_records( ) */


// ========================================================================

