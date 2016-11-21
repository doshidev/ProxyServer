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
	char dataDomain[255];
	int datafd;
	char dataPort[100];
	struct addrinfo *svrInetAddr;
	struct addrinfo	*dataInetAddr;
	struct addrinfo hints;
	int serverfd;
	char resBuf[10000000];
	int resrecd;
};

struct Command {
	char cmd[200];
	char response[1500];
	char rcode[10];
	char rtext[200];
};

struct HttpOK {
	char code[10];
	time_t rawtime;
   	struct tm *info;
   	char length[20];
   	char contenttype[50];
   	char modified[20];
   	char fullresponse[2000];
};

int checkPort(int proxyPort); 
int startServer(int proxyPort);
int connectServer(struct sockaddr* saddr, size_t saddrlen);
void parseheader(struct Request* rptr);
int getai(struct Request* rptr, char type);
int sendr(struct Request* rptr);
int sendf(int fd, char cmd[]);
void rw(struct Request* rptr, int rfd);
int sel(int fd);
int selw(int fd);
void buildResponse(struct HttpOK * r);


int main(int argc, char **argv){
	int fd = 0, r = 0, portno;
	portno = atoi(argv[1]);
	char ftpresponse[1500];

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
				int gainfo = getai(req, 's');
				if(gainfo != 0){
					printf("Get Address Info failed\n");
					close(req->browserfd);			
					break;
				}
				printf("> Get Address Info successful\n");

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
					rw(req, req->serverfd);	
				} else if(strcmp(req->reqType, "ftp") == 0){
					/* Server Connect and Response */
					read(req->serverfd, ftpresponse, 1500);
					printf("\n> FTP Response: %s\n", ftpresponse);

					/* TODO: check if response is 220? */
					if(strncmp(ftpresponse, "220", 3) == 0){
						/* USER */
						struct Command comm;
						bzero(&comm, sizeof(comm));
						strcpy(comm.cmd, "USER anonymous\r\n");
						talk(&comm, req);

						if(strcmp(comm.rcode, "331") == 0 || strcmp(comm.rcode, "230") == 0){

							/* PASSWORD */
							bzero(&comm, sizeof(comm));
							strcpy(comm.cmd, "PASS user@example.com\r\n");
							talk(&comm, req);

							if(strcmp(comm.rcode, "230") == 0){
								/* TYPE I */
								bzero(&comm, sizeof(comm));
								strcpy(comm.cmd, "TYPE I\r\n");
								talk(&comm, req);
								
								if(strcmp(comm.rcode, "200") == 0){
									/* SIZE */
									struct HttpOK h;
									strcpy(h.code, "200");

									bzero(&comm, sizeof(comm));
									strcpy(comm.cmd, "SIZE /");
									strcat(comm.cmd, req->reqFile);
									strcat(comm.cmd, "\r\n");
									talk(&comm, req);
									strcpy(h.length, comm.rtext);

									/* PASV */
									bzero(&comm, sizeof(comm));
									strcpy(comm.cmd, "PASV\r\n");
									talk(&comm, req);
									
									if(strcmp(comm.rcode, "227") == 0){
										/* Parse PASV Response*/
										parsePasv(&comm, req);

										/* Get Addr Info */
										int gainfo1 = getai(req, 'd');
										if(gainfo1 != 0){
											printf("Get Address Info failed\n");
											close(req->browserfd);			
											break;
										}
										printf("\n> Data Get Address Info successful\n");
										/* Connect to Data Server */
										req->datafd = connectServer(req->dataInetAddr->ai_addr, req->dataInetAddr->ai_addrlen);			
										if(req->datafd == -1){
											printf("Connection to Data server failed\n");
											close(req->browserfd);			
											break;
										}

										/* RETR */
										bzero(&comm, sizeof(comm));
										strcpy(comm.cmd, "RETR /");
										strcat(comm.cmd, req->reqFile);
										strcat(comm.cmd, "\r\n");
										talk(&comm, req);

										if(strcmp(comm.rcode, "150") == 0 || strcmp(comm.rcode, "125") == 0){
											buildResponse(&h);
											printf("HTTP Response:\n%s\n", h.fullresponse);
											
											write(req->browserfd, &h.fullresponse, strlen(h.fullresponse));
											rw(req, req->datafd);

											char lastresponse[2000];
											read(req->serverfd, lastresponse, 10000000);
											printf("LR: %s\n", lastresponse);
											if(strncmp(lastresponse, "226", 3) == 0){
												// unsigned char e1 = 0xff;
												// unsigned char e2 = 0x2;
												//write(req->browserfd, &e1, 1);
												//write(req->browserfd, &e2, 1);
												printf("Closing datafd....\n");
												
												bzero(&comm, sizeof(comm));
												strcpy(comm.cmd, "QUIT\r\n");
												close(req->datafd);
												talk(&comm, req);
												close(req->browserfd);
											}
										} else {
											printf("Request Failed: RETR failed!!!\n");
											close(req->browserfd);
										} // RETR

											
									} else {
										printf("Request Failed: PASV Failed!!!\n");
										close(req->browserfd);	
									} // PASV

														
								} else {
									printf("Request Failed: TYPE I failed!!!\n");
									close(req->browserfd);	
								} // TYPE I
										
							} else {
								printf("Request Failed: Authentication failed!!!\n");
								close(req->browserfd);	
							} // PASS
								
						} else {
							printf("Request Failed: USER failed!!!\n");
							close(req->browserfd);
						} // USER
					} else {
						printf("Request Failed: FTP server not ready!!!\n");
						close(req->browserfd);
					} // 220: Server not ready
				} // else if "ftp"

			} // if "GET"
			
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
	printf("SD: %d\n", sd);
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
int getai(struct Request* rptr, char type){
	rptr->hints.ai_family = INADDR_ANY ; 				
	rptr->hints.ai_socktype = SOCK_STREAM;
	if(type == 's')
		return getaddrinfo(rptr->reqDomain, rptr->reqPort, &rptr->hints, &rptr->svrInetAddr);
	if(type == 'd')
		return getaddrinfo(rptr->dataDomain, rptr->dataPort, &rptr->hints, &rptr->dataInetAddr);
}

