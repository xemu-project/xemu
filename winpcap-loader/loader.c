/*
 * [wi]npcap dynamic loader
 *
 * Copyright (C) 2021 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include <pcap/pcap.h>
#include <string.h>

const char *lib_not_loaded_err = "winpcap library is not loaded";

static pcap_t *pcap_open_live_stub(const char *device, int snaplen, int promisc, int to_ms, char *errbuf)
{
	strncpy(errbuf, lib_not_loaded_err, PCAP_ERRBUF_SIZE);
	return NULL;
}

static int pcap_findalldevs_stub(pcap_if_t **alldevsp, char *errbuf)
{
	strncpy(errbuf, lib_not_loaded_err, PCAP_ERRBUF_SIZE);
	return -1;
}

void (*fptr_pcap_close)(pcap_t *);
int (*fptr_pcap_next_ex)(pcap_t *, struct pcap_pkthdr **, const u_char **);
char *(*fptr_pcap_geterr)(pcap_t *);
pcap_t *(*fptr_pcap_open_live)(const char *, int, int, int, char *) = pcap_open_live_stub;
int	(*fptr_pcap_set_datalink)(pcap_t *, int);
int (*fptr_pcap_setmintocopy)(pcap_t *p, int size);
HANDLE (*fptr_pcap_getevent)(pcap_t *p);
int (*fptr_pcap_sendpacket)(pcap_t *, const u_char *, int);
int (*fptr_pcap_findalldevs)(pcap_if_t **, char *) = pcap_findalldevs_stub;
void (*fptr_pcap_freealldevs)(pcap_if_t *);

void pcap_close(pcap_t *p)
{ fptr_pcap_close(p); }

int pcap_next_ex(pcap_t *p, struct pcap_pkthdr **pkt_header, const u_char **pkt_data)
{ return fptr_pcap_next_ex(p, pkt_header, pkt_data); }

char *pcap_geterr(pcap_t *p)
{ return fptr_pcap_geterr(p); }

pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf)
{ return fptr_pcap_open_live(device, snaplen, promisc, to_ms, errbuf); }

int pcap_set_datalink(pcap_t *p, int dlt)
{ return fptr_pcap_set_datalink(p, dlt); }

int pcap_setmintocopy(pcap_t *p, int size)
{ return fptr_pcap_setmintocopy(p, size); }

HANDLE pcap_getevent(pcap_t *p)
{ return fptr_pcap_getevent(p); }

int pcap_sendpacket(pcap_t *p, const u_char *buf, int size)
{ return fptr_pcap_sendpacket(p, buf, size); }

int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf)
{ return fptr_pcap_findalldevs(alldevsp, errbuf); }

void pcap_freealldevs(pcap_if_t *alldevs)
{ fptr_pcap_freealldevs(alldevs); }

int pcap_load_library(void)
{
	static int is_loaded = 0;
	if (is_loaded) {
		return 0;
	}

	HANDLE hwpcap = LoadLibrary("wpcap.dll");
	if (hwpcap == NULL) {
		return 1;
	}

	#define LOAD_FN(fname) do {\
		void *p = (void*)GetProcAddress(hwpcap, stringify(fname)); \
		if (p == NULL) { \
			return 1; \
		} else { \
			fptr_ ## fname = p; \
		} \
	} while(0)

	LOAD_FN(pcap_close);
	LOAD_FN(pcap_next_ex);
	LOAD_FN(pcap_geterr);
	LOAD_FN(pcap_open_live);
	LOAD_FN(pcap_set_datalink);
	LOAD_FN(pcap_setmintocopy);
	LOAD_FN(pcap_getevent);
	LOAD_FN(pcap_sendpacket);
	LOAD_FN(pcap_findalldevs);
	LOAD_FN(pcap_freealldevs);

	is_loaded = 1;
	return 0;
}
