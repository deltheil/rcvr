#include <stdlib.h>
#include <stdio.h>

#include <rcvr.h>

#define DIE(...) \
do { \
  fprintf(stderr, "error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0)

/* Digest Auth testing thanks to http://httpbin.org */
#define USERPWD "user:pass"
#define TESTURL "http://httpbin.org/digest-auth/auth/user/pass"

int main() {
  rcvr_pool_t *pool = rcvr_pool_new();

  /* start using the pool */
  if (!rcvr_pool_open(pool)) DIE("pool open failed");

  for (int i = 0; i < 2; i++) {
    /* get a curl handle from the pool */
    CURL *curl = NULL;
    if (!rcvr_pool_checkout(pool, &curl)) DIE("pool checkout failed");

    /* set current options */
    curl_easy_setopt(curl, CURLOPT_URL, TESTURL);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    curl_easy_setopt(curl, CURLOPT_USERPWD, USERPWD);

    /* start the transfer */
    curl_easy_perform(curl);

    /* return the handle to the pool */
    rcvr_pool_checkin(pool, curl);
  }

  /* close the pool after use */
  if (!rcvr_pool_close(pool)) DIE("pool close failed");

  rcvr_pool_del(pool);

  return 0;
}
