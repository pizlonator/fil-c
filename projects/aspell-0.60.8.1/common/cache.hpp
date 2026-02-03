#ifndef ACOMMON_CACHE__HPP
#define ACOMMON_CACHE__HPP

#include "posib_err.hpp"

namespace acommon {

class GlobalCacheBase;
template <class Data> class GlobalCache;

// get_cache_data (both versions) and release_cache_data will acquires
// the cache's lock

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> *, 
                                typename Data::CacheConfig *, 
                                const typename Data::CacheKey &);
template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> *, 
                                typename Data::CacheConfig *, 
                                typename Data::CacheConfig2 *, 
                                const typename Data::CacheKey &);

class Cacheable;
void release_cache_data(GlobalCacheBase *, const Cacheable *);
static inline void release_cache_data(const GlobalCacheBase * c, const Cacheable * d)
{
  release_cache_data(const_cast<GlobalCacheBase *>(c),d);
}

class Cacheable
{
public: // but don't use
  Cacheable * next;
  Cacheable * * prev;
  mutable int refcount;
  GlobalCacheBase * cache;
public:
  bool attached() {return prev;}
  void copy_no_lock() const {refcount++;}
  void copy() const; // Acquires cache->lock
  void release() const {release_cache_data(cache,this);} // Acquires cache->lock
  Cacheable(GlobalCacheBase * c = 0) : next(0), prev(0), refcount(1), cache(c) {}
  virtual ~Cacheable() {}
};

template <class Data>
class CachePtr
{
  Data * ptr;

public:
  void reset(Data * p) {
    if (ptr) ptr->release();
    ptr = p;
  }
  void copy(Data * p) {if (p) p->copy(); reset(p);}
  Data * release() {Data * tmp = ptr; ptr = 0; return tmp;}

  Data & operator*  () const {return *ptr;}
  Data * operator-> () const {return ptr;}
  Data * get()         const {return ptr;}
  operator Data * ()   const {return ptr;}

  CachePtr() : ptr(0) {}
  CachePtr(const CachePtr & other) {ptr = other.ptr; if (ptr) ptr->copy();}
  void operator=(const CachePtr & other) {copy(other.ptr);}
  ~CachePtr() {reset(0);}
};

template <class Data>
PosibErr<void> setup(CachePtr<Data> & res,
                     GlobalCache<Data> * cache, 
                     typename Data::CacheConfig * config, 
                     const typename Data::CacheKey & key) {
  PosibErr<Data *> pe = get_cache_data(cache, config, key);
  if (pe.has_err()) return pe;
  res.reset(pe.data);
  return no_err;
}

bool reset_cache(const char * = 0);

}

#endif
