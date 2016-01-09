#!/bin/sh

Reset='[0m'    # Text Reset

# Regular           Bold                Underline           High Intensity      BoldHigh Intens     Background          High Intensity Backgrounds
Bla='[0;30m';     BBla='[1;30m';    UBla='[4;30m';    IBla='[0;90m';    BIBla='[1;90m';   On_Bla='[40m';    On_IBla='[0;100m';
Red='[0;31m';     BRed='[1;31m';    URed='[4;31m';    IRed='[0;91m';    BIRed='[1;91m';   On_Red='[41m';    On_IRed='[0;101m';
Gre='[0;32m';     BGre='[1;32m';    UGre='[4;32m';    IGre='[0;92m';    BIGre='[1;92m';   On_Gre='[42m';    On_IGre='[0;102m';
Yel='[0;33m';     BYel='[1;33m';    UYel='[4;33m';    IYel='[0;93m';    BIYel='[1;93m';   On_Yel='[43m';    On_IYel='[0;103m';
Blu='[0;34m';     BBlu='[1;34m';    UBlu='[4;34m';    IBlu='[0;94m';    BIBlu='[1;94m';   On_Blu='[44m';    On_IBlu='[0;104m';
Pur='[0;35m';     BPur='[1;35m';    UPur='[4;35m';    IPur='[0;95m';    BIPur='[1;95m';   On_Pur='[45m';    On_IPur='[0;105m';
Cya='[0;36m';     BCya='[1;36m';    UCya='[4;36m';    ICya='[0;96m';    BICya='[1;96m';   On_Cya='[46m';    On_ICya='[0;106m';
Whi='[0;37m';     BWhi='[1;37m';    UWhi='[4;37m';    IWhi='[0;97m';    BIWhi='[1;97m';   On_Whi='[47m';    On_IWhi='[0;107m';

log() {
	echo "${Cya}[+] $1${Reset}"
}

fatal() {
	echo "${Red}[!] $1${Reset}"
	exit 1
}

platform() {
    local  __resultvar=$1
    local result=""
    local arch=`arch`

    if [ -f "/etc/lsb-release" ]; then
	. /etc/lsb-release
	result="${DISTRIB_ID-}"
    fi
    if [ "$result" ]; then
	# do nothing
	true
    elif [ -f "/etc/centos-release" ]; then
	result="centos"
    elif [ -f "/etc/redhat-release" ]; then
	result="redhat"
    else
        result=`uname -s`
    fi
    result=`echo $result | tr '[A-Z]' '[a-z]'`
    if [ "$result" = "linux" ]; then
	result=`cat /etc/issue | sed 's/ .*//' | tr '[A-Z]' '[a-z]'`
    fi

    eval $__resultvar="$result"
}

package() {
    log "Checking package $1..."
    case "$OS" in
	"ubuntu" | "debian")
	    if dpkg --get-selections | grep --quiet $1; then
		log "$1 is already installed. skipping."
	    else
		sudo apt-get install $@
	    fi
	    ;;
	"centos" | "redhat")
	    if rpm -qa | grep --quiet $1; then
		log "$1 is already installed. skipping."
	    else
		sudo yum install -y $@
	    fi
	    ;;
	"darwin")
	    if [ "$pkgmgr" = "brew" ]; then
		if brew list | grep --quiet $1; then
		    log "$1 is already installed. skipping."
		else
		    brew install --build-bottle $@ || brew upgrade $@
		fi
	    else
		if port installed $1 | grep -q "."; then
		    log "$1 is already installed. skipping."
		else
		    sudo port install $1
		fi
	    fi
	    ;;
	"freebsd")
	    export PATH="/sbin:/usr/sbin:$PATH"
	    if sudo pkg info -q $1; then
		log "$1 is already installed. skipping."
	    else
		sudo pkg install $@
	    fi
	    ;;
	"gentoo")
	    if equery list $1 >& /dev/null; then
		log "$1 is already installed. skipping."
	    else
		sudo emerge $1
	    fi
	    ;;
	*)
	    fatal "unrecognized OS $OS"
	    ;;
    esac
}

platform OS
case "$OS" in
    "ubuntu" | "debian")
	sudo apt-get update
	sudo apt-get clean
	package libevent-dev
	package libevent-pthreads
	package libssl-dev
	package lighttpd
	package libjansson-dev
	package libavahi-common-dev
	package libavahi-client-dev
	package avahi-daemon
	;;

    "darwin")
	pkgmgr=none
	if type brew > /dev/null 2>&1 && [ ! -z "`brew list`" ]; then
	    pkgmgr=brew
	    sudo brew update
	fi
	if type port > /dev/null 2>&1 && port installed | grep -q .; then
	    if [ "$pkgmgr" = "none" ]; then
		pkgmgr=port
		sudo port selfupdate
	    else
		log "You seem to have both macports and homebrew installed."
		fatal "You will have to deactivate one (or modify this script)"
	    fi
	fi
	if [ "$pkgmgr" = "none" ]; then
	    fatal "you must install macports or homebrew"
	fi
	package libevent
	package openssl
	package lighttpd
	package jansson
	package avahi
	;;

    "freebsd")
	package libevent2
	package openssl
	package lighttpd
	package jansson
	package avahi
	;;

    "gentoo" | "redhat")
	package libevent-devel
	package openssl-devel
	package lighttpd
	package jansson-devel
	package avahi-devel
	;;

    "centos")
	package epel-release
	package libevent-devel
	package openssl-devel
	package lighttpd
	package jansson-devel
	package avahi-devel
	;;

    *)
	fatal "oops, we don't support $OS"
	;;
esac

# vim: set ai sw=8 sts=8 ts=8 :
