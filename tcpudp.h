/*
 *  Socket connector
 *
 *  Implements TCP Server, TCP Client, UDP Server and UDP Client
 */
 
#ifndef _TCPUDP_H_ 

#define _TCPUDP_H_ 

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
//#include <unistd.h>


//extern int errno;


class SocketConnector
{

    #define BUFFERSIZE  1024  		        /* Initial size of receiving buffer.
    						 * Max size of packets to be received.
    						 */
    protected:
        int  Soc;                         	/* Socket descriptor */
        struct sockaddr_in myaddr_in;  	/* For local socket address */
        int BufferSize = BUFFERSIZE;		/* Actual max size of packets to be received */
    	int cc;                   		/* Contains the number of bytes read */
    	char buffer[BUFFERSIZE];  		/* Buffer for packets to be read into */
    	int  retryDelay;			/* Delay before next connection retry */
    	double retryDelayCountDown;
    	double readTimeout;
    public:
    	int  status;
    	
    	SocketConnector() {
    	    Soc = -1;
    	    memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    	    cc = 0;
    	    status = 0;
    	    retryDelay = 1;		// 1 second
    	    retryDelayCountDown = -1; 	// Not counting
    	    readTimeout = -1.;		// Infinity. Not set
    	    //printf("SocketConnector constructed\n");
    	}
    	virtual ~SocketConnector() {
    	    //printf("Socket destruido\n");
    	}
    	// Bind is only used by servers
    	virtual int Bind(int port) {
    		fprintf(stderr,"Error. Virtual method SocketConnector::Bind() called\n");
    		return 0;
    	}
    	
    	// Bind is only used by clients.
    	// Both STREAM and DATAGRAM Socket connector use Connect().
    	virtual int Connect(const char *hostname, int port) {
    		fprintf(stderr,"Error. Virtual method SocketConnector::Connect() called\n");
    		return 0;
    	}
    	virtual int Init(const char *hostname, int port) {
    		fprintf(stderr,"Error. Virtual method SocketConnector::Init() called\n");
    		return 0;
    	}
    	int SetReadTimeout( double timeout) {
		if ( Soc<0 ) return -3;

		struct timeval tv;
		tv.tv_sec  = (int)(timeout); 				// timeout in seconds
		tv.tv_usec = (int)( (timeout-tv.tv_sec)*1000000. );	// timeout in micro seconds
		int res = setsockopt(Soc, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
		if ( res<0 ) {
	    	    fprintf(stderr,"setsockopt() failed with error code %d\n", errno);
	    	    perror("SetReadTimeout");
		}
		readTimeout = timeout;
		return res;
    	}
    	
    	int DelayRetryStepped() {
    	    //printf("Waiting for %f usec\n", retryDelayCountDown);
    	    if ( retryDelayCountDown<0 ) {
    		retryDelayCountDown = retryDelay;
    		retryDelay += 1;
    		retryDelay = ( retryDelay>10 ? 10 : retryDelay );
	    }    		
    	    double timeStep = 1.;
    	    if ( readTimeout>0. ) timeStep = ( readTimeout<timeStep ? readTimeout : timeStep );
    	    if ( retryDelayCountDown>timeStep ) {
    	    	usleep(timeStep*1000000);
    	    	retryDelayCountDown -= timeStep;
    	    	return 1;
    	    }  
    	    else {
    	    	usleep(retryDelayCountDown*1000000);
    	    	retryDelayCountDown = -1;
    	    }
    	    return 0;		// Delay timeout
    	}
    	
    	void DelayRetry() {
    	    while ( DelayRetryStepped() );
    	}

    	virtual int Receive(unsigned char *buffer, int bufsize)
    	{
    		printf("Receiving\n");
		cc = recv(Soc, buffer, bufsize , 0);
		if ( cc == -1) {
		    if ( errno!=EAGAIN && errno!=EWOULDBLOCK ) {
		    	// Unkown error.
			printf("Error: recv() errno=%d\n", errno);
			usleep(100000);
			return -2;
		    }
		    // Recv() returned by timeout.
		    // Not an error.
		    // Let the application know and act according.
		    return -1;
		}
		if ( cc==0 ) {
		    // Zero lenght message received.
		    // For TCP servers, this usually means that client disconnected.
		    // Inform application to proceed to further re-listen().
		    usleep(50000); 
		    return -2;
		}
		#ifdef _DEBUG_
		if ( cc>0 )	printf("  message (%d bytes) = %s\n", cc, buffer);
		else		printf("  null message\n");
		#endif
		return cc;
	}
	
	virtual int Send(const char * msg, int msize)
	{
		int res = send (Soc, msg, msize, 0);
		if ( res==-1 )	{
	                perror("SocketConnector::Send");
	                fprintf(stderr, "%s: unable to send message\n", "SocketConnector::Send");
		}
		return res;
	}
	
	virtual int Send(const char *msg) {
		return Send(msg, strlen(msg));
	}
	
    	virtual int Reply(const char *b, int len) {
    		fprintf(stderr,"Error. Virtual method SocketConnector::Reply() called\n");
    		return 0;
    	}
        void Close() {
	    close(Soc);
	}
	virtual void EndServer() {	// Only TCP Servers
    		fprintf(stderr,"Error. Virtual method SocketConnector::EndServer() called\n");
    	}
};

class ServerConnector : public SocketConnector
{
    protected:
        struct sockaddr_in clientaddr_in;		/* for client socket address */
        int    clientaddr_len = 0;
        
