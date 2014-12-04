#!/bin/bash

echo -e '\nTests of writer-test.js and reader-test.js'
echo ''

#
# Alec Dara-Abrams
# 2014-11-06
#
# TBD: Consider adding:
#      -  tests of the -test.js versions with the -test.c versions;
#         prehaps, parameterizing the tests with the -test to be run.
#      -  tests with multiple simultaneous subscriptions and appending
#      -  tests of more reader-test options: -f, -n, and -m .
#      Remove the dependency on knowing where the gcl directory is.
#      Handle the gdpd already running issue better.
#      Diagnose the text output ordering issue for reader-test.js -s .


# Some script-global variables

# Will be > 0 if we generate errors below
TEST_01_ERRORS=0
TEST_02_ERRORS=0
TOTAL_ERRORS=0

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.


# Parse command line arguments right here:

function show_usage ()
{
   echo -e "Usage: ${0}  [-?hv]"
   echo -e "        -? | -h  - this usage message"
   echo -e "        -v       - even more verbose output"
}


# Initialize argument-dependent script global variables
output_file=""  # not currently used
verbose=0

while getopts "h?vf:" opt; do
    case "$opt" in
    h|\?)
        show_usage
        exit 0
        ;;
    v)  verbose=1
        ;;
    f)  output_file=$OPTARG  # not currently used
        ;;
    esac
done
shift $((OPTIND-1))
[ "$1" = "--" ] && shift
# DEBUG - keep around for a while
# echo "verbose=$verbose, output_file='$output_file', Leftovers: $@"


if [ $verbose = 1 ]; then
echo -e '{\c'
date
echo ''
fi

cat << 'EOF1'
# These tests exercise the above JS GDP test programs.  The tests here
# are NOT completely automatic giving, say, a Passed/Not Passed disposition.
#
# User interaction is required to interpret the results by manually
# checking the consistency of the output of programs run during this test
# with pre-recorded known good output files.  See the result of diffs below.
# Look for "ERROR".
#
# Caveats:
# Because of differences in stdout interleaving with JS console.log() we
# often get spurious differences between our know good output files and
# actual test runs. Oddly, we get fewer errors (differences in the diffs below)
# with verbose output ( -v option). TBD
#
EOF1

if [ $verbose = 1 ]; then
cat << 'EOF2'
# Here's a reminder of writer-test.js and reader-test.js command line interface
#
# Usage:
# node ./writer-test.js [-a] [-D dbgspec] [-G gdpd_addr] <gcl_name>
#
# Usage:
# node ./reader-test.js [-D dbgspec] [-f firstrec] [-G gdpd_addr] [-m] [-n nrecs] [-s] <gcl_name>
EOF2
fi

# Also note that the order in which the output (to stdout and stderr) from
# these JS programs running on Node.js is different depending on whether
# the output destination is a terminal (ordering looks good) or a file
# (scrambled order).  This is in spite of judicious flushing of stdxxx within
# the JS.
#
# If differences DO arise the ordering of output should be checked as well as
# the actual content of the lines of output.  If only the ordering due to
# variation of buffering changes, perhaps, a new known good file should be
# generated.

# for the curious or for when this test script needs debugging
## set -o verbose
## set -o xtrace


# Helpful functions
# Change all the timestamps in file ${1} to a normalized form (i.e. a contant
# string).
# Used to remove the effect of differing timestamps when comparing gcl listings.
# Note the dependence on the exact format of the timestamp - Z at its end.
function remove_timestamp ()
{
   sed -e 's/timestamp.*Z/timestamp-removed/g' ${1}
}


if [ $verbose = 1 ]; then
echo -e '\nWe assume that we are running in gdp/lang/js/test/'
echo 'pwd = '`pwd`;
fi

GCL_DIR='/var/tmp/gcl/'
if [ $verbose = 1 ]; then
echo -e '\nWe assume the local GCL directory is in the directory: '${GCL_DIR}
echo ''
echo "ls -ld ${GCL_DIR}"
ls -ld ${GCL_DIR}
echo ''

# TBD: check for a GDP daemon instance running and do something reasonable
echo -e 'We assume a GDP daemon (gdpd) is NOT currently running.'
echo -e 'This program will generate gdp-related error output if one is running.\n'
fi


