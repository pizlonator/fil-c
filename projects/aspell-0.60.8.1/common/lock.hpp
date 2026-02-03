// File: lock.hpp
//
// Copyright (c) 2002,2003,2011
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation.  Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#ifndef DISTRIBNET_LOCK__HPP
#define DISTRIBNET_LOCK__HPP

#include <assert.h>

#include "settings.h"

#ifdef USE_POSIX_MUTEX
#  include <pthread.h>
#endif

namespace acommon {

#define LOCK(l) const Lock the_lock(l);

#ifdef USE_POSIX_MUTEX
  class Mutex {
    pthread_mutex_t l_;
  private:
    Mutex(const Mutex &);
    void operator=(const Mutex &);
  public:
    Mutex() {pthread_mutex_init(&l_, 0);}
    ~Mutex() {pthread_mutex_destroy(&l_);}
    void lock() {pthread_mutex_lock(&l_);}
    void unlock() {pthread_mutex_unlock(&l_);}
  };
#else
  class Mutex {
  private:
    Mutex(const Mutex &);
    void operator=(const Mutex &);
  public:
    Mutex() {}
    ~Mutex() {}
    void lock() {}
    void unlock() {}
  };
#endif

  class Lock {
  private:
    Lock(const Lock &);
    void operator= (const Lock &);
    Mutex * lock_;
  public:
    Lock(Mutex * l) : lock_(l) {if (lock_) lock_->lock();}
    void set(Mutex * l) {assert(!lock_); lock_ = l; if (lock_) lock_->lock();}
    void release() {if (lock_) lock_->unlock(); lock_ = NULL;}
    ~Lock() {if (lock_) lock_->unlock();}
  };
};

#endif
