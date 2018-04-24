/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *        Distributed Programming - Lab2 [Client] == Jacopo Nasi           *
 *      Repo avail: https://github.com/Jacopx/FileTransferProtocol         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include     <stdlib.h>
#include     <string.h>
#include     <inttypes.h>
#include     "../errlib.h"
#include     "../sockwrap.h"

#define BUFLEN	1024 /* BUFFER LENGTH */

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

/* GLOBAL VARIABLES */
char *prog_name;

int main (int argc, char *argv[]) {
	char     buf[BUFLEN];		/* transmission buffer */
	char	   rbuf[BUFLEN];	/* reception buffer */

	uint16_t	   tport_n, tport_h;	/* server port number (net/host ord) */
	uint32_t size = 0, timestamp = 0; /* Size and Time  variable */

	int				s = 0;
	int				result = 0;
	int				bytesReceived = 0, totalBytes = 0;
	struct sockaddr_in	saddr;		/* server address structure */
	struct in_addr	sIPaddr; 	/* server IP addr. structure */
	FILE *fp = NULL; /* File pointer for saving */
	size_t	len;

	prog_name = argv[0];
	if( argc < 4) {
		printf("Usage: prog_nae [IP Address] [Port #] [FileName1] [FileName2] ...");
		exit(2);
	}

	/* Save IP and port # in struct */
	result = inet_aton(argv[1], &sIPaddr);

	if (!result)
		err_sys("Invalid address");

	if (sscanf(argv[2], "%" SCNu16, &tport_h)!=1)
		err_sys("Invalid port number");

	tport_n = htons(tport_h);

	/* create the socket */
	trace( printf("Creating socket\n") );
	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	trace( printf("done. Socket fd number: %d\n",s) );

	/* prepare address structure */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = tport_n;
	saddr.sin_addr   = sIPaddr;

	/* connect */
	showAddr("Connecting to target address", &saddr);
	Connect(s, (struct sockaddr *) &saddr, sizeof(saddr));
	trace( printf("done.\nStarting cycling...") );

	/* One cycle for each file remaining */
	for(int i = 0; i < (argc - 3); ++i) {

		/* Preparing message for server */
		strcpy(buf, "GET ");
		strcat(buf, argv[3 + i]);
		strcat(buf, "\r\n");

		len = strlen(buf);
		if(writen(s, buf, len) != len) {
			trace( err_msg("(%s) -- Write error\n", prog_name) );
			break;
		}

		trace( printf("waiting for file #%d...\n", i) );

		/* Create file where data will be stored */
		fp = fopen(argv[3 + i], "w");
		if(fp == NULL) {
				trace( err_msg("(%s) -- Error opening file", prog_name) );
				return 1;
		}

		memset(rbuf,0,sizeof(rbuf));

		/* Receive RESPONSE info */
		bytesReceived = read(s, rbuf, 5);
		if(bytesReceived == -1) {
			err_sys("(%s) -- Error receving reply #%d...", prog_name, i);
		}
		trace( printf("Bytes received %d: %s\n", bytesReceived, rbuf) );
		if(strstr(rbuf, "-ERR") != NULL) {
			trace( err_msg("(%s) -- Server reply with -ERR\n\tConnection will be closed...\n", prog_name) );
			break;
		}

		/* Get SIZE and MODIFICATION TIMESTAMP */
		readn(s, &size, 4);
		readn(s, &timestamp, 4);

		/* Print basic information */
		printf("Received file %s\n", argv[3 + i]);
		printf("Received file size %u\n", ntohl(size));
		printf("Received file timestamp %u\n", ntohl(timestamp));

		trace( printf("Start reading...\n") );
		memset(rbuf, '\0', sizeof(rbuf));

		totalBytes = 0;
		while((bytesReceived = readn(s, &rbuf, (ntohl(size) - totalBytes < BUFLEN)?ntohl(size) - totalBytes:BUFLEN) ) > 0) {
			if(bytesReceived == -1) {
				err_sys("Error receving file #%d...", i);
			}
			trace( printf("Bytes received %d\n", totalBytes) );
			totalBytes += bytesReceived;
			fwrite(rbuf, 1, bytesReceived, fp);

			if(bytesReceived < BUFLEN ) {
				break;
			}
		}

		trace( printf("File saved with name: %s\n", argv[3 + i]) );
		fclose(fp);

		trace( printf("===========================================================\n") );
	}

	/* Preparing closing messages */
	strcpy(buf, "QUIT\r\n");

	len = strlen(buf);
	if(writen(s, buf, len) != len) {
		trace( err_msg("(%s) -- Closing ended with error\n", prog_name) );
	} else {
		trace( printf("Closed message sended\n") );
	}

	close(s);
	exit(0);
}
