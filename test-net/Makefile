
# Run as follows:
#   make images
#   make 


# Launch multiple docker instances for gdp-routers/log-servers in a 
#   private network for testing purposes.
# We use pipework (https://github.com/jpetazzo/pipework) to assign 
#   static IP addresses to docker instances. The convention is to use
#   172.30.0.1-255 for Routers, 172.30.1.1-255 for log-servers and 
#   172.30.0.255 for the host
# XXX: Double check the parameters below to confirm is this is true

.DEFAULT_GOAL = gdp-logds
all = gdp-logds

GDP_ROOT = ../
GDP_LOGD_DOCKERFILE = Dockerfile.gdplogd
GDP_LOGD_VERSION = 0.3-1
GDP_LOGD_DEB = gdp-server_$(GDP_LOGD_VERSION)_amd64.deb

GDP_ROUTER_ROOT = ./routia
GDP_ROUTER_DOCKERFILE = Dockerfile.gdp-router
GDP_ROUTER_VERSION = 0.3-1
GDP_ROUTER_DEB = gdp-router_$(GDP_ROUTER_VERSION)_all.deb

UNAME_S := $(shell uname -s)

PIPEWORK = ./pipework/pipework
PIPEWORK_CMD = sudo ./pipework/pipework
ifeq ($(UNAME_S),Darwin)
	PIPEWORK_CMD = boot2docker ssh $(PIPEWORK_CMD)
	PIPEWORK_IFACE = eth1
endif
ifeq ($(UNAME_S),Linux)
	PIPEWORK_IFACE = br1
endif


# Make sure you change boot2docker params below too
HOST_IP = 172.30.0.255
SUBNET = 20
# Some Mac OS related parameters for boot2docker
B2D_DHCP_IP = 172.30.0.254
B2D_NETMASK = [255, 255, 240, 0]
B2D_LOWER_IP = 172.30.0.253
B2D_UPPER_IP = 172.30.0.253

NUM_ROUTERS = 2
ROUTER_IP_PREFIX = 172.30.0.
ROUTER_PORT = 8007

NUM_LOGDS_PER_ROUTER = 2    # max 15. Change subnet if you want more
LOGD_IP_PREFIX = 172.30.
LOGD_CMDLINE_ARGS = -D *=40       # this is in addition to '-G <router>'

ROUTER_IPS = $(foreach num,$(shell seq 1 $(NUM_ROUTERS)),$(ROUTER_IP_PREFIX)$(num))
ROUTER_CMDLINE_ARGS_ROUTERS = $(shell echo "$(foreach router,$(ROUTER_IPS),$(router):$(ROUTER_PORT))" | sed -e 's/ /,/g')
ROUTER_CMDLINE_ARGS = -w 120 -r $(ROUTER_CMDLINE_ARGS_ROUTERS)
TMP_FILE = /tmp/.gdp-test-launched-routers


init:
	-@if [ "$(UNAME_S)" = "Darwin" ] ; then \
		set -x ; \
		echo 'HostIP = "$(HOST_IP)"'      > ~/.boot2docker/profile ; \
		echo 'DHCPIP = "$(B2D_DHCP_IP)"'  >> ~/.boot2docker/profile ; \
		echo 'NetMask = $(B2D_NETMASK)'   >> ~/.boot2docker/profile ; \
		echo 'LowerIP = "$(B2D_LOWER_IP)"'>> ~/.boot2docker/profile ; \
		echo 'UpperIP = "$(B2D_UPPER_IP)"'>> ~/.boot2docker/profile ; \
		echo 'DHCPEnabled = false'        >> ~/.boot2docker/profile ; \
		boot2docker destroy || true; \
		VBoxManage hostonlyif remove vboxnet0 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet1 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet2 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet3 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet4 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet5 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet6 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet7 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet8 > /dev/null 2>&1|| true; \
		VBoxManage hostonlyif remove vboxnet9 > /dev/null 2>&1|| true; \
		boot2docker init ; \
		VBoxManage modifyvm boot2docker-vm --nictype2 Am79C973 ; \
		VBoxManage modifyvm boot2docker-vm --nicpromisc2 allow-all ; \
		boot2docker start ; \
		boot2docker shellinit > .boot2docker_params ; \
		eval "$$(cat .boot2docker_params)" ; \
		boot2docker ssh git clone https://github.com/jpetazzo/pipework.git ; \
		boot2docker ssh sudo ip addr del $(B2D_LOWER_IP)/$(SUBNET) dev eth1 ; \
		boot2docker ssh sudo ip link add link eth1 dev eth1m type macvlan mode bridge; \
		boot2docker ssh sudo ip link set eth1m up; \
		boot2docker ssh sudo ip addr add $(B2D_LOWER_IP)/$(SUBNET) dev eth1m; \
		boot2docker ssh sudo route add -net $(ROUTER_IP_PREFIX)0/$(SUBNET) dev eth1m ; \
		set +x ; \
	fi
	-@if [ "$(UNAME_S)" = "Linux" ]; then \
		set -x ; \
		git clone https://github.com/jpetazzo/pipework.git ; \
		set +x ; \
	fi

