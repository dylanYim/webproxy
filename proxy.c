#include <stdio.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/*End 서버에 요청을 보낼때 사용하는 req 구조체*/
typedef struct request_struct {
    char* uri;
    char* method;
    char* header;
}request;

/*thread에 전달할 포인터를 담은 구조체*/
typedef struct {
    int *connfdp;
    cache *ch;
}vargs;

void *proxy(void *vargp);
void read_requesthdrs(rio_t *rp, char* header, char* dest_host);
void get_response_and_send_to_client(char *dest_host, char *dest_port, rio_t *rp, request *req, int connfd, cache *ch);
void separate_host_and_port(const char *input, char *hostname, char *port);
char* url_to_uri(char *url);

int main(int argc, char **argv) {
  int listnefd, *connfdp;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  cache *ch = Malloc(sizeof(cache));
  cache_init(ch);

  listnefd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listnefd, (SA *) &clientaddr, &clientlen);
    vargs *varg = Malloc(sizeof(vargs));
    varg->connfdp = connfdp;
    varg->ch = ch;

    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    Pthread_create(&tid, NULL, proxy, varg);

  }
}

/*프록시 스레드 함수*/
void *proxy(void *vargp){
  /*캐시 아이템의 멤버가 되는 키 값들은 동적할당 (내부 단편화가 심한 코드 수정 필요)*/
  char buf[MAXLINE],  url[MAXLINE], header[MAXLINE], dest_host[MAXLINE];
  char *method = (char *)Malloc(MAXLINE);
  char *uri = (char *)Malloc(MAXLINE);
  char *dest_hostname = (char *)Malloc(MAXLINE);
  char *dest_port = (char *)Malloc(MAXLINE);
  rio_t rio_client, rio_server;
  request req = {NULL,NULL,NULL};
  cache_item *cache_item;

  /*인자로 받은 connfd 및 cache 포인터 할당 후 구조체 free*/
  int connfd = *(((vargs *) vargp)->connfdp);
  cache *ch = (((vargs *) vargp)->ch);
  Pthread_detach(pthread_self());
  Free(vargp);

  /* Read request line and headers*/
  Rio_readinitb(&rio_client, connfd);
  Rio_readlineb(&rio_client, buf, MAXLINE);

  sscanf(buf, "%s %s", method, url);
  read_requesthdrs(&rio_client, header, dest_host);
  sscanf(url_to_uri(url), "%s", uri);

  req.method = method;
  req.uri = uri;
  req.header = header;

  separate_host_and_port(dest_host, dest_hostname, dest_port);
  cache_item = find_item(ch, dest_hostname, dest_port, method, uri);

  if (cache_item != NULL) {/*cache hit*/
    Rio_writen(connfd, cache_item->response, cache_item->response_size);
    Close(connfd);
  } else{
    get_response_and_send_to_client(dest_hostname, dest_port, &rio_server, &req, connfd, ch);
    Close(connfd);
  }
  return NULL;
}

/*client의 요청 헤더 파싱*/
void read_requesthdrs(rio_t *rp, char* header, char* dest_host){
  char buf[MAXLINE];
  char tmp1[MAXLINE];
  char tmp2[MAXLINE];

  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    sscanf(buf, "%s %s", tmp1, tmp2);
    if(strcasecmp(tmp1, "HOST:") == 0){
      sscanf(tmp2, "%s", dest_host);
    }
    if(strcasecmp(tmp1, "Proxy-Connection:") == 0){
      continue;
    }
    if(strcasecmp(tmp1, "User-Agent:") == 0){
      continue;
    }
    strcat(header, tmp1);
    strcat(header, " ");
    strcat(header, tmp2);
    strcat(header, "\r\n");
  }
  return;
}

/*end server 와 통신 후 응답을 client에 전달하는 함수*/
void get_response_and_send_to_client(char *dest_host, char *dest_port, rio_t *rp, request *req, int connfd, cache *ch){
  int clientfd;
  char request_buf[MAXLINE], response_header_buf[MAXLINE], tmp1[MAXLINE], tmp2[MAXLINE], header[MAXLINE];
  int body_size;
  clientfd = Open_clientfd(dest_host, dest_port);
  /*client 요청 읽기*/
  Rio_readinitb(rp, clientfd);

  /*end server에 요청 및 요청헤더 전송*/
  sprintf(request_buf, "%s %s HTTP/1.0\r\n", req->method, req->uri);
  sprintf(request_buf, "%s%s", request_buf, req->header);
  sprintf(request_buf, "%s%s", request_buf, user_agent_hdr);
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);
  sprintf(request_buf, "%sProxy-Connection: close\r\n\r\n", request_buf);
  Rio_writen(clientfd, request_buf, MAXLINE);

  /*end server 응답 헤더 파싱*/
  Rio_readlineb(rp, response_header_buf, MAXLINE);
  sprintf(header, "%s", response_header_buf);
  while(strcmp(response_header_buf, "\r\n") != 0){
    Rio_readlineb(rp, response_header_buf, MAXLINE);
    sscanf(response_header_buf, "%s %s", tmp1, tmp2);
    sprintf(header, "%s%s", header, response_header_buf);
  }

  /*end server 응답 바디 파싱*/
  char response_body_buf[MAX_OBJECT_SIZE];
  body_size = (int) Rio_readnb(rp, response_body_buf, MAX_OBJECT_SIZE);

  /*end server의 응답을 client로 전송*/
  Rio_writen(connfd, header, strlen(header));
  Rio_writen(connfd, response_body_buf, body_size);

  /*cache에 응답 저장 (더 효율적인 코드로 수정 필요)*/
  char *node_body = (char *)Malloc(body_size);
  memcpy(node_body, response_body_buf, body_size);
  char *node_header = (char *) Malloc(strlen(header));
  strcpy(node_header, header);
  char *node_response = (char *) Malloc(body_size + strlen(header));
  memcpy(node_response,node_header, strlen(header));
  memcpy(node_response + strlen(header),node_body, body_size);

  cache_item *item = Malloc(sizeof(cache_item));
  item->response = node_response;
  item->next = NULL;
  item->prev = NULL;
  item->response_size = body_size + (int)strlen(header);
  item->uri = req->uri;
  item->method = req->method;
  item->port = dest_port;
  item->host = dest_host;

  cache_insert(ch, item);
  Free(node_body);
  Free(node_header);
}

/*endserver host 와 port 분리*/
void separate_host_and_port(const char *input, char *hostname, char *port) {
  const char *colon_ptr = strchr(input, ':');

  if (colon_ptr != NULL) {
    strncpy(hostname, input, colon_ptr - input);
    hostname[colon_ptr - input] = '\0';
    strcpy(port, colon_ptr + 1);
  } else {
    strcpy(hostname, input);
    port = "80";
  }
}

/*url 을 uri로 파싱*/
char* url_to_uri(char *url) {
  char *ptr_protocol = strstr(url, "://");
  char *ptr_path;
  if (ptr_protocol == NULL) {
    ptr_path = strchr(url, '/');
    if (ptr_path == NULL) {
      return strdup("/");
    } else{
      return strdup(ptr_path);
    }
  } else{
    ptr_protocol += 3;
    url = strdup(ptr_protocol);
    ptr_path = strchr(url, '/');
    if (ptr_path == NULL) {
      return strdup("/");
    } else{
      return strdup(ptr_path);
    }
  }
}