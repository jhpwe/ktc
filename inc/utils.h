#ifndef __KTC_UTILS_H__
#define __KTC_UTILS_H__

#include <stdlib.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/pkt_sched.h>


//add
#include <limits.h>
#include <linux/if_ether.h>
#include <errno.h>
#include <arpa/inet.h>
//end

#define TCA_BUF_MAX			(64*1024)
#define TIME_UNITS_PER_SEC	1000000
#define MAX_MSG 16384

#define __PF(f,n) { ETH_P_##f, #n },
static const struct {
	int id;
	const char *name;
} llproto_names[] = {
__PF(LOOP,loop)
__PF(PUP,pup)
__PF(PUPAT,pupat)
__PF(IP,ip)
__PF(X25,x25)
__PF(ARP,arp)
__PF(BPQ,bpq)
__PF(IEEEPUP,ieeepup)
__PF(IEEEPUPAT,ieeepupat)
__PF(DEC,dec)
__PF(DNA_DL,dna_dl)
__PF(DNA_RC,dna_rc)
__PF(DNA_RT,dna_rt)
__PF(LAT,lat)
__PF(DIAG,diag)
__PF(CUST,cust)
__PF(SCA,sca)
__PF(RARP,rarp)
__PF(ATALK,atalk)
__PF(AARP,aarp)
__PF(IPX,ipx)
__PF(IPV6,ipv6)
__PF(PPP_DISC,ppp_disc)
__PF(PPP_SES,ppp_ses)
__PF(ATMMPOA,atmmpoa)
__PF(ATMFATE,atmfate)
__PF(802_3,802_3)
__PF(AX25,ax25)
__PF(ALL,all)
__PF(802_2,802_2)
__PF(SNAP,snap)
__PF(DDCMP,ddcmp)
__PF(WAN_PPP,wan_ppp)
__PF(PPP_MP,ppp_mp)
__PF(LOCALTALK,localtalk)
__PF(CAN,can)
__PF(PPPTALK,ppptalk)
__PF(TR_802_2,tr_802_2)
__PF(MOBITEX,mobitex)
__PF(CONTROL,control)
__PF(IRDA,irda)
__PF(ECONET,econet)
__PF(TIPC,tipc)
__PF(AOE,aoe)
__PF(8021Q,802.1Q)
__PF(8021AD,802.1ad)

{ 0x8100, "802.1Q" },
{ 0x88cc, "LLDP" },
{ ETH_P_IP, "ipv4" },
};
#undef __PF

enum link_layer
{
	LINKLAYER_UNSPEC,
	LINKLAYER_ETHERNET,
	LINKLAYER_ATM,
};

struct req_s 
{
	struct nlmsghdr	n;
	struct tcmsg	t;
	char			buf[TCA_BUF_MAX];
};

extern int __iproute2_hz_internal;

int get_rate64(__u64 *rate, const char *str);
int __get_hz(void);

static __inline__ int get_hz(void)
{
	if (__iproute2_hz_internal == 0)
		__iproute2_hz_internal = __get_hz();
	return __iproute2_hz_internal;
}

int tc_calc_rtable(struct tc_ratespec *r, __u32 *rtab, int cell_log, unsigned int mtu, enum link_layer linklayer);
unsigned int tc_adjust_size(unsigned int sz, unsigned int mpu, enum link_layer linklayer);
unsigned int tc_calc_xmittime(__u64 rate, unsigned int size);
unsigned int tc_core_time2tick(unsigned int time);
unsigned int tc_align_to_atm(unsigned int size);



int get_u32(__u32 *val, const char *arg, int base);
int get_be16(__be16 *val, const char *arg, int base);
int ll_proto_a2n(unsigned short *id, const char *buf);
int get_u16(__u16 *val, const char *arg, int base);

#endif /* __KTC_UTILS_H__ */
