#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

int create_tcp_socket();
char *get_ip(char *host);
char *build_get_query(char *host, char *page);
void usage();
 
#define PORT 80
#define USERAGENT "HTMLGET 1.0"

typedef struct arg_struct {
    char *host;
    char *page;
    int  start;
    int  id;
    size_t result;
} arg_struct;

 

void usage()
{
  fprintf(stderr, "USAGE: htmlget host [page]\n\
\thost: the website hostname. ex: coding.debuntu.org\n\
\tpage: the page to retrieve. ex: index.html, default: /\n");
}
 
 
int create_tcp_socket()
{
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  return sock;
}
 
 
char *get_ip(char *host)
{
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}
 
char *build_get_query(char *host, char *page)
{
  char *query;
  char *getpage = page;
  char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
  if(getpage[0] == '/'){
    getpage = getpage + 1;
//    fprintf(stderr,"Removing leading \"/\", converting %s to %s\n", page, getpage);
  }
  // -5 is to consider the %s %s %s in tpl and the ending \0
  query = (char *)malloc(strlen(host)+strlen(getpage)+strlen(USERAGENT)+strlen(tpl)-5);
  sprintf(query, tpl, getpage, host, USERAGENT);
  return query;
}

void *FileGetter (void *arguments)
{
  arg_struct *args = (arg_struct *)arguments;
  struct sockaddr_in *remote;
  int sock;
  int tmpres;
  char *ip;
  char *get;
  char buf[BUFSIZ+1];
  char *host;
  char *page;

  struct timeval t1, t2;
  double msu,msl,ms;
  size_t bits;

 
  host = args->host;
  page = args->page;
  printf("Thread ID: %d, time: %d, url: %s\n", args -> id, args -> start, args -> host);
  sock = create_tcp_socket();
  ip = get_ip(host);
  fprintf(stderr, "IP is %s\n", ip);
  remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
  remote->sin_family = AF_INET;
  tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
  if( tmpres < 0)  
  {
    perror("Can't set remote->sin_addr.s_addr");
    exit(1);
  }else if(tmpres == 0)
  {
    fprintf(stderr, "%s is not a valid IP address\n", ip);
    exit(1);
  }
  remote->sin_port = htons(PORT);
 
  if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
    perror("Could not connect");
    exit(1);
  }
  get = build_get_query(host, page);
  //fprintf(stderr, "Query is:\n<<START>>\n%s<<END>>\n", get);
 
  //Send the query to the server
  int sent = 0;
  while(sent < strlen(get))
  {
    tmpres = send(sock, get+sent, strlen(get)-sent, 0);
    if(tmpres == -1){
      perror("Can't send query");
      exit(1);
    }
    sent += tmpres;
  }
  //now it is time to receive the page
  memset(buf, 0, sizeof(buf));
  int htmlstart = 0;
  char * htmlcontent;
  size_t total_bytes;
  while((tmpres = recv(sock, buf, BUFSIZ, 0)) > 0){
    if(htmlstart == 0)
    {
      /* Under certain conditions this will not work.
      * If the \r\n\r\n part is splitted into two messages
      * it will fail to detect the beginning of HTML content
      */
      htmlcontent = strstr(buf, "\r\n\r\n");
      if(htmlcontent != NULL){
        printf("found starting pattern: %s\n", htmlcontent);
        htmlstart = 1;
        htmlcontent += 4;
        gettimeofday(&t1, NULL);
      }
    }else{
      htmlcontent = buf;
    }
    if(htmlstart){
      //fprintf(stdout, "%s", htmlcontent);
    }
 
    memset(buf, 0, tmpres);
    total_bytes = total_bytes + tmpres;
    //printf("total bytez: %zu\n", total_bytes);
  }

   gettimeofday(&t2, NULL);

  if(tmpres < 0)
  {
    perror("Error receiving data");
  }
  if(tmpres = 0)
  {
    printf("exiting recv: %d\n", tmpres);
  }
  free(get);
  free(remote);
  free(ip);
  close(sock);


   msu   = (t2.tv_sec - t1.tv_sec) * 1000.0;     // sec to ms
   msl   = (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
   ms    = msu + msl;
   bits  = total_bytes * 8;
   args->result = bits;
   printf("%zu bits retrieved in %lf ms \n", bits , ms);
   printf("Bandwidth in Kbps= %lf \n", bits/ms);
}
 
double htmlget() {

    pthread_t tid1,tid2,tid3,tid4,tid5,tid6;
    arg_struct args1,args2,args3,args4,args5,args6;
    static char host1[] = "103.18.67.61";
    static char host2[] = "103.18.67.61";
    static char page1[] = "/mini/speedtest/random350x350.jpg";
    static char page2[] = "/mini/speedtest/random350x350.jpg";
    struct timeval t3, t4;
    double mslo,msup,mst;
    double bandwidth;

    args1.id     = 1;
    args1.host   = host1;
    args1.page   = page1;
    args1.start  = 10;
    args2.id     = 2;
    args2.host   = host2;
    args2.page   = page2;
    args2.start  = 10;
    args3.id     = 3;
    args3.host   = host2;
    args3.page   = page2;
    args3.start  = 10;
    args4.id     = 4;
    args4.host   = host2;
    args4.page   = page2;
    args4.start  = 10;

    gettimeofday(&t3, NULL);

    pthread_create(&tid1, NULL, FileGetter , (void *)&args1);
    pthread_create(&tid2, NULL, FileGetter , (void *)&args2);
    pthread_create(&tid3, NULL, FileGetter , (void *)&args3);
    pthread_create(&tid4, NULL, FileGetter , (void *)&args4);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);

    gettimeofday(&t4, NULL);

    size_t total_bits = args1.result + args2.result + args3.result + args4.result;
    printf("Total bits: %zu\n", total_bits);
    msup   = (t4.tv_sec - t3.tv_sec) * 1000.0;     // sec to ms
    mslo   = (t4.tv_usec - t3.tv_usec) / 1000.0;   // us to ms
    mst    = msup + mslo;
    bandwidth = total_bits/mst;
    printf("time elapsed: %lf\n", mst);
    printf("Bandwidth in Kbps= %lf \n", bandwidth);

    return bandwidth;

}
