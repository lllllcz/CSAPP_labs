/*
 * proxy.c - ICS Web proxy
 * 520021911275
 * 李忱泽
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */
// int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

void doit(int clientfd);
int parse_uri(char *uri,char *hostname,char *path,char *port,char *request_head);
void read_requesthdrs(rio_t *rp,int fd);
void return_content(int serverfd, int clientfd);


/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }
    
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];

    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
            client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        /* echo here (in CSAPP:11.4.9) */
        doit(connfd);

        Close(connfd);
    }
    
    exit(0);

    
}

void doit(int clientfd){

    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char hostname[MAXLINE],path[MAXLINE],port[MAXLINE],request_head[MAXLINE];
    int serverfd;
    rio_t rio;


    Rio_readinitb(&rio,clientfd);
    Rio_readlineb(&rio,buf,MAXLINE);
    sscanf(buf,"%s %s %s",method,uri,version);
    if(strcasecmp(method,"GET")){
        printf("Not implemented");
        return;
    }


    parse_uri(uri,hostname,path,port,request_head);


    serverfd = Open_clientfd(hostname,port);


    Rio_writen(serverfd,request_head,strlen(request_head));
    read_requesthdrs(&rio,serverfd);


    return_content(serverfd,clientfd);


}


int parse_uri(char *uri,char *hostname,char *path,char *port,char *request_head){

    sprintf(port,"80");

    char *end,*bp;
    char *tail = uri+strlen(uri);

    char *bg = strstr(uri,"//");

    bg = (bg!=NULL ? bg+2 : uri);
    end = bg;                        

    while(*end != '/' && *end != ':') end++;
    strncpy(hostname,bg,end-bg);

    bp = end + 1;
    if(*end == ':'){
        end++;
        bp = strstr(bg,"/");

        strncpy(port,end,bp-end);   
        end = bp;
    }
    strncpy(path,end,(int)(tail-end)+1);

    
    sprintf(request_head,"GET %s HTTP/1.1\r\n",path);
    
    return 1;
}


void read_requesthdrs(rio_t *rp,int fd){
    
    char buf[MAXLINE];

    // sprintf(buf, "%s", user_agent_hdr);
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Connection: close\r\n");
    // Rio_writen(fd, buf, strlen(buf));
    // sprintf(buf, "Proxy-Connection: close\r\n");
    // Rio_writen(fd, buf, strlen(buf));



    time_t now;
    char time_str[MAXLINE];
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));
    sprintf(buf, "%s", time_str);


    for(Rio_readlineb(rp,buf,MAXLINE);strcmp(buf,"\r\n");Rio_readlineb(rp,buf,MAXLINE)){
        // if(strncmp("Host",buf,4) == 0 || strncmp("User-Agent",buf,10) == 0
        //     || strncmp("Connection",buf,10) == 0 || strncmp("Proxy-Connection",buf,16) == 0)
        //         continue;
        printf("%s",buf);
        Rio_writen(fd,buf,strlen(buf));
    }
    Rio_writen(fd,buf,strlen(buf));


    return;
}


void return_content(int serverfd, int clientfd){

    size_t n;
    char buf[MAXLINE];
    rio_t srio;

    Rio_readinitb(&srio,serverfd);
    while((n = Rio_readlineb(&srio,buf,MAXLINE)) != 0){
        Rio_writen(clientfd,buf,n);
    }
    
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
// int parse_uri(char *uri, char *hostname, char *pathname, char *port)
// {
//     char *hostbegin;
//     char *hostend;
//     char *pathbegin;
//     int len;

//     if (strncasecmp(uri, "http://", 7) != 0) {
//         hostname[0] = '\0';
//         return -1;
//     }

//     /* Extract the host name */
//     hostbegin = uri + 7;
//     hostend = strpbrk(hostbegin, " :/\r\n\0");
//     if (hostend == NULL)
//         return -1;
//     len = hostend - hostbegin;
//     strncpy(hostname, hostbegin, len);
//     hostname[len] = '\0';

//     /* Extract the port number */
//     if (*hostend == ':') {
//         char *p = hostend + 1;
//         while (isdigit(*p))
//             *port++ = *p++;
//         *port = '\0';
//     } else {
//         strcpy(port, "80");
//     }

//     /* Extract the path */
//     pathbegin = strchr(hostbegin, '/');
//     if (pathbegin == NULL) {
//         pathname[0] = '\0';
//     }
//     else {
//         pathbegin++;
//         strcpy(pathname, pathbegin);
//     }

//     return 0;
// }

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    char host[INET_ADDRSTRLEN];

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (inet_ntop(AF_INET, &sockaddr->sin_addr, host, sizeof(host)) == NULL)
        unix_error("Convert sockaddr_in to string representation failed\n");

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %s %s %zu", time_str, host, uri, size);
}
