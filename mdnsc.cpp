/*
 * mdnsd.cpp
 *
 * Multicast DNS client.
 *
 * Usage: ./mdnsc 
 */


#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <string>
#include "tcpudp.h"

#define MAX_NAME_LINE 250
#define MAX_NAME_TAG 100
#define MAX_NAMES_N  8

char *names_table[MAX_NAMES_N];
int nNames = 0;

SocketConnector *myClient;



void putByte(char *buf, unsigned char v) {
	char *p = (char *)&v;
	buf[0] = p[0];
}

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

char *putString(char *buf, std::string str) {
	const char *s = str.c_str();
	putByte(buf, strlen(s));
	buf += 1;
	while( *s ) {
		*buf = *s;
		buf++;
		s++;
	}
	return buf;
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



int main (int argc, char *argv[])
{
  int destPort = 5353;
  
  myClient = new ClientUDP();
  
  /*
   * Socket is binded to any any fixed port.
   * From rfc6762:5.1: "One-shot Multicast DNS Queries ... MUST NOT be sent using UDP source port 5353".
   */
  myClient->Connect("224.0.0.251", destPort);
  
  //myClient->SetReadTimeout(4.);
  char aPacket[MAX_NAME_LINE+12];

  
  unsigned short int id = 0xa614;
  unsigned short int flags = 0x0120;
  putWord(aPacket + 0, id);
  putWord(aPacket + 2, flags);
  putWord(aPacket + 4, 1);			// 1 record in Query section
  putWord(aPacket + 6, 0);			// 0 record in Answer section
  putWord(aPacket + 8, 0);
  putWord(aPacket +10, 1);
  
  // Single question
  //putWord(aPacket + 12, id);
  char *qname = aPacket + 12;
  
  qname = putString(qname, argv[1]);
  
  //qname = putString(qname, "www");
  //qname = putString(qname, "local");
  //qname = putString(qname, "net");
  *qname = 0;
  qname++;
  
  // QTYPE
  putWord(qname+0, 0x0001);	// QType: A (Host Address)
  
  // QCLASS
  // Top bit on QCLASS field is Unicast-response
  unsigned short unicast_resp = 0;			// 0 (false): Response will be multicasted
  							// 1 (true):  Response will be unicasted
  unsigned short qclass = 1;				// QCLASS: 1 = "IN"  for Internet or IP networks
  putWord(qname+2, unicast_resp*0x8000 + qclass);	// QU question
  
  qname += 4;
  *qname = 0;
  qname++;
  
  putWord(qname+0,  0x0029);	// ??
  putWord(qname+2,  0x1000);	// ??
  putWord(qname+4,  0x0000);	// ??
  putWord(qname+6,  0x0000);	// ??
  putWord(qname+8,  0x000c);	// ??
  putWord(qname+10, 0x000a);	// ??
  putWord(qname+12, 0x0008);	// ??
  qname += 14;
  
  // Parte variavel
  putWord(qname+0,  0x8c51);	// ??
  putWord(qname+2,  0x1f47);	// ??
  putWord(qname+4,  0x3054);	// ??
  putWord(qname+6,  0x2eb7);	// ??
  qname += 8;
  
  int pack_len = qname-aPacket; 
  
  myClient->Send(aPacket,pack_len);
  
  //printf("Query sent. %d Bytes\n", pack_len);
  
  ///////////////
  char buffer[280];
  int res;
  do {
      printf("recebendo...\n"); 
      res = myClient->Receive((unsigned char *)buffer,250);
      printf("Retornou de receive %d bytes\n", res);
      int query_len = res-16;
      
      if ( res>0 && res<MAX_NAME_LINE+2 ) {
  	  buffer[res] = 0;
  	  
  	  printf("Received: %d bytes\n", res);
  	    	      
  	      for(int i=0;i<res;i++) {
  	          printf("  %d: %d, %c\n", i, buffer[i], buffer[i]);
  	      }
  	      printf("--- End packect dump\n");
  	  
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
  	  char nline[MAX_NAME_LINE+1];
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
      }
      else {
	  fprintf(stderr,"Error. Invalid response packet\n");
      }
  } while (res>1);
  /*-- Return 0 if exit is successful --*/
  myClient->Close();
  delete myClient;
  return 0;
}
