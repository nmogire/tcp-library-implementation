/*****************************************************************
AUTHOR: Nancy M.C.,

FILE: tcp.c

DESCRIPTION: Implements tcp socket library, simulation

NOTES: 
- header file(common.h)included below and its functions, 
as well as the server and client used to test this library, 
are by different authors, shared with special license. 
- See README for details and links
****************************************************************/

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

#define check_init() if (! initialized) tcp_init ()
#ifndef TCP_RCV_BUFFER_SIZE
#define TCP_RCV_BUFFER_SIZE 4096
#endif 
#define BUFSIZE 556 //MSS + HEADER
#define MSS 536 //max bytes per send

//testing function
int assert(int condition, const char* message){
	if(condition){
		return 0;
	}
	if(!condition) {
		printf("%s\n",message);
		
		return -1;
	}
	return 0;
}

//from buffer, print characters received
int printer(char* buffer){
      int num_bytes=0, i;
      printf("Received Message: \n");
      for(i=0; i<strlen(buffer)-1; i++){
              printf("%s", &buffer[i]);
              num_bytes++;
              }
      printf("\n");
      return num_bytes;
}


//dfn tcp header struct
struct tcp_header{
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t control;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
    //uint32_t options;
} tcp_hdr;

//dfn tcp_message struct 
struct tcp_message{
    char msg[536];
};


//dfn tcp packet struct 
//packet contents: tcp header and a message(stream of characters)
struct t_packet{
        struct tcp_header header;
        char* msg;
}sndpacket, recvpacket;

//dfn rcv buffer
char recv_buffer[TCP_RCV_BUFFER_SIZE];
int msg_count=0;

char *recv_buf_ptr =recv_buffer;
        //memset(recv_buffer, 0, sizeof(recv_buffer));

//dfn send buffer
char send_buffer[556];
char *sent_buf_ptr=send_buffer;
        //memset(send_buffer, 0, sizeof(send_buffer));

//other globals
char buf[BUFSIZE];

//for receiving the packet        
struct tcp_header* header = (struct tcp_header*)buf;
struct tcp_message* msg_rcvd=(struct tcp_message*)buf+20;

//dfn tcp pseudoheader struct 
//for checksum function
//NOT USED-->CHECKSUM NOT IMPLENTED
/*struct tcp_pseudoheader{
    uint16_t src;
    uint16_t dest;
    int protocol;
    int tcp_length;
}tcp_pseudo;*/
 
//tcp states
enum tcp_state{LISTEN, SYN_SENT, SYN_RCVD, ESTAB, FIN_WAIT_1,
	 FIN_WAIT_2, CLOSING, TIME_WAIT, CLOSE_WAIT, LAST_ACK, CLOSED, OPEN};
	 
//struct tcb
struct tcb{
enum tcp_state state;
int local_sock_fd;
int remote_sock_fd;
int local_port;
int remote_port;
int local_ip;
int remote_ip;

//Send values
unsigned int SND_UNA; //sent unacknowledged
unsigned int SND_NXT; //next send
unsigned int SND_WND; //send window
unsigned int ISS; //initial send sequence number

//Receive values
unsigned int RCV_NXT; //receive next
unsigned int RCV_WND; //receive window
unsigned int IRS;     //initial receive sequence number
};

pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

//transmission control block(tcb)
struct tcb tcb;

void tcb_init(){
tcb.state=CLOSED;
tcb.local_sock_fd=0;
tcb.remote_sock_fd=0;
tcb.local_port=0;
tcb.remote_port=0;
tcb.local_ip=0;
tcb.remote_ip=0;
tcb.SND_UNA=0;
tcb.SND_NXT=1;
tcb.SND_WND=htons(MSS);
tcb.ISS=0;
tcb.RCV_NXT=0;
tcb.RCV_WND=htons(MSS);
tcb.IRS=0;
}