# Virtual Window A: -----------------------------------
# (The reader is invited to extend this script to open these "virtual"
# windows in separate real (e.g., Terminal or iTerm) windows -- without
# the experience for the tester being a blur of window system activity.
# And, of course, cleaning up and closing windows appropriately.)
if [ $verbose = 1 ]; then
echo 'First, we start a GDP daemon in the background'
fi
## You might want to add the -D debug option to monitor gdpd activity.
## ( ../../../gdpd/gdpd -D'*=30' ) &
../../../gdpd/gdpd &
gdpd_PID=$!
# stash gdpd's PID to be able to terminate it when we're done.
if [ $verbose = 1 ]; then
echo 'gdpd.PID = '${gdpd_PID}
echo ''
fi


echo '{ ===================================================================='
echo 'Test 1: writer-test.js creates a new gcl with a number of records;'
echo '        reader-test.js reads it back'


# Virtual Window B: -----------------------------------
TEST_01_GCL_NAME_BASE='test_gcl_01'
UNIQUE_SFX=  # just use the constant gcl name just above for now
##UNIQUE_SFX="_`date '+%Y-%m-%d:%H-%M.%s'`"
TEST_01_GCL_NAME="${TEST_01_GCL_NAME_BASE}${UNIQUE_SFX}"
T01_FPFX='wr-test_01_'
TEST_01_WINPUT_FILE=${T01_FPFX}'Winput.txt'
TEST_01_WOUTPUT_FILE=${T01_FPFX}'Woutput.tmp'
TEST_01_ROUTPUT_FILE=${T01_FPFX}'Routput.tmp'
# Our know good output files -- generated manually
TEST_01_WOUTPUT_GOOD_FILE=${T01_FPFX}'Woutput_Good.txt'
TEST_01_ROUTPUT_GOOD_FILE=${T01_FPFX}'Routput_Good.txt'

# We just KNOW this below corresponds to TEST_01_GCL_NAME_BASE='test_gcl_01'
TEST_01_GCL_INT_NAME='OMz-prG_B0mLFA7nxoshWyB5S2MvgMBHBOkHeX7Kn5A'

echo "  New GCL:"
echo "    Readable name:      \"${TEST_01_GCL_NAME_BASE}\""
echo "    Base64Encoded name: \"${TEST_01_GCL_INT_NAME}\""
echo ''

# Do some cleanup from a possible previous run of this test script:

# GDP cleanup:
## Notes:
## Would be nice to do an rm like:
##   rm -f "${GCL_DIR}${TEST_01_GCL_INT_NAME}"'*'
## or
##   rm -f "${GCL_DIR}${TEST_01_GCL_INT_NAME}*"
## but we seem not to be able to get shell globbing wrt GCL_DIR on the '*'
## no matter how we include it here in the arg to rm.
## Well, OK, here is the handstand we do to get the * into the right shell env
##   (cd ${GCL_DIR}; pwd; ls -al -- *.data); pwd
## For now we just explicitly remove the current gcl implementation's gcl files:
rm -f "${GCL_DIR}${TEST_01_GCL_INT_NAME}.data"
rm -f "${GCL_DIR}${TEST_01_GCL_INT_NAME}.index"

# Test cleanup of debris from previous run:
rm -f ${TEST_01_WOUTPUT_FILE}; rm -f ${TEST_01_ROUTPUT_FILE}

# echo 'Note, we are providing the explicit -D and -G options for this test;'
# echo 'the values here are just the current default values.'
if [ $verbose = 1 ]; then
echo "Running: node ../apps/writer-test.js ${TEST_01_GCL_NAME}"
echo "cat ${TEST_01_WINPUT_FILE} | \\"
echo "node ../apps/writer-test.js -D '*=40' -G '127.0.0.1:2468' ${TEST_01_GCL_NAME} \\"
echo "     > ${TEST_01_WOUTPUT_FILE} 2>&1"
fi
cat ${TEST_01_WINPUT_FILE} | \
node ../apps/writer-test.js -D '*=40' -G '127.0.0.1:2468' ${TEST_01_GCL_NAME} \
     > ${TEST_01_WOUTPUT_FILE} 2>&1
if [ $verbose = 1 ]; then
echo ''

