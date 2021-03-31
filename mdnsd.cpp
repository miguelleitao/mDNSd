/*
 * mdnsd.cpp
 *
 * Multicast DNS daemon.
 *
 * Usage: ./mdnsd
 */


#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>

      #include <sys/socket.h>
       #include <netinet/in.h>
       #include <arpa/inet.h>


#include "tcpudp.h"

#define MAX_NAME_LINE 250
#define MAX_NAME_TAG 100
#define MAX_NAMES_N  8
#define NAMES_TAB_FILE "mdnsd.tab"

int verbose = 0;
char *names_table[MAX_NAMES_N];
int nNames = 0;

SocketConnector *myServer;

void putWord(char *buf, unsigned short int v) {
	char *p = (char *)&v;
	buf[0] = p[1];
	buf[1] = p[0];
}

void putDWord(char *buf, unsigned int v) {
	// Used for IPv4 addresses.
	char *p = (char *)&v;
	buf[0] = p[0];
	buf[1] = p[1];
	buf[2] = p[2];
	buf[3] = p[3];
}

int GetDefaultNetInterface(char *dev, int dlen=10) {
    FILE *f;
    char line[71];
    char *p = NULL;
    char *c;
    
    f = fopen("/proc/net/route", "r");
    if ( ! f ) return -1;

    int dn = 0;
    while(fgets(line, 70, f))
    {
	p = strtok(line, " \t");
	c = strtok(NULL, " \t");
	
	if ( p!=NULL && c!=NULL ) {
	    if ( strcmp(c, "00000000") == 0) {
		printf("Default interface is '%s'.\n", p);
		break;
	    }
	}
	dn += 1;
    }
    fclose(f);
    if ( ! p ) {
	// no default net interface
	return -1;
    }
    strncpy(dev, p, dlen);
    return dn;
}


unsigned int GetLocalIPv4Address ( )
{
    unsigned int host = 0x0100007f; // default local addr (127.0.0.1)

    char defdev[24];
    GetDefaultNetInterface(defdev);
    if ( ! defdev[0] ) {
	// no default net interface
	return host;
    }
    //which family do we require , AF_INET or AF_INET6
    int fm = AF_INET;
    struct ifaddrs *ifaddr, *ifa;
	int family;

	if ( getifaddrs(&ifaddr) == -1 ) {
	    perror("getifaddrs");
	    return host;
	}

	//Walk through linked list, maintaining head pointer so we can free list later
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr == NULL)
		continue;

	    family = ifa->ifa_addr->sa_family;

	    if ( strcmp( ifa->ifa_name, defdev) == 0 ) {
		if (family == fm) {
		    host = *(unsigned int*)(ifa->ifa_addr->sa_data+2);
		    break;
		}
	    }
	}
	freeifaddrs(ifaddr);
	// Success
	return host;
}

int GetLocalAddress ( char *host )
{
    strcpy(host,"127.0.0.1"); // default local addr
    
    char defdev[24];
    GetDefaultNetInterface(defdev);
    if ( ! defdev[0] ) {
	// no default net interface
	return -1;
    }
    /*
    FILE *f;
    char line[71];
    char *p = NULL;
    char *c;
    
    f = fopen("/proc/net/route", "r");
    if ( ! f ) return -1;

    while(fgets(line, 70, f))
    {
	p = strtok(line, " \t");
	c = strtok(NULL, " \t");
	
	if ( p!=NULL && c!=NULL ) {
		if ( strcmp(c, "00000000") == 0) {
			printf("Default interface is : %s \n", p);
			break;
		}
	}
    }
    fclose(f);
    */
    if ( ! defdev[0] ) {
	// no default net interface
	return -1;
    }
    //which family do we require , AF_INET or AF_INET6
    int fm = AF_INET;
    struct ifaddrs *ifaddr, *ifa;
	int family , s;
	//char host[NI_MAXHOST];

	if ( getifaddrs(&ifaddr) == -1 ) {
	    perror("getifaddrs");
	    return -1;
	}

	//Walk through linked list, maintaining head pointer so we can free list later
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr == NULL)
		continue;

	    family = ifa->ifa_addr->sa_family;

	    if ( strcmp( ifa->ifa_name, defdev) == 0 ) {
		if (family == fm) {
		    s = getnameinfo( ifa->ifa_addr,
			(family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
			host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			
		    if (s != 0) {
			printf("getnameinfo() failed: %s\n", gai_strerror(s));
			continue;
		    }
		    printf("address: %s\n", host);
		    break;
		}
	    }
	}
	freeifaddrs(ifaddr);
	// Success
	return 0;
}