//dfn tcp state machine events
enum tcp_events{
    BEGIN, //is in TCP_INIT, if incoming state is START--leads to--> CLOSED
	PASSIVE_OPEN,//in tcp_LISTEN, if incoming state is CLOSED--to--> LISTEN
	ACTIVE_OPEN,//in tcp_CONNECT, if incoming state is CLOSED--to--> SYN_SENT
    RCV_SYN, // in tcp_ACCEPT, if incoming state is LISTEN & Socket not in use --> SYN_RCVD
	RCV_SYNACK,//in tcp_CONNECT, if incoming state is SYN_SENT --> ESTAB
	RCV_ACKofSYN, //in tcp_ACCEPT, if incoming state is SYN_RCVD --> ESTAB
	CLOSE,// in CLOSE, if incoming state is ESTAB or SYN_RCVD --> FIN_WAIT_1; if incoming state is CLOSE_WAIT --> LAST_ACK
	RCV_FIN,// in CLOSE, if incoming state is ESTAB && --> CLOSE_WAIT; if incoming state is FIN_WAIT_2 --> TIME_WAIT
	RCV_ACK_OF_FIN,//im CLOSE, if incoming state is FIN_WAIT_1 --> FIN_WAIT_2; if incoming state is LAST_ACK --> CLOSED
	TIMEOUT// in TIMER AFTER 5 SECONDS IF INCOMING STATE IS TIME_WAIT --> CLOSED   
	
} event;
/*
enum tcp_actions{
tcb_init(),//on event PASSIVE_OPEN --leads to--> LISTEN
tcb_init(),//on event ACTIVE_OPEN 
snd_SYN(),//on event ACTIVE_OPEN --leads to--> SYN_SENT
snd_SYNACK()//on event RCV_SYN ---> SYN_RCVD
snd_ACK()//on event rcv_SYNACK --> ESTAB
snd_FIN()//on event CLOSE --> FIN_WAIT_1 or LAST_ACK
snd_FINACK()//on event RCV_FIN --> CLOSE_WAIT or TIME_WAIT
};*/


//tcp state machine
//HAS TCB.STATE GLOBAL VARIABLE
//TAKES EVENT PARAMETER --> RETURNS NEW STATE
void state_machine(enum tcp_events event) {
pthread_mutex_lock(&lock);
    switch(tcb.state) {
    case CLOSED: 
        switch(event) {
        case PASSIVE_OPEN:
            tcb.state = LISTEN;
            break;
        case ACTIVE_OPEN:
            tcb_init();
            //snd_SYN();
            tcb.state = SYN_SENT;
            break;
        default:
            break;
        }  
        break;                    
    case LISTEN:
        switch(event) {
        case RCV_SYN:
            //rcv_SYN(); //NOT-IN-TCP-SM
            //snd_SYNACK();
            tcb.state = SYN_RCVD;
            break;
        default:
            break;
        } 
        break; 
    case SYN_SENT:
        switch(event) {
        case RCV_SYNACK://***rcv_synack..to estab..send ack of syn_ack
            //rcv_SYNACK(); //NOT-IN-TCP-SM
            //snd_ACK();
            tcb.state = ESTAB;
            break;
        default:
            break;
        } 
        break; 
    case SYN_RCVD:
        switch(event) {
        case RCV_ACKofSYN://**rcv ack.. goes to estab
            //rcv_ACKofSYN(); //NOT-IN-TCP-SM
            tcb.state = ESTAB ;
            break;
        case CLOSE:
            //snd_FIN();
            tcb.state = FIN_WAIT_1;
            break;
        default:
            break;
        } 
        break; 
    case ESTAB:
        switch(event) {
        case CLOSE:
            tcb.state = FIN_WAIT_1;
            break;
        case RCV_FIN:
            //rcv_FIN(); //NOT-IN-TCP-SM
            //snd_FINACK();
            tcb.state = CLOSE_WAIT;
            break;       
        default:
            break;
        } 
        break; 
    case FIN_WAIT_1:
        switch(event) {
        case RCV_ACK_OF_FIN:
            //rcv_ACKofFIN(); //NOT-IN-TCP-SM
            tcb.state = FIN_WAIT_2;
            break;
        default:
            break;
        }  
        break; 
    case CLOSE_WAIT:
        switch(event) {
        case CLOSE:
            //snd_FIN();
            tcb.state = LAST_ACK;
            break;
        default:
            break;
        } 
        break; 
    case FIN_WAIT_2:
        switch(event){
        case RCV_FIN:
            //snd_FINACK();
            tcb.state = TIME_WAIT;
            break;
        default:
            break;
        }   
        break;       
    case LAST_ACK:
        switch(event) {
        case RCV_ACK_OF_FIN:
            tcb.state = CLOSED;
            break;
        default:
            break;
        }
        break;  
    case TIME_WAIT:
        switch(event) {
        case TIMEOUT:
            //timeout() -- NOT-IN-TCP-SM
            sleep(1);
            tcb.state = CLOSED;
            break;
        default:
            break;
        } 
        break; 
    default:
        break;   
    }
    pthread_mutex_unlock(&lock);    
}


