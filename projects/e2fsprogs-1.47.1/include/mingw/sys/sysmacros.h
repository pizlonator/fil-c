#pragma once

#ifndef makedev
#define makedev(maj, min) (((maj) << 8) + (min))
#endif
