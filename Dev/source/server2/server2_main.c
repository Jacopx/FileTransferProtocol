/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *        Distributed Programming - Lab3 [Server] == Jacopo Nasi           *
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
#include <libgen.h>
#include <errno.h>

#define RBUFLEN	4096 /* Buffer length */
#define OK "+OK\r\n" /* Deflaut OK message */
#define ERR "-ERR\r\n" /* Deflaut ERROR message */

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

	int conn_request_skt, bklog = 128, s, new;	/* passive socket & listen backlog & socket */
	uint16_t lport_n, lport_h;	/* port used by server (net/host ord.) */
	socklen_t addrlen;
	struct sockaddr_in saddr, caddr;	/* server and client addresses */
	pid_t pid;

	prog_name = argv[0];

	/* Setting handler NOT WAIT CHILD, avoid child to become zombies */
	signal(SIGCHLD, SIG_IGN);

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
		/* accept() managing */
		addrlen = sizeof(struct sockaddr_in);
	again:
		if ( (new = accept(conn_request_skt, (struct sockaddr *) &caddr, &addrlen)) < 0) {
			/* Handling accept() errors */
			if (INTERRUPTED_BY_SIGNAL ||
				errno == EPROTO || errno == ECONNABORTED ||
				errno == EMFILE || errno == ENFILE ||
				errno == ENOBUFS || errno == ENOMEM
					)
				goto again;
			else
				err_msg ("(%s) error - accept() failed", prog_name);
		} else {
			/* Starting service after positive accept() */
			trace( showAddr("Accepted connection from", &caddr) );
			trace( printf("new socket: %u\n",new) );

			/* Concurrent implementation - PROCESS ON DEMAND */
			if ((pid = fork()) < 0)
				err_msg("(%s) error - fork() failed", prog_name);

			if(pid > 0) {
				/* Parent Process */
				close(new);			/* Master close new socket */
			} else {
				/* Child Process */
				close(s);				/* Slave close passive socket */
				service(new); 	/* serve the client on socket NEW */
				trace( printf("Terminated socket: %u\n", new) );
				exit(0);
			}
		}
	}

	return 0;
}

void service(int s) {
  char	buf[RBUFLEN], rbuf[RBUFLEN], handShake[4], *file;		/* Buffers & file pointer */
  int	 	n, fildes, rst = 0, nameLen;
	uint32_t f_size, m_time;

	/* Infinite service loop */
	for (;;) {
			memset(rbuf, '\0', sizeof(rbuf));
	    n = Recv(s, rbuf, RBUFLEN, 0);

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
						 Send(s, ERR, 6, 0);
						 close(s);
						 break;
					 }
				 }

	       trace( printf("Received data from socket %03d :\n", s) );

				 /* Removing CR LF from the ending file */
				 nameLen = strlen(basename(&rbuf[4]));
				 file = calloc(nameLen + 1, sizeof(char));
				 strncpy(file, basename(&rbuf[4]), nameLen - 2);

				 /* Opening file READ-WRITE mode*/
				 fildes = open(file, O_RDONLY);

				 trace( printf("FILE: %s ###\n", file) );

				 if(fildes == -1) {
						 /* Missing file */
						 err_msg("(%s) -- File not found", prog_name);
						 Send(s, ERR, 6, 0);
						 close(fildes);
						 free(file);
						 break;

				 } else {
						 /* File Available */
						 struct stat st;
						 fstat(fildes,&st);

						 f_size=htonl(st.st_size);
						 m_time=htonl(st.st_mtime);

						 /* Sending to client the handshake OK */
						 Send(s, OK, 5, 0);
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