//dfn socket usage status
enum sock_usage_status{IN_USE, NOT_IN_USE};

//dfn tcp socket struct
static struct t_sockfd{
	int fd;
	enum sock_usage_status usage_status;
	enum tcp_state  state;
	struct sockaddr local_address;
	struct sockaddr server_address;
	struct sockaddr temp_address;
	
} sock01, sock02;

		
//function to receive data from remote port
//incomplete implementation - not threading
void wait_to_read()
{
pthread_mutex_lock(&lock);

	printf("waiting for data...\n");
			//char buf[BUF]; see global vars
			//struct tcp_header* header=(struct tcp_header*)buf; see global
	int r;
	r=recvfrom(udp_sim.socket, buf, sizeof(buf),0, NULL, NULL);//replace 0 with MSG_DONTWAIT to prevent blocking when no msg
	assert(r!=-1, "Error: read failed!");
		
	printf("%d bytes received.\n", r);
	printf("Source port: %d\n", ntohs(header->src_port));
	printf("Destination port: %d\n",ntohs(header->dest_port));
	printf("Sequence no.: %d\n", ntohl(header->seq));
	printf("Ack no.: %d\n", ntohl(header->ack));
	printf("Flags: %d\n", ntohs(header->control));
	printf("Window size: %d\n", ntohs(header->window));
	printf("Checksum: %d\n", header->checksum);
	printf("Urgent pointer: %d\n", header->urgent_ptr);

	if(header->control==1){
		printf("Incoming FIN\n");
							
	}

	if(header->control==2){
		printf("Incoming SYN\n");
	}
			
	if(header->control==16){
		printf("Incoming ACK\n");
		tcb.SND_NXT=header->ack;
	}

	if(header->control==17){
		printf("Incoming FINACK\n");
	}

	if(header->control==18){
		printf("Incoming SYNACK\n");
	}

	if(header->control==0){
		printf("No Controls Set\n");
	}
			
	int received_data = r-20;
	if(received_data>0){
		printf ("[received data bytes]: %d\n", received_data);
	}
			
		
	//update tcb		
	//if received data, reduce window
	if(received_data>0){
		tcb.RCV_WND=tcb.RCV_WND - received_data;
					
		if(tcb.RCV_WND>536){
			tcb.RCV_WND = 536;
		}
				
		tcb.SND_UNA=htonl(ntohl(header->ack)+received_data);
		tcb.RCV_NXT=htonl(ntohl(header->seq)+received_data);
	}
			
			
	tcb.local_port=tcb.remote_port;
	tcb.remote_port=tcb.local_port;
	tcb.SND_UNA=htonl(ntohl(header->ack));
	tcb.SND_NXT=htonl(ntohl(header->ack));//will be OUTGOING SEQ i.e incoming ack+datasize
	tcb.SND_WND=htons(536);
	tcb.RCV_NXT=htonl(ntohl(header->seq)+1);//will be outgoing ACK
	tcb.RCV_WND=htons(536);
                
pthread_mutex_unlock(&lock);  	
}
    