    public:
        ServerConnector() {
	    /* clear out address structures */
   	    memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));
   	    
   	    /* Set up address structure for the socket. */
   	    myaddr_in.sin_family = AF_INET;
            /* The server should receive on the wildcard address,
             * rather than its own Internet address.  This is
             * generally good practice for servers, because on
             * systems which are connected to more than one
             * network at once will be able to have one server
             * listening on all networks at once.  Even when the
             * host is connected to only one network, this is good
             * practice, because it makes the server program more
             * portable.
             */
  	    myaddr_in.sin_addr.s_addr = INADDR_ANY;
        }
};

class ClientConnector : public SocketConnector
{
        #define RETRIES 5 			/* # of times to retry before giving up */
    protected:
        struct sockaddr_in serveraddr_in;		/* for server socket address */
        int SetServer(const char *sname, int portnum)
	{
	    struct hostent *hp;

	    /* Get the host info for the server's hostname */
	    hp = gethostbyname(sname);
	    if (hp == NULL) {
	        fprintf(stderr, "%s not found in /etc/hosts\n", sname);
	        return 0;
	    }
	
	    /* Set up the server address. */
	    serveraddr_in.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
	    serveraddr_in.sin_family = hp->h_addrtype;
	    //sa.sin_family = AF_INET;
	    serveraddr_in.sin_port = htons(portnum);
	    return 1;
	}
    public:
        ClientConnector() {
	    /* clear out address structures */
	    memset ((char *)&serveraddr_in, 0, sizeof(struct sockaddr_in));
        }
        ~ClientConnector() {
        }
};

/*
 *                      C L I E N T . U D P
 *
 */
class ClientUDP : public ClientConnector
{ 
        struct hostent *hp; 			/* pointer to info for nameserver host */

    public: 
	  ClientUDP()
	  {
		
	  }
	  ~ClientUDP() {
	  }
	  int Connect(const char* servername, int portnum)
	  {
	        SetServer(servername, portnum);

		/*  allocate an open socket */
		Soc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (Soc == -1) {
		        perror("ClientConnector");
		        fprintf(stderr, "%s: unable to create socket\n", "ClientUDP");
			return 2;
		}
		#ifdef _DEBUG_
		else
	      		printf("socket created\n");
		#endif

		/*  Connect to the server.
		 *  Connecting a DGRAM socket is optional.
		 *  connect() on a datagram socket is a convenience to set a default destination peer.
		 *  Allows using send(), rather than explicitly specifying a destination for each datagram with sendto(). 
		 *  It will limit recv()s to data originating from the peer.
		 */
		if (connect(Soc, (struct sockaddr *)&serveraddr_in, sizeof serveraddr_in) < 0)
	      	{
	      		fprintf(stderr, "Connect error: %i\n",errno);
		     	return 3;
	      	}	

		// printf( "connected\n");
		retryDelay = 1;
		return 0;
	  }


};

 
//		 S E R V . U D P
class ServerUDP : public ServerConnector
{
        struct hostent *hp;       /* pointer to info of requested host */

