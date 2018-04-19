/*
 * 	File Server0.c
 *	ECHO TCP SERVER with the following features:
 *      - Gets port from keyboard
 *      - SEQUENTIAL: serves one client at a time
 */

#include    <stdlib.h>
#include    <string.h>
#include    <inttypes.h>
#include    "errlib.h"
#include    "sockwrap.h"

#define RBUFLEN		128 /* Buffer length */

/* FUNCTION PROTOTYPES */
int mygetline(char *line, size_t maxline, char *prompt);
void service(int s);

/* GLOBAL VARIABLES */
char *prog_name;

int main(int argc, char *argv[])
{
    int		conn_request_skt;	/* passive socket */
    uint16_t 	lport_n, lport_h;	/* port used by server (net/host ord.) */
    int		bklog = 2;		/* listen backlog */
    int	 	s;			/* connected socket */
    socklen_t 	addrlen;
    struct sockaddr_in 	saddr, caddr;	/* server and client addresses */

    prog_name = argv[0];

    if (argc != 2) {
	printf("Usage: %s <port number>\n", prog_name);
	exit(1);
    }

    /* get server port number */
    if (sscanf(argv[1], "%" SCNu16, &lport_h)!=1)
	err_sys("Invalid port number");
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
    for (;;)
    {
	/* accept next connection */
	addrlen = sizeof(struct sockaddr_in);
	s = Accept(conn_request_skt, (struct sockaddr *) &caddr, &addrlen);
	showAddr("Accepted connection from", &caddr);
	printf("new socket: %u\n",s);

	/* serve the client on socket s */
	service(s);
    }
}

void service(int s) {
    	char	buf[RBUFLEN];		/* reception buffer */
    	int	 	n;

	for (;;)
	{
	    n=recv(s, buf, RBUFLEN-1, 0);
            if (n < 0)
	    {
	       printf("Read error\n");
	       close(s);
	       printf("Socket %d closed\n", s);
	       break;
	    }
	    else if (n==0)
	    {
	       printf("Connection closed by party on socket %d\n",s);
	       close(s);
	       break;
	    }
	    else
	    {
	       printf("Received data from socket %03d :\n", s);
	       buf[n]=0;
	       printf("[%s]\n",buf);
	       if(writen(s, buf, n) != n)
	    	   printf("Write error while replying\n");
	       else
	    	   printf("Reply sent\n");
	    }
	}
}


/* Gets a line of text from standard input after having printed a prompt string
   Substitutes end of line with '\0'
   Empties standard input buffer but stores at most maxline-1 characters in the
   passed buffer
*/
int mygetline(char *line, size_t maxline, char *prompt)
{
	char	ch;
	size_t  i;

	printf("%s", prompt);
	for (i=0; i< maxline-1 && (ch = getchar()) != '\n' && ch != EOF; i++)
		*line++ = ch;
	*line = '\0';
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	if (ch == EOF)
		return(EOF);
	else    return(1);
}