/*****************************************************************
DESCRIPTION: tcp socket functions, implement tcp state machine
****************************************************************/

static int initialized = 0;

static void tcp_init (){
    printf ("initializing tcp...\n");   
    	
	read_config_file();
        assert(udp_sim.in_use==1, "udp simconfig not initialized");
		//pthread_create(&read_thread, NULL, &wait_to_read, NULL); //to be used with pthread version of reader function
		//printf ("udp sim socket's local port is %d\n", udp_sim.local_port);
        tcb_init();
        sock01.fd=-2;
        sock02.fd=-3;
        sock01.usage_status=NOT_IN_USE;
	
        initialized = 1;
        	
}


#define check_init() if (! initialized) tcp_init ()

//creates a socket file descriptor, 
//otherwise returns -1 for failure
int tcp_socket (int domain, int type, int protocol){
	
	//............
	//verifying function inputs
	//to separate to debugmode --> **global var, extern bool debugmode; then wrap as, if(debugmode==true){assert...}**	
	assert(domain==PF_INET,"Error: unexpected domain name ");
	assert(type==SOCK_STREAM,"Error:unexpected type");
	assert(protocol== 0,"Error: unexpected protocol ");
	//............
	
    check_init ();
	
   	if(sock01.usage_status==IN_USE){
		printf("Socket Error:The socket is in use\n");
		return -1;		
	}
	
	sock01.fd=1;	
			
	printf ("starting a socket of domain %d, type %d and protocol %d\n",
	domain, type, protocol);	
    
	return sock01.fd;  
}

//binds specified socket, 
//returns -1 if it fails(e.g. if port in use)
int tcp_bind (int sockfd, struct sockaddr * myaddr, int addrlen){
	
	//............
	assert(sockfd==sock01.fd,"Bind Error: unexpected file descriptor");
	assert(myaddr != NULL,"Error: null address! ");
	assert(addrlen>=sizeof(sock01.local_address),"Error: address size is too short");
    //............
	
    check_init ();
    printf ("binding socket %d, of address %p , and an address length of %d\n",
	sockfd, (void *)myaddr, addrlen);
	
	memcpy(&sock01.local_address, myaddr, sizeof(sock01.local_address));
    struct sockaddr_in* local_address=(struct sockaddr_in*)&sock01.local_address;
	printf("local port number: %d\n", ntohs(local_address-> sin_port ));	
	printf("local address family: %d\n", local_address-> sin_family); 

	char ip[INET_ADDRSTRLEN];	
	inet_ntop(AF_INET, &local_address-> sin_addr.s_addr, ip, INET_ADDRSTRLEN);
	printf("local address IP: %s\n",ip );
        
	return 0; 
}
       
//starts listening at specified socket, returns -1 if it fails
//tcb is set to listen
int tcp_listen (int sockfd, int backlog){
	
	//............
	assert(sockfd==sock01.fd,"Listen Error: unexpected file descriptor");
	assert(backlog>0,"Error: backlog too small");
	assert(backlog==1," Error: listen queues not supported");
	//assert(tcb.state==CLOSED,"Error: socket is in use");
	assert(udp_sim.in_use == 1,"Error: structure or socket not valid");
	//............
	
    check_init ();
    if (tcb.state==CLOSED){
	state_machine(PASSIVE_OPEN);
	}
	
	printf ("listening at socket %d, with a backlog of %d.\n", sockfd, backlog);	
	
    return 0;  
}
      
