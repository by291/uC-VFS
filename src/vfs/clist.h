#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "list.h"

typedef list_node_t clist_node_t;

static inline bool clist_is_empty(const clist_node_t *list) {
  return list->next == NULL;
}

static inline void clist_rpush(clist_node_t *list, clist_node_t *new_node) {
  if (list->next) {
    new_node->next = list->next->next;
    list->next->next = new_node;
  } else {
    new_node->next = new_node;
  }
  list->next = new_node;
}

static inline void clist_lpush(clist_node_t *list, clist_node_t *new_node) {
  if (list->next) {
    new_node->next = list->next->next;
    list->next->next = new_node;
  } else {
    new_node->next = new_node;
    list->next = new_node;
  }
}

static inline clist_node_t *clist_lpop(clist_node_t *list) {
  if (list->next) {
    clist_node_t *first = list->next->next;
    if (list->next == first) {
      list->next = NULL;
    } else {
      list->next->next = first->next;
    }
    return first;
  } else {
    return NULL;
  }
}

static inline void clist_lpoprpush(clist_node_t *list) {
  if (list->next) {
    list->next = list->next->next;
  }
}

static inline clist_node_t *clist_lpeek(const clist_node_t *list) {
  if (list->next) {
    return list->next->next;
  }
  return NULL;
}

static inline clist_node_t *clist_rpeek(const clist_node_t *list) {
  return list->next;
}

static inline clist_node_t *clist_rpop(clist_node_t *list) {
  if (list->next) {
    list_node_t *last = list->next;
    while (list->next->next != last) {
      clist_lpoprpush(list);
    }
    return clist_lpop(list);
  } else {
    return NULL;
  }
}

static inline clist_node_t *clist_find_before(const clist_node_t *list,
                                              const clist_node_t *node) {
  clist_node_t *pos = list->next;

  if (!pos) {
    return NULL;
  }
  do {
    pos = pos->next;
    if (pos->next == node) {
      return pos;
    }
  } while (pos != list->next);

  return NULL;
}

static inline clist_node_t *clist_find(const clist_node_t *list,
                                       const clist_node_t *node) {
  clist_node_t *tmp = clist_find_before(list, node);

  if (tmp) {
    return tmp->next;
  } else {
    return NULL;
  }
}

static inline clist_node_t *clist_remove(clist_node_t *list,
                                         clist_node_t *node) {
  if (list->next) {
    if (list->next->next == node) {
      return clist_lpop(list);
    } else {
      clist_node_t *tmp = clist_find_before(list, node);
      if (tmp) {
        tmp->next = tmp->next->next;
        if (node == list->next) {
          list->next = tmp;
        }
        return node;
      }
    }
  }

  return NULL;
}

static inline clist_node_t *clist_foreach(clist_node_t *list,
                                          int (*func)(clist_node_t *, void *),
                                          void *arg) {
  clist_node_t *node = list->next;

  if (node) {
    do {
      node = node->next;
      if (func(node, arg)) {
        return node;
      }
    } while (node != list->next);
  }

  return NULL;
}

typedef int (*clist_cmp_func_t)(clist_node_t *a, clist_node_t *b);

clist_node_t *_clist_sort(clist_node_t *list_head, clist_cmp_func_t cmp);

static inline void clist_sort(clist_node_t *list, clist_cmp_func_t cmp) {
  if (list->next) {
    list->next = _clist_sort(list->next->next, cmp);
  }
}

static inline size_t clist_count(clist_node_t *list) {
  clist_node_t *node = list->next;
  size_t cnt = 0;

  if (node) {
    do {
      node = node->next;
      ++cnt;
    } while (node != list->next);
  }

  return cnt;
}

static inline bool clist_exactly_one(clist_node_t *list) {
  return !clist_is_empty(list) && (list->next == list->next->next);
}

static inline bool clist_more_than_one(clist_node_t *list) {
  return !clist_is_empty(list) && (list->next != list->next->next);
}