// This file is part of The New Aspell
// Copyright (C) 2002 by Christoph Hintermüller under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
//
// Added by Christoph Hintermüller
// renamed from loadable-filter-API.hpp

#ifndef ASPELL_FILTER_DEBUG__HPP
#define ASPELL_FILTER_DEBUG__HPP

#include <stdio.h>

#ifdef FILTER_PROGRESS_CONTROL
static FILE * controllout=stderr;
#define FDEBUGCLOSE do {\
  if ((controllout != stdout) && (controllout != stderr)) {\
    fclose(controllout);\
    controllout=stderr;\
  } } while (false)

#define FDEBUGNOTOPEN do {\
  if ((controllout == stdout) || (controllout == stderr)) {\
    FDEBUGOPEN; \
  } } while (false)

#define FDEBUGOPEN do {\
  FDEBUGCLOSE; \
  if ((controllout=fopen(FILTER_PROGRESS_CONTROL,"w")) == NULL) {\
    controllout=stderr;\
  }\
  setbuf(controllout,NULL);\
  fprintf(controllout,"Debug Destination %s\n",FILTER_PROGRESS_CONTROL);\
  } while (false)

#define FDEBUG fprintf(controllout,"File: %s(%i)\n",__FILE__,__LINE__)
#define FDEBUGPRINTF(a) fprintf(controllout,a)
#endif // FILTER_PROGRESS_CONTROL

#endif