echo "Running: node ../apps/reader-test.js ${TEST_01_GCL_NAME}"
echo "node ../apps/reader-test.js -D '*=40' -G '127.0.0.1:2468' ${TEST_01_GCL_NAME} \\"
echo "     > ${TEST_01_ROUTPUT_FILE} 2>&1"
fi
node ../apps/reader-test.js -D '*=40' -G '127.0.0.1:2468' ${TEST_01_GCL_NAME} \
     > ${TEST_01_ROUTPUT_FILE} 2>&1
if [ $verbose = 1 ]; then
echo ''

echo -e 'Now compare output from the runs above with known good versions.'
echo -e 'These two diffs might show differences because of odd file output'
echo -e 'versus terminal output buffering differences of these Node.js JS'
echo -e 'programs.  If unexpected differences do appear, verify the programs'
echo -e 'manually inside of separate Terminal windows.  See comments within'
echo -e 'this script for more details.\n'
fi

echo '>>>>>>>> Checking writer-test.js output'
file1="${TEST_01_WOUTPUT_FILE}"
file2="${TEST_01_WOUTPUT_GOOD_FILE}"
echo diff \
   ${file1} \
   ${file2}
diff \
   <(remove_timestamp ${file1}) \
   <(remove_timestamp ${file2})
if [ $? -eq 0 ]; then
   echo "Identical Files: ${file1} and ${file2}"
elif [ $? -eq 1 ]; then
   echo "ERROR: Differing Files: ${file1} and ${file2}"
   TEST_01_ERRORS=$(( TEST_01_ERRORS + 1 ))
else
   echo "ERROR: There was an error executing the above diff command"
fi
echo ''

echo '>>>>>>>> Checking reader-test.js output'
file1="${TEST_01_ROUTPUT_FILE}"
file2="${TEST_01_ROUTPUT_GOOD_FILE}"
echo diff \
   ${file1} \
   ${file2}
diff \
   <(remove_timestamp ${file1}) \
   <(remove_timestamp ${file2})
if [ $? -eq 0 ]; then
   echo "Identical Files: ${file1} and ${file2}"
elif [ $? -eq 1 ]; then
   echo "ERROR: Differing Files: ${file1} and ${file2}"
   TEST_01_ERRORS=$(( TEST_01_ERRORS + 1 ))
else
   echo "ERROR: There was an error executing the above diff command"
fi
echo ''

echo "Test 1: Done >>>>>>>> ${TEST_01_ERRORS} errors were found. <<<<<<<<"
echo '====================================================================== }'
echo ''


echo '{ ===================================================================='
echo 'Test 2: reader-test.js subscribes to an existing gcl'
echo '        writer-test.js appends to that existing gcl'


# Virtual Window A: -----------------------------------
# We still have a gdp daemon running in Virtual Window A


# Virtual Window C: -----------------------------------
# We use the same GCL we created in Test 1 above to append to.
TEST_02_GCL_NAME=${TEST_01_GCL_NAME}
# We use and generate different files from Test 1 above for Test 2 here
T02_FPFX='wr-test_02_'
TEST_02_WINPUT_FILE=${T02_FPFX}'Winput.txt'
# These intermediate "log" files can be buffer/stdxxx scrambled by console.log()
TEST_02_WAPPOUTPUT_FILE=${T02_FPFX}'WAPPoutput.tmp'
TEST_02_RSUBOUTPUT_FILE=${T02_FPFX}'RSUBoutput.tmp'
TEST_02_RALLOUTPUT_FILE=${T02_FPFX}'RALLoutput.tmp'
# Our know good output files -- generated manually
TEST_02_WAPPOUTPUT_GOOD_FILE=${T02_FPFX}'WAPPoutput_Good.txt'
TEST_02_RSUBOUTPUT_GOOD_FILE=${T02_FPFX}'RSUBoutput_Good.txt'
TEST_02_RALLOUTPUT_GOOD_FILE=${T02_FPFX}'RALLoutput_Good.txt'

# We just KNOW this below corresponds to TEST_01_GCL_NAME_BASE='test_gcl_01'
# Same as for Test 1
TEST_02_GCL_INT_NAME=${TEST_01_GCL_INT_NAME}

echo "  Existing GCL:"
echo "    Readable name:      \"${TEST_01_GCL_NAME_BASE}\""
echo "    Base64Encoded name: \"${TEST_02_GCL_INT_NAME}\""
echo ''

# Do some cleanup from a possible previous run of this test script:
# No GDP cleanup -- we want to append to the GCL we created in Test 1 above.

