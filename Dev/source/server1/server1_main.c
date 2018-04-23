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
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define RBUFLEN		128 /* Buffer length */

/* FUNCTION PROTOTYPES */
int clientWriten(int s, char *buf, int n);
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
		printf("Usage: %s [port number]\n", prog_name);
		exit(1);
	}

	/* get server port number */
	if (sscanf(argv[1], "%" SCNu16, &lport_h)!=1) {
		err_sys("Invalid port number");
	}

	lport_n = htons(lport_h);

	/* create the socket */
	printf("creating socket...\n");
	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	printf("done, socket number %u\n",s);

	/* bind the socket to any local IP address */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family      = AF_INET;
	saddr.sin_port        = lport_n;
	saddr.sin_addr.s_addr = INADDR_ANY;
	showAddr("Binding to address", &saddr);
	Bind(s, (struct sockaddr *) &saddr, sizeof(saddr));
	printf("done.\n");

	/* listen */
	printf ("Listening at socket %d with backlog = %d \n",s,bklog);
	Listen(s, bklog);
	printf("done.\n");

	conn_request_skt = s;

	/* main server loop */
	for (;;) {
		/* accept next connection */
		addrlen = sizeof(struct sockaddr_in);
		s = Accept(conn_request_skt, (struct sockaddr *) &caddr, &addrlen);
		showAddr("Accepted connection from", &caddr);
		printf("new socket: %u\n",s);

		/* serve the client on socket s */
		service(s);
	}

	return 0;
}

void service(int s) {
  char	buf[RBUFLEN];		/* reception buffer */
  int	 	n, fildes, f_size;
	char *t;

	/* Infinite service loop */
	for (;;) {
	    n=recv(s, buf, RBUFLEN-1, 0);

			if (n < 0) {

	       printf("Read error\n");
	       close(s);
	       printf("Socket %d closed\n", s);
	       break;

	    } else {

	       printf("Received data from socket %03d :\n", s);
	       buf[n]=0;
				 t = strtok(buf, " ");
	       printf("%s\n", t);
				 t = strtok(NULL, " ");
				 printf("%s", t);

				 fildes = open("small_file1.txt", O_RDWR);
				 strcpy(buf, "");

				 if(fildes == -1) {
						 /* Missing file */
						 printf("File not found! Replying the error to the client...");
						 strcpy(buf, "-ERR\r\n");

						 /* Replying to the client with error */
						 if(writen(s, buf, n) != n) {
							 printf("Write error while replying\n");
						 } else {
							 printf("Reply sent\n");
						 }

				 } else {
					 /* Available file */
					 struct stat st;
				   fstat(fildes,&st);

					 f_size=st.st_size;

					 memset(buf, 0, strlen(buf));

					 strcpy(buf, "+OK\r\n");
					 printf("htnol: %u\nnormal: %u\n", htonl(f_size), f_size);
					 //strcat(buf, htonl(f_size));
					 printf("htnol: %u\nnormal: %u\n", htonl(st.st_mtime), st.st_mtime);
					 // strcat(buf, htonl(st.st_mtime));
					 send(s, buf, n, 0);
					 strcpy(buf, "");

					 while( (n = read(fildes, buf, f_size)) > 0 ){
						 send(s, buf, n, 0);
					 }

					 close(s);
				 }
				 break;

	    }
	}

}
