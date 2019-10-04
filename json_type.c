#include <json/json.h>
#include <stdio.h>
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

void error(const char *msg) { perror(msg); exit(0); }

int main() {

	int a=10,b=2,c=11,d=12;
	char string[1024] ;
	sprintf(string,"{\"stream\": \"BT_device_statistics\",\
                   \"sensor\": \"d9842127-528b-4bbd-a10b-6ccf54616645\",\
                  \"values\":[ \"time\" :2015-10-23T15:30:00+0200\",\
                               \"components\": {\
                                \"pedestrian_cossing\":%d,\
                                \"pedestrian_sidewalk\":%d,\
                                \"cars_on_red_light\":%d,\
                                \"moving_cars\":%d\
                                   } \
                           ]\
               }",a,b,c,d);

		int portno = 80;
	struct hostent *server;
	struct sockaddr_in serv_addr;
	int sockfd, bytes, sent, received, total, message_size;
	char message[4096], response[4096];
	char *host= "http://stream.smartdatanet.it/api/input/limpid";
        char *content = "application/json";
	char *authorization = "Basic bGltcGlkOm90eTVKbTd1WGM=";
	char *method = "POST";
	char *sym = "/";

	/* How big is the message? */
    	message_size=0;
        sprintf(message,"%s %s HTTP/1.0\r\n",
            method,                  				/* method         */
            sym);                    				/* path           */

        sprintf(message+strlen(message),"Content-Length: %zd\r\n",strlen(string));
        sprintf(message+strlen(message),"Content-Type: %s\r\n",content);
        sprintf(message+strlen(message),"Authorization: %s\r\n",authorization);
        strcat(message,"\r\n");                                /* blank line     */
        strcat(message,string);                                /* body           */

	/* What are we going to send? */
	//printf("Request:\n%s\n",message);

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

	//free(message);
	return 0;
}
