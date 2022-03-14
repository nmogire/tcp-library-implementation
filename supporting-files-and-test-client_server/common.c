/* common.c: program to send files and print received data. */
/* released under the X11 license -- see "license" for details */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <strings.h>

#include "common.h"

/* global error number for functions -- usable anywhere, declared here */
int tcp_error = 0;

/* after reading CONFIG_FILE, store the UDP configuration here */
struct udp_sim_t udp_sim;

void error (char * function)
{
  fprintf (stderr, "error %d in function %s\n", tcp_error, function);
  exit (1);
}

/* globals to store information for the IP pseudo-header */
in_addr_t local_IP;
in_addr_t remote_IP;

void * do_receive (void * arg)
{
  struct receive_args * my_args = (struct receive_args *) arg;
  int pos, count;
  char * endpos;
  int session, bufsize;
  char * buf;

  session = my_args->socket;
  bufsize = my_args->bufsize;
  buf = malloc (bufsize + 1 /* for the null character at the end */ );
  if (buf == NULL)
    error ("malloc/receive");
  pos = 0;
  while (1) {			/* loop ends by calling return */
    count = tcp_recv (session, buf + pos, bufsize - pos);
    if (count <= 0) {		/* socket must have been closed by peer */
      printf ("receiving on socket is done\n");
      free (buf);
      return NULL;		/* receiving thread is finished */
    }
    buf [count + pos] = '\0';
    pos += count;
    /* print each of the lines in the buffer (there may be more than one). */
    while ((endpos = index (buf, '\n')) != NULL) {
      *endpos = '\0';		/* replace the end of the line with EOS */
      printf ("%s\n", buf);	/* print the string */
      /* copy the rest of the string to the start of the buffer */
      pos = 0;
      do {
	endpos++;
	buf [pos++] = *endpos;
      } while (*endpos != '\0');
      pos--;
    }
    if (pos >= bufsize) {  /* slow down the system to test the windowing */
      putchar (buf [0]);
      for (pos = 0; pos < bufsize; pos++) {
	buf [pos] = buf [pos + 1];
      }
      pos = bufsize - 1;	/* read one more character */
      sleep (2);
    }
  }
}

void * do_send (void * arg)
{
  struct send_args * my_args = (struct send_args *) arg;
  int fd, session, bufsize, count, pos, written;
  char * buf;
  char * filename;

  session = my_args->socket;
  bufsize = my_args->bufsize;
  filename = my_args->filename;

  fd = open (filename, 0);
  if (fd < 0) {
    perror ("open");
    printf ("unable to open file %s, aborting\n", filename);
    exit (0);
  }
  buf = malloc (bufsize);
  if (buf == NULL)
    error ("malloc/send");
  while ((count = read (fd, buf, bufsize)) > 0) {
    pos = 0;
    while (pos < count) {
      written = tcp_send (session, buf + pos, count - pos);
      if (written < 0) {
	printf ("error %d in tcp_send\n", tcp_error);
	free (buf);
	return NULL;
      }
      pos += written;
    }
  }
  if (count < 0) {
    perror ("read");
    exit (0);
  }
  printf ("write thread done, closing my end of the connection and exiting\n");
  if (tcp_shutdown (session, SHUT_WR) < 0)
    error ("tcp_shutdown");
  free (buf);
  return NULL;
}

/* Parse CONFIG_FILE and populate the udp_sim structure
 * which will contain a live UDP socket in udp_sim.socket
 * that is bound to the local port.  It is up to the caller to:
 *   1. call listen() or connect() on this socket
 *   2. close this socket
 */
