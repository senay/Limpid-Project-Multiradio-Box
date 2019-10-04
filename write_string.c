
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#define MAXBUF 65536

int udp_bclient()
{
  int sock, status, buflen, sinlen;
  char buffer[MAXBUF];
  struct sockaddr_in sock_in;
  int yes = 1;

  sinlen = sizeof(struct sockaddr_in);
  memset(&sock_in, 0, sinlen);
  buflen = MAXBUF;

  sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_in.sin_port = htons(0);
  sock_in.sin_family = PF_INET;

  status = bind(sock, (struct sockaddr *)&sock_in, sinlen);
  printf("Bind Status = %d\n", status);

  status = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int) );
  printf("Setsockopt Status = %d\n", status);

  status= setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, "vehi0", strlen("vehi0"));
  printf("Setsockopt Status = %d\n", status);

  sock_in.sin_addr.s_addr=inet_addr("127.0.0.1"); /* send message to 255.255.255.255 */
  sock_in.sin_port = htons(15000); /* port number */
  sock_in.sin_family = PF_INET;

  sprintf(buffer, "denmset 0 104,101,15,0,0,450628810,76589520,10,11,450,10,12,0,0,20,0,0,1\n");
  buflen = strlen(buffer);
  status = sendto(sock, buffer, buflen, 0, (struct sockaddr *)&sock_in, sinlen);
  printf("sendto Status = %d\n", status);
 
  shutdown(sock, 2);
  close(sock);
}
