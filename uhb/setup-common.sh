#!/bin/sh

#
#  Common code for setup scripts
#

# Regular           Bold                Underline           High Intensity      BoldHigh Intens     Background          High Intensity Backgrounds
Bla='[0;30m';     BBla='[1;30m';    UBla='[4;30m';    IBla='[0;90m';    BIBla='[1;90m';   On_Bla='[40m';    On_IBla='[0;100m';
Red='[0;31m';     BRed='[1;31m';    URed='[4;31m';    IRed='[0;91m';    BIRed='[1;91m';   On_Red='[41m';    On_IRed='[0;101m';
Gre='[0;32m';     BGre='[1;32m';    UGre='[4;32m';    IGre='[0;92m';    BIGre='[1;92m';   On_Gre='[42m';    On_IGre='[0;102m';
Yel='[0;33m';     BYel='[1;33m';    UYel='[4;33m';    IYel='[0;93m';    BIYel='[1;93m';   On_Yel='[43m';    On_IYel='[0;103m';
Blu='[0;34m';     BBlu='[1;34m';    UBlu='[4;34m';    IBlu='[0;94m';    BIBlu='[1;94m';   On_Blu='[44m';    On_IBlu='[0;104m';
Pur='[0;35m';     BPur='[1;35m';    UPur='[4;35m';    IPur='[0;95m';    BIPur='[1;95m';   On_Pur='[45m';    On_IPur='[0;105m';
Cya='[0;36m';     BCya='[1;36m';    UCya='[4;36m';    ICya='[0;96m';    BICya='[1;96m';   On_Cya='[46m';    On_ICya='[0;106m';
Whi='[0;37m';     BWhi='[1;37m';    UWhi='[4;37m';    IWhi='[0;97m';    BIWhi='[1;97m';   On_Whi='[47m';    On_IWhi='[0;107m';

Reset='[0m'    # Text Reset

info() {
	echo "${Gre}${On_Bla}[+] $1${Reset}"
}

warn() {
	echo "${Yel}${On_Bla}[*] $1${Reset}"
}

fatal() {
	echo "${Whi}${On_Red}[!] $1${Reset}"
	exit 1
}

OS=""
OSVER=""
INITSYS=""

#  figure out operating system and version number
if [ -f "/etc/os-release" ]; then
    . /etc/os-release
    OS="${ID-}"
    OSVER="${VERSION_ID-}"
fi
if [ -z "$OS" -a -f "/etc/lsb-release" ]; then
    . /etc/lsb-release
    OS="${DISTRIB_ID-}"
    OSVER="${DISTRIB_VERSION-}"
fi
if [ "$OS" ]; then
    # it is set --- do nothing
    true
elif [ -f "/etc/centos-release" ]; then
    OS="centos"
    #OSVER=???
elif [ -f "/etc/redhat-release" ]; then
    OS="redhat"
    OSVER=`sed -e 's/.* release //' -e 's/ .*//' /etc/redhat-release`
else
    OS=`uname -s`
fi
OS=`echo $OS | tr '[A-Z]' '[a-z]'`
if [ "$OS" = "linux" ]; then
    OS=`head -1 /etc/issue | sed 's/ .*//' | tr '[A-Z]' '[a-z]'`
fi
if [ -z "$OSVER" ]; then
    OSVER="0"
else
    # clean up OSVER to make it a single integer
    OSVER=`echo $OSVER | sed -e 's/[^0-9.]*//g' -e 's/\./00/'`
fi

# check to make sure we understand this OS release
case $OS in
  "debian")
	if expr $OSVER \< 8 > /dev/null
	then
		fatal "Must be running Debian 8 (Jessie) or later (have $VERSION_ID)"
	fi
	;;

  "ubuntu")
	if expr $OSVER \< 140004
	then
		fatal "Must be running Ubuntu 14.04 or later (have $VERSION_ID)"
	fi
	;;

    *)
	fatal "Unrecognized system $OS"
	;;
esac

# determine what init system we are using (heuristic!)
#  ... by this time we know we are on linux, so we can use non-portable tricks
if [ -z "$INITSYS" ]
then
	proc1exe=`sudo stat /proc/1/exe | grep 'File: '`
	if echo "$proc1exe" | grep -q "systemd"
	then
		INITSYS="systemd"
	else
		# this is really a shot in the dark
		INITSYS="upstart"
	fi
fi

info "System Info: OS=$OS, OSVER=$OSVER, INITSYS=$INITSYS"

sudo apt-get update
if ! ls /etc/apt/sources.list.d/mosquitto* > /dev/null 2>&1
then
	echo ""
	info "Setting up mosquitto repository"
	if [ "$OS" = "ubuntu" ]
	then
		sudo apt-get install -y \
			software-properties-common \
			python-software-properties
		sudo apt-add-repository -y ppa:mosquitto-dev/mosquitto-ppa
	elif [ "$OS" = "debian" ]
	then
		sudo apt-get install -y wget
		wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
		sudo apt-key add mosquitto-repo.gpg.key
		if expr $OSVER = 8 > /dev/null
		then
			dver="jessie"
		else
			fatal "unknown debian version $OSVER"
		fi
		cd /etc/apt/sources.list.d
		sudo wget http://repo.mosquitto.org/debian/mosquitto-$dver.list
		cd $root
	else
		fatal "Unknown linux distribution $OS"
	fi
fi
