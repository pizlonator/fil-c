#include <assert.h>

#include "stack_ptr.hpp"
#include "cache-t.hpp"

namespace acommon {

static GlobalCacheBase * first_cache = 0;
Mutex GlobalCacheBase::global_cache_lock;

void Cacheable::copy() const
{
  //CERR << "COPY\n";
  LOCK(&cache->lock);
  copy_no_lock();
}

void GlobalCacheBase::del(Cacheable * n)
{
  *n->prev = n->next;
  if (n->next) n->next->prev = n->prev;
  n->next = 0;
  n->prev = 0;
}

void GlobalCacheBase::add(Cacheable * n) 
{
  assert(n->refcount > 0);
  n->next = first;
  n->prev = &first;
  if (first) first->prev = &n->next;
  first = n;
  n->cache = this;
}

void GlobalCacheBase::release(Cacheable * d) 
{
  //CERR << "RELEASE\n";
  LOCK(&lock);
  d->refcount--;
  assert(d->refcount >= 0);
  if (d->refcount != 0) return;
  //CERR << "DEL\n";
  if (d->attached()) del(d);
  delete d;
}

void GlobalCacheBase::detach(Cacheable * d)
{
  LOCK(&lock);
  if (d->attached()) del(d);
}

void GlobalCacheBase::detach_all()
{
  LOCK(&lock);
  Cacheable * p = first;
  while (p) {
    *p->prev = 0;
    p->prev = 0;
    p = p->next;
  }
}

void release_cache_data(GlobalCacheBase * cache, const Cacheable * d)
{
  cache->release(const_cast<Cacheable *>(d));
}

GlobalCacheBase::GlobalCacheBase(const char * n)
  : name (n)
{
  LOCK(&global_cache_lock);
  next = first_cache;
  prev = &first_cache;
  if (first_cache) first_cache->prev = &next;
  first_cache = this;
}

GlobalCacheBase::~GlobalCacheBase()
{
  detach_all();
  LOCK(&global_cache_lock);
  *prev = next;
  if (next) next->prev = prev;
}

bool reset_cache(const char * which)
{
  LOCK(&GlobalCacheBase::global_cache_lock);
  bool any = false;
  for (GlobalCacheBase * i = first_cache; i; i = i->next)
  {
    if (which && strcmp(i->name, which) == 0) {i->detach_all(); any = true;}
  }
  return any;
}

extern "C"
int aspell_reset_cache(const char * which)
{
  return reset_cache(which);
}

#if 0

struct CacheableImpl : public Cacheable
{
  class CacheConfig;
  typedef String CacheKey;
  bool cache_key_eq(const CacheKey &);
  static PosibErr<CacheableImpl *> get_new(const CacheKey &, CacheConfig *);
};

template
PosibErr<CacheableImpl *> get_cache_data(GlobalCache<CacheableImpl> *, 
                                         CacheableImpl::CacheConfig *, 
                                         const CacheableImpl::CacheKey &);

#endif

}
