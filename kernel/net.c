#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define MAX_SOCKETS 16
#define MAX_QUEUE_LEN 16

struct packet {
  struct packet *next;
  uint32 src_ip;
  uint16 src_port;
  uint16 len;
  char *buf;
};

struct sock {
  int used;
  uint16 port;
  struct spinlock lock;
  struct packet *head;
  struct packet *tail;
  int qlen;
};

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;
static struct sock sockets[MAX_SOCKETS];
static struct spinlock net_lock;

void
netinit(void)
{
  initlock(&net_lock, "net_lock");
  for (int i = 0; i < MAX_SOCKETS; i++) {
    initlock(&sockets[i].lock, "sock_lock");
    sockets[i].used = 0;
    sockets[i].head = 0;
    sockets[i].tail = 0;
    sockets[i].qlen = 0;
  }
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  argint(0, &port);

  acquire(&netlock);
  struct sock *free_sk = 0;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i].used && sockets[i].port == (uint16)port) {
      release(&netlock);
      return 0;
    }
    if (!sockets[i].used && !free_sk) {
      free_sk = &sockets[i];
    }
  }

  if (!free_sk) {
    release(&netlock);
    return -1;
  }

  acquire(&free_sk->lock);
  free_sk->used = 1;
  free_sk->port = (uint16)port;
  free_sk->head = 0;
  free_sk->tail = 0;
  free_sk->qlen = 0;
  release(&free_sk->lock);

  release(&netlock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  int dport, maxlen;
  uint64 src_addr, sport_addr, buf_addr;

  argint(0, &dport);
  argaddr(1, &src_addr);
  argaddr(2, &sport_addr);
  argaddr(3, &buf_addr);
  argint(4, &maxlen);

  acquire(&netlock);
  struct sock *sk = 0;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i].used && sockets[i].port == (uint16)dport) {
      sk = &sockets[i];
      break;
    }
  }
  release(&netlock);

  if (!sk)
    return -1;

  acquire(&sk->lock);
  while (sk->head == 0) {
    if (myproc()->killed) {
      release(&sk->lock);
      return -1;
    }
    sleep(sk, &sk->lock);
  }

  struct packet *pkt = sk->head;
  sk->head = pkt->next;
  if (sk->head == 0) {
    sk->tail = 0;
  }
  sk->qlen--;
  release(&sk->lock);

  struct proc *p = myproc();
  int copy_len = pkt->len < maxlen ? pkt->len : maxlen;
  char *payload = pkt->buf + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);

  if (copyout(p->pagetable, buf_addr, payload, copy_len) < 0 ||
      copyout(p->pagetable, src_addr, (char *)&pkt->src_ip, sizeof(pkt->src_ip)) < 0 ||
      copyout(p->pagetable, sport_addr, (char *)&pkt->src_port, sizeof(pkt->src_port)) < 0) {
    kfree(pkt->buf);
    kfree((char *)pkt);
    return -1;
  }

  kfree(pkt->buf);
  kfree((char *)pkt);
  return copy_len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  if (len < sizeof(struct eth) + sizeof(struct ip)) {
    kfree(buf);
    return;
  }

  struct ip *ip = (struct ip *)(buf + sizeof(struct eth));

  if (ip->ip_p != IPPROTO_UDP) {
    kfree(buf);
    return;
  }

  if (len < sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp)) {
    kfree(buf);
    return;
  }

  struct udp *udp = (struct udp *)((char *)ip + sizeof(struct ip));

  uint16 dport = ntohs(udp->dport);
  uint16 sport = ntohs(udp->sport);
  uint32 src_ip = ntohl(ip->ip_src);
  uint16 ulen = ntohs(udp->ulen);

  if (ulen < sizeof(struct udp)) {
    kfree(buf);
    return;
  }

  uint16 payload_len = ulen - sizeof(struct udp);

  acquire(&netlock);
  struct sock *sk = 0;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (sockets[i].used && sockets[i].port == dport) {
      sk = &sockets[i];
      break;
    }
  }
  release(&netlock);

  if (!sk) {
    kfree(buf);
    return;
  }

  acquire(&sk->lock);

  if (sk->qlen >= MAX_QUEUE_LEN) {
    release(&sk->lock);
    kfree(buf);
    return;
  }

  struct packet *pkt = (struct packet *)kalloc();
  if (!pkt) {
    release(&sk->lock);
    kfree(buf);
    return;
  }

  pkt->next = 0;
  pkt->src_ip = src_ip;
  pkt->src_port = sport;
  pkt->len = payload_len;
  pkt->buf = buf;

  if (sk->tail) {
    sk->tail->next = pkt;
    sk->tail = pkt;
  } else {
    sk->head = pkt;
    sk->tail = pkt;
  }
  sk->qlen++;

  wakeup(sk);
  release(&sk->lock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
