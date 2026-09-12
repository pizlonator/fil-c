#include <arpa/inet.h>
#include <stdint.h>

static void
inet_ntop_ipv4 (uint32_t addr)
{
  struct in_addr saddr = { .s_addr = addr };
  char dst[INET_ADDRSTRLEN];
  inet_ntop (AF_INET, &saddr, dst, sizeof dst);
}
