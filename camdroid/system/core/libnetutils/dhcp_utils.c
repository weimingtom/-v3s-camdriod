/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

/* Utilities for managing the dhcpcd DHCP client daemon */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cutils/log.h>
#include <cutils/properties.h>

static const char DAEMON_NAME[]        = "dhcpcd";
static const char DAEMON_PROP_NAME[]   = "init.svc.dhcpcd";
static const char HOSTNAME_PROP_NAME[] = "net.hostname";
static const char DHCP_PROP_NAME_PREFIX[]  = "dhcp";
static const char DHCP_CONFIG_PATH[]   = "/system/etc/dhcpcd/dhcpcd.conf";
static const int NAP_TIME = 200;   /* wait for 200ms at a time */
                                  /* when polling for property values */
static const char DAEMON_NAME_RENEW[]  = "iprenew";
static char errmsg[100];
/* interface length for dhcpcd daemon start (dhcpcd_<interface> as defined in init.rc file)
 * or for filling up system properties dhcpcd.<interface>.ipaddress, dhcpcd.<interface>.dns1
 * and other properties on a successful bind
 */
#define MAX_INTERFACE_LENGTH 25
/*
 * P2p interface names increase sequentially p2p-p2p0-1, p2p-p2p0-2.. after
 * group formation. This does not work well with system properties which can quickly
 * exhaust or for specifiying a dhcp start target in init which requires
 * interface to be pre-defined in init.rc file.
 *
 * This function returns a common string p2p for all p2p interfaces.
 */
void get_net_interface_replacement(const char *interface, char *net_interface,char *rest_interface) {
    /* Use p2p for any interface starting with p2p. */
    if (strncmp(interface, "p2p",3) == 0) {
        strncpy(net_interface, "p2p", MAX_INTERFACE_LENGTH);
        strncpy(rest_interface, "p2p", MAX_INTERFACE_LENGTH);
    } else if(strncmp(interface, "eth_", 4) == 0){
        memset(net_interface, 0, MAX_INTERFACE_LENGTH);
        sprintf(net_interface, "eth");
		strncpy(rest_interface, interface + 4, strlen(interface + 4) + 1);
    }else{
    	strncpy(net_interface, interface, MAX_INTERFACE_LENGTH);
        strncpy(rest_interface, interface, MAX_INTERFACE_LENGTH);
    }
}

/*
 * Wait for a system property to be assigned a specified value.
 * If desired_value is NULL, then just wait for the property to
 * be created with any value. maxwait is the maximum amount of
 * time in seconds to wait before giving up.
 */
static int wait_for_property(const char *name, const char *desired_value, int maxwait)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int maxnaps = (maxwait * 1000) / NAP_TIME;

    if (maxnaps < 1) {
        maxnaps = 1;
    }

    while (maxnaps-- > 0) {
        usleep(NAP_TIME * 1000);
        if (property_get(name, value, NULL)) {
        	ALOGD("name = %s,value = %s",name,value);
            if (desired_value == NULL || 
                    strcmp(value, desired_value) == 0) {
                return 0;
            }
        }
    }
    return -1; /* failure */
}

