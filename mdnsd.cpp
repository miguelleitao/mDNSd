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
#include "tcpudp.h"

#define MAX_NAME_LINE 250
#define MAX_NAME_TAG 100
#define MAX_NAMES_N  8
#define NAMES_TAB_FILE "names.tab"

char *names_table[MAX_NAMES_N];
int nNames = 0;

SocketConnector *myServer;

void putWord(char *buf, unsigned short int v) {
	char *p = (char *)&v;
	buf[0] = p[1];
	buf[1] = p[0];
}

void putDWord(char *buf, unsigned int v) {
	char *p = (char *)&v;
	buf[0] = p[3];
	buf[1] = p[2];
	buf[2] = p[1];
	buf[3] = p[0];
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
  myServer->Bind(destPort);
  
  char buffer[280];
  int res;
  do {
      res = myServer->Receive((unsigned char *)buffer,250);
      if ( res>0 && res<MAX_NAME_LINE+2 ) {
  	  buffer[res] = 0;
  	  /*
  	  printf("Received: %d bytes\n", res);
  	    	      
  	      for(int i=0;i<res;i++) {
  	          printf("  %d: %d, %c\n", i, buffer[i], buffer[i]);
  	      }
  	      printf("--- End packect dump\n");
  	  */
  	  // Header is 12 bytes long, 6 * 16bits words: id + flags + 4*counters
  	  unsigned short int id = *(unsigned short int *)buffer;
  	  unsigned short int flags = *(unsigned short int *)buffer;
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
  	  // printf("LINE: '%s'\n", nline);
  	  
  	  // Got query.
  	  // Now search name in table
  	  for( int i=0 ; i<nNames ; i++ ) {
  	      if ( ! strncmp(nline,names_table[i],nline_len) ) {
  	          // printf("Found match: %d, '%s'\n", i, names_table[i]);
  	          char aPacket[MAX_NAME_LINE+12];
  	          flags = 0x8400;
  	          int query_len = res-16;
  	          putWord(aPacket + 0, id);
  	          putWord(aPacket + 2, flags);
  	          putWord(aPacket + 4, 1);			// 1 record in Query section
  	          putWord(aPacket + 6, 1);			// 1 record in Answer section
  	          putWord(aPacket + 8, 0);
  	          putWord(aPacket +10, 0);
  	          memcpy( aPacket +12, buffer+12, query_len);
  	          int a_ptr = 12+query_len;
  	          // End of query section
  	          
  	          //memcpy( aPacket+a_ptr, buffer+12, query_len);
  	          //a_ptr += query_len;
  	          
  	          //putWord(aPacket+a_ptr+0, 0x0c);	// Name is a pointer
  	          //putWord(aPacket+a_ptr+2, 0x00C);
  	          putWord(aPacket+a_ptr+0, 0x01);	// Type A
  	          putWord(aPacket+a_ptr+2, 0x01);	// Class IN
  	          
  	          
  	          putWord(aPacket+a_ptr+4, 0xc00c);	// Name is a pointer
  	          
  	          putWord(aPacket+a_ptr+6, 0x01);	// Type A
  	          putWord(aPacket+a_ptr+8, 0x01);	// Class IN
  	          
  	          putWord(aPacket+a_ptr+10, 0x1111);	//   timeout
  	          putWord(aPacket+a_ptr+12, 0x1111);	//
  	          putWord(aPacket+a_ptr+14, 0x04);	// Addr len
  	          putDWord(aPacket+a_ptr+16, myIpAddr);	// IP addr.    Must be set !!
  	          
  	  
  	  	  int pack_len =         12+query_len+20;
  	  	  /*
  	  	  for(int i=0;i<pack_len;i++) {
  	              printf("%d: %d, %c\n", i, aPacket[i], aPacket[i]);
  	          }
  	          */
       
  	          myServer->Send(aPacket,pack_len);
  	      }
  	  }
  	  // Not found. Ignore request.
      }
      else {
	  fprintf(stderr,"Error. Invalid query packet\n");
      }
  } while (res>1);
  /*-- Return 0 if exit is successful --*/
  myServer->Close();
  myServer->EndServer();
  delete myServer;
  return 0;
}