//starts a connection at a specified socket, returns -1 for failure
int tcp_connect (int sockfd, struct sockaddr * servaddr, int addrlen){

	//............
	assert(sockfd==sock01.fd,"Connect Error: unexpected file descriptor ");
	assert(servaddr != NULL,"Connect Error: server address is null! ");
	assert(addrlen >= sizeof(sock01.server_address),"Error: address size is too short");
    //............
	
    check_init ();
       
    //get remote & local address, port number and ip
    //copy them to sock01.server_address
	//from temporary holder get_server_address.
	memcpy(&sock01.server_address, servaddr, sizeof(sock01.server_address));
        struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;	

	//make address printable: bin-->text
	char remote_ip[INET_ADDRSTRLEN];	
	inet_ntop(AF_INET, &get_server_address-> sin_addr.s_addr, remote_ip, INET_ADDRSTRLEN);

	struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
	
    //setup for syn      
		if(tcb.state==CLOSED){//call active open
        
		//update transmission control block
		printf("Updating TCB with new socket, port and address data\n");
		
		tcb.local_sock_fd=sockfd;
        tcb.local_port=get_local_address -> sin_port;
        tcb.remote_port=get_server_address -> sin_port;
        tcb.local_ip=get_server_address -> sin_addr.s_addr;
        tcb.remote_ip=get_server_address -> sin_addr.s_addr;
                
			//build syn
            struct tcp_header syn;	
	        syn.src_port= get_local_address -> sin_port;
	        syn.dest_port= get_server_address -> sin_port;
	        syn.ack=0;
	        syn.seq=0;
	        syn.checksum=0;
	        syn.control=htons(2);
	        syn.urgent_ptr=0;
	        syn.window=htons(tcb.SND_WND);
	 
	        state_machine(ACTIVE_OPEN);
	        
	       
	        sendto(udp_sim.socket, &syn, sizeof(syn), 0, &udp_sim.remote, sizeof(sock01.server_address));//snd_SYN();
	        	                        
            printf("sent SYN to remote port %d of address family %d at ip address %s\n",ntohs(get_server_address-> sin_port), get_server_address-> sin_family,remote_ip);              
                                 
        }//sent syn, wait for the syn_ack then send ack
                          
        
		if(tcb.state==SYN_SENT){
                
            state_machine(RCV_SYNACK); //call snd_ACK in SM
            wait_to_read();//calling reader to wait for syn_ack
			
            struct tcp_header ack;	
	        ack.src_port= get_local_address -> sin_port;
	        ack.dest_port= get_server_address-> sin_port;
	        ack.ack=tcb.RCV_NXT;
	        ack.seq=tcb.SND_NXT;
	        ack.checksum=0;
	        ack.control=htons(16);
	        ack.urgent_ptr=0;
	        ack.window=htons(TCP_RCV_BUFFER_SIZE);
	 
            sendto(udp_sim.socket, &ack, sizeof(ack), 0, &udp_sim.remote, sizeof(sock01.server_address));
                
            printf("sent ACK to remote port %d of address family %d at ip address %s\n",ntohs(get_server_address-> sin_port), get_server_address-> sin_family,remote_ip);
                
            printf ("Current state: %d --> ", tcb.state);
	        if(tcb.state==3){printf ("IN ESTAB!\n");
	        }                                    
        }
  
  return 0;  
}