/* sendr()
 * This function will send the request to the server 
 * @param struct Request * rptr
 */
int sendr(struct Request* rptr){
	rptr->cblen = strlen(rptr->reqHeader);
	return send(rptr->serverfd, rptr->reqHeader, rptr->cblen, 0);
}


/* rw()
 * This function will read the response from server and send it to the client 
 * @param struct Request * rptr
 */
void rw(struct Request* rptr, int rfd){
	rptr->resrecd = 0;
	int totalread = 0;
	if(sel(rfd) > 0 && selw(rptr->browserfd) > 0){
		again:
		do {    
	    	bzero(rptr->resBuf, sizeof(rptr->resBuf));
	    	rptr->resrecd = read(rfd, rptr->resBuf, 10000000);
			totalread += rptr->resrecd;
			
			// printf(" [r: %d] ", rptr->resrecd);
			// printf("%s", rptr->resBuf);
			write(rptr->browserfd, &rptr->resBuf, rptr->resrecd);
			if (rptr->resrecd < 0 && errno == EINTR)
				goto again;
			else if (rptr->resrecd < 0)
				printf("Read Error: %d\n", errno); 
		} while(rptr->resrecd > 0);// End While
		printf("[Total Read: %d]\n\n", totalread);
	}
}

/* sel()
 * This function will use the select function to check if the fd is ready to read/write 
 * @param int fd
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
 * @param int fd
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

/* talk()
 * This function will talk to FTP server - send command and read response
 * @param struct Command * rptr
 * @param struct Request * rptr
 */
int talk(struct Command* ftpcmd, struct Request* rptr){
	char rstring[1500];
	char check1[4], check2[4];
	int i = 0, loop = 0;
	bzero(rstring, sizeof(rstring));
	bzero(check1, 4);
	bzero(check2, 4);
	
	printf("[C]: %s", ftpcmd->cmd);
	
	/* Send Command */
	if(send(rptr->serverfd, ftpcmd->cmd, strlen(ftpcmd->cmd), 0) == -1){
		printf("Send Error\n");
		return -1;
	}
	/* Read Response*/
	do{
		bzero(rstring, sizeof(rstring));
		if( read(rptr->serverfd, rstring, 5000) <= 0){
			printf("Read Error\n");
			break;
			return -1;
		}
		// printf("Response: %s", rstring);
		memcpy(check1, rstring, 3);	
		for(i = 0; i < strlen(rstring); i++){
			if(rstring[i] == ' '){
				memcpy(check2, rstring+i-3, 3);
			}
			if(strcmp(check1, check2) == 0){
				loop = 1;
				break;
			}
		}
	} while(loop < 1);
	
	
	strcpy(ftpcmd->response, rstring);
	printf("[S]: %s", ftpcmd->response);

	/* Parse Response */
	memcpy(ftpcmd->rcode, ftpcmd->response, 3);	
	memcpy(ftpcmd->rtext, ftpcmd->response+4, strlen(ftpcmd->response));	

	return atoi(ftpcmd->rcode);
}


