/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *        Distributed Programming - Lab2 [Client] == Jacopo Nasi           *
 *      Repo avail: https://github.com/Jacopx/FileTransferProtocol         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>
#include     "../errlib.h"
#include     "../sockwrap.h"

#define BUFLEN	128 /* BUFFER LENGTH */

/* FUNCTION PROTOTYPES */


/* GLOBAL VARIABLES */
char *prog_name;

int main (int argc, char *argv[]) {
	char     buf[BUFLEN];		/* transmission buffer */
	char	   rbuf[BUFLEN];	/* reception buffer */

	uint16_t	   tport_n, tport_h;	/* server port number (net/host ord) */

	int				s = 0;
	int				result = 0;
	int				bytesReceived = 0;
	struct sockaddr_in	saddr;		/* server address structure */
	struct in_addr	sIPaddr; 	/* server IP addr. structure */
	FILE *fp = NULL; /* File pointer for saving */

	prog_name = argv[0];
	if( argc < 4) {
		printf("Usage: prog_nae [IP Address] [Port #] [FileName1] [FileName2] ...");
		exit(2);
	}


	/* Save IP and port # in struct */
	result = inet_aton(argv[1], &sIPaddr);

	if (!result)
		err_quit("Invalid address");

	if (sscanf(argv[2], "%" SCNu16, &tport_h)!=1)
		err_quit("Invalid port number");

	tport_n = htons(tport_h);

	/* create the socket */
	printf("Creating socket\n");
	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	printf("done. Socket fd number: %d\n",s);

	/* prepare address structure */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = tport_n;
	saddr.sin_addr   = sIPaddr;

	/* connect */
	showAddr("Connecting to target address", &saddr);
	Connect(s, (struct sockaddr *) &saddr, sizeof(saddr));
	printf("done.\n");

	/* main client loop */
	printf("Enter line 'close' or 'stop' to close connection and stop client.\n");

	/* One cycle for each file remaining */
	for(int i = 0; i < (argc - 3); ++i) {
		size_t	len;

		/* Cleaning buffer */
		memset(buf,0,sizeof(buf));
		/* Preparing message for server */
		strcat(buf,"GET ");
		strcat(buf, argv[4 + i]);
		strcat(buf, "\r\n");

		len = strlen(buf);
		if(writen(s, buf, len) != len) {
			printf("Write error\n");
			break;
		}

		printf("waiting for file #%d...\n", i);

		/* Create file where data will be stored */
		fp = fopen(argv[4 + i], "ab");
		if(NULL == fp) {
				printf("Error opening file");
				return 1;
		}

		/* Receive data in chunks of 256 bytes */
		while((bytesReceived = read(s, rbuf, BUFLEN)) > 0) {
			if(bytesReceived == -1) {
				err_quit("Error receving file #%d...", i);
			}
			printf("Bytes received %d\n", bytesReceived);
			fwrite(rbuf, 1, bytesReceived, fp);
		}

		printf("File saved with name: %s\n", argv[4 + i]);
		fclose(fp);

		printf("===========================================================\n");
	}

	close(s);
	exit(0);
}