//starts accepting at specified socket, returns -1 if it fails
int tcp_accept (int sockfd, struct sockaddr * myaddr, int *addrlen){
	
	//............
	assert(sockfd==sock01.fd||sock02.fd,"Accept Error: unexpected file descriptor ");
	assert(myaddr!=NULL,"Accept Error: null Address in connect request");
	assert(*addrlen >= sizeof(sock01.local_address), "Accept Error: too short address");
	//............
	
    check_init();
    wait_to_read();//wait for syn
    
		printf ("Done reading!!\n");
		printf ("Current state: %d\n", tcb.state);
      
		sock02.fd=2;
        
    if(tcb.state==LISTEN){
       memcpy(&sock02.temp_address, myaddr, sizeof(sock02.temp_address));
       struct sockaddr_in* get_temp_address=(struct sockaddr_in*)&sock02.temp_address;
       printf ("accepting connection at local port %d on socket %d and address family of %d\n",
	   ntohs(get_temp_address-> sin_port), sock02.fd, get_temp_address-> sin_family);//net to host byte order
        
        //update TCB
         tcb.local_sock_fd=sockfd;
         tcb.local_port=get_temp_address -> sin_port;
         tcb.remote_port=tcb.local_port;
         tcb.local_ip=get_temp_address -> sin_addr.s_addr;
         tcb.remote_ip=tcb.local_ip;
        
                  
		struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
		struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	
		//sock02.usage_status=IN_USE;
	
		state_machine(RCV_SYN);
	
		//send syn_ack
		struct tcp_header syn_ack;	
	        syn_ack.src_port= get_local_address -> sin_port;		
	        syn_ack.dest_port= get_server_address -> sin_port;
	        syn_ack.ack=tcb.RCV_NXT;
	        syn_ack.seq=tcb.SND_NXT;
	        syn_ack.checksum=0;
	        syn_ack.control=htons(18);
	        syn_ack.urgent_ptr=0;
			//syn_ack.options=0;
	        syn_ack.window=htons(TCP_RCV_BUFFER_SIZE);
	 
	        sendto(udp_sim.socket, &syn_ack, sizeof(syn_ack), 0, &udp_sim.remote, sizeof(sock01.server_address));
	        printf("sent SYN-ACK to remote port %d \n",ntohs(header->src_port));
	
			//state_machine(SND_SYNACK)
	}
			
	
	if(tcb.state==SYN_RCVD){	
	    wait_to_read();//wait for ack-of-syn
		//struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
		//struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	              	        
	    printf("Received ack from remote port %d \n",ntohs(header->src_port));  
	    printf("Ready to estab..\n");
	    
		state_machine(RCV_ACKofSYN);
	        
		printf("Current state: %d --> ", tcb.state);
	    if(tcb.state==3){printf("IN ESTAB!\n");	}
	}
	
	return sock02.fd;
	
	//printf("socket is in use\n");    	
    //return -1; if error 
}

// return the no. of bytes written, else returns -1 for failure
int tcp_send (int sockfd, const void * buffer, int length){
	
	//............
	assert(sockfd==sock02.fd ||sock01.fd,"Send Error: unexpected file descriptor ");
	assert(buffer!=NULL, "Error: null message");
	assert(length>=sizeof(buffer), "Error:buffer length too short");
	//............
	
    check_init ();
       
	//char buf3[BUFSIZE];
	//char *data=buf3;
	if(tcb.state == ESTAB){	
	struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
	struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	
	    struct tcp_header tcp_data;	
	    tcp_data.src_port= ntohs(get_local_address -> sin_port);
	    tcp_data.dest_port= ntohs(get_server_address -> sin_port);
	    tcp_data.ack=tcb.RCV_NXT;
	    tcp_data.seq=tcb.SND_NXT;
	    tcp_data.checksum=0;
	    tcp_data.control=2;
	    tcp_data.urgent_ptr=0;
	    //syn.options=0;
	    tcp_data.window=0;

	
        //to build packet--i.e. header+data onto buffer
        //struct t_packet sndpacket;
        char send_buffer[556];
        int bytes_to_send=length;
        
        if(bytes_to_send> tcb.SND_WND){
        bytes_to_send= tcb.SND_WND;
        }
        
        if(bytes_to_send>536){
          bytes_to_send=536;
        }
        
        if(bytes_to_send>sizeof(send_buffer)-20){
          bytes_to_send=sizeof(send_buffer)-20;
		}
        
        
		int sentbytes, remainingbytes;
		memcpy(send_buffer, &tcp_data, 20);//20 is sizeof(header)
		memcpy(send_buffer+20, buffer, bytes_to_send);
		
		//memcpy(&sendbuf, &sndpacket, sizeof(sendbuf));
		sentbytes = bytes_to_send;
		remainingbytes = 0;	
	
		if(length>bytes_to_send){
		remainingbytes = length- bytes_to_send;	        
		printf ("writing data of size %d bytes to socket. Remaining data is %d bytes..\n",bytes_to_send, remainingbytes );
		
		}
    
        sendto(udp_sim.socket, send_buffer, bytes_to_send, 0, &udp_sim.remote, sizeof(sock01.server_address));	
		printf ("writing data of size %d bytes to socket.\n",bytes_to_send);
		
		return sentbytes;  //return no. of bytes written
    }
 
    return 0;
}