static int fill_ip_info(const char *interface,
                     char *ipaddr,
                     char *gateway,
                     uint32_t *prefixLength,
                     char *dns1,
                     char *dns2,
                     char *server,
                     uint32_t *lease,
                     char *vendorInfo)
{
    char prop_name[PROPERTY_KEY_MAX];
    char prop_value[PROPERTY_VALUE_MAX];
    /* Interface name after converting p2p0-p2p0-X to p2p to reuse system properties */
    char net_interface[MAX_INTERFACE_LENGTH];
    char rest_interface[MAX_INTERFACE_LENGTH];


    get_net_interface_replacement(interface, net_interface,rest_interface);
	//ALOGD("interface = %s,rest_interface = %s",net_interface,rest_interface);

    snprintf(prop_name, sizeof(prop_name), "%s.%s.ipaddress", DHCP_PROP_NAME_PREFIX, rest_interface);
    property_get(prop_name, ipaddr, NULL);
    //ALOGD("prop_name = %s,ipaddr = %s",prop_name,ipaddr);

    snprintf(prop_name, sizeof(prop_name), "%s.%s.gateway", DHCP_PROP_NAME_PREFIX, rest_interface);
    property_get(prop_name, gateway, NULL);
    //ALOGD("net_interface = %s",net_interface);
    //ALOGD("prop_name = %s,gateway = %s",prop_name,gateway);

    snprintf(prop_name, sizeof(prop_name), "%s.%s.server", DHCP_PROP_NAME_PREFIX, rest_interface);
    property_get(prop_name, server, NULL);
    //ALOGD("prop_name = %s,server = %s",prop_name,server);

    //TODO: Handle IPv6 when we change system property usage
    if (strcmp(gateway, "0.0.0.0") == 0) {
        //DHCP server is our best bet as gateway
        strncpy(gateway, server, PROPERTY_VALUE_MAX);
    }

    snprintf(prop_name, sizeof(prop_name), "%s.%s.mask", DHCP_PROP_NAME_PREFIX, rest_interface);
    if (property_get(prop_name, prop_value, NULL)) {
        int p;
        // this conversion is v4 only, but this dhcp client is v4 only anyway
        in_addr_t mask = ntohl(inet_addr(prop_value));
        // Check netmask is a valid IP address.  ntohl gives NONE response (all 1's) for
        // non 255.255.255.255 inputs.  if we get that value check if it is legit..
        if (mask == INADDR_NONE && strcmp(prop_value, "255.255.255.255") != 0) {
            snprintf(errmsg, sizeof(errmsg), "DHCP gave invalid net mask %s", prop_value);
            return -1;
        }
        for (p = 0; p < 32; p++) {
            if (mask == 0) break;
            // check for non-contiguous netmask, e.g., 255.254.255.0
            if ((mask & 0x80000000) == 0) {
                snprintf(errmsg, sizeof(errmsg), "DHCP gave invalid net mask %s", prop_value);
                return -1;
            }
            mask = mask << 1;
        }
        *prefixLength = p;
    }
    snprintf(prop_name, sizeof(prop_name), "%s.%s.dns1", DHCP_PROP_NAME_PREFIX, rest_interface);
    property_get(prop_name, dns1, NULL);

    snprintf(prop_name, sizeof(prop_name), "%s.%s.dns2", DHCP_PROP_NAME_PREFIX, rest_interface);
    property_get(prop_name, dns2, NULL);

    snprintf(prop_name, sizeof(prop_name), "%s.%s.leasetime", DHCP_PROP_NAME_PREFIX, rest_interface);
    if (property_get(prop_name, prop_value, NULL)) {
        *lease = atol(prop_value);
    }

    snprintf(prop_name, sizeof(prop_name), "%s.%s.vendorInfo", DHCP_PROP_NAME_PREFIX,
            rest_interface);
    property_get(prop_name, vendorInfo, NULL);

    return 0;
}

static const char *ipaddr_to_string(in_addr_t addr)
{
    struct in_addr in_addr;

    in_addr.s_addr = addr;
    return inet_ntoa(in_addr);
}

/*
 * Start the dhcp client daemon, and wait for it to finish
 * configuring the interface.
 *
 * The device init.rc file needs a corresponding entry for this work.
 *
 * Example:
 * service dhcpcd_<interface> /system/bin/dhcpcd -ABKL -f dhcpcd.conf
 */
