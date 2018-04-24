/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *        Distributed Programming - Lab2 [Server] == Jacopo Nasi           *
 *      Repo avail: https://github.com/Jacopx/FileTransferProtocol         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../errlib.h"
#include "../sockwrap.h"
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#define RBUFLEN		1024 /* Buffer length */

/* FUNCTION PROTOTYPES */
void service(int s);
void sendError(int s);

/* GLOBAL VARIABLES */
char *prog_name;

int main (int argc, char *argv[]) {

	int					conn_request_skt;	/* passive socket */
	uint16_t 		lport_n, lport_h;	/* port used by server (net/host ord.) */
	int					bklog = 2;		/* listen backlog */
	int	 				s;			/* connected socket */
	socklen_t 	addrlen;
	struct sockaddr_in 	saddr, caddr;	/* server and client addresses */

	prog_name = argv[0];

	/* Argument Check */
	if (argc != 2) {
		printf("Usage: %s [port number]\n", prog_name);
		exit(1);
	}

	/* get server port number */
	if (sscanf(argv[1], "%" SCNu16, &lport_h)!=1) {
		err_sys("Invalid port number");
	}

	lport_n = htons(lport_h);

	/* create the socket */
	// printf("creating socket...\n");
	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	// printf("done, socket number %u\n",s);

	/* bind the socket to any local IP address */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family      = AF_INET;
	saddr.sin_port        = lport_n;
	saddr.sin_addr.s_addr = INADDR_ANY;
	showAddr("Binding to address", &saddr);
	Bind(s, (struct sockaddr *) &saddr, sizeof(saddr));
	// printf("done.\n");

	/* listen */
	// printf ("Listening at socket %d with backlog = %d \n",s,bklog);
	Listen(s, bklog);
	// printf("done.\n");

	conn_request_skt = s;

	/* main server loop */
	for (;;) {
		/* accept next connection */
		addrlen = sizeof(struct sockaddr_in);
		s = Accept(conn_request_skt, (struct sockaddr *) &caddr, &addrlen);
		showAddr("Accepted connection from", &caddr);
		// printf("new socket: %u\n",s);

		/* serve the client on socket s */
		service(s);
	}

	return 0;
}

void service(int s) {
  char	buf[RBUFLEN], rbuf[RBUFLEN], handShake[4];		/* reception buffer */
  int	 	n, fildes, nfile = 0, i;
	uint32_t f_size, m_time;
	char *file;


	/* Infinite service loop */
	for (;;) {
			memset(rbuf, '\0', sizeof(rbuf));
	    n=recv(s, rbuf, RBUFLEN-1, 0);

			if (n < 0) {

	       printf("Read error\n");
	       close(s);
	       printf("Socket %d closed\n", s);
	       break;

	    } else {
				 if(strstr(rbuf, "GET ") == NULL) {
					 if(strstr(rbuf, "QUIT") != NULL) {
						 printf("Closed received!\n");
						 close(s);
						 break;
					 } else {
						 sendError(s);
						 break;
					 }
				 }

	       // printf("Received data from socket %03d :\n", s);

				 // Removing CR LF from the ending file
				 file = calloc((strlen(&rbuf[4]) - 2), sizeof(char));
				 strncpy(file, &rbuf[4], strlen(&rbuf[4]) - 2);

				 printf("FILE: %s ###\n", file);

				 fildes = open(file, O_RDWR);
				 strcpy(rbuf, "");

				 if(fildes == -1) {
						 /* Missing file */
						 printf("File not found! Replying the error to the client...");
						 sendError(s);

				 } else {
						 /* Available file */
						 struct stat st;
						 fstat(fildes,&st);

						 f_size=htonl(st.st_size);
						 m_time=htonl(st.st_mtime);

						 // Sending to client the handshake
						 send(s, "+OK\r\n", 5, 0);
						 memcpy(handShake, &f_size, 4);
						 send(s, handShake, sizeof(handShake), 0);
						 memcpy(handShake, &m_time, 4);
						 send(s, handShake, sizeof(handShake), 0);

						 // Sending file to client
						 while ((n = read(fildes, buf, sizeof(buf))) != 0) {
							 write(s, buf, n);
						 }
						 free(file);
				 }
	    }
	}
	printf("Closing service routine!\n");
}

void sendError(int s) {
	int n;
	/* Replying to the client with error */
	if(writen(s, "-ERR\r\n", n) != n) {
		printf("Write error while replying\n");
	} else {
		printf("Reply sent\n");
	}
}
