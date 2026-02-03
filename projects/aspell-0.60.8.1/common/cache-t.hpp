#ifndef ACOMMON_CACHE_T__HPP
#define ACOMMON_CACHE_T__HPP

#include "lock.hpp"
#include "cache.hpp"

//#include "iostream.hpp"

namespace acommon {

class GlobalCacheBase
{
public:
  mutable Mutex lock;
public: // but don't use
  const char * name;
  GlobalCacheBase * next;
  GlobalCacheBase * * prev;
  // The global cache lock must exist while any cache instance is active
  static Mutex global_cache_lock;
protected:
  Cacheable * first;
  void del(Cacheable * d);
  void add(Cacheable * n);
  GlobalCacheBase(const char * n);
  ~GlobalCacheBase();
public:
  void release(Cacheable * d);
  void detach(Cacheable * d);
  void detach_all();
};

template <class D>
class GlobalCache : public GlobalCacheBase
{
public:
  typedef D Data;
  typedef typename Data::CacheKey Key;
public:
  GlobalCache(const char * n) : GlobalCacheBase(n) {}
  // "find" and "add" will _not_ acquire a lock
  Data * find(const Key & key) {
    D * cur = static_cast<D *>(first);
    while (cur && !cur->cache_key_eq(key))
      cur = static_cast<D *>(cur->next);
    return cur;
  }
  void add(Data * n) {GlobalCacheBase::add(n);}
  // "release" and "detach" _will_ acquire a lock
  void release(Data * d) {GlobalCacheBase::release(d);}
  void detach(Data * d) {GlobalCacheBase::detach(d);}
};

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                typename Data::CacheConfig * config, 
                                const typename Data::CacheKey & key)
{
  LOCK(&cache->lock);
  Data * n = cache->find(key);
  //CERR << "Getting " << key << " for " << cache->name << "\n";
  if (n) {
    n->refcount++;
    return n;
  }
  PosibErr<Data *> res = Data::get_new(key, config);
  if (res.has_err()) {
    //CERR << "ERROR\n"; 
    return res;
  }
  n = res.data;
  cache->add(n);
  //CERR << "LOADED FROM DISK\n";
  return n;
}

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                typename Data::CacheConfig * config, 
                                typename Data::CacheConfig2 * config2,
                                const typename Data::CacheKey & key)
{
  LOCK(&cache->lock);
  Data * n = cache->find(key);
  //CERR << "Getting " << key << "\n";
  if (n) {
    n->refcount++;
    return n;
  }
  PosibErr<Data *> res = Data::get_new(key, config, config2);
  if (res.has_err()) {
    //CERR << "ERROR\n"; 
    return res;
  }
  n = res.data;
  cache->add(n);
  //CERR << "LOADED FROM DISK\n";
  return n;
}

}

#endif
