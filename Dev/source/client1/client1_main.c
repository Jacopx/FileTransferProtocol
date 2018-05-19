/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *        Distributed Programming - Lab2 [Client] == Jacopo Nasi           *
 *      Repo avail: https://github.com/Jacopx/FileTransferProtocol         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include	<stdlib.h>
#include	<string.h>
#include	<inttypes.h>
#include	<libgen.h>
#include	"../errlib.h"
#include	"../sockwrap.h"

#define BUFLEN	2048 /* BUFFER LENGTH */
#define TIMEOUT 5 	 /* TIMEOUT [s] */
#define GET "GET "
#define QUIT "QUIT\r\n"
#define CRLF "\r\n"

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
	uint32_t size = 0, timestamp = 0; /* Size and Time variable */

	int	s = 0, result = 0, bytesReceived = 0, totalBytes = 0;
	struct sockaddr_in	saddr;		/* server address structure */
	struct in_addr	sIPaddr; 	/* server IP addr. structure */
	FILE *fp = NULL; /* File pointer for saving */
	size_t	len;
	struct timeval	tval;

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
	trace( showAddr("Connecting to target address", &saddr) );
	Connect(s, (struct sockaddr *) &saddr, sizeof(saddr));
	trace( printf("done.\nStarting cycling...\n") );

	/* Setting timeout in socket option */
	tval.tv_sec = TIMEOUT;
	tval.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tval, sizeof(tval));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tval, sizeof(tval));

	/* One cycle for each file remaining */
	for(int i = 0; i < (argc - 3); ++i) {

		/* Preparing message for server */
		strcpy(buf, GET);
		strcat(buf, argv[3 + i]);
		strcat(buf, CRLF);

		len = strlen(buf);
		Writen(s, buf, len);

		trace( printf("Waiting for file #%d...\n", i) );

		/* Create file where data will be stored */
		fp = Fopen(basename(argv[3 + i]), "w");

		memset(rbuf,'\0',sizeof(rbuf));

		/* Receive RESPONSE info */
		bytesReceived = Readn(s, rbuf, 5);

		trace( printf("Bytes received %d: %s\n", bytesReceived, rbuf) );

		/* Check server reply */
		if(strstr(rbuf, "-ERR") != NULL) {
			err_msg("(%s) -- Server reply with -ERR\n", prog_name);
			/* Close FilePointer and Delete file */
			Fclose(fp);
			if (remove(basename(argv[3 + i])) != 0) {
  			/* Handle error */
				err_msg("(%s) -- File deleting not allowed\n", prog_name);
			}
			break;
		}

		/* Get SIZE and MODIFICATION TIMESTAMP */
		Readn(s, &size, 4);
		Readn(s, &timestamp, 4);

		trace( printf("Start reading...\n") );
		memset(rbuf, '\0', sizeof(rbuf));

		totalBytes = 0;
		while((bytesReceived = Readn(s, &rbuf, (ntohl(size) - totalBytes < BUFLEN)?ntohl(size) - totalBytes:BUFLEN) ) > 0) {

			// trace( printf("Bytes received %d\n", totalBytes) );
			totalBytes += bytesReceived;
			fwrite(rbuf, 1, bytesReceived, fp);

			if(bytesReceived < BUFLEN ) {
				break;
			}
		}

		/* If the file is NOT COMPLETE prompt everything */
		if(totalBytes < ntohl(size)) {
			trace( printf("File not complete...\n") );
			trace( printf("Missing: %d bytes\n", ntohl(size) - totalBytes) );
			err_msg("(%s) -- File received not complete.\n", prog_name);

			/* Close FilePointer and Delete file */
			Fclose(fp);
			if (remove(basename(argv[3 + i])) != 0) {
				/* Handle error */
				err_msg("(%s) -- File deleting not allowed\n", prog_name);
			}
			exit(2);
		}

		Fclose(fp);
		/* Print basic information */
		printf("Received file %s\n", basename(argv[3 + i]));
		printf("Received file size %u\n", ntohl(size));
		printf("Received file timestamp %u\n", ntohl(timestamp));

		trace( printf("===========================================================\n") );
	}

	/* Closing messages */
	strcpy(buf, "QUIT\r\n");
	Writen(s, QUIT, sizeof(QUIT));

	close(s);
	exit(0);
}
