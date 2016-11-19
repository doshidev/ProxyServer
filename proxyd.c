/*
 * File:    proxyd.c
 * Version:	1.0
 * Date:    Nov 12, 2016
 * Desc:    This program is written in C language and is compiled with a gcc,
 * 			the GNU compiler collection (GCC) 4.4.7 20120313 (Red Hat 4.4.7-17). 
 * 			It is tested on AFS server
 *
 * Usage:   Provide a <port> input to the command line while executing the program. 
 *          The proxy server will start listening on that port on the server. 
 */

// Include statements
#include <stdio.h>
#include <stdlib.h>		
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

extern int errno;

struct Request {
	struct 	sockaddr_in clientbrowser;
	int cblen;
	int browserfd;
	char reqHeader[2000000];
	int bytesrecd;
	char reqType[10];
	char reqDomain[255];
	char reqFile[2083];
	char reqPort[10];
	char reqAction[100];
	struct addrinfo *svrInetAddr;
	struct addrinfo hints;
	int serverfd;
	char resBuf[10000000];
	int resrecd;
};


int checkPort(int proxyPort); 
int startServer(int proxyPort);
int connectServer(struct sockaddr* saddr, size_t saddrlen);
void parseheader(struct Request* rptr);
int getai(struct Request* rptr);
int sendr(struct Request* rptr);
int sendf(int fd, char cmd[]);
void rw(struct Request* rptr);
int sel(int fd);
int selw(int fd);


int main(int argc, char **argv){
	int fd = 0, r = 0, portno;
	portno = atoi(argv[1]);

	/* 1. Check Arguments */
	if(argc != 2 || checkPort(atoi(argv[1])) == 0)
	{
		printf("Invalid port! Please enter a permissible numerical port value\n");
		exit(1);
	} 
	printf("\n> Starting Server on port %d...\n", portno);
	
	/* 2. Start Server */
	fd = startServer(portno);
	
	/* 3. For each accepted connection */
	for(r = 0; r >= 0; r++){	
		struct Request request;
		bzero(&request, sizeof(request));
		struct Request *req = &request;
		
		req->cblen = sizeof(req->clientbrowser);
		
		printf("\n\n[%d]. New client request ", r);
		
		req->browserfd = accept(fd, (struct sockaddr *) &req->clientbrowser, &req->cblen); 

		if(req->browserfd == -1){
			printf("- Accept Failed\n");
			// break;
		}

		printf("- Ip:<%x>, Port: <%d>\n", req->clientbrowser.sin_addr.s_addr, req->clientbrowser.sin_port);
		
		/* 4. Read Request */
        req->bytesrecd = 0;
        
        if(sel(req->browserfd) > 0){
			req->bytesrecd = read(req->browserfd, req->reqHeader, 1024);
	        printf("error: %d, fd = %d, bytes = %d\n", errno, req->bytesrecd, req->browserfd);
			printf("Request received from Browser: \n");
			printf("%s\n", req->reqHeader);
        }

		if(req->bytesrecd > 0)	{

			/* 5. Parse Header */
			parseheader(req);
			printf("Parsed Information\n");
			printf("- Action: %s\n", req->reqAction);
			printf("- Type: %s\n", req->reqType);
			printf("- Domain: %s\n", req->reqDomain);
			printf("- Port: %s\n", req->reqPort);
			printf("- File: %s\n\n", req->reqFile);
			
			if(strcmp(req->reqAction, "GET") == 0){
				/* 6. Get Addr Info */
				int gainfo = getai(req);
				if(gainfo != 0){
					printf("Get Address Info failed\n");
					close(req->browserfd);			
					break;
				}
				printf("Get Address Info successful\n");

				/* 7. Connect to Destination Server */
				req->serverfd = connectServer(req->svrInetAddr->ai_addr, req->svrInetAddr->ai_addrlen);
				if(req->serverfd == -1){
					printf("Connection to destination server failed\n");
					close(req->browserfd);			
					break;
				}

				if(strcmp(req->reqType, "http") == 0){
					/* 8. Send Request */
					if(sendr(req) == -1){
						printf("Send request to server failed\n");
						close(req->browserfd);			
						break;	
					}
					/* 9. Read & Write */
					rw(req);	
				} else if(strcmp(req->reqType, "ftp") == 0){
					
				}
			}
			
		} // end if (bytesrecd > 0)
	} // End for		
	
	/* Close Main Socket */
	close(fd); 

}// end main()


// Functions

/* checkPort()
 * This function checks if the port number specified by user is valid
 * @param int port number
 */
int checkPort(int proxyPort){
	if(proxyPort < 1025 || proxyPort > 65535)
		return 0;
	return 1;
}

/* startServer()
 * This function creates a sockets, binds it, and starts listening
 * @param int port number
 * @return int fd
 */
int startServer(int proxyPort){
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int bindstatus, listenstatus;
	struct 	sockaddr_in proxyserver;

	/* Socket: creating new interface */	
	if(fd == -1){
		printf("Socket creation failed\n");
		exit(1);
	}
	printf("> Socket created successfully: %d\n", fd);

	/* Bind: assign address to socket */
	bzero(&proxyserver, sizeof(&proxyserver));
	proxyserver.sin_family = AF_INET;			
	proxyserver.sin_addr.s_addr = INADDR_ANY;	
	proxyserver.sin_port = htons(proxyPort);	
	if((bindstatus = bind(fd, (struct sockaddr *) &proxyserver, sizeof(proxyserver))) == -1 ){
		printf("Bind error\n\n");
		exit(1);
	}
	printf("> Bind successful, Status: %d\n", bindstatus);

	/* Listen */
	if((listenstatus=listen(fd, 1)) == -1 ){
		printf("Listen error");
		exit(1);
	}
	printf("> Listening successfully, Status: %d\n", listenstatus);	
	return fd;
}

