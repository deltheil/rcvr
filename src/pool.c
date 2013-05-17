#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <pthread.h>

#include "pool.h"
#include "list.h"

#define RCVRPMAXSIZ 4
#define RCVRPINISIZ 2

/** set to 1 to force `CURLOPT_NOSIGNAL` option to 1 */
#define RCVRFORCENOSIGNAL 0

enum {
  RCVR_ASYNCHDNS_UNDEF = -1,
  RCVR_ASYNCHDNS_ON,
  RCVR_ASYNCHDNS_OFF
};

static pthread_once_t rcvr_once = PTHREAD_ONCE_INIT;
static int rcvr_async_dns = RCVR_ASYNCHDNS_UNDEF;

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
static void rcvr_handle_reset(rcvr_handle_t *h);
static void rcvr_curl_check(void);

rcvr_pool_t *rcvr_pool_new(void) {
  pthread_once(&rcvr_once, rcvr_curl_check);
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
  bool err = false;
  rcvr_handle_t *h = NULL;
  int size = rcvr_list_size(p->pool);
  for (int i = 0; i < size; i++) {
    rcvr_handle_t *hcur = rcvr_list_get(p->pool, i);
    if (hcur->available) {
      hcur->available = false;
      h = hcur;
      goto done;
    }
  }
  if (size < RCVRPMAXSIZ) {
    h = rcvr_handle_new();
    h->available = false;
    rcvr_list_push(p->pool, h);
    goto done;
  }
  err = true; /* pool over capacity */
done:
  *curl = h ? h->curl : NULL;
  RCVRPUNLOCK(p);
  return !err;
}

bool rcvr_pool_checkin(rcvr_pool_t *p, CURL *curl) {
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
    if (hcur->curl == curl) {
      rcvr_handle_reset(hcur);
      h = hcur;
      break;
    }
  }
  RCVRPUNLOCK(p);
  return h ? true : false;
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
  rcvr_handle_reset(h);
  return h;
}

void rcvr_handle_del(rcvr_handle_t *h) {
  assert(h);
  curl_easy_cleanup(h->curl);
  free(h);
}

static void rcvr_handle_reset(rcvr_handle_t *h) {
  assert(h);
  curl_easy_reset(h->curl);
#if RCVRFORCENOSIGNAL
  curl_easy_setopt(h->curl, CURLOPT_NOSIGNAL, 1L);
#else
  if (rcvr_async_dns == RCVR_ASYNCHDNS_OFF) {
    /* By default the DNS resolution uses signals to implement the timeout logic
       but this is not thread-safe (the signal could be executed on another
       thread than the original thread that started it). When libcurl is not
       built with async DNS support (threaded resolver or c-ares) one must set
       the `CURLOPT_NOSIGNAL` option to 1. See:
       - http://curl.haxx.se/libcurl/c/libcurl-tutorial.html#Multi-threading
       - http://www.redhat.com/archives/libvir-list/2012-September/msg01960.html
       - http://curl.haxx.se/mail/lib-2013-03/0086.html
       */
    curl_easy_setopt(h->curl, CURLOPT_NOSIGNAL, 1L);
  }
#endif
  h->available = true;
}

static void rcvr_curl_check(void) {
  rcvr_async_dns = RCVR_ASYNCHDNS_ON;
  curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
  if (!(info->features & CURL_VERSION_ASYNCHDNS)) {
    rcvr_async_dns = RCVR_ASYNCHDNS_OFF;
    fprintf(stderr, "warning: libcurl is built without async DNS support\n");
  }
}
