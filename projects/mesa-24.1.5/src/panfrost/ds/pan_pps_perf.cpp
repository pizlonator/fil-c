#include "pan_pps_perf.h"

#include <lib/kmod/pan_kmod.h>
#include <perf/pan_perf.h>

#include <pps/pps.h>
#include <util/ralloc.h>

namespace pps {
PanfrostDevice::PanfrostDevice(int fd): fd(fd)
{
   assert(fd >= 0);
}

PanfrostDevice::~PanfrostDevice()
{
}

PanfrostDevice::PanfrostDevice(PanfrostDevice &&o): fd{o.fd}
{
   o.fd = -1;
}

PanfrostDevice &
PanfrostDevice::operator=(PanfrostDevice &&o)
{
   std::swap(fd, o.fd);
   return *this;
}

PanfrostPerf::PanfrostPerf(const PanfrostDevice &dev)
    : perf{reinterpret_cast<struct panfrost_perf *>(
         rzalloc(nullptr, struct panfrost_perf))}
{
   assert(perf);
   assert(dev.fd >= 0);
   panfrost_perf_init(perf, dev.fd);
}

PanfrostPerf::~PanfrostPerf()
{
   if (perf) {
      panfrost_perf_disable(perf);
      ralloc_free(perf);
   }
}

PanfrostPerf::PanfrostPerf(PanfrostPerf &&o): perf{o.perf}
{
   o.perf = nullptr;
}

PanfrostPerf &
PanfrostPerf::operator=(PanfrostPerf &&o)
{
   std::swap(perf, o.perf);
   return *this;
}

int
PanfrostPerf::enable() const
{
   assert(perf);
   return panfrost_perf_enable(perf);
}

void
PanfrostPerf::disable() const
{
   assert(perf);
   panfrost_perf_disable(perf);
}

int
PanfrostPerf::dump() const
{
   assert(perf);
   return panfrost_perf_dump(perf);
}

} // namespace pps