/* connectServer()
 * This function creates a new socket and connects to the destination server
 * @param struct server address struct
 * @return int server file descriptor
 */
int connectServer(struct sockaddr* saddr, size_t saddrlen){
	
	/* Socket: creating new interface */	
	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == -1){
		printf("connectServer: Socket creation failed\n");
		exit(1);
	}
	printf("> connectServer: Socket created successfully: %d\n", sd);

	/* Connect */
	if(connect(sd, saddr, saddrlen) == -1 ){
		return -1;
	}
	printf("> Connected to destination server successfully\n");	
	return sd;
}

/* parseheader()
 * This function will parse information 
 * @param struct Request * rptr
 */
void parseheader(struct Request* rptr){
	int i = 0, space = 0, slash = 0, colon = 0;
	int s1 = 0, c = 0, d = 0, f = 0;
	for(i=0; i < strlen(rptr->reqHeader); i++){
		if(rptr->reqHeader[i] == ' ')
			space++;
		else if(rptr->reqHeader[i] == '/'){
			slash++;
			if(slash == 2)
				d = i+1;	
		}
		else if(rptr->reqHeader[i] == ':'){
			colon++;
			if(colon == 2)
				c = i;
		}

		if(rptr->reqHeader[i] == ' ' && space == 1){
			//Action
			memcpy(rptr->reqAction, rptr->reqHeader, i);
			s1 = i;
		} else if(rptr->reqHeader[i] == ':' && colon == 1){
			//Type
			memcpy(rptr->reqType, rptr->reqHeader+s1+1, i-s1-1);
		} else if(rptr->reqHeader[i] == '/' && slash == 3){
			f = i + 1;
			if(colon == 1)
				//Domain
				memcpy(rptr->reqDomain, rptr->reqHeader+d, i-d);
			if(colon == 2){
				//Domain
				memcpy(rptr->reqDomain, rptr->reqHeader+d, c-d);
				//Port
				memcpy(rptr->reqPort, rptr->reqHeader+c+1, i-c-1);
			}
		} else if(rptr->reqHeader[i] == ' ' && space == 2){
			memcpy(rptr->reqFile, rptr->reqHeader+f, i-f);
			break;
		}

		if(rptr->reqHeader[i] == '\n')
			break;
	}

	if(strlen(rptr->reqPort) == 0){
		if(strcmp(rptr->reqType, "ftp") == 0)
			strcpy(rptr->reqPort, "21");
		else if(strcmp(rptr->reqType, "http") == 0)
			strcpy(rptr->reqPort, "80");
	}
}

/* getai()
 * This function will resolve inet addr of the server 
 * @param struct Request * rptr
 */
int getai(struct Request* rptr){
	rptr->hints.ai_family = INADDR_ANY ; 				
	rptr->hints.ai_socktype = SOCK_STREAM;
	return getaddrinfo(rptr->reqDomain, rptr->reqPort, &rptr->hints, &rptr->svrInetAddr);
}

/* sendr()
 * This function will send the request to the server 
 * @param struct Request * rptr
 */
int sendr(struct Request* rptr){
	rptr->cblen = strlen(rptr->reqHeader);
	return send(rptr->serverfd, rptr->reqHeader, rptr->cblen, 0);
}


/* sendf()
 * This function will send the commands for ftp request 
 * @param struct Request * rptr
 */
int sendf(int fd, char cmd[]){
	int cmlen = strlen(cmd);
	return send(fd, &cmd, cmlen, 0);
}


/* rw()
 * This function will read the response from server and send it to the client 
 * @param struct Request * rptr
 */
void rw(struct Request* rptr){
	rptr->resrecd = 0;
	if(sel(rptr->serverfd) > 0 && selw(rptr->browserfd) > 0){
		again:
		do {    
	    	bzero(rptr->resBuf, strlen(rptr->resBuf));
	    	rptr->resrecd = read(rptr->serverfd, rptr->resBuf, 10000000);
			// printf("%s\n", rptr->resBuf);
			printf(" [r: %d] ", rptr->resrecd);
			write(rptr->browserfd, &rptr->resBuf, rptr->resrecd);
			if (rptr->resrecd < 0 && errno == EINTR)
				goto again;
			else if (rptr->resrecd < 0)
				printf("Read Error: %d\n", errno); 
		} while(rptr->resrecd > 0);// End While
		printf("\n");
	}
}


/* sel()
 * This function will use the select function to check if the fd is ready to read/write 
 * @param struct Request * rptr
 */
int sel(int fd){
	int fdplus, rv = 0;
	fd_set myfdset;
	struct timeval tv;
	
	// clear the set ahead of time
	FD_ZERO(&myfdset);

	// wait until either socket has data ready to be recv()d (timeout 2.5 secs)
	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	FD_SET(fd, &myfdset);

	fdplus = fd + 1;

	rv = select(fdplus, &myfdset, NULL, NULL, &tv);
	if (FD_ISSET(fd, &myfdset) == 0)
		return 0;
	return rv;
}


/* selw()
 * This function will use the select function to check if the fd is ready to read/write 
 * @param struct Request * rptr
 */
int selw(int fd){
	int fdplus, rv = 0;
	fd_set myfdset;
	struct timeval tv;
	
	// clear the set ahead of time
	FD_ZERO(&myfdset);

	// wait until either socket has data ready to be recv()d (timeout 2.5 secs)
	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	FD_SET(fd, &myfdset);

	fdplus = fd + 1;

	rv = select(fdplus, NULL, &myfdset, NULL, &tv);
	if (FD_ISSET(fd, &myfdset) == 0)
		return 0;
	return rv;
}