#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
typedef struct request_struct {
    char* uri;
    char* method;
    char* header;
}request;

void *proxy(void *vargp);
void read_requesthdrs(rio_t *rp, char* header, char* dest_host);
void get_response_and_send_to_client(char *dest_host, char *dest_port, rio_t *rp, request *req, int connfd);
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

  listnefd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listnefd, (SA *) &clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    Pthread_create(&tid, NULL, proxy, connfdp);

  }
}

void *proxy(void *vargp){
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], url[MAXLINE], header[MAXLINE], dest_host[MAXLINE], dest_hostname[MAXLINE], dest_port[MAXLINE];
  rio_t rio_client, rio_server;
  request req = {NULL,NULL,NULL, NULL};

  int connfd = *((int *) vargp);
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

  get_response_and_send_to_client(dest_hostname, dest_port, &rio_server, &req, connfd);
  Close(connfd);
  return NULL;
}

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

void get_response_and_send_to_client(char *dest_host, char *dest_port, rio_t *rp, request *req, int connfd){
  int clientfd;
  char request_buf[MAXLINE], response_header_buf[MAXLINE], header[MAXLINE], tmp1[MAXLINE], tmp2[MAXLINE];
  int body_size;
  clientfd = Open_clientfd(dest_host, dest_port);

  Rio_readinitb(rp, clientfd);
  sprintf(request_buf, "%s %s HTTP/1.0\r\n", req->method, req->uri);
  sprintf(request_buf, "%s%s", request_buf, req->header);
  sprintf(request_buf, "%s%s", request_buf, user_agent_hdr);
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);
  sprintf(request_buf, "%sProxy-Connection: close\r\n\r\n", request_buf);

  Rio_writen(clientfd, request_buf, MAXLINE);

  Rio_readlineb(rp, response_header_buf, MAXLINE);
  sprintf(header, "%s", response_header_buf);
  while(strcmp(response_header_buf, "\r\n") != 0){
    Rio_readlineb(rp, response_header_buf, MAXLINE);
    sscanf(response_header_buf, "%s %s", tmp1, tmp2);
    if (strcasecmp(tmp1, "Content-length:") == 0) {
      body_size = atoi(tmp2);
    }
    sprintf(header, "%s%s", header, response_header_buf);
  }

  char response_body_buf[body_size];
  Rio_readnb(rp, response_body_buf, body_size);

  Rio_writen(connfd, header, strlen(header));
  Rio_writen(connfd, response_body_buf, body_size);
}

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