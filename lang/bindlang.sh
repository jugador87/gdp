#!/bin/sh

pathsearch() {
	val=""
	#echo Searching $1 for $2
	for i in $1
	do
		if [ -e $i$2 ]
		then
			val=$i$2
			break;
		fi
	done
	#echo Result is $val
}

# pre-initialize some optional variables
XTRAINC=""

LANG=${1-none}
if [ "$LANG" = "none" ]
then
	echo Usage: $0 language
	exit 1
fi
shift

# create a directory for it if it doesn't already exist
if [ ! -d $LANG ]
then
	mkdir $LANG
fi

# work in that directory
cd $LANG

# find /opt/local or /usr/local
pathsearch "/usr/local /opt/local"
OPTDEF=-D_OPT_=$val

# find python include files
if [ "$LANG" = "python" ]
then
	pypath=""
	for i in /usr/include $OPTDEF/include
	do
		for j in python python2.7 python2.6
		do
			pypath="$pypath $i/$j"
		done
	done
	pathsearch "$pypath"
	XTRAINC="-D_XTRA_INCLUDES_=-I$val"
fi

# create the Makefile
if [ ! -f Makefile ]
then
	echo m4 -D_LANG_=$LANG $OPTDEF $XTRAINC ../Makefile.m4
	m4 -D_LANG_=$LANG $OPTDEF $XTRAINC ../Makefile.m4 > Makefile
	chmod 444 Makefile
fi

# run the Makefile
make $*