# Test cleanup:
# Remove temporary working files; known good files hang around permanently
# and, in fact, are read-only.
rm -f ${TEST_02_WAPPOUTPUT_FILE}
rm -f ${TEST_02_RSUBOUTPUT_FILE}
rm -f ${TEST_02_RALLOUTPUT_FILE}

## if [ $verbose = 1 ]; then
## fi
if [ $verbose = 1 ]; then
echo ''
echo "Running: node ../apps/reader-test.js -s ${TEST_02_GCL_NAME} in background"
echo "node ../apps/reader-test.js -s ${TEST_02_GCL_NAME} \\"
echo "     > ${TEST_02_RSUBOUTPUT_FILE} 2>&1 &"
fi
node ../apps/reader-test.js -s ${TEST_02_GCL_NAME} \
     > ${TEST_02_RSUBOUTPUT_FILE} 2>&1 \
&
readerSubscribe_PID=$!
if [ $verbose = 1 ]; then
echo 'readerSubscribe.PID = '${readerSubscribe_PID}
ps -p ${readerSubscribe_PID}
# We want to gracefully stop reader-test.js so save its PID.
# However, we seem to have problems -- see kill_readerSubscribe_output below.
echo ''
fi

append_delay_in_seconds=0  # rate of appending is likely not an issue.
if [ $verbose = 1 ]; then
echo "Running: node ../apps/writer-test.js -a ${TEST_02_GCL_NAME}"
# We slow the appends down -- we think reader-test.js may not be able to
# keep up with writer-test.js appending as fast as node and the OS will
# let it execute. Result: speed of appends seems not to be an issue;
# reader-test.js writing to a file is still out of order no matter how
# slowly the appends occur - writing to a terminal, though, is just fine.
# TBD: fake terminal output

echo "cat ${TEST_02_WINPUT_FILE} | \\"
echo "node ../apps/writer-test.js -a ${TEST_02_GCL_NAME} \\"
echo "     > ${TEST_02_WAPPOUTPUT_FILE} 2>&1"
fi
cat ${TEST_02_WINPUT_FILE} | \
( while read LINE
do      # put out lines with delays inbetween
if [ $verbose = 1 ]; then
   echo $LINE;
fi
   sleep ${append_delay_in_seconds} 
done
) | \
node ../apps/writer-test.js -a ${TEST_02_GCL_NAME} \
     > ${TEST_02_WAPPOUTPUT_FILE} 2>&1
if [ $verbose = 1 ]; then
echo ''
fi

if [ $verbose = 1 ]; then
echo ''
echo 'We now kill the reader subscribe process, PID='${readerSubscribe_PID}
ps -p ${readerSubscribe_PID}
fi
kill_readerSubscribe_output="$( kill ${readerSubscribe_PID} 2>&1 )"
if [ -n "${kill_readerSubscribe_output}" ]; then 
   # some kind of a problem with the kill
   # kill_readerSubscribe_output="./writer_reader-test.js_tests_with_v_switch.sh: line 416: kill: (12748) - No such process"
   echo -e "kill_readerSubscribe_output=\"${kill_readerSubscribe_output}\""
fi

# An issue with reliably testing JS programs running on Node.js:
#
# reader-test.js output to a file is different from
# its output to a terminal  On output to a file, lines can be scrambled
# out of order or even on top of one another.  Output to a terminal
# is in the order of the actual writes.
# This makes reliable file-based testing impossible until we can devise
# a way to control (i.e., force line buffering) for file output.
# Note, we have used fflush() in the JS after, we believe, every write
# we perform; we still get this out of order behavior for output to a file.

# Now end the reader-test.js process before we end gdpd but, first, delay
# a little to let reader-test.js see writer-test.js's last append.
# As noted above, the rate of appending is likely not an issue.
sleep 0

if [ $verbose = 1 ]; then
echo ''
echo 'Check to see if all the appends have been made to the gcl.'
echo 'Read the newly appended gcl starting with the first record (-f 1)'
echo "Running: node ../apps/reader-test.js -f 1 ${TEST_02_GCL_NAME}"
echo "node ../apps/reader-test.js -f 1 ${TEST_02_GCL_NAME} \\"
echo "     > ${TEST_02_RALLOUTPUT_FILE} 2>&1"
fi
node ../apps/reader-test.js -f 1 ${TEST_02_GCL_NAME} \
     > ${TEST_02_RALLOUTPUT_FILE} 2>&1 


