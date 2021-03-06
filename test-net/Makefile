
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

GDP_ROUTER_ROOT = ./gdp_router
GDP_ROUTER_DOCKERFILE = Dockerfile.gdp-router
GDP_ROUTER_VERSION = 0.3-1

PIPEWORK = ./pipework/pipework

HOST_IP = 172.30.0.255
SUBNET = 20

NUM_ROUTERS = 1
ROUTER_IP_PREFIX = 172.30.0.
ROUTER_PORT = 8007

NUM_LOGDS_PER_ROUTER = 1    # max 15. Change subnet if you want more
LOGD_IP_PREFIX = 172.30.
LOGD_CMDLINE_ARGS = -D *=10       # this is in addition to '-G <router>'

ROUTER_IPS = $(foreach num,$(shell seq 1 $(NUM_ROUTERS)),$(ROUTER_IP_PREFIX)$(num))
ROUTER_CMDLINE_ARGS_ROUTERS = $(shell echo "$(foreach router,$(ROUTER_IPS),$(router):$(ROUTER_PORT))" | sed -e 's/ /,/g')
ROUTER_CMDLINE_ARGS = -w 10 -r $(ROUTER_CMDLINE_ARGS_ROUTERS)
TMP_FILE = /tmp/.gdp-test-launched-routers


$(PIPEWORK):
	git clone https://github.com/jpetazzo/pipework.git

$(GDP_ROUTER_ROOT):
	git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp_router.git

gdp-router-docker-image: $(GDP_ROUTER_ROOT) $(GDP_ROUTER_DOCKERFILE)
	$(GDP_ROUTER_ROOT)/deb-pkg/package.sh $(GDP_ROUTER_VERSION)
	cp $(GDP_ROUTER_DOCKERFILE) Dockerfile
	docker build -t gdp-router .
	rm -f Dockerfile

gdp-logd-docker-image: $(GDP_ROOT) $(GDP_LOGD_DOCKERFILE)
	$(GDP_ROOT)/deb-pkg/package-server.sh $(GDP_LOGD_VERSION)
	mv $(GDP_ROOT)/gdp-server_$(GDP_LOGD_VERSION)_amd64.deb .
	cp $(GDP_LOGD_DOCKERFILE) Dockerfile
	docker build -t gdplogd .
	rm -f Dockerfile

images: gdp-router-docker-image gdp-logd-docker-image

# Start the gdp routers
gdp-routers: $(PIPEWORK) 
	@sudo ls / > /dev/null    # So that IP configuration is right after container start
	-@rm -f $(TMP_FILE)
	@for IP in $(ROUTER_IPS); do \
		echo "Launching a docker container..."; \
		tmp=`docker run -d --name=gdp-router_$$IP gdp-router $(ROUTER_CMDLINE_ARGS)`;\
		echo gdp-router_$$IP $$tmp; \
		echo $$tmp >> $(TMP_FILE); \
		echo "Setting up networking..."; \
		sudo $(PIPEWORK) br1 $$tmp $$IP/$(SUBNET);\
	done
	-@sudo ip addr add $(HOST_IP)/$(SUBNET) dev br1


# Start the logd's
gdp-logds: $(PIPEWORK) gdp-routers
	@sudo ls / > /dev/null  # So that IP configuration doesn't wait for user to enter password
	for ROUTER_IP in $(ROUTER_IPS); do \
	    for i in `seq 1 $(NUM_LOGDS_PER_ROUTER)` ; do \
			this_logd_ip=`echo $$ROUTER_IP | sed -e "s/$(ROUTER_IP_PREFIX)/$(LOGD_IP_PREFIX)$$i./g"` ; \
			echo "Launching a docker container running logd ($$this_logd_ip) to connect to $$ROUTER_IP" ; \
			tmp=`docker run -d --name=gdp-logd_$$this_logd_ip gdplogd gdp-logd_$$this_logd_ip -G $$ROUTER_IP $(LOGD_CMDLINE_ARGS)` ; \
			echo "Setting up networking..."; \
			sudo $(PIPEWORK) br1 $$tmp $$this_logd_ip/$(SUBNET) ; \
		done ; \
	done

logd_containers := $(shell docker ps -a -q --filter="name=gdp-logd_*")
router_containers := $(shell docker ps -a -q --filter="name=gdp-router_*")

clean:
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
	-sudo ip link set br1 down
	-sudo brctl delbr br1

clean-images: clean
	-docker rmi gdp-router gdplogd
