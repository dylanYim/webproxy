#ifndef WEBPROXY_CACHE_H
#define WEBPROXY_CACHE_H

#include "csapp.h"
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/*cache list item 구조체*/
typedef struct node_struct {
    char *host;
    char *port;
    char *method;
    char *uri;
    char *response;
    int response_size;
    struct node_struct *prev;
    struct node_struct *next;
}cache_item;

/*cache 구조체*/
typedef struct {
  cache_item *root;
  cache_item *oldest;
  int size;
  sem_t mutex;
} cache;

void cache_init(cache *ch);
void cache_insert(cache *ch, cache_item *item);
cache_item *find_item(cache *ch, char *host, char *port, char *method, char *uri);
void remove_oldest_and_insert_new(cache *ch, cache_item *item);
void move_to_root(cache *ch, cache_item *item);
void delete_node(cache_item *item);
#endif //WEBPROXY_CACHE_H
