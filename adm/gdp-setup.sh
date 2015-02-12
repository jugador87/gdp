#!/bin/sh

Reset='\033[0m'    # Text Reset

# Regular           Bold                Underline           High Intensity      BoldHigh Intens     Background          High Intensity Backgrounds
Bla='\033[0;30m';     BBla='\033[1;30m';    UBla='\033[4;30m';    IBla='\033[0;90m';    BIBla='\033[1;90m';   On_Bla='\033[40m';    On_IBla='\033[0;100m';
Red='\033[0;31m';     BRed='\033[1;31m';    URed='\033[4;31m';    IRed='\033[0;91m';    BIRed='\033[1;91m';   On_Red='\033[41m';    On_IRed='\033[0;101m';
Gre='\033[0;32m';     BGre='\033[1;32m';    UGre='\033[4;32m';    IGre='\033[0;92m';    BIGre='\033[1;92m';   On_Gre='\033[42m';    On_IGre='\033[0;102m';
Yel='\033[0;33m';     BYel='\033[1;33m';    UYel='\033[4;33m';    IYel='\033[0;93m';    BIYel='\033[1;93m';   On_Yel='\033[43m';    On_IYel='\033[0;103m';
Blu='\033[0;34m';     BBlu='\033[1;34m';    UBlu='\033[4;34m';    IBlu='\033[0;94m';    BIBlu='\033[1;94m';   On_Blu='\033[44m';    On_IBlu='\033[0;104m';
Pur='\033[0;35m';     BPur='\033[1;35m';    UPur='\033[4;35m';    IPur='\033[0;95m';    BIPur='\033[1;95m';   On_Pur='\033[45m';    On_IPur='\033[0;105m';
Cya='\033[0;36m';     BCya='\033[1;36m';    UCya='\033[4;36m';    ICya='\033[0;96m';    BICya='\033[1;96m';   On_Cya='\033[46m';    On_ICya='\033[0;106m';
Whi='\033[0;37m';     BWhi='\033[1;37m';    UWhi='\033[4;37m';    IWhi='\033[0;97m';    BIWhi='\033[1;97m';   On_Whi='\033[47m';    On_IWhi='\033[0;107m';

log() {
  echo "${Cya}[+] $1${Reset}"
}

fatal() {
  echo "${Red}[!] $1${Reset}"
  exit 1
}

platform() {
    local  __resultvar=$1
    local result
    if [ -f "/etc/redhat-release" ]; then
	result="centos"
    else
        result=`uname -s | tr '[A-Z]' '[a-z]'`
	if [ "$result" = "linux" ]; then
	    result=`cat /etc/issue | sed 's/ .*//' | tr '[A-Z]' '[a-z]'`
	fi
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
	"centos")
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
	;;

    "darwin")
	pkgmgr=none
	if type brew > /dev/null 2>&1 && [ ! -z `brew list` ]; then
	    pkgmgr=brew
	    brew update
	fi
	if type port > /dev/null 2>&1 && port installed | grep -q .; then
	    if [ "$pkgmgr" = "none" ]; then
		pkgmgr=port
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
	;;

    "freebsd")
	package libevent2
	package openssl
	package lighttpd
	package jansson
	;;

    *)
	fatal "oops, we don't support $OS"
	;;
esac
