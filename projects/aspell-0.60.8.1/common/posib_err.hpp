// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef PCOMMON_POSIB_ERR__HPP
#define PCOMMON_POSIB_ERR__HPP

#include "string.hpp"
#include "error.hpp"

#include "errors.hpp"

#include "ndebug.hpp"

namespace acommon {

  // PosibErr<type> is a special Error handling device that will make
  // sure that an error is properly handled.  It is expected to be
  // used as the return type of the function It will automatically
  // convert to the "normal" return type however if the normal
  // returned type is accessed and there is an "unhandled" error
  // condition it will abort It will also abort if the object is
  // destroyed with an "unhandled" error condition.  This includes
  // ignoring the return type of a function returning an error
  // condition.  An error condition is handled by simply checking for
  // the presence of an error, calling ignore, or taking ownership of
  // the error.

  enum WhichErr { PrimErr, SecErr };

  extern "C" const ErrorInfo * const perror_bad_file_format;

  template <typename Ret> class PosibErr;
  
  class PosibErrBase {
  private:
    struct ErrPtr {
      const Error * err;
#ifndef NDEBUG
      bool handled;
#endif
      int refcount;
      ErrPtr(const Error * e) : err(e), 
#ifndef NDEBUG
                                handled(false), 
#endif
                                refcount(1) {}
    };
    ErrPtr * err_;

  protected:

    void posib_handle_err() const {
#ifndef NDEBUG
      if (err_ && !err_->handled)
	handle_err();
#endif
    }

    void copy(const PosibErrBase & other) {
      err_ = other.err_;
      if (err_) {
	++ err_->refcount;
      }
    }
    void destroy() {
      if (err_ == 0) return;
      -- err_->refcount;
      if (err_->refcount == 0) {
#ifndef NDEBUG
	if (!err_->handled)
	  handle_err();
#endif
	del();
      }
    }

  public:
    PosibErrBase() 
      : err_(0) {}
    PosibErrBase(const PosibErrBase & other) 
    {
      copy(other);
    }
    PosibErrBase& operator= (const PosibErrBase & other) {
      destroy();
      copy(other);
      return *this;
    }
    ~PosibErrBase() {
      destroy();
    }

    Error * release_err() {
      if (err_ == 0)
	return 0;
      else
	return release();
    }
    void ignore_err() {
#ifndef NDEBUG
      if (err_ != 0)
	err_->handled = true;
#endif
    }
    const Error * get_err() const {
      if (err_ == 0) {
	return 0;
      } else {
#ifndef NDEBUG
	err_->handled = true;
#endif
	return err_->err;
      }
    }
    const Error * prvw_err() const {
      if (err_ == 0)
	return 0;
      else
	return err_->err;
    }
    bool has_err() const {
      return err_ != 0;
    }
    bool has_err(const ErrorInfo * e) const {
      if (err_ == 0) {
	return false;
      } else {
	if (err_->err->is_a(e)) {
#ifndef NDEBUG
	  err_->handled = true;
#endif
	  return true;
	} else {
	  return false;
	}
      }
    }
    PosibErrBase & prim_err(const ErrorInfo * inf, ParmString p1 = 0,
			    ParmString p2 = 0, ParmString p3 = 0, 
			    ParmString p4 = 0)
    {
      return set(inf, p1, p2, p3, p4);
    }

    // These should only be called _after_ set is called
    PosibErrBase & with_file(ParmString fn, int lineno = 0);
    PosibErrBase & with_key(ParmString prefix, ParmString key);
    
    PosibErrBase & set(const ErrorInfo *, 
		       ParmString, ParmString, ParmString, ParmString);

  private:

#ifndef NDEBUG
    void handle_err() const;
#endif
    Error * release();
    void del();
  };

  template <>
  class PosibErr<void> : public PosibErrBase
  {
  public:
    PosibErr(const PosibErrBase & other) 
      : PosibErrBase(other) {}

    PosibErr() {}
  };

  template <typename Ret>
  class PosibErr : public PosibErrBase
  {
  public:
    PosibErr() {}

    PosibErr(const PosibErrBase & other) 
      : PosibErrBase(other) {}

    template <typename T>
    PosibErr(const PosibErr<T> & other)
      : PosibErrBase(other), data(other.data) {}

    PosibErr(const PosibErr<void> & other)
      : PosibErrBase(other) {}

    PosibErr& operator= (const PosibErr & other) {
      data = other.data;
      PosibErrBase::destroy();
      PosibErrBase::copy(other);
      return *this;
    }
    PosibErr(const Ret & d) : data(d) {}
    operator const Ret & () const {posib_handle_err(); return data;}

    Ret data;
  };

//
//
//
#define RET_ON_ERR_SET(command, type, var) \
  type var;do{PosibErr< type > pe(command);if(pe.has_err())return PosibErrBase(pe);var=pe.data;} while(false)
#define RET_ON_ERR(command) \
  do{PosibErrBase pe(command);if(pe.has_err())return PosibErrBase(pe);}while(false)

  
  //
  //
  //

  static inline PosibErrBase make_err(const ErrorInfo * inf, 
				      ParmString p1 = 0, ParmString p2 = 0,
				      ParmString p3 = 0, ParmString p4 = 0)
  {
    return PosibErrBase().prim_err(inf, p1, p2, p3, p4);
  }

  static const PosibErr<void> no_err;

  //
  //
  //
  inline String & String::operator= (const PosibErr<const char *> & s)
  {
    *this = s.data;
    return *this;
  }

  inline bool operator== (const PosibErr<String> & x, const char * y)
  {
    return x.data == y;
  }
  inline bool operator!= (const PosibErr<String> & x, const char * y)
  {
    return x.data != y;
  }

  inline ParmString::ParmString(const PosibErr<const char *> & s)
    : str_(s.data), size_(UINT_MAX) {}

  inline ParmString::ParmString(const PosibErr<String> & s)
    : str_(s.data.c_str()), size_(s.data.size()) {}

}

#endif