int dhcp_do_request(const char *interface,
                    char *ipaddr,
                    char *gateway,
                    uint32_t *prefixLength,
                    char *dns1,
                    char *dns2,
                    char *server,
                    uint32_t *lease,
                    char *vendorInfo)
{
    char result_prop_name[PROPERTY_KEY_MAX];
    char daemon_prop_name[PROPERTY_KEY_MAX];
    char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
    char daemon_cmd[PROPERTY_VALUE_MAX * 2 + sizeof(DHCP_CONFIG_PATH)];
    const char *ctrl_prop = "ctl.start";
    const char *desired_status = "running";
    /* Interface name after converting p2p0-p2p0-X to p2p to reuse system properties */
    char net_interface[MAX_INTERFACE_LENGTH];
    char rest_interface[MAX_INTERFACE_LENGTH];

	get_net_interface_replacement(interface, net_interface,rest_interface);
	//ALOGD("interface = %s,rest_interface = %s",net_interface,rest_interface);
		
	if (strncmp(interface, "eth", 3) == 0) {
		snprintf(result_prop_name, sizeof(result_prop_name), "%s.%s.result",
				DHCP_PROP_NAME_PREFIX,
				rest_interface);
	} else {
		snprintf(result_prop_name, sizeof(result_prop_name), "%s.%s.result",
				DHCP_PROP_NAME_PREFIX,
				net_interface);
	}

    snprintf(daemon_prop_name, sizeof(daemon_prop_name), "%s_%s",
            DAEMON_PROP_NAME,
            net_interface);

    /* Erase any previous setting of the dhcp result property */
    property_set(result_prop_name, "");

    /* Start the daemon and wait until it's ready */
    if (property_get(HOSTNAME_PROP_NAME, prop_value, NULL) && (prop_value[0] != '\0'))
        snprintf(daemon_cmd, sizeof(daemon_cmd), "%s_%s:-h %s %s", DAEMON_NAME, net_interface,
                 prop_value, rest_interface);
    else
        snprintf(daemon_cmd, sizeof(daemon_cmd), "%s_%s:%s", DAEMON_NAME, net_interface, rest_interface);
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_set(ctrl_prop, daemon_cmd);
    ALOGD("ctrl_prop = %s,daemon_cmd = %s",ctrl_prop,daemon_cmd);
    if (wait_for_property(daemon_prop_name, desired_status, 10) < 0) {
        snprintf(errmsg, sizeof(errmsg), "%s", "Timed out waiting for dhcpcd to start");
        return -1;
    }
    
    //ALOGD("result_prop_name = %s",result_prop_name);

    /* Wait for the daemon to return a result */
    if (wait_for_property(result_prop_name, NULL, 30) < 0) {
        snprintf(errmsg, sizeof(errmsg), "%s", "Timed out waiting for DHCP to finish");
        return -1;
    }
    
    //ALOGD("result_prop_name = %s",result_prop_name);

    if (!property_get(result_prop_name, prop_value, NULL)) {
        /* shouldn't ever happen, given the success of wait_for_property() */
        snprintf(errmsg, sizeof(errmsg), "%s", "DHCP result property was not set");
        return -1;
    }
    ALOGD("prop_value = %s",prop_value);
    if (strcmp(prop_value, "ok") == 0) {
        char dns_prop_name[PROPERTY_KEY_MAX];
        if (fill_ip_info(interface, ipaddr, gateway, prefixLength,
                dns1, dns2, server, lease, vendorInfo) == -1) {
            return -1;
        }

        /* copy dns data to system properties - TODO - remove this after we have async
         * notification of renewal's */
        snprintf(dns_prop_name, sizeof(dns_prop_name), "net.%s.dns1", rest_interface);
        property_set(dns_prop_name, *dns1 ? ipaddr_to_string(*dns1) : "");
        snprintf(dns_prop_name, sizeof(dns_prop_name), "net.%s.dns2", rest_interface);
        property_set(dns_prop_name, *dns2 ? ipaddr_to_string(*dns2) : "");
        return 0;
    } else {
        snprintf(errmsg, sizeof(errmsg), "DHCP result was %s", prop_value);
        return -1;
    }
}

/**
 * Stop the DHCP client daemon.
 */
