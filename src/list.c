#include <stdlib.h>
#include <assert.h>

#include "list.h"

rcvr_list_t *rcvr_list_new(int size) {
  rcvr_list_t *list = malloc(sizeof(*list));
  if (size < 1) size = 1;
  list->ary = calloc(size, sizeof(void *));
  list->alloc = size;
  list->size = 0;
  return list;
}

void rcvr_list_del(rcvr_list_t *list) {
  assert(list);
  free(list->ary);
  free(list);
}

void *rcvr_list_get(const rcvr_list_t *list, int index) {
  assert(list && index >= 0);
  if (index >= list->size) return NULL;
  return list->ary[index];
}

void rcvr_list_push(rcvr_list_t *list, void *elem) {
  assert(list && elem);
  if (list->size >= list->alloc) {
    list->alloc = 2 * list->alloc;
    list->ary = realloc(list->ary, list->alloc * sizeof(void *));
  }
  list->ary[list->size++] = elem;
}

void *rcvr_list_pop(rcvr_list_t *list) {
  assert(list);
  if (list->size < 1) return NULL;
  return list->ary[--list->size];
}

int rcvr_list_size(const rcvr_list_t *list) {
  assert(list);
  return list->size;
}