if [ $verbose = 1 ]; then
echo -e 'Now compare output from the runs above with known good versions.'
echo -e 'These two diffs might show spurious differences because of buffering oddities.'
echo -e 'See the comments above in Test 1 and within this script.\n'
fi

echo '>>>>>>>> Checking writer-test.js -a output'
file1="${TEST_02_WAPPOUTPUT_FILE}"
file2="${TEST_02_WAPPOUTPUT_GOOD_FILE}"
echo diff  \
   ${file1} \
   ${file2}
echo ''
diff  \
   <(remove_timestamp ${file1}) \
   <(remove_timestamp ${file2})
if [ $? -eq 0 ]; then
   echo "Identical Files: ${file1} and ${file2}"
elif [ $? -eq 1 ]; then
   echo "ERROR: Differing Files: ${file1} and ${file2}"
   TEST_02_ERRORS=$(( TEST_02_ERRORS + 1 ))
else
   echo "ERROR: There was an error executing the above diff command"
fi
echo ''

echo '>>>>>>>> Checking reader-test.js -s output'
echo 'This diff is especially likely to show spurious buffering-related differences.'
file1="${TEST_02_RSUBOUTPUT_FILE}"
file2="${TEST_02_RSUBOUTPUT_GOOD_FILE}"
echo diff  \
   ${file1} \
   ${file2}
echo ''
diff  \
   <(remove_timestamp ${file1}) \
   <(remove_timestamp ${file2})
if [ $? -eq 0 ]; then
   echo "Identical Files: ${file1} and ${file2}"
elif [ $? -eq 1 ]; then
   echo "ERROR: Differing Files: ${file1} and ${file2}"
   TEST_02_ERRORS=$(( TEST_02_ERRORS + 1 ))
else
   echo "ERROR: There was an error executing the above diff command"
fi
echo ''

echo -e 'Now compare output from the reading the fully appended gcl with a'
echo -e 'known good version.'
echo ''
echo '>>>>>>>> Checking reader-test.js output reading the newly appended gcl'
echo diff  \
   ${TEST_02_RALLOUTPUT_FILE} \
   ${TEST_02_RALLOUTPUT_GOOD_FILE}
diff  \
   <(remove_timestamp ${TEST_02_RALLOUTPUT_FILE}) \
   <(remove_timestamp ${TEST_02_RALLOUTPUT_GOOD_FILE})
if [ $? -eq 0 ]; then
   echo "Identical Files: ${file1} and ${file2}"
elif [ $? -eq 1 ]; then
   echo "ERROR: Differing Files: ${file1} and ${file2}"
   TEST_02_ERRORS=$(( TEST_02_ERRORS + 1 ))
else
   echo "ERROR: There was an error executing the above diff command"
fi
echo ''


echo "Test 2: Done >>>>>>>> ${TEST_02_ERRORS} errors were found. <<<<<<<<"
if [ "${TEST_02_ERRORS}" -gt 0 ]; then
echo 'These may be spurious differences because of stdout/stderr buffering oddities.'
fi
echo '====================================================================== }'
echo ''


# Virtual Window A: -----------------------------------
if [ $verbose = 1 ]; then
echo ''
echo 'We now kill the GDP daemon, PID='${gdpd_PID}; echo ''
fi
## killall gdpd    # use this if we have trouble killing using gdpd_PID
kill_gdpd_output="$( kill ${gdpd_PID} 2>&1 )"
if [ -n "${kill_gdpd_output}" ]; then 
   # some kind of a problem with the kill
   echo -e "kill_gdpd_output=\"${kill_gdpd_output}\""
   # TBD: even though kill_gdpd_output seems to be null, we still get output
   # to stdout or stderr as this script stops:
   #   {......
   #   Tue Dec  2 00:16:33 PST 2014
   #   ./writer_reader-test.js.sh: line 452: 10679 Terminated: 15          ../../../gdpd/gdpd
   #   }
fi

TOTAL_ERRORS=$(( TEST_01_ERRORS + TEST_02_ERRORS ))
echo ">>>>>>>> There were ${TOTAL_ERRORS} total errors found. <<<<<<<< "
echo ''

if [ $verbose = 1 ]; then
date; echo '}'
fi

# Return an indication of success or failure in our exit code
exit "${TOTAL_ERRORS}" ;

