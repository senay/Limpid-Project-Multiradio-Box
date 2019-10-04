#include <json/json.h>
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <time.h>

void error(const char *msg) { perror(msg); exit(0); }

int publish(int a, int b, int c, int d) {

	//int a=10,b=2,c=11,d=12;
	time_t now;
	//now = time(NULL);
	time(&now);
	char ts[sizeof "1970-01-01T00:00:00+0200"];
	strftime(ts, sizeof ts, "%FT%T%z", localtime(&now));
	printf("Timestamp: %s\n\n", ts);
	char string[4096] ;
	sprintf(string,"{\"stream\": \"BT_device_statistics\",\
                   \"sensor\": \"d9842127-528b-4bbd-a10b-6ccf54616645\",\
                  \"values\": [\
				{\
				  \"time\": \"%s\",\
                                  \"components\": {\
                                  \"pedestrian_cossing\": %d,\
                                  \"pedestrian_sidewalk\": %d,\
                                  \"cars_on_red_light\": %d,\
                                  \"moving_cars\": %d\
                                 }\
                               }\
			]\
               }",ts,a,b,c,d);

		int portno = 80;
	struct hostent *server;
	struct sockaddr_in serv_addr;
	int sockfd, bytes, sent, received, total, message_size;
	char *message, response[4096];
	char *host= "stream.smartdatanet.it";
        char *content = "application/json";
	char *authorization = "Basic bGltcGlkOm90eTVKbTd1WGM=";
	char *method = "POST";
	char *path = "/api/input/limpid";
	FILE *fp;
        fp = fopen( "log.txt", "a" );
	fprintf(fp,"%s\n",string);
	fclose(fp);

	/* How big is the message? */
    	message_size=0;

	message_size+=strlen("%s %s HTTP/1.0\r\n");
        message_size+=strlen(method);                         /* method         */
        message_size+=strlen(path);                           /* path           */
        message_size+=strlen("Content-Length: %d\r\n")+10;    /* content length */

	message_size+=strlen("Content-Type: %d\r\n")+ strlen(content);      /* content type */

	message_size+=strlen("Authorization: %d\r\n")+ strlen(authorization);     /* authorization */

        message_size+=strlen("\r\n");                          /* blank line     */
        message_size+=strlen(string); 

	/* allocate space for the message */
    	message=malloc(message_size);
 
	

        sprintf(message,"%s %s HTTP/1.0\r\n",
            method,                  				/* method         */
            path);                    				/* path           */
        sprintf(message+strlen(message),"Content-Length: %d\r\n",(int) strlen(string)); /*content length*/
        sprintf(message+strlen(message),"Content-Type: %s\r\n",content);                /*content type*/
        sprintf(message+strlen(message),"Authorization: %s\r\n",authorization);         /*authorization*/

        strcat(message,"\r\n");                                /* blank line     */
        strcat(message,string);                                /* body           */

	/* What are we going to send? */
	printf("Request:\n%s\n",message);

	/* create the socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");
	/* lookup the ip address */
	server = gethostbyname(host);
	if (server == NULL) error("ERROR, no such host");
		/* fill in the structure */
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);
	/* connect the socket */
	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	error("ERROR connecting");

	/* send the request */
	total = strlen(message);
	sent = 0;
	do {
	bytes = write(sockfd,message+sent,total-sent);
	if (bytes < 0)
	    error("ERROR writing message to socket");
	if (bytes == 0)
	    break;
	sent+=bytes;
	} while (sent < total);

	/* receive the response */
	memset(response,0,sizeof(response));
	total = sizeof(response)-1;
	received = 0;
	do {
	bytes = read(sockfd,response+received,total-received);
	if (bytes < 0)
	    error("ERROR reading response from socket");
	if (bytes == 0)
	    break;
	received+=bytes;
	} while (received < total);
	

	if (received == total)
	error("ERROR storing complete response from socket");

	/* close the socket */
	close(sockfd);

	/* process response */
	printf("Response:\n%s\n",response);

	free(message);
	return 0;
}
