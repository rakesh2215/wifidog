/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/* $Id$ */
/** @file ping_thread.c
    @brief Periodically checks in with the central auth server so the auth
    server knows the gateway is still up.  Note that this is NOT how the gateway
    detects that the central server is still up.
    @author Copyright (C) 2004 Alexandre Carmel-Veilleux <acv@miniguru.ca>
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#include "../config.h"
#include "safe.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "ping_thread.h"
#include "util.h"
#include "centralserver.h"

#define PORT 80
#define USERAGENT "HTMLGET 1.0"

static void ping(void);

extern time_t started_time;

/** Launches a thread that periodically checks in with the wifidog auth server to perform heartbeat function.
@param arg NULL
@todo This thread loops infinitely, need a watchdog to verify that it is still running?
*/  
void
thread_ping(void *arg)
{
	pthread_cond_t		cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t		cond_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct	timespec	timeout;
	
	while (1) {
		/* Make sure we check the servers at the very begining */
		debug(LOG_DEBUG, "Running ping()");
		ping();
		
		/* Sleep for config.checkinterval seconds... */
		timeout.tv_sec = time(NULL) + config_get_config()->checkinterval;
		timeout.tv_nsec = 0;

		/* Mutex must be locked for pthread_cond_timedwait... */
		pthread_mutex_lock(&cond_mutex);
		
		/* Thread safe "sleep" */
		pthread_cond_timedwait(&cond, &cond_mutex, &timeout);

		/* No longer needs to be locked */
		pthread_mutex_unlock(&cond_mutex);
	}
}

/** @internal
 * This function does the actual request.
 */
int create_tcp_socket();
char *get_ip(char *host);
char *build_get_query(char *host, char *page);
void usage();
 
typedef struct arg_struct {
    char *host;
    char *page;
    int  start;
    int  id;
    size_t result;
    double time;
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
  size_t total_bytes = 0;
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
   args->time   = ms;
   printf("%zu bits retrieved in %lf ms \n", bits , ms);
   printf("Bandwidth in Kbps= %lf \n", bits/ms);
}
 
double htmlget() {

    pthread_t tid1,tid2,tid3,tid4,tid5,tid6;
    arg_struct args1,args2,args3,args4,args5,args6;
    static char host1[] = "103.18.67.61";
    static char host2[] = "103.18.67.61";
    static char page1[] = "/mini/speedtest/random500x500.jpg";
    static char page2[] = "/mini/speedtest/random500x500.jpg";
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
    double avg_time   = (args1.time + args2.time + args3.time + args4.time)/4;
    printf("Total bits: %zu\n", total_bits);
    msup   = (t4.tv_sec - t3.tv_sec) * 1000.0;     // sec to ms
    mslo   = (t4.tv_usec - t3.tv_usec) / 1000.0;   // us to ms
    mst    = msup + mslo;
    //bandwidth = total_bits/avg_time;
    bandwidth = total_bits/mst;
    printf("time elapsed: %lf avg time: %lf\n", mst,avg_time);
    printf("Bandwidth in Kbps= %lf \n", bandwidth);

    return bandwidth;

}

static void
ping(void)
{
        ssize_t			numbytes;
        size_t	        	totalbytes;
	int			sockfd, nfds, done;
	char			request[MAX_BUF];
	fd_set			readfds;
	struct timeval		timeout;
	FILE * fh;
	unsigned long int sys_uptime  = 0;
	unsigned int      sys_memfree = 0;
	float             sys_load    = 0;
	t_auth_serv	*auth_server = NULL;
	auth_server = get_auth_server();
	
	debug(LOG_DEBUG, "Entering ping()");
	
	/*
	 * The ping thread does not really try to see if the auth server is actually
	 * working. Merely that there is a web server listening at the port. And that
	 * is done by connect_auth_server() internally.
	 */
	sockfd = connect_auth_server();
	if (sockfd == -1) {
		/*
		 * No auth servers for me to talk to
		 */
		return;
	}

	/*
	 * Populate uptime, memfree and load
	 */
	if ((fh = fopen("/proc/uptime", "r"))) {
		if(fscanf(fh, "%lu", &sys_uptime) != 1)
			debug(LOG_CRIT, "Failed to read uptime");

		fclose(fh);
	}
	if ((fh = fopen("/proc/meminfo", "r"))) {
		while (!feof(fh)) {
			if (fscanf(fh, "MemFree: %u", &sys_memfree) == 0) {
				/* Not on this line */
				while (!feof(fh) && fgetc(fh) != '\n');
			}
			else {
				/* Found it */
				break;
			}
		}
		fclose(fh);
	}
	if ((fh = fopen("/proc/loadavg", "r"))) {
		if(fscanf(fh, "%f", &sys_load) != 1)
			debug(LOG_CRIT, "Failed to read loadavg");

		fclose(fh);
	}

	/*
	 * Prep & send request
	 */
        double ul_speed;
        ul_speed = htmlget();
	snprintf(request, sizeof(request) - 1,
			"GET %s%sgw_id=%s&sys_uptime=%lu&sys_memfree=%u&sys_load=%.2f&ul_speed=%lf&wifidog_uptime=%lu HTTP/1.0\r\n"
			"User-Agent: WiFiDog %s\r\n"
			"Host: %s\r\n"
			"\r\n",
			auth_server->authserv_path,
			auth_server->authserv_ping_script_path_fragment,
			config_get_config()->gw_id,
			sys_uptime,
			sys_memfree,
			sys_load,
                        ul_speed,
			(long unsigned int)((long unsigned int)time(NULL) - (long unsigned int)started_time),
			VERSION,
			auth_server->authserv_hostname);

	debug(LOG_DEBUG, "HTTP Request to Server: [%s]", request);
	
	send(sockfd, request, strlen(request), 0);

	debug(LOG_DEBUG, "Reading response");
	
	numbytes = totalbytes = 0;
	done = 0;
	do {
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		timeout.tv_sec = 30; /* XXX magic... 30 second */
		timeout.tv_usec = 0;
		nfds = sockfd + 1;

		nfds = select(nfds, &readfds, NULL, NULL, &timeout);

		if (nfds > 0) {
			/** We don't have to use FD_ISSET() because there
			 *  was only one fd. */
			numbytes = read(sockfd, request + totalbytes, MAX_BUF - (totalbytes + 1));
			if (numbytes < 0) {
				debug(LOG_ERR, "An error occurred while reading from auth server: %s", strerror(errno));
				/* FIXME */
				close(sockfd);
				return;
			}
			else if (numbytes == 0) {
				done = 1;
			}
			else {
				totalbytes += numbytes;
				debug(LOG_DEBUG, "Read %d bytes, total now %d", numbytes, totalbytes);
			}
		}
		else if (nfds == 0) {
			debug(LOG_ERR, "Timed out reading data via select() from auth server");
			/* FIXME */
			close(sockfd);
			return;
		}
		else if (nfds < 0) {
			debug(LOG_ERR, "Error reading data via select() from auth server: %s", strerror(errno));
			/* FIXME */
			close(sockfd);
			return;
		}
	} while (!done);
	close(sockfd);

	debug(LOG_DEBUG, "Done reading reply, total %d bytes", totalbytes);

	request[totalbytes] = '\0';

	debug(LOG_DEBUG, "HTTP Response from Server: [%s]", request);
	
	if (strstr(request, "Pong") == 0) {
		debug(LOG_WARNING, "Auth server did NOT say pong!");
		/* FIXME */
	}
	else {
		debug(LOG_DEBUG, "Auth Server Says: Pong");
	}

	return;	
}