int dhcp_stop(const char *interface)
{
    char result_prop_name[PROPERTY_KEY_MAX];
    char daemon_prop_name[PROPERTY_KEY_MAX];
    char daemon_cmd[PROPERTY_VALUE_MAX * 2];
    const char *ctrl_prop = "ctl.stop";
    const char *desired_status = "stopped";

    char net_interface[MAX_INTERFACE_LENGTH];
    char rest_interface[MAX_INTERFACE_LENGTH];

	get_net_interface_replacement(interface, net_interface,rest_interface);

	if (strncmp(interface, "eth", 3) == 0) {
		snprintf(result_prop_name, sizeof(result_prop_name), "%s.%s.result",
				DHCP_PROP_NAME_PREFIX,
				rest_interface);
	} else {
		snprintf(result_prop_name, sizeof(result_prop_name), "%s.%s.result",
				DHCP_PROP_NAME_PREFIX,
				net_interface);
	}

    snprintf(daemon_prop_name, sizeof(daemon_prop_name), "%s_%s",
            DAEMON_PROP_NAME,
            net_interface);

    snprintf(daemon_cmd, sizeof(daemon_cmd), "%s_%s", DAEMON_NAME, net_interface);

    /* Stop the daemon and wait until it's reported to be stopped */
    property_set(ctrl_prop, daemon_cmd);
    if (wait_for_property(daemon_prop_name, desired_status, 5) < 0) {
        return -1;
    }
    property_set(result_prop_name, "failed");
    return 0;
}

/**
 * Release the current DHCP client lease.
 */
int dhcp_release_lease(const char *interface)
{
    char daemon_prop_name[PROPERTY_KEY_MAX];
    char daemon_cmd[PROPERTY_VALUE_MAX * 2];
    const char *ctrl_prop = "ctl.stop";
    const char *desired_status = "stopped";
	char rest_interface[MAX_INTERFACE_LENGTH];
    char net_interface[MAX_INTERFACE_LENGTH];

    get_net_interface_replacement(interface, net_interface,rest_interface);

    snprintf(daemon_prop_name, sizeof(daemon_prop_name), "%s_%s",
            DAEMON_PROP_NAME,
            net_interface);

    snprintf(daemon_cmd, sizeof(daemon_cmd), "%s_%s", DAEMON_NAME, net_interface);

    /* Stop the daemon and wait until it's reported to be stopped */
    property_set(ctrl_prop, daemon_cmd);
    if (wait_for_property(daemon_prop_name, desired_status, 5) < 0) {
        return -1;
    }
    return 0;
}

char *dhcp_get_errmsg() {
    return errmsg;
}

/**
 * The device init.rc file needs a corresponding entry.
 *
 * Example:
 * service iprenew_<interface> /system/bin/dhcpcd -n
 *
 */
int dhcp_do_request_renew(const char *interface,
                    char *ipaddr,
                    char *gateway,
                    uint32_t *prefixLength,
                    char *dns1,
                    char *dns2,
                    char *server,
                    uint32_t *lease,
                    char *vendorInfo)
{
    char result_prop_name[PROPERTY_KEY_MAX];
    char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
    char daemon_cmd[PROPERTY_VALUE_MAX * 2];
    const char *ctrl_prop = "ctl.start";
	char rest_interface[MAX_INTERFACE_LENGTH];
    char net_interface[MAX_INTERFACE_LENGTH];

    get_net_interface_replacement(interface, net_interface,rest_interface);

    snprintf(result_prop_name, sizeof(result_prop_name), "%s.%s.result",
            DHCP_PROP_NAME_PREFIX,
            net_interface);

    /* Erase any previous setting of the dhcp result property */
    property_set(result_prop_name, "");

    /* Start the renew daemon and wait until it's ready */
    snprintf(daemon_cmd, sizeof(daemon_cmd), "%s_%s:%s", DAEMON_NAME_RENEW,
            net_interface, rest_interface);
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_set(ctrl_prop, daemon_cmd);

    /* Wait for the daemon to return a result */
    if (wait_for_property(result_prop_name, NULL, 30) < 0) {
        snprintf(errmsg, sizeof(errmsg), "%s", "Timed out waiting for DHCP Renew to finish");
        return -1;
    }

    if (!property_get(result_prop_name, prop_value, NULL)) {
        /* shouldn't ever happen, given the success of wait_for_property() */
        snprintf(errmsg, sizeof(errmsg), "%s", "DHCP Renew result property was not set");
        return -1;
    }
    if (strcmp(prop_value, "ok") == 0) {
        fill_ip_info(interface, ipaddr, gateway, prefixLength,
                dns1, dns2, server, lease, vendorInfo);
        return 0;
    } else {
        snprintf(errmsg, sizeof(errmsg), "DHCP Renew result was %s", prop_value);
        return -1;
    }
}
