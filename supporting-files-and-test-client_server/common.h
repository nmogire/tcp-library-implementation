/* common.h: common header file for server and client. */

#ifndef P2_COMMON_H
#define P2_COMMON_H

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* global error number for functions */
extern int tcp_error;

extern void error (char * function);

/* globals to store information for the IP pseudo-header */
extern in_addr_t local_IP;
extern in_addr_t remote_IP;

#ifndef TCP_RCV_BUFFER_SIZE
#define TCP_RCV_BUFFER_SIZE		4096
#endif /* TCP_RCV_BUFFER_SIZE */

struct receive_args {
  int socket;
  int bufsize;
};

extern void * do_receive (void * arg);

struct send_args {
  int socket;
  int bufsize;
  char * filename;
};

extern void * do_send (void * arg);

/* ---------- 2016 Additions ---------- */
/* Patrick Karjala and Mark Nelson */

/* Configuration file for the underlying UDP stack */
#define CONFIG_FILE "./simconfig"

extern struct udp_sim_t {
  short local_port;        /* from CONFIG_FILE -- in host byte order */
  union {
    struct sockaddr remote_sa;  /* from CONFIG_FILE, port and IP number --
                              * in network byte order */
    struct sockaddr_storage s;  /* reserve storage for any possible sockaddr */
  } u;
/* after Spring 2016 should rename to something other than "remote" --
 * remote could be a variable name, and this define would mess with it. */
#define remote u.remote_sa /* udp_sim.remote is a struct sockaddr */
  int socket;              /* read_config_file binds this to the local port */
  int in_use;              /* = 1 when the structure and socket are valid */
} udp_sim;                 /* global variable, declared in common.c */

/* Parse CONFIG_FILE and populate the udp_sim structure
 * which will contain a live UDP socket in udp_sim.socket
 * that is bound to the local port.  It is up to the caller
 * to eventually close this socket.
 */
extern void read_config_file();

/* ---------- end 2016 Additions ---------- */

/* to be implemented by tcp.c */
extern int tcp_socket (int domain, int type, int protocol);

extern int tcp_bind (int sockfd, struct sockaddr * myaddr, int addrlen);

extern int tcp_listen (int sockfd, int backlog);

extern int tcp_accept (int sockfd, struct sockaddr * myaddr, int * addrlen);

extern int tcp_connect (int sockfd, struct sockaddr * servaddr, int addrlen);

extern int tcp_send (int sockfd, const void * buffer, int length);

extern int tcp_recv (int sockfd, void * buffer, int length);

extern int tcp_close (int sockfd);

extern int tcp_shutdown (int sockfd, int how);

extern unsigned short inverted_checksum (char * buffer, unsigned int size);

#endif /* P2_COMMON_H */