/* parsePasv()
 * This function will use the select function to check if the fd is ready to read/write 
 * @param struct Command * rptr
 * @param struct Request * rptr
 */
int parsePasv(struct Command* ftpcmd, struct Request* rptr){
	int i = 0, start = 0, count = 0, port = 0;
	char ip[20];
	char octet[4];
	char port1[4], port2[4];
	for(i=0; i < strlen(ftpcmd->rtext); i++){
		if(ftpcmd->rtext[i] == '(')
			start = i + 1;

		if(ftpcmd->rtext[i] == ',' && start > 0){
			if(count <= 3){
				memcpy(octet, ftpcmd->rtext+start, i - start);	
				
				if(strlen(ip) > 0){
					strcat(ip, octet);
				} else {
					strcpy(ip, octet);
				}
				
				if(count <= 2)
					strcat(ip, ".");

				start = i + 1;
				count++;
				bzero(&octet, 4);
			} else {
				memcpy(port1, ftpcmd->rtext+start, i - start);	
				start = i + 1;
				count++;
			}
			
		} else if(ftpcmd->rtext[i] == ')'){
			memcpy(port2, ftpcmd->rtext+start, i - start);
			break;
		} 

	} // end for
	port = atoi(port1) * 256 + atoi(port2);
	strcpy(rptr->dataDomain, ip);
	//strcpy(rptr->dataPort, itoa(port));
	sprintf(rptr->dataPort, "%d", port);
	printf("\n - Data Domain: %s:%d\n", ip, port);
}


/* buildResponse()
 * This function will use the build a HTTP 200 response for FTP 
 */
void buildResponse(struct HttpOK * r){
	char w[7][3];
	strcpy(w[0], "Sun") ; 
	strcpy(w[1], "Mon") ; 
	strcpy(w[2], "Tue") ; 
	strcpy(w[3], "Wed") ; 
	strcpy(w[4], "Thu") ; 
	strcpy(w[5], "Fri") ; 
	strcpy(w[6], "Sat") ; 

	char m[13][3];
	strcpy(m[1], "Jan") ; 
	strcpy(m[2], "Feb") ; 
	strcpy(m[3], "Mar") ; 
	strcpy(m[4], "Apr") ; 
	strcpy(m[5], "May") ; 
	strcpy(m[6], "Jun") ; 
	strcpy(m[7], "Jul") ; 
	strcpy(m[8], "Aug") ; 
	strcpy(m[9], "Sep") ; 
	strcpy(m[10], "Oct") ; 
	strcpy(m[11], "Nov") ; 
	strcpy(m[12], "Dec") ; 
	char len[20];

	/* HTTP/1.1 200 OK */
	strcpy(r->fullresponse, "HTTP/1.1 ");
	strcat(r->fullresponse, r->code);
	strcat(r->fullresponse, " OK\r\n");

	time(&r->rawtime);
	r->info = gmtime(&r->rawtime);
		
	/* Date: Mon, 20 Nov 2016 10:28:53 GMT */
	char date[30];
	// sprintf(date, "Date: %c%c%c, %d %c%c%c %d ", w[r->info->tm_wday][0],w[r->info->tm_wday][1],w[r->info->tm_wday][2], r->info->tm_mday, m[r->info->tm_mon][0],m[r->info->tm_mon][1],m[r->info->tm_mon][2], r->info->tm_year+1900);
	strcat(r->fullresponse,"Date: Mon, 20 Nov 2016 10:28:53 GMT\r\n");
	// strcat(r->fullresponse, date);
	// char time[20];
	// sprintf(time, "%d:%d:%d GMT\r\n", r->info->tm_hour, r->info->tm_min, r->info->tm_sec);
	// strcat(r->fullresponse, time);
	strcat(r->fullresponse, "Server: Apache/2.2.14 (Win32)\r\n");
	// strcat(r->fullresponse, "Via: 1.1 DD\r\n");
	strcat(r->fullresponse, "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n");
	strcat(r->fullresponse, "Accept-Ranges: bytes\r\n");
	strcat(r->fullresponse, "Content-Length: ");
	sprintf(len, "%d\r\n", atoi(r->length));
	strcat(r->fullresponse, len);
	strcat(r->fullresponse, "Content-Type: application/octet-stream; charset=binary\r\n");
	strcat(r->fullresponse, "Connection: Closed\r\n\r\n");
}