    public:
        ServerUDP()
        {

        }
    int Bind(int port)
    {  
  	myaddr_in.sin_port = htons(port);

        /* Create the socket. */
  	Soc = socket (AF_INET, SOCK_DGRAM, 0);
  	if (Soc == -1) {
                perror("ServerUDP");
                printf("%s: unable to create socket\n", "ServerUDP");
                return 1;
  	}
        /* Bind the server's address to the socket. */
  	if (bind(Soc, (const sockaddr*)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
                perror("ServerUDP");
                printf("%s: unable to bind address\n", "ServerUDP");
                return 1;
  	}

        /* This will open the /etc/hosts file and keep
         * it open.  This will make accesses to it faster.
         * If the host has been configured to use the NIS
         * server or name server (BIND), it is desirable
         * not to call sethostent(1), because a STREAM
         * socket is used instead of datagrams for each
         * call to gethostbyname().
         */
        //sethostent(1);
        retryDelay = 1;
	return 0;
    };

    int Receive(unsigned char *buffer, int bufsize)
    {
        /* The addrlen is passed as a pointer 
         * so that the recvfrom call can return
         * the size of the returned address.
         */
        clientaddr_len = sizeof(struct sockaddr_in);

        /* This call will block until a new request arrives.
         * Then, it will return the recieved data on buffer
         * and the address of the client on from.
         */
        cc = recvfrom(Soc, buffer, bufsize, 0, (struct sockaddr *)&clientaddr_in, (socklen_t *)&clientaddr_len);
        if ( cc == -1) {
	    if ( errno!=EAGAIN && errno!=EWOULDBLOCK ) {
		printf("Bad message\n");
		return -2;
	    }
	    // recv returned by timeout.
	    // Not an error.
	    // Let the application know.
	    return -1;
	}
	//printf("  message (%d bytes) = %s\n", cc, buffer);
	return cc;
    };
    // Reply to requester.
    int Reply(const char *msg, int msize)
    {
    	// Requester is identified in clientaddr_in property.
    	if ( clientaddr_len <=0 ) {
    	    fprintf(stderr,"Cannot Reply() to null requester\n");
    	    return -1;
    	}
	int res;
	res = sendto (Soc, msg, msize, 0, (struct sockaddr *)&clientaddr_in, clientaddr_len);
	if ( res==-1 ) {
            perror("ServerUDP::Reply");
            fprintf(stderr, "%s: unable to send messge to requester\n", "ServerUDP::Reply");
	}
	return res;
    };
    
    int Send(const char *msg, int msize) {
    	return Reply(msg,msize);
    }
    
};


 
//		 S E R V . T C P
class ServerTCP : public ServerConnector
{
    int listenSoc;			/* Listen descriptor */

  public:
    ServerTCP()
    {
    	listenSoc = -1;
    };
    int Bind(int port)
    {
        if ( status<1 ) {
		//sp = new struct servent;   
	  	myaddr_in.sin_port = htons(port);

		/* Create the socket. */
	  	listenSoc = socket (AF_INET, SOCK_STREAM, 0);
	  	if (listenSoc == -1) {
		        perror("ServerTCP");
		        printf("%s: unable to create socket\n", "ServerTCP");
		        return 1;
	  	}
	  	status = 1;
	}
  	
  	if ( status<2 ) {
		/* Bind the server's address to the socket. */
	  	if (bind(listenSoc, (const sockaddr*)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
		        perror("ServerTCP");
		        printf("%s: unable to bind address\n", "ServerTCP");
		        return 1;
	  	}
	  	status = 2;
	}
  	
  	if ( status<3 ) {
	  	if (listen(listenSoc, 1) < 0) { 
			perror("ServerTCP listen"); 
			exit(EXIT_FAILURE); 
	    	}
	    	status = 3;
	}
    	 
    	if ( status<4 ) { 
	    	int clientaddr_len = sizeof(clientaddr_in);
	    	printf("Waiting for TCP connection...\n");
	    	if (( Soc = accept(listenSoc, (struct sockaddr *)&clientaddr_in,  
		               (socklen_t*)&clientaddr_len))<0) 
	    	{ 
			perror("accept"); 
			exit(EXIT_FAILURE); 
	    	}
	    	status = 4;
    	}
	retryDelay = 1;
    		 
	return 0;
    };
    
    void EndServer() {
    	close(listenSoc);
    }
};

class ClientTCP : public ClientConnector
{ 
 
          struct hostent *hp; 		/* pointer to info for nameserver host */
    public: 
	  ClientTCP()
	  {

	  };
	  int Connect(const char* servername, int portnum)
	  {
	        SetServer(servername, portnum);

		/*  allocate an open socket */
		Soc = socket(AF_INET, SOCK_STREAM, 0);
		if (Soc == -1) {
		        perror("ClientTCP");
		        fprintf(stderr, "%s: unable to create socket\n", "ClientTCP");
			return 2;
		}
		#ifdef _DEBUG_
	      		printf("socket created\n");
		#endif

		/*  connect to the server */
		if (connect(Soc, (struct sockaddr *)&serveraddr_in, sizeof serveraddr_in) < 0) {
	      		fprintf(stderr, "connect error - %i\n",errno);
		     	return 3;
	      	}	
		retryDelay = 1;
		#ifdef _DEBUG_
		printf( "Connected to server %s:%d\n",servername,portnum);
		#endif
		return 0;
	  }
};



#endif






