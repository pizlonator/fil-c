// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_FSTREAM__HPP
#define ASPELL_FSTREAM__HPP

#include <stdio.h>

#include "string.hpp"
#include "istream.hpp"
#include "ostream.hpp"
#include "posib_err.hpp"

// NOTE: See iostream.hpp for the standard stream (ie standard input,
//       output, error)

namespace acommon {
  class String;

  class FStream : public IStream, public OStream
  {
  private:
    FILE * file_;
    bool   own_;

  public:
    FStream(char d = '\n') 
      : IStream(d), file_(0), own_(true) {}
    FStream(FILE * f, bool own = true) 
      : IStream('\n'), file_(f), own_(own) {}
    ~FStream() {close();}

    PosibErr<void> open(ParmStr, const char *);
    void close();
 
    operator bool() {return file_ != 0 && !feof(file_) && !ferror(file_);}

    int get() {return getc(file_);}
    void ignore() {getc(file_);}
    int peek() {int c = getc(file_); ungetc(c, file_); return c;}

    FILE * c_stream();
    int file_no();
    
    int vprintf(const char * format, va_list ap)
    {
      return vfprintf(file_, format, ap);
    }

    void flush() {fflush(file_);}

    // flushes the stream and goes to the beginning of the file
    void restart();

    void skipws();

    // Will return false if there is no more data
    bool append_line(String &, char d);

    // These perform raw io with any sort of formatting
    bool read(void *, unsigned int i);
    void write(ParmStr);
    void write(char c);
    void write(const void *, unsigned int i);

    long int tell() {return ftell(file_);}
    bool seek(long int offset, int whence = SEEK_SET) {
      return fseek(file_, offset, whence) == 0;
    }
    

    // The << >> operators are designed to work about they same
    // as they would with A C++ stream.
    FStream & operator>> (char & c)
    {
      skipws();
      c = getc(file_);
      return *this;
    }
    
    FStream & operator<< (char c)
    {
      putc(c, file_);
      return *this;
    }

    FStream & operator>> (String &);
    FStream & operator>> (unsigned int &);
    FStream & operator>> (int &);
    FStream & operator<< (ParmStr);
    FStream & operator<< (unsigned long);
    FStream & operator<< (unsigned int);
    FStream & operator<< (int);
    FStream & operator<< (double);

  };
}

#endif
