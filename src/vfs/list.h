#pragma once

typedef struct list_node {
  struct list_node *next; /**< pointer to next list entry */
} list_node_t;

static inline void list_add(list_node_t *node, list_node_t *new_node) {
  new_node->next = node->next;
  node->next = new_node;
}

static inline list_node_t *list_remove_head(list_node_t *list) {
  list_node_t *head = list->next;

  if (head) {
    list->next = head->next;
  }
  return head;
}

static inline list_node_t *list_remove(list_node_t *list, list_node_t *node) {
  while (list->next) {
    if (list->next == node) {
      list->next = node->next;
      return node;
    }
    list = list->next;
  }
  return list->next;
}