void read_config_file () {
  printf("Parsing %s\n", CONFIG_FILE);
  /* Zero out the udp_sim struct */
  memset(&udp_sim, 0, sizeof(udp_sim));

  if (udp_sim.in_use == 0) {  /* nothing initialized yet */
    char linebuf [1000];
    FILE * f;
    int line = 0;
    struct protoent * protocolentry = getprotobyname ("udp");
    int protocol;

    if (protocolentry == NULL) { /* problem with getting udp protocol */
      error ("getprotobyname");
    }

    protocol = protocolentry->p_proto;

    f = fopen (CONFIG_FILE, "r"); /* Config file defined in common.h */
    if (f == NULL) {
      error ("opening " CONFIG_FILE " for reading");
    }

    /* Parse each line of the opened file, up to 1000 bytes */
    while (fgets (linebuf, sizeof(linebuf), f) != NULL) {
      char * comment;
      char * rport;
      char * hostname;
      int remote_port, local_port;
      struct hostent * hostentry;

      line++;

      if ((comment = index (linebuf, '#')) != NULL) { /* comment found */
        *comment = '\0';
      }
      /* Parse local port from buffer, set pointer to remote port */
      local_port = strtol (linebuf, &rport, 10);
      /* Parse remote port from buffer, set pointer to remote port */
      remote_port = strtol (rport, &hostname, 10);

      /* Validity check */
      if ((rport != linebuf) && (hostname != rport) && /* conversion ok */
          (local_port <= 65535) && (local_port > 0) && /* looks good */
          (remote_port <= 65535) && (remote_port > 0)) {

        /* Set local port for udp_sim */
        udp_sim.local_port = local_port;

        /* get rid of whitespace before calling gethostbyname */
        /* first get rid of any initial whitespace */
        while ((*hostname == ' ') || (*hostname == '\t')) {
          hostname++;
        }
        /* now get rid of any final whitespace */
        while ((hostname [strlen (hostname) - 1] == ' ') ||
               (hostname [strlen (hostname) - 1] == '\t') ||
               (hostname [strlen (hostname) - 1] == '\n')) {
          hostname [strlen (hostname) - 1] = '\0';
        }

        /* Attempt to parse hostname */
        printf ("resolving host name %s\n", hostname);
        /* note gethostbyname also accepts dotted IP addresses, i.e. 1.2.3.4 */
        hostentry = gethostbyname(hostname);
        if ((hostentry == NULL) || (hostentry->h_addr_list == NULL) ||
            ((hostentry->h_addrtype != AF_INET) &&
             (hostentry->h_addrtype != AF_INET6))) {
          /* assume this is a bad entry */
          printf ("line %d of %s, hostname unknown, ignoring (%s)\n",
            line, CONFIG_FILE, linebuf);
        } else {  /* create the socket for this simulated interface */
          struct sockaddr_in * sinp = (struct sockaddr_in *)(&(udp_sim.remote));
          struct sockaddr_in6 * sin6p = (struct sockaddr_in6 *)(sinp);
          struct sockaddr_in6 bind_sin6;  /* another address, used for bind */

          /* create the address that this socket sends data to */
          bzero (sinp, sizeof (udp_sim.remote));
          udp_sim.remote.sa_family = hostentry->h_addrtype;

          if (hostentry->h_addrtype == AF_INET) {
            if (hostentry->h_length != sizeof (sinp->sin_addr)) {
              printf ("error: address size %d, expected %d\n",
                      hostentry->h_length, (int) sizeof (sinp->sin_addr));
              error ("wrong address size");
            }
            memcpy (&(sinp->sin_addr), hostentry->h_addr_list[0],
                    sizeof (sinp->sin_addr));
            sinp->sin_port = htons (remote_port);
          } else {
            if (hostentry->h_length != sizeof (sin6p->sin6_addr)) {
              printf ("error: address size %d, expected %d\n",
                      hostentry->h_length, (int) sizeof (sin6p->sin6_addr));
              error ("wrong address size");
            }
            memcpy (&(sin6p->sin6_addr), hostentry->h_addr_list[0],
                    sizeof (sin6p->sin6_addr));
            sin6p->sin6_port = htons (remote_port);
          }

          /* create a UDP socket and bind it to the port for listening */
          udp_sim.socket = socket (PF_INET, SOCK_DGRAM, protocol);
          if (udp_sim.socket < 0) {
            error ("socket");
          }
          /* binding to IPv6 supports both v4 and v6, but may fail
           * if the system doesn't support IPv6 */
          bind_sin6.sin6_family = AF_INET6;
          memcpy (&(bind_sin6.sin6_addr), &(in6addr_any),
                  sizeof (bind_sin6.sin6_addr));
          bind_sin6.sin6_port = htons (local_port);
          if (bind (udp_sim.socket, (struct sockaddr *) (&bind_sin6),
                    sizeof (bind_sin6)) != 0) {
             /* IPv6 failed, report, then try IPv4 */
            struct sockaddr_in bind_sin;
            perror ("bind v6");
            printf ("error binding to UDP port %d on IPv6, trying IPv4\n",
                    local_port);
            bind_sin.sin_family = AF_INET;
            bind_sin.sin_port = htons (local_port);
            bind_sin.sin_addr.s_addr = INADDR_ANY;
            if (bind (udp_sim.socket, (struct sockaddr *) (&bind_sin),
                      sizeof (bind_sin)) != 0) {
              error("bind");
            }
          }
          udp_sim.in_use = 1;   /* we have initialized the udp_sim item */
        }
      } else {      /* some error, ignore this line */
        char * thiserror = "remote port < 0";
        if (remote_port > 65535)
          thiserror = "remote port > 65535";
        if (local_port < 0)
          thiserror = "local port < 0";
        if (local_port > 65535)
          thiserror = "local port > 65535";
        if (hostname == rport)
          thiserror = "no number given for the remote port";
        if (rport == linebuf)
          thiserror = "no number given for the local port";
        while (linebuf [strlen (linebuf) - 1] == '\n')
          linebuf [strlen (linebuf) - 1] = '\0';
        printf ("line %d of %s, %s, ignoring (%s)\n",
          line, CONFIG_FILE, thiserror, linebuf);
      }
    }
  }
}

