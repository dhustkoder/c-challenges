#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#include "upnp.h"
#include "network.h"


static struct UPNPInfo {
	struct UPNPDev* dev;
	struct UPNPUrls urls;
	struct IGDdatas data;
	char* port;
	const char* proto;
} upnp_info = { .dev = NULL, .port = NULL, .proto = NULL };


bool initialize_upnp(const char* const port)
{
	int error = 0;
	char lan_addr[IP_STR_SIZE];
	char wan_addr[IP_STR_SIZE];
	upnp_info.port = malloc(strlen(port) + 1);
	strcpy(upnp_info.port, port);
	upnp_info.proto = "TCP";

	upnp_info.dev = upnpDiscover(
	        4500   , // time to wait (milliseconds)
	        NULL   , // multicast interface (or null defaults to 239.255.255.250)
	        NULL   , // path to minissdpd socket (or null defaults to /var/run/minissdpd.sock)
	        0      , // source port to use (or zero defaults to port 1900)
	        0      , // 0==IPv4, 1==IPv6
	        #if MINIUPNPC_API_VERSION >= 16
		0      , // ttl (only on >= 16 MINIUPNC_API_VERSION)
	        #endif
	        &error); // error condition

	if (error != UPNPDISCOVER_SUCCESS) {
		fprintf(stderr, "Couldn't set UPnP: %s\n", strupnperror(error));
		return false;
	}

	const int status = UPNP_GetValidIGD(upnp_info.dev, &upnp_info.urls, &upnp_info.data,
	                                    lan_addr, sizeof(lan_addr));
	// look up possible "status" values, the number "1" indicates a valid IGD was found
	if (status == 0) {
		fprintf(stderr, "Couldn't find a valid IGD.\n");
		goto Lfree_upnp_dev;
	}
	

	// get the external (WAN) IP address
	error = UPNP_GetExternalIPAddress(upnp_info.urls.controlURL,
	                                  upnp_info.data.first.servicetype,
				          wan_addr);

	if (error != UPNPCOMMAND_SUCCESS)
		goto Lupnp_command_error;

	// add a new TCP port mapping from WAN port 12345 to local host port 24680
	error = UPNP_AddPortMapping(
	            upnp_info.urls.controlURL,
	            upnp_info.data.first.servicetype,
	            upnp_info.port ,  // external (WAN) port requested
	            upnp_info.port ,  // internal (LAN) port to which packets will be redirected
	            lan_addr       ,  // internal (LAN) address to which packets will be redirected
	            "Chat"         ,  // text description to indicate why or who is responsible for the port mapping
	            upnp_info.proto,  // protocol must be either TCP or UDP
	            NULL           ,  // remote (peer) host address or nullptr for no restriction
	            NULL           ); // port map lease duration (in seconds) or zero for "as long as possible"

	if (error != UPNPCOMMAND_SUCCESS)
		goto Lupnp_command_error;

	return true;

Lupnp_command_error:
	fprintf(stderr, "Couldn't set UPnP: %s\n", strupnperror(error));
	FreeUPNPUrls(&upnp_info.urls);
Lfree_upnp_dev:
	freeUPNPDevlist(upnp_info.dev);
	return false;
}


void terminate_upnp(void)
{
	const int error = UPNP_DeletePortMapping(upnp_info.urls.controlURL,
	                       upnp_info.data.first.servicetype,
			       upnp_info.port,
			       upnp_info.proto,
			       NULL);
	
	freeUPNPDevlist(upnp_info.dev);
	FreeUPNPUrls(&upnp_info.urls);
	free(upnp_info.port);

	if (error != 0) {
		fprintf(stderr, "Couldn't delete port mapping: %s",
		        strupnperror(error));
	}
}