int main (int argc, char *argv[])
{ 

  if ( argc>1 ) {
      if ( argv[1][0]=='-' ) {
          switch( argv[0][1] ) {
              case 'v':
                  verbose=1;
                  break;
          }
      }
  }
  FILE *ftab = fopen(NAMES_TAB_FILE, "r");
  if ( ! ftab ) {
      fprintf(stderr, "Error. Names table file '%s' not found.\n", NAMES_TAB_FILE);
      exit(1);
  }
  char nline[MAX_NAME_LINE+1];
  while( nNames<MAX_NAMES_N && fgets(nline, MAX_NAME_LINE, ftab) ) {
  	names_table[nNames] = strdup(nline);
  	nNames++;
  }
  fclose(ftab);
  if ( ! nNames ) {
      fprintf(stderr, "Error. No names found.\n");
      exit(2);
  }
  int destPort = 5353;
  
  unsigned int myIpAddr = GetLocalIPv4Address();
  printf("MyAddr: 0x%x\n", myIpAddr); 

  myServer = new ServerUDP();

  /* Initialize the group sockaddr structure with a */
  /* group address of 224.0.0.251 and port 5353. */
  struct sockaddr_in groupSock;
  memset((char *) &groupSock, 0, sizeof(groupSock));
  groupSock.sin_family = AF_INET;
  groupSock.sin_addr.s_addr = inet_addr("224.0.0.251");
  groupSock.sin_port = htons(destPort);

  struct in_addr localInterface;

  /* Set local interface for outbound multicast datagrams. */
  /* The IP address specified must be associated with a local, */
  /* multicast capable interface. */
  
  /*
  localInterface.s_addr = inet_addr("192.168.1.82");
  if(setsockopt(myServer->Soc, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
    perror("Setting local interface error");
    exit(1);
  }
  else
    printf("Setting the local interface...OK\n");
*/
  myServer->Bind(destPort);
  
  char buffer[280];
  int res;
  do {
      res = myServer->Receive((unsigned char *)buffer,250);
      if ( res>0 && res<MAX_NAME_LINE+2 ) {
  	  buffer[res] = 0;
  	  if ( verbose ) {
  	      printf("Received: %d bytes\n", res);
  	      unsigned int i;	      
  	      for( i=0; i<(unsigned)res ; i++ ) {
  	          printf("  %u: %u, %c\n", i, (unsigned char)buffer[i], buffer[i] );
  	      }
  	      printf("--- End packect dump\n");
  	  }
  	  // Header is 12 bytes long, 6 * 16bits words: id + flags + 4*counters
  	  //unsigned short int id    = *(unsigned short int *)(buffer+0);
  	  unsigned short int id = 256*(unsigned char)(buffer[0]);
  	  id += (unsigned char)(buffer[1]);
  	  printf("  got id:%u\n", id);
  	  unsigned short int flags = *(unsigned short int *)(buffer+2);
  	  unsigned short int counters[4];
  	  for( int i=0 ; i<4 ; i++ ) {
  	  	counters[i] = 0x100*buffer[4+2*i] + buffer[4+2*i+1];
  	  	// printf("  counter %d: %d\n", i, counters[i]);
  	  }   
  	  if ( flags & 0x80 ) {		// Response packet
  	  	printf("Response packet received. Ignoring.\n");
  	  	continue;
  	  }
  	  if ( counters[0]==0 ) {	// No query
  	  	printf("No query in received packet. Ignoring.\n");
  	  	continue;
  	  }
  	  int ini=12;			// Position Data Field
  	  char ntag[MAX_NAME_TAG+1];
  	  nline[0] = 0;
  	  int nline_len = 0;
  	  while( buffer[ini] ) {
  	      memcpy(ntag,buffer+ini+1,buffer[ini]);
  	      ntag[(int)(buffer[ini])] = 0;
  	      memcpy(nline+nline_len,ntag,buffer[ini]);
  	      nline_len += buffer[ini];
  	      nline[nline_len] = '.';
  	      nline_len++;
  	      ini += buffer[ini]+1;
  	  }
  	  nline_len--;
  	  nline[nline_len] = 0;
  	  printf("LINE: '%s' (%d)\n", nline, nline_len);
  	  
  	  // Got query.
  	  // Now search name in table
  	  for( int i=0 ; i<nNames ; i++ ) {
  	      if ( ! strncmp(nline,names_table[i],nline_len) ) {
  	          printf("Found match: %d, '%s'\n", i, names_table[i]);
  	          char aPacket[MAX_NAME_LINE+12];
  	          flags = 0x8400;
  	          putWord(aPacket + 0, id);
  	          putWord(aPacket + 2, flags);
  	          putWord(aPacket + 4, 1);			// 1 record in Query section
  	          putWord(aPacket + 6, 1);			// 1 record in Answer section
  	          putWord(aPacket + 8, 0);			// No Auth records
  	          putWord(aPacket +10, 0);			// No Additional records
  	          
  	          memcpy( aPacket+12, buffer+12, nline_len+6);
  	          if (verbose) printf("acrescentando %d a posicao 12.\n", nline_len+6);
  	          
  	          int a_ptr = 12+nline_len+6;
  	          // End of query section
  	          
  	          memcpy( aPacket+a_ptr, buffer+12, nline_len+6);

  	          a_ptr += nline_len+6;
  	          putWord(aPacket+a_ptr+0, 0x0000);	//   TTL (Hi Word)
  	          putWord(aPacket+a_ptr+2, 0xa000);	//   TTL (Low Word)
  	          putWord(aPacket+a_ptr+4, 0x04);	// Addr len
  	          putDWord(aPacket+a_ptr+6, myIpAddr);	// IP addr.    Must be set !!
  	          
  	  	  //int pack_len =         12+query_len+20-16;
  	  	  int pack_len = a_ptr + 10;
  	  	  if ( verbose ) {
	  	  	  printf("Output packet\n");
	  	  	  int i;
	  	  	  for(i=0;i<pack_len;i++) {
	  	              printf("%d: %u, %c\n", i, (unsigned char)aPacket[i], aPacket[i]);
	  	          }
  	          }
  	          myServer->Send(aPacket,pack_len);
  	      }
  	  }
  	  // Not found. Ignore request.
      }
      else {
	  fprintf(stderr,"Error. Invalid query packet. Len:%d\n", res);
      }
  } while (res>1);
  /*-- Return 0 if exit is successful --*/
  myServer->Close();
  myServer->EndServer();
  delete myServer;
  return 0;
}
