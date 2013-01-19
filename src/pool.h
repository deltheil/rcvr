#ifndef _RCVR_POOL_H
#define _RCVR_POOL_H

#include <rcvr.h>

#include "list.h"

struct rcvr_pool_t_ {
  void *mtx;
  bool open;
  rcvr_list_t *pool;
};

#endif
