#ifndef _RCVR_LIST_H
#define _RCVR_LIST_H

typedef struct rcvr_list_t_ {
  void **ary;
  int size;
  int alloc;
} rcvr_list_t;

rcvr_list_t *rcvr_list_new(int size);
void rcvr_list_del(rcvr_list_t *list);
void *rcvr_list_get(const rcvr_list_t *list, int index);
void rcvr_list_push(rcvr_list_t *list, void *elem);
void *rcvr_list_pop(rcvr_list_t *list);
int rcvr_list_size(const rcvr_list_t *list);

#endif
