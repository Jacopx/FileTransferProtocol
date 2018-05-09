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

#define RBUFLEN	1024 /* Buffer length */
#define TIMEOUT 5 	 /* TIMEOUT [s] */

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

/* FUNCTION PROTOTYPES */
void service(int s);

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
		err_sys("Usage: %s [port number]", prog_name);
		exit(1);
	}

	/* get server port number */
	if (sscanf(argv[1], "%" SCNu16, &lport_h)!=1) {
		err_sys("Invalid port number");
	}

	lport_n = htons(lport_h);

	/* create the TCP socket */
	trace( printf("creating socket...\n") );
	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	trace( printf("done, socket number %u\n",s) );

	/* bind the socket to any local IP address */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family      = AF_INET;
	saddr.sin_port        = lport_n;
	saddr.sin_addr.s_addr = INADDR_ANY;
	trace( showAddr("Binding to address", &saddr) );
	Bind(s, (struct sockaddr *) &saddr, sizeof(saddr));
	trace( printf("done.\n") );

	/* listen */
	trace( printf ("Listening at socket %d with backlog = %d \n",s,bklog) );
	Listen(s, bklog);
	trace( printf("done.\n") );

	conn_request_skt = s;

	/* main server loop */
	for (;;) {
		/* accept next connection */
		addrlen = sizeof(struct sockaddr_in);
		s = Accept(conn_request_skt, (struct sockaddr *) &caddr, &addrlen);
		trace( showAddr("Accepted connection from", &caddr) );
		trace( printf("new socket: %u\n",s) );

		/* serve the client on socket s */
		service(s);
	}

	printf("Routine server closed gracefully\n");

	return 0;
}

void service(int s) {
  char	buf[RBUFLEN], rbuf[RBUFLEN], handShake[4];		/* reception buffer */
  int	 	n, fildes, rst = 0;
	uint32_t f_size, m_time;
	char *file;

	/* Infinite service loop */
	for (;;) {
			memset(rbuf, '\0', sizeof(rbuf));
	    n=Recv(s, rbuf, RBUFLEN, 0);

			if (n < 0) {

	       err_msg("(%s) -- Recv() error", prog_name);
	       close(s);
	       trace( printf("Socket %d closed\n", s) );
	       break;

	    } else {
				 if(strstr(rbuf, "GET ") == NULL) {
					 if(strstr(rbuf, "QUIT") != NULL) {
						 trace( printf("Closed received!\n") );
						 close(s);
						 break;
					 } else {
						 err_msg("(%s) -- Message format not correct", prog_name);
						 break;
					 }
				 }

	       trace( printf("Received data from socket %03d :\n", s) );

				 /* Removing CR LF from the ending file */
				 file = calloc((strlen(&rbuf[4]) - 2), sizeof(char));
				 strncpy(file, &rbuf[4], strlen(&rbuf[4]) - 2);

				 /* Opening file READ-WRITE mode*/
				 fildes = open(file, O_RDWR);

				 trace( printf("FILE: %s ###\n", file) );

				 if(fildes == -1) {
						 /* Missing file */
						 err_msg("(%s) -- File not found", prog_name);
						 Send(s, "-ERR\r\n", 6, 0);
						 close(fildes);
						 free(file);
						 break;

				 } else {
						 /* File Available */
						 struct stat st;
						 fstat(fildes,&st);

						 f_size=htonl(st.st_size);
						 m_time=htonl(st.st_mtime);

						 /* Sending to client the handshake */
						 Send(s, "+OK\r\n", 5, 0);
						 memcpy(handShake, &f_size, 4);
						 Send(s, handShake, sizeof(handShake), 0);
						 memcpy(handShake, &m_time, 4);
						 Send(s, handShake, sizeof(handShake), 0);

						 rst = 0;

						 /* Sending file to client */
						 while ((n = readn(fildes, buf, sizeof(buf))) != 0) {
							 /* Communitaction error will stop the sending but not the server */
							 if(sendn(s, buf, n, 0) == -1) {
								 err_msg("(%s) -- send() failed: broken pipe", prog_name);
								 rst = 1;
								 break;
							 }
						 }

						 /* In case of broken pipe close service */
						 if(rst == 1) {
							 break;
						 }
				 }
				 free(file);
				 close(fildes);
	    }
	}
	/* Exiting from service routine */
	trace( printf("Closing service routine!\n") );
}
