#include <arpa/inet.h>
#include <stdint.h>

static void
inet_ntop_ipv6 (uint32_t addr1, uint32_t addr2, uint32_t addr3, uint32_t addr4)
{
  struct in6_addr saddr = { .s6_addr32 = { addr1, addr2, addr3, addr4 } };
  char dst[INET6_ADDRSTRLEN];
  inet_ntop (AF_INET6, &saddr, dst, sizeof dst);
}