// returns the no. of bytes read, else if peer closes connection return 0
// return -1 for failure
int tcp_recv (int sockfd, void * buffer, int length){
	
	//............
	assert(sockfd==sock01.fd||sock02.fd,"Receive Error: unexpected file descriptor ");
	assert(buffer!=NULL,"Error: Empty buffer");
	assert(length !=0, "Error: 0 bytes");
	//............
	
    
	if(tcb.state == ESTAB ||tcb.state ==FIN_WAIT_1){
		
	check_init ();
		
	//data buffer
	char final_receiver[TCP_RCV_BUFFER_SIZE];
		
	//int num_bytes;
	//get data
	while(msg_count>0){
			memmove(final_receiver, &recv_buffer, 536); //to shift data in source buffer after reading
			msg_count --;   
		}

        //num_bytes=printer(final_receiver);
		//printf ("%s\n",final_receiver);
	
		//  printf("received %d bytes at socket %d\n", num_bytes, sockfd);
    
	return 0;        
    }
	
	printf("No connection to receive at.\n");    
	return -1;
}

//closes the specified socket, returns 0 on success, returns -1 for failure
//closes read end?...
int tcp_close (int sockfd){
	
	//............
    assert(sockfd==sock01.fd ||sockfd==sock02.fd ,"Close Error: unexpected file descriptor ");
	//............
	
    check_init ();
       		
	if(tcb.state==ESTAB){// || SYN_RCVD
	     
	    printf ("First call to tcp_close, Current state is ESTAB. Going to FIN_WAIT_1\n");
	    struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
		struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	                
	    if(header->control!=htons(1)){         
	        struct tcp_header FIN;
	        FIN.src_port=get_local_address -> sin_port;		
	        FIN.dest_port= get_server_address -> sin_port;
	        FIN.ack=tcb.RCV_NXT;
	        FIN.seq=tcb.SND_NXT;
	        FIN.checksum=0;
	        FIN.control=htons(1);
	        FIN.urgent_ptr=0;
	        FIN.window=htons(TCP_RCV_BUFFER_SIZE);
	        printf ("Current state: %d -->\n ", tcb.state);
	        
	        sendto(udp_sim.socket, &FIN, sizeof(FIN), 0, &udp_sim.remote, sizeof(sock01.server_address));
	        
	              state_machine(CLOSE);
	      	        
	              printf("Current state:%d\n", tcb.state);
	        
	              if(tcb.state==FIN_WAIT_1){
	                printf("FIN_WAIT_1!\n");
	                }
	       }
	       else if(header->control==htons(1)){
	                   
	        printf ("Second call to tcp_close from ESTAB, will go to FIN_WAIT_2\n"); 
	        //send ack for fin       
	        struct tcp_header FINACK;
	        FINACK.src_port= get_local_address -> sin_port;		
	        FINACK.dest_port= get_server_address -> sin_port;
	        FINACK.ack=tcb.RCV_NXT;
	        FINACK.seq=tcb.SND_NXT;
	        FINACK.checksum=0;
	        FINACK.control=htons(17);
	        FINACK.urgent_ptr=0;
	        FINACK.window=htons(TCP_RCV_BUFFER_SIZE);
	        
	        
	        sendto(udp_sim.socket, &FINACK, sizeof(FINACK), 0, &udp_sim.remote, sizeof(sock01.server_address));
	        state_machine(RCV_FIN);
	        
			}
	
	}
	      
	if(tcb.state==FIN_WAIT_1){// 
	    printf ("in FIN_WAIT_1, waiting to receive Ack of Fin and go to FIN_WAIT_2...\n");
	    wait_to_read();//wait for fin
	     
	        
	    state_machine(RCV_ACK_OF_FIN);
	        	        
	    printf("Current state:%d\n", tcb.state);
	        
	   	
	}
	
	if(tcb.state==FIN_WAIT_2){//
	    printf ("in FIN_WAIT_2, sending ACK of FIN and going to TIME_WAIT...\n");
	    wait_to_read();//wait for finack
	        
	    struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
	    struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	                
	               
	        struct tcp_header ACKofFIN;
	        ACKofFIN.src_port=get_local_address-> sin_port;		
	        ACKofFIN.dest_port=get_server_address -> sin_port;
	        ACKofFIN.ack=tcb.RCV_NXT;
	        ACKofFIN.seq=tcb.SND_NXT;
	        ACKofFIN.checksum=0;
	        ACKofFIN.control=htons(16);
	        ACKofFIN.urgent_ptr=0;
	        ACKofFIN.window=htons(TCP_RCV_BUFFER_SIZE);
	        
	        sendto(udp_sim.socket, &ACKofFIN, sizeof(ACKofFIN), 0, &udp_sim.remote, sizeof(sock01.server_address));
	        
	        
	        state_machine(RCV_FIN);
	        printf("Current state:%d\n", tcb.state);
	        	   	
	}
	
	if(tcb.state==CLOSE_WAIT){
	    printf ("in CLOSE_WAIT, sending another FIN and going to LAST_ACK...\n");
	        
	    state_machine(CLOSE);
	        
	    struct sockaddr_in* get_local_address=(struct sockaddr_in*)&sock01.local_address;
		struct sockaddr_in* get_server_address=(struct sockaddr_in*)&sock01.server_address;
	                
	               
	        struct tcp_header FIN;
	        FIN.src_port= get_local_address -> sin_port;		
	        FIN.dest_port= get_server_address -> sin_port;
	        FIN.ack=tcb.RCV_NXT;
	        FIN.seq=tcb.SND_NXT;
	        FIN.checksum=0;
	        FIN.control=htons(1);
	        FIN.urgent_ptr=0;
	        FIN.window=htons(TCP_RCV_BUFFER_SIZE);
	        printf ("Current state: %d -->\n ", tcb.state);
	        
	        sendto(udp_sim.socket, &FIN, sizeof(FIN), 0, &udp_sim.remote, sizeof(sock01.server_address));
	        
	        
	        printf("Current state:%d\n", tcb.state);	        
	       if(tcb.state==FIN_WAIT_2){printf("FIN_WAIT_2!\n");}
	}
	        
	if(tcb.state==LAST_ACK){// 
	    printf ("in last ack, waiting to receive Ack of Fin...\n");
	    wait_to_read();//wait for AckofFin
	    state_machine(RCV_ACK_OF_FIN);
	    printf("Current state:%d\n", tcb.state);
	}
	      
	if(tcb.state==TIME_WAIT){// 
	    printf ("in TIME_WAIT, going to timeout...\n");
	    printf("Starting Timeout..\n");
	    state_machine(TIMEOUT);
	    printf("Socket CLOSED.\n");
	}		

    return 0; 
}

//check that socket is closed. if not, call close
int tcp_shutdown (int sockfd, int how)
{
    //............
	assert(sockfd==sock01.fd||sockfd==sock02.fd,"Receive Error: unexpected file descriptor ");
	//............
	
    
	check_init ();        
    
	printf ("shutting down socket %d\n", sockfd);
        
    if(tcb.state!=CLOSED){
        tcp_close(sockfd);
	}
	
    return 0;  /* return 0 for success
	//-1 for failure */
}

