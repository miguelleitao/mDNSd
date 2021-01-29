/*
 * mdnsd.cpp
 *
 * Multicast DNS daemon.
 *
 * Usage: ./mdnsd
 */


#include <unistd.h>
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
  	  // printf("id: %d, flags:%x\n", id, flags);   
  	  if ( flags & 0x80 ) {		// Response packet
  	  	// printf("Response packet received. Ignoring.\n");
  	  	continue;
  	  }
  	  if ( counters[0]==0 ) {	// No query
  	  	// printf("No query in received packet. Ignoring.\n");
  	  	continue;
  	  }
  	  int ini=12;			// Position Data Field
  	  char ntag[MAX_NAME_TAG+1];
  	  nline[0] = 0;
  	  int nline_len = 0;
  	  while( buffer[ini] ) {
  	      memcpy(ntag,buffer+ini+1,buffer[ini]);
  	      ntag[(int)(buffer[ini])] = 0;
  	      // printf("tag: '%s'\n", ntag);
  	      /*
  	      for(i=0;i<buffer[ini];i++) {
  	          printf("%d: %d, %c\n", i, buffer[i+ini+1], buffer[i+ini+1]);
  	      }
  	      printf("--- ini=%d\n",ini);
  	      */
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
  	          putWord(aPacket+a_ptr+16, 0x9b21);
  	          putWord(aPacket+a_ptr+18, 0x1144);
  	  
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
