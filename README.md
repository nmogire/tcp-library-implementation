# tcp-library-implementation
A network protocol programming exercise. Implementing a simulation of the tcp socket library specified in RFC793 

This project implements the TCP connection state machine in the diagram below, reproduced from rfc793:

  ```
                                            +---------+ ---------\         active OPEN
                                            |   CLOSED |            \      -----------
                                            +---------+<---------\    \    create TCB
                                               |     ^              \   \   snd SYN
                                  passive OPEN |     | CLOSE          \   \
                                  ------------ |     | ----------       \   \
                                  create TCB   |     | delete TCB         \   \
                                               V     |                      \   \
                                            +---------+             CLOSE     |    \
                                            | LISTEN  |           ----------  |     |
                                            +---------+            delete TCB |     |
                                 rcv SYN       |    |        SEND             |     |
                                -----------    |    |       -------           |     V
                 +---------+     snd SYN,ACK  /      \      snd SYN         +---------+
                 |         |<-----------------          ------------------> |         |
                 |   SYN   |                    rcv SYN                     |   SYN   |
                 |   RCVD  |<-----------------------------------------------|   SENT  |
                 |         |                    snd ACK                     |         |
                 |         |------------------           -------------------|         |
                 +---------+ rcv ACK of SYN    \       /  rcv SYN,ACK       +---------+
                   |         --------------     |      |  -----------
                   |                x           |      |  snd ACK
                   |                            V      V
                   |  CLOSE                    +---------+
                   | -------                   |  ESTAB  |
                   | snd FIN                   +---------+
                   |                    CLOSE   |      |    rcv FIN
                   V                    ------- |      |    -------
                 +---------+           snd FIN /        \   snd ACK         +---------+
                 |   FIN   |<-----------------           ------------------>|  CLOSE  |

                 |  WAIT-1 |------------------                              |   WAIT  |
                 +---------+           rcv FIN \                            +---------+
                    | rcv ACK of FIN   -------  |                             CLOSE |
                    | --------------   snd ACK  |                           ------- |
                    V        x                  V                           snd FIN V
                 +---------+                  +---------+                   +---------+
                 |FINWAIT-2|                  | CLOSING |                   | LAST-ACK|
                 +---------+                  +---------+                   +---------+
                    |               rcv ACK of FIN |                 rcv ACK of FIN |
                    | rcv FIN       -------------- |    Timeout=2MSL -------------- |
                    | -------              x       V    ------------        x       V
                     \ snd ACK                +---------+delete TCB         +---------+
                       ---------------------->|TIME WAIT|------------------>| CLOSED  |
                                              +---------+                   +---------+

```
RFC 793 [https://datatracker.ietf.org/doc/html/rfc793#section-3.2]
