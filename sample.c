#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> /* usleep */
#include <pthread.h>

#include <rcvr.h>

#include "tools/threadpool.h"

#define DIE(...) \
do { \
  fprintf(stderr, "error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0)

/* Delays responding for N seconds */
#define TESTURL "http://httpbin.org/delay/%d"

#define NTHREADS 3
#define NREQS    10
#define STATFMT  "%-20s %02d connection(s) / %02d request(s)\n"

typedef struct {
  rcvr_pool_t *pool;
  int nconn;
  int nreq;
} ctx_t;

pthread_mutex_t ctx_lock;

static void run(ctx_t *ctx, int nthreads, int nreqs);
static void do_request(void *arg);
static size_t write_cb(char *d, size_t n, size_t l, void *p);

int main() {
  curl_global_init(CURL_GLOBAL_ALL);
  pthread_mutex_init(&ctx_lock, NULL);

  /* 1. Basic run w/o pooling */
  ctx_t ctx_basic = {0};
  run(&ctx_basic, NTHREADS, NREQS);

  /* 2. Run with curl easy handle pooling */
  ctx_t ctx_pool = { .pool = rcvr_pool_new() };
  run(&ctx_pool, NTHREADS, NREQS);

  /* Print some stats */
  printf(STATFMT, "pooling OFF", ctx_basic.nconn, ctx_basic.nreq);
  printf(STATFMT, "pooling ON (rcvr)", ctx_pool.nconn, ctx_pool.nreq);

  rcvr_pool_del(ctx_pool.pool);
  pthread_mutex_destroy(&ctx_lock);
  curl_global_cleanup();

  return 0;
}

static void run(ctx_t *ctx, int nthreads, int nreqs) {
  if (ctx->pool) {
    /* start using the curl easy handle pool */
    if (!rcvr_pool_open(ctx->pool)) DIE("pool open failed");
  }

  threadpool_t *pool = threadpool_create(nthreads, nreqs, 0);

  for (int i = 0; i < nreqs; i++) {
    if (threadpool_add(pool, &do_request, ctx, 0) < 0)
      DIE("thread pool failed");
  }

  while (ctx->nreq < nreqs) usleep(10);

  threadpool_destroy(pool, 0);

  if (ctx->pool) {
    /* close the pool after use */
    if (!rcvr_pool_close(ctx->pool)) DIE("pool close failed");
  }

  fprintf(stderr, "\n");
}

static void do_request(void *arg) {
  ctx_t *ctx = (ctx_t *) arg;
  char url[32];
  pthread_mutex_lock(&ctx_lock);
  sprintf(url, TESTURL, ctx->nreq + 1);
  pthread_mutex_unlock(&ctx_lock);

  CURL *curl = NULL;
  if (ctx->pool) {
    /* get a curl handle from the pool */
    if (!rcvr_pool_checkout(ctx->pool, &curl)) DIE("pool checkout failed");
    /* TODO: sleep then retry if no curl handle is available */
  }
  else {
    /* no pooling: create a new handle */
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  }

  /* set custom options */
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  /* ... */

  /* start the transfer */
  CURLcode rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    DIE("curl easy perform failed: %s", curl_easy_strerror(rc));
  }

  /* get how many connections have been made */
  long nconn;
  curl_easy_getinfo(curl, CURLINFO_NUM_CONNECTS, &nconn);

  pthread_mutex_lock(&ctx_lock);
  ctx->nconn += nconn;
  ctx->nreq++;
  pthread_mutex_unlock(&ctx_lock);

  if (ctx->pool) {
    /* return the handle to the pool */
    rcvr_pool_checkin(ctx->pool, curl);
  }
  else {
    curl_easy_cleanup(curl);
  }

  fprintf(stderr, ".");
}

/* NOP write callback */
static size_t write_cb(char *d, size_t n, size_t l, void *p) {
  (void)d;
  (void)p;
  return n*l;
}
