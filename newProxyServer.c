/*
 * File:    newProxyServer.c
 * Date:    Oct 12, 2016
 *
 * Group: 	David Bernstein, Devang Doshi, Jake Mola, Shrey Hitendraprasad Upadhyay
 * Course:  Internet and Higher Level Protocols | Section: 656003 
 *
 * Desc:    The program is developed as part of the class project. 
 *			It is a basic implementation of proxy server. This program is written in
 *			C language and is compiled with a gcc, the GNU compiler collection
 *			(GCC) 4.4.7 20120313 (Red Hat 4.4.7-17). It is tested on school's AFS server
 *
 * Usage:   Provide a <port> input to the command line while executing the program. 
 *          The proxy server will start listening on that port on the server. It can process
 *          HTTP GET requests only. i.e. all the HTTPs request and HTTP request types other than
 *          GET are not processed.  
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

void	 Writen(int, void *, size_t);

int main(int argc, char **argv){
	
	/* Check if there is a valid numerical input */
	
	if(argc != 2 || checkPort(atoi(argv[1])) == 0)
	{
		printf("Invalid port! Please enter a permissible numerical port value\n");
		exit(1);
	} 
	printf("\n> Starting Server on port %d...\n", atoi(argv[1]));
	
	int socketfd = startServer(atoi(argv[1]));

	// Accept
	int c=0;
	int i=0;
	for(c; c >= 0; c++){
		struct 	sockaddr_in clientbrowser;
		int cblen = sizeof(clientbrowser);
		int browserfd;
		browserfd = accept(socketfd, (struct sockaddr *) &clientbrowser, &cblen);
		if(browserfd == -1){
			printf("Accept failed\n");
			exit(1);		
		}
		/* Accept successful */
		printf("\n\n [%d]. New client request. Ip:<%x>, Port: <%d>\n", c, clientbrowser.sin_addr.s_addr, clientbrowser.sin_port);
		printf("Browser FD: %d\n", browserfd);

		/* Read request from Browser */
		int bytesRecieved = 0;
		char recBuf[2000000];
		bzero(recBuf, 2000000);
		int x = 0;
		// Read the request till you find '\r\n\r\n'
		for(x=0; x >= 0; x++){
			printf("Reading request [%d]\n", x);
			bytesRecieved += read(browserfd, recBuf, 500000);
			if(strstr(recBuf, "\r\n\r\n")){
				break;
			}
		}

		// For large requests, read till you get \r\n\r\n (it is end of the header request) 
		// bytesRecieved = read(browserfd, recBuf, 2000000);
		
		printf("Bytes received: %d\n", bytesRecieved);
		printf("Request received from Browser: \n");
		printf("------------------------------|| \n");
		printf("%s\n", recBuf);
		printf("||-----------------\n\n");

		/* Parsing request */
		char requestType[10];
		bzero(requestType, 10);
		int spaceCounter = 0;
		int hostStart = 0;
		int slashCounter = 0;
		char requestHost[2048];
		bzero(requestHost, 2048);
		char httpType[10];
		bzero(httpType, 10);

		for(i=0; i < strlen(recBuf); i++){

			if(recBuf[i] == '/')
				{ slashCounter++; }

			if(recBuf[i] == ' ' && spaceCounter == 0){
				memcpy(requestType, recBuf, i);
				hostStart = i+1;
				spaceCounter++;
				
			}

			if(slashCounter == 3 && recBuf[i] == '/') {
				strncpy(requestHost, recBuf+hostStart, i-hostStart);
			}

			if(recBuf[i] == '\n') { break; }
		}

		int colonCounter = 0;
		char requestDomain[1024];
		char requestPort[100];
		bzero(requestDomain, 1024);
		bzero(requestPort, 100);
		int j=0;

		for(j = 0; j < strlen(requestHost); j++){
			if(requestHost[j] == ':'){
				colonCounter++;
				if(colonCounter == 2){
					memcpy(requestDomain, requestHost+7, j-7);
					strncpy(requestPort, requestHost+j, strlen(requestHost));
					break;
				}
				 
			}
		}
		printf("Colon Counter: %d\n", colonCounter);
		if(colonCounter <= 1){
			strncpy(requestDomain, requestHost+7, strlen(requestHost)-7);
			strcpy(requestPort, "80");
		}

		printf("Parsed Request Type: %s\n", requestType);
		printf("Parsed Hostname: %s\n", requestHost);
		printf("Parsed Domain: %s\n", requestDomain);
		printf("Parsed Port: %s\n", requestPort);

		// Instead of 200 ok send not found or any other error http response codes
		if (!strstr(requestType, "GET")){
			char eresponse[4096] = 
		    "HTTP/1.1 400 OK\n"
		    "Content-Type: text/html\n"
		    "Content-Length: 105\n"
		    "Accept-Ranges: bytes\n"
		    "Connection: close\n"
		    "\n"
		    "<html><body>Error: Proxy cannot process this request: only http &quot;GET&quot; requests are processed.</body></html>";

  			write(browserfd, eresponse, 2000);
  			printf("Response sent to Browser:\n");
  			printf("-------------------------||\n:");
  			write(2, eresponse, 2000); /* print client "req" to screen */
  			printf("\n||--------------\n\n:");

  		} else {
  			struct addrinfo hints, *svrRes;
			memset(&hints, 0, sizeof hints);			
			hints.ai_family = INADDR_ANY ; 				
			hints.ai_socktype = SOCK_STREAM;
			int gia;

			if((gia = getaddrinfo(requestDomain, requestPort, &hints, &svrRes)) != 0){
				printf("getaddrinfo() failed: %d\n", gia);
				// exit(1); instead of exit(1) send http/400 Bad request
				char gresponse[4096] = 
			    "HTTP/1.1 400 OK\n"
			    "Content-Type: text/html\n"
			    "Content-Length: 105\n"
			    "Accept-Ranges: bytes\n"
			    "Connection: close\n"
			    "\n"
			    "<html><body>Error: 400<br/>Bad Request</body></html>";

	  			write(browserfd, gresponse, 2000);
	  			printf("Response sent to Browser:\n");
	  			printf("-------------------------||\n:");
	  			write(2, gresponse, 2000); 
	  			printf("\n||--------------\n\n:");
			} else {
				printf("getaddrinfo(): %x\n\n", svrRes->ai_canonname);
				// Connect to the Server
				int serverfd;
				serverfd = connectServer(svrRes->ai_addr, svrRes->ai_addrlen);

				// Send request to Server
				int svrSend;
				if (svrSend = send(serverfd, recBuf, sizeof(recBuf),0) == -1)
		
				{
					perror("Request to server failed!!!");
					exit(1);
				}

				char svrRecBuf[10000000];
				ssize_t		n;
				again:
					// while ((n = read(serverfd, svrRecBuf, sizeof(svrRecBuf))) > 0)
					while ((n = read(serverfd, svrRecBuf, 10000000)) > 0)
						Writen(browserfd, svrRecBuf, n);

				if (n < 0 && errno == EINTR)
					goto again;
				else if (n < 0)
					printf("str_echo: read error");

				//close browserfd
				bzero(svrRecBuf, 10000000);
				close(serverfd); // Closes the created socket. Returns 0 is successful and -1 if not
			}	
  		} // End if (!"GET")

  		//close browserfd
		bzero(recBuf, 2000000);
		close(browserfd); // Closes the created socket. Returns 0 is successful and -1 if not

		

	} // End for loop
	
	close(socketfd);

	exit(0);

}// end main()

// Functions

/* checkPort()
 * This function checks if the port number specified by user is valid
 * @param int port number
 * @return 0 for not valid and 1 for valid
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
		printf("connectServer: Connect failed\n");
		exit(1);
	}
	printf("> Connected to destination server successfully\n");	
	return sd;
}


// Writen function from the unp book example
// This function is called by the below function
/* Write "n" bytes to a descriptor. */
ssize_t	writen(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}

void
Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		printf("writen error");
}