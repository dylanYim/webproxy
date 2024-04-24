#include "csapp.h"
#include "cache.h"

/*캐시 초기화*/
void cache_init(cache *ch)
{
  ch->root = ch->oldest = NULL;
  ch->size = 0;
  Sem_init(&ch->mutex, 0, 1);
}

/*캐시 삽입*/
void cache_insert(cache *ch, cache_item *item)
{
  if(item->response_size <= MAX_OBJECT_SIZE){
    if((ch->size + item->response_size) <= MAX_CACHE_SIZE){/*캐시 사이즈에 여유가 있는경우*/
      if (ch->root == NULL) {/*캐시가 빈 경우*/
        P(&ch->mutex);
        ch->root = item;
        ch->oldest = item;
        ch->size += item->response_size;
        V(&ch->mutex);
      } else {
        P(&ch->mutex);
        ch->root->prev = item;
        item->next = ch->root;
        ch->root = item;
        ch->size += item->response_size;
        V(&ch->mutex);
      }
    } else{/*캐시 사이즈가 여유 없을 경우*/
      remove_oldest_and_insert_new(ch, item); /*캐시 제일 뒤에 있는 아이템을 삭제 후 현재 아이템을 삽입*/
    }
  }
}

/*캐시 탐색*/
cache_item *find_item(cache *ch, char *host, char *port, char *method, char *uri)
{
  cache_item *curr = ch->root;

  while (curr != NULL){
    if ((strcmp(curr->host, host) == 0) &&
        (strcmp(curr->port, port) == 0) &&
        (strcasecmp(curr->method, method) == 0) &&
        (strcasecmp(curr->uri, uri) == 0))/*키 검증*/
    {
      move_to_root(ch, curr);/*cache hit인 경우 해당 캐시를 캐시 리스트의 루트로 이동시킨다.*/
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

/*캐시의 제일 뒤 아이템을 삭제하고, 새로운 아이템을 캐시에 삽입하는 함수*/
void remove_oldest_and_insert_new(cache *ch, cache_item *item)
{
  P(&ch->mutex);
  cache_item *oldest = ch->oldest;
  cache_item *root = ch->root;
  if (oldest == root) {
    ch->root = item;
    ch->size -= oldest->response_size;
    ch->size += item->response_size;
    delete_node(oldest);
    return;
  }
  ch->size -= oldest->response_size;
  oldest->prev->next = NULL;
  delete_node(oldest);
  root->prev = item;
  item->next = root;
  ch->size += item->response_size;
  ch->root = item;
  V(&ch->mutex);
}

/*cache hit의 경우 해당 item을 캐시 루트로 이동시킨다.*/
void move_to_root(cache *ch, cache_item *item)
{
  cache_item *root = ch->root;
  cache_item *oldest = ch->oldest;

  if (item == root) {
    return;
  }

  P(&ch->mutex);
  if (item == oldest) {
    ch->oldest = item->prev;
    item->next = root;
    item->prev = NULL;
    root->prev = item;
    ch->root = item;
  } else {
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->prev = NULL;
    item->next = root;
    root->prev = item;
    ch->root = item;
  }
  V(&ch->mutex);
}

/*캐시 아이템이 삭제 되는 경우 동적 할당된 item member 및 item 자신 메모리 해제*/
void delete_node(cache_item *item){
  Free(item->host);
  Free(item->port);
  Free(item->method);
  Free(item->uri);
  Free(item->response);
  Free(item);
}