#include <stdlib.h>
#include <assert.h>

#include <pthread.h>

#include "pool.h"
#include "list.h"

#define RCVRPMAXSIZ 4
#define RCVRPINISIZ 2

typedef struct rcvr_handle_t_ {
  CURL *curl;
  bool available;
} rcvr_handle_t;

#define RCVRPLOCK(RCVR_pool)                                                    \
  (pthread_mutex_lock((RCVR_pool)->mtx) != 0 ? false : true)
#define RCVRPUNLOCK(RCVR_pool) do {                                             \
  pthread_mutex_unlock((RCVR_pool)->mtx);                                       \
} while(0)

static void rcvr_pool_fill(rcvr_pool_t *p);
static void rcvr_pool_drain(rcvr_pool_t *p);
static rcvr_handle_t *rcvr_handle_new(void);
static void rcvr_handle_del(rcvr_handle_t *h);

rcvr_pool_t *rcvr_pool_new(void) {
  rcvr_pool_t *p = malloc(sizeof(*p));
  p->mtx = malloc(sizeof(pthread_mutex_t));
  if (pthread_mutex_init(p->mtx, NULL) != 0) {
    pthread_mutex_destroy(p->mtx);
    free(p);
    return NULL;
  }
  p->open = false;
  p->pool = rcvr_list_new(RCVRPMAXSIZ);
  return p;
}

void rcvr_pool_del(rcvr_pool_t *p) {
  assert(p);
  if (p->open) rcvr_pool_close(p);
  if (p->mtx) {
    pthread_mutex_destroy(p->mtx);
    free(p->mtx);
  }
  rcvr_list_del(p->pool);
  free(p);
}

bool rcvr_pool_open(rcvr_pool_t *p) {
  assert(p);
  if (!RCVRPLOCK(p)) return false;
  if (p->open) {
    RCVRPUNLOCK(p);
    return false;
  }
  rcvr_pool_fill(p);
  p->open = true;
  RCVRPUNLOCK(p);
  return true;
}

bool rcvr_pool_close(rcvr_pool_t *p) {
  assert(p);
  if (!RCVRPLOCK(p)) return false;
  if (!p->open) {
    RCVRPUNLOCK(p);
    return false;
  }
  rcvr_pool_drain(p);
  p->open = false;
  RCVRPUNLOCK(p);
  return true;
}

bool rcvr_pool_checkout(rcvr_pool_t *p, CURL **curl) {
  assert(p && curl);
  if (!RCVRPLOCK(p)) return false;
  if (!p->open) {
    RCVRPUNLOCK(p);
    return false;
  }
  rcvr_handle_t *h = NULL;
  int size = rcvr_list_size(p->pool);
  for (int i = 0; i < size; i++) {
    rcvr_handle_t *hcur = rcvr_list_get(p->pool, i);
    if (hcur->available) {
      hcur->available = false;
      h = hcur;
      break;
    }
  }
  if (!h && size < RCVRPMAXSIZ) {
    h = rcvr_handle_new();
    h->available = false;
    rcvr_list_push(p->pool, h);
  }
  *curl = h ? h->curl : NULL;
  RCVRPUNLOCK(p);
  return true;
}

bool rcvr_pool_checkin(rcvr_pool_t *p, CURL *curl) {
  assert(p && curl);
  if (!RCVRPLOCK(p)) return false;
  if (!p->open) {
    RCVRPUNLOCK(p);
    return false;
  }
  int size = rcvr_list_size(p->pool);
  for (int i = 0; i < size; i++) {
    rcvr_handle_t *h = rcvr_list_get(p->pool, i);
    if (h->curl == curl) {
      h->available = true;
      curl_easy_reset(h->curl);
      break;
    }
  }
  RCVRPUNLOCK(p);
  return true;
}

static void rcvr_pool_fill(rcvr_pool_t *p) {
  assert(p);
  for (int i = 0; i < RCVRPINISIZ; i++)
    rcvr_list_push(p->pool, rcvr_handle_new());
}

static void rcvr_pool_drain(rcvr_pool_t *p) {
  assert(p);
  int size = rcvr_list_size(p->pool);
  for (int i = 0; i < size; i++)
    rcvr_handle_del(rcvr_list_pop(p->pool));
}

rcvr_handle_t *rcvr_handle_new(void) {
  rcvr_handle_t *h = malloc(sizeof(*h));
  h->curl = curl_easy_init();
  h->available = true;
  return h;
}

void rcvr_handle_del(rcvr_handle_t *h) {
  assert(h);
  curl_easy_cleanup(h->curl);
  free(h);
}
