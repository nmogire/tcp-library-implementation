/* server.c: program to exchange TCP data with a client. */
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
#include <string.h>
#include "../common.h"

void usage (char * message)
{
  fprintf (stderr,
	   "usage: server localIP localPort remoteIP %s\n",
           "recvbuf sendbuf filename");
  if (message != NULL)
    fprintf (stderr, "  %s\n", message);
  else
    fprintf (stderr,
	     "  example: server 127.0.0.1 1954 127.0.0.1 200 900 server.c\n");
  exit (1);
}

int main (int argc, char ** argv)
{
  int passive, session, count;
  struct sockaddr_in sin;
  struct sockaddr * sap = (struct sockaddr *) &sin;
  int portnumber, recv_bufsize, send_bufsize;
  pthread_t receive_thread, send_thread;
  struct receive_args receive_arg;
  struct send_args send_arg;

  if (argc != 7)
    usage (NULL);
  
  /* read arguments */
  memset (&sin, 0, sizeof (sin));
  sin.sin_family = AF_INET;

  portnumber = strtol (argv[2], NULL, 10);
  if ((portnumber == 0) || (portnumber > 0xffff))
    usage ("bad port number");
  sin.sin_port = htons (portnumber);

  sin.sin_addr.s_addr = inet_addr (argv [1]);
  if (sin.sin_addr.s_addr == 0)
    usage ("bad IP address");
  local_IP = sin.sin_addr.s_addr;

  remote_IP = inet_addr (argv [3]);

  recv_bufsize = strtol (argv [4], NULL, 10);
  send_bufsize = strtol (argv [5], NULL, 10);


  if ((passive = tcp_socket (PF_INET, SOCK_STREAM, 0)) < 0)
    error("tcp_socket");
  if (tcp_bind (passive, sap, sizeof (sin)) != 0)
    error("tcp_bind");
  /* specify the maximum queue length and accept connections */
  if(tcp_listen (passive, 1) < 0) error("tcp_listen");

//nancy comment...start accepting!!
  count = sizeof (sin);
  while ((session = tcp_accept (passive, sap, &count)) >= 0) {
    receive_arg.socket = session;
    receive_arg.bufsize = recv_bufsize;
    pthread_create (&receive_thread, NULL, do_receive, &receive_arg);
    send_arg.socket = session;
    send_arg.bufsize = send_bufsize;
    send_arg.filename = argv [6];
    pthread_create (&send_thread, NULL, do_send, &send_arg);
    /* before accepting another connection, wait for these two to finish */
    if (pthread_join (receive_thread, NULL) < 0) {
      perror ("pthread_join/receive");
      exit (0);
    }
    if (pthread_join (send_thread, NULL) < 0) {
      perror ("pthread_join/send");
      exit (0);
    }
    /* now close the socket, get ready to accept the next */
    tcp_close (session);
  }
  if (session < 0) {
    error ("tcp_accept");
  }
  return 0;
}
