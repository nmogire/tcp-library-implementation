/* client.c: program to exchange TCP data with a server. */
/* released under the X11 license -- see "license" for details */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>

#include "../common.h"

void usage (char * message)
{
  fprintf (stderr,
	   "usage: client localIP localPort remoteIP remotePort %s\n", 
	   "recvbuf sendbuf filename");
  if (message != NULL)
    fprintf (stderr, "  %s\n", message);
  else
    fprintf (stderr, "  example: client %s\n",
	     "127.0.0.1 1955 127.0.0.1 1954 900 20 client.c");
  exit (1);
}

int main (int argc, char ** argv)
{
  int sock, count;
  struct sockaddr_in sin;
  struct sockaddr * sap = (struct sockaddr *) &sin;
  int portnumber, recv_bufsize, send_bufsize;
  pthread_t receive_thread, send_thread;
  struct receive_args receive_arg;
  struct send_args send_arg;

  if (argc != 8)
    usage (NULL);
  
  /* create the socket */
  if ((sock = tcp_socket (PF_INET, SOCK_STREAM, 0)) < 0)
    error("tcp_socket");

  /* read arguments */
  bzero (&sin, sizeof (sin));  /* same as memset(&sin, 0, sizeof(sin)) */
  sin.sin_family = AF_INET;
  
  portnumber = strtol (argv[2], NULL, 10);
  if ((portnumber == 0) || (portnumber > 0xffff))
    usage ("bad local port number");
  sin.sin_port = htons (portnumber);
  sin.sin_addr.s_addr = inet_addr (argv [1]);
  if (sin.sin_addr.s_addr == 0)
    usage ("bad local IP address");
  local_IP = sin.sin_addr.s_addr;


  if (tcp_bind (sock, sap, sizeof (sin)) != 0)
    error("tcp_bind");

  portnumber = strtol (argv[4], NULL, 10);
  if ((portnumber == 0) || (portnumber > 0xffff))
    usage ("bad remote port number");
  sin.sin_port = htons (portnumber);

  sin.sin_addr.s_addr = inet_addr (argv [3]);
  if (sin.sin_addr.s_addr == 0)
    usage ("bad remote IP address");
  remote_IP = sin.sin_addr.s_addr;

  count = sizeof (sin);
  if (tcp_connect (sock, sap, count) < 0)
    error ("tcp_connect");
//----------------------------------------till here done!!!------------------sent SYN---now working on syn_send--//
  recv_bufsize = strtol (argv [5], NULL, 10);
  send_bufsize = strtol (argv [6], NULL, 10);

  receive_arg.socket = sock;
  receive_arg.bufsize = recv_bufsize;
  pthread_create (&receive_thread, NULL, do_receive, &receive_arg);
  send_arg.socket = sock;
  send_arg.bufsize = send_bufsize;
  send_arg.filename = argv [7];
  pthread_create (&send_thread, NULL, do_send, &send_arg);
  /* before returning, wait for these two to finish */
  if (pthread_join (receive_thread, NULL) < 0) {
    perror ("pthread_join/receive");
    exit (0);
  }
  if (pthread_join (send_thread, NULL) < 0) {
    perror ("pthread_join/send");
    exit (0);
  }
  /* now close the socket */
  tcp_close (sock);
  return 0;
}
