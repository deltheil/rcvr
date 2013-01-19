#ifndef _RCVR_H
#define _RCVR_H

#include <stdbool.h>

#include <curl/curl.h>

/* pool of re-usable curl easy handles */
typedef struct rcvr_pool_t_ rcvr_pool_t;

/* create a pool */
rcvr_pool_t *rcvr_pool_new(void);
/* destroy a pool */
void rcvr_pool_del(rcvr_pool_t *p);
/* open a pool before using it (true if OK, false otherwise) */
bool rcvr_pool_open(rcvr_pool_t *p);
/* close a pool after use (true if OK, false otherwise) */
bool rcvr_pool_close(rcvr_pool_t *p);
/* get a curl handle from the pool (true if OK, false otherwise) */
bool rcvr_pool_checkout(rcvr_pool_t *p, CURL **curl);
/* return a connection to the pool (true if OK, false otherwise) */
bool rcvr_pool_checkin(rcvr_pool_t *p, CURL *curl);

#endif
