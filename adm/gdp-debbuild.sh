#!/bin/sh

# staging area for building the package
stage=$HOME/gdp-stage

# email address for maintainer
email=$USER@cs.berkeley.edu

# address of repository
repo=repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git

# root of source tree
root=UNKNOWN



Reset='\033[0m'    # Text Reset

# Regular             Bold                  Underline             High Intensity        BoldHigh Intens       Background            High Intensity Backgrounds
Bla='\033[0;30m';     BBla='\033[1;30m';    UBla='\033[4;30m';    IBla='\033[0;90m';    BIBla='\033[1;90m';   On_Bla='\033[40m';    On_IBla='\033[0;100m';
Red='\033[0;31m';     BRed='\033[1;31m';    URed='\033[4;31m';    IRed='\033[0;91m';    BIRed='\033[1;91m';   On_Red='\033[41m';    On_IRed='\033[0;101m';
Gre='\033[0;32m';     BGre='\033[1;32m';    UGre='\033[4;32m';    IGre='\033[0;92m';    BIGre='\033[1;92m';   On_Gre='\033[42m';    On_IGre='\033[0;102m';
Yel='\033[0;33m';     BYel='\033[1;33m';    UYel='\033[4;33m';    IYel='\033[0;93m';    BIYel='\033[1;93m';   On_Yel='\033[43m';    On_IYel='\033[0;103m';
Blu='\033[0;34m';     BBlu='\033[1;34m';    UBlu='\033[4;34m';    IBlu='\033[0;94m';    BIBlu='\033[1;94m';   On_Blu='\033[44m';    On_IBlu='\033[0;104m';
Pur='\033[0;35m';     BPur='\033[1;35m';    UPur='\033[4;35m';    IPur='\033[0;95m';    BIPur='\033[1;95m';   On_Pur='\033[45m';    On_IPur='\033[0;105m';
Cya='\033[0;36m';     BCya='\033[1;36m';    UCya='\033[4;36m';    ICya='\033[0;96m';    BICya='\033[1;96m';   On_Cya='\033[46m';    On_ICya='\033[0;106m';
Whi='\033[0;37m';     BWhi='\033[1;37m';    UWhi='\033[4;37m';    IWhi='\033[0;97m';    BIWhi='\033[1;97m';   On_Whi='\033[47m';    On_IWhi='\033[0;107m';

warning()
{
	echo "${Yel}${On_Bla}[W] $1${Reset}"
}

error()
{
	echo "${Cya}${On_bla}[E] $1${Reset}"
}

fatal()
{
	echo "${Red}${On_Whi}[F] $1${Reset}" >&2
	exit 1
}


args=`getopt e:r:v: $*`
if [ $? != 0 ]
then
	fatal "Usage: [-e email] -r srcroot -v version XXX"
fi
set -- $args

ver=UNKNOWN
for arg
do
	case "$arg" in
	   -e)
		email=$2
		shift 2;;
	   -r)
		root=$2
		shift 2;;
	   -v)
		ver=$2
		shift 2;;
	esac
done

echo email=$email
echo root=$root
echo stage=$stage
echo ver=$ver
if [ "$ver" = "UNKNOWN" ]
then
	fatal "-v (version) flag must be specified"
fi

if [ "$root" = "UNKNOWN" ]
then
	warning "using staging area as source root"

	[ -d $stage ] || mkdir $stage || fatal "cannot access $stage"
	root=$stage
	cd $stage
	[ -e gdp-$ver ] && rm -rf gdp-$ver
	git clone --depth 1 $repo gdp-$ver
	rm -rf gdp-$ver/.git*
else
	cd $root
	[ -d gdp ] || fatal "cannot find gdp source tree"
	[ -d gdp-$ver ] && rm gdp-$ver
	ln -s gdp gdp-$ver || fatal "cannot create gdp-$ver"
fi
tar czf gdp_$ver.orig.tar.gz --exclude '.git*' gdp-$ver
cd gdp-$ver
debuild -us -uc
