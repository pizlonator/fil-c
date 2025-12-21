/* macOS and iOS have the .h files but the tcp_info struct is private API */
#if defined(HAVE_NETINET_TCP_H) && defined(HAVE_NETINET_IN_H) && defined(HAVE_SYS_SOCKET_H)
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#if defined(TCP_INFO)
#define HAVE_SOCKET_METRIC_HEADERS
#endif
#endif

#include "gsttcpsrcstats.h"

void
gst_tcp_stats_from_socket (GstStructure * structure, GSocket * socket)
{
#ifdef HAVE_SOCKET_METRIC_HEADERS
  if (getsockopt (fd, IPPROTO_TCP, TCP_INFO, &info, &info_len) == 0) {
    /* this is system-specific */
#ifdef HAVE_BSD_TCP_INFO
    gst_structure_set (s,
        "reordering", G_TYPE_UINT, info.__tcpi_reordering,
        "unacked", G_TYPE_UINT, info.__tcpi_unacked,
        "sacked", G_TYPE_UINT, info.__tcpi_sacked,
        "lost", G_TYPE_UINT, info.__tcpi_lost,
        "retrans", G_TYPE_UINT, info.__tcpi_retrans,
        "fackets", G_TYPE_UINT, info.__tcpi_fackets, NULL);
#elif defined(HAVE_LINUX_TCP_INFO)
    gst_structure_set (s,
        "reordering", G_TYPE_UINT, info.tcpi_reordering,
        "unacked", G_TYPE_UINT, info.tcpi_unacked,
        "sacked", G_TYPE_UINT, info.tcpi_sacked,
        "lost", G_TYPE_UINT, info.tcpi_lost,
        "retrans", G_TYPE_UINT, info.tcpi_retrans,
        "fackets", G_TYPE_UINT, info.tcpi_fackets, NULL);
#endif
  }
#endif
}