$(GDP_ROUTER_ROOT):
	git clone https://github.com/niteshmor/routia.git

$(GDP_LOGD_DEB): gdplogd_compile-wrapper.sh
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	docker run -v `pwd -P`/$(GDP_ROOT):/gdp ubuntu:14.04 /gdp/test-net/gdplogd_compile-wrapper.sh $(GDP_LOGD_VERSION)

$(GDP_ROUTER_DEB): $(GDP_ROUTER_ROOT) gdp-router_compile-wrapper.sh
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	docker run -v `pwd -P`/$(GDP_ROOT):/gdp ubuntu:14.04 /gdp/test-net/gdp-router_compile-wrapper.sh $(GDP_ROUTER_VERSION)

gdp-router-docker-image: init $(GDP_ROUTER_DEB) $(GDP_ROUTER_DOCKERFILE)
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	cp $(GDP_ROUTER_DOCKERFILE) Dockerfile
	docker build -t gdp-router .
	rm -f Dockerfile

gdp-logd-docker-image: init $(GDP_LOGD_DEB) $(GDP_LOGD_DOCKERFILE)
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	cp $(GDP_LOGD_DOCKERFILE) Dockerfile
	docker build -t gdplogd .
	rm -f Dockerfile

images: gdp-router-docker-image gdp-logd-docker-image

# Start the gdp routers
gdp-routers: $(PIPEWORK) 
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	@sudo ls / > /dev/null    # So that IP configuration is right after container start
	-@rm -f $(TMP_FILE)
	@for IP in $(ROUTER_IPS); do \
		echo "Launching a docker container..."; \
		tmp=`docker run -d --name=gdp-router_$$IP gdp-router $(ROUTER_CMDLINE_ARGS)`;\
		echo gdp-router_$$IP $$tmp; \
		echo $$tmp >> $(TMP_FILE); \
		echo "Setting up networking..."; \
		$(PIPEWORK_CMD) $(PIPEWORK_IFACE) $$tmp $$IP/$(SUBNET);\
	done
	-@ [ "$(UNAME_S)" = "Linux" ] && (sudo ip addr add $(HOST_IP)/$(SUBNET) dev br1)


# Start the logd's
gdp-logds: $(PIPEWORK) gdp-routers
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	@sudo ls / > /dev/null  # So that IP configuration doesn't wait for user to enter password
	for ROUTER_IP in $(ROUTER_IPS); do \
	    for i in `seq 1 $(NUM_LOGDS_PER_ROUTER)` ; do \
			this_logd_ip=`echo $$ROUTER_IP | sed -e "s/$(ROUTER_IP_PREFIX)/$(LOGD_IP_PREFIX)$$i./g"` ; \
			echo "Launching a docker container running logd ($$this_logd_ip) to connect to $$ROUTER_IP" ; \
			tmp=`docker run -d --name=gdp-logd_$$this_logd_ip gdplogd gdp-logd_$$this_logd_ip -G $$ROUTER_IP $(LOGD_CMDLINE_ARGS)` ; \
			echo "Setting up networking..."; \
			$(PIPEWORK_CMD) $(PIPEWORK_IFACE) $$tmp $$this_logd_ip/$(SUBNET) ; \
		done ; \
	done

logd_containers := $(shell docker ps -a -q --filter="name=gdp-logd_*")
router_containers := $(shell docker ps -a -q --filter="name=gdp-router_*")

clean:
	-[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	rm -rf gdp-router*.deb gdp-server*.deb
	rm -f Dockerfile
	-if [ -n "$(logd_containers)" ] ; then \
		echo "Killing running docker containers..." ;\
		docker kill $(logd_containers) ;\
		echo "Removing all docker containers..." ;\
		docker rm $(logd_containers) ;\
	fi
	-if [ -n "$(router_containers)" ] ; then \
		echo "Killing running docker containers..." ;\
		docker kill $(router_containers) ;\
		echo "Removing all docker containers..." ;\
		docker rm $(router_containers) ;\
	fi
	-[ "$(UNAME_S)" = "Linux" ] && (sudo ip link set br1 down)
	-[ "$(UNAME_S)" = "Linux" ] && sudo brctl delbr br1

clean-images: clean
	[ "$(UNAME_S)" = "Darwin" ] && (eval "$$(cat .boot2docker_params)")
	-docker rmi gdp-router gdplogd
