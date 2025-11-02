
/*  
 *  uBlox sensor component code. 
 *  Reads UBX and NMEA messages from USB port uBlox is connected, 
 *  saves messages to UBX and NMEA files, and processes uBlox status queries
*/


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "misc.h"
#include "ubloxreader.h"


//char sensorDataPath[30] = "/home/koodit/sensordata/";
//char tbswPath[35] = "/home/koodit/sensors/threaded/";


/* Structs for UBX message processing */ 
struct UBXhead {
  char s1;
  char s2;
  char msgclass;
  char id;
  uint16_t length;
}; 

struct UBXplStart {
  uint32_t itow;
  char version;
  char numSvs;    
};

struct UBXinfoblock {
  char gnssId;
  char svId;
  char cno;
};

//----


int ubloxOn = 0;
int readStatus = 0;
char *uStatusStr = NULL;
int statusAvailable = 0;


/* Status request functions */

void ubloxStatusReq() {
  printf("ublox status requested\n");

  if(!ubloxOn) {
    uStatusStr = "Ublox is not on\n";
    statusAvailable = 1;
  } else {
    readStatus = 1;
  }
}

int uStatusAvailable() {
  printf("ublox status available? %d\n", statusAvailable);
  return statusAvailable;
}

void uStatusReceived() {
  //if (uStatusStr)
  //  strcpy(uStatusStr, "");
  statusAvailable = 0;
}

char *readUbloxStatus() {
  printf("ublox: return status called\n");
  return uStatusStr;
}



/* Main control functions */

void stopUblox() {
  if(ubloxOn) fprintf(stderr, "stopping ublox logging\n");
  else fprintf(stderr, "Ublox not logging\n");
  ubloxOn = 0;
}


void startUblox(struct SenInfoST *runstat, void (*sendMsgPtr)(char *msg)) {
  printf("ublox: runstat: %d\n", runstat->u);
  // apply ublox settings (baud rate and raw data)   
  system("/home/tbuser/koodit/sensors/threaded/scripts/ubloxsettings.sh");

  //fprintf(stderr, "ublox path: %s\n", SENSORDATAPATH);
  //variables for stats and debug print rate:
  int partialNmeas = 0;
  int nmeaTotal = 0;
  int latestEnd = 0;
  int latestStart = 0;
  int linecount = 0;
  int ubPrintRate = 500;
  int ubcounter = 0;

  
  int uStatusLen = 8000;
  uStatusStr = malloc(uStatusLen*sizeof(char));

  printf("UBLOX starting logging\n");
  //(*sendMsgPtr)("UBLOX starting");
  ubloxOn = 1;

  int maxBytes = 8000;
  char nextline[maxBytes];
  char nmeastr[255];

  int partialNMEA = 0;
  int partialUBX = 0;
  long curTime;

  // port for uBlox is always /dev/ttyACM0
  FILE* ubxp = fopen("/dev/ttyACM0", "rb");
  
  if(!ubxp) {
    fprintf(stderr, "UBLOX: could not open ttyACM0 for reading\n");
    (*sendMsgPtr)("UBLOX: could not open ttyACM0 for reading. Check cables.");
    return;
  }

  // ---- filenames ---
  //char nmeafilename[44] = "../../sensordata/ubloxnmea-";
  //char ubxfilename[43] = "../../sensordata/ubloxUBX-";
  char *datapath = SENSORDATAPATH;

  //printf("datapath: %s\n", datapath);
  char nmeafilename[56]; //= SENSORDATAPATH;
  char ubxfilename[55]; //= SENSORDATAPATH;

  strcpy(nmeafilename, datapath);
  strcpy(ubxfilename, datapath);

  //printf("filename: %s\n", nmeafilename);

  strcat(nmeafilename, "ubloxnmea-");
  strcat(ubxfilename, "ubloxUBX-");
 
  //printf("filename2: %s\n", nmeafilename);
  //printf("filename2: %s\n", ubxfilename);
  char utimestr[14];
  long uStartTime = getCurrentTime();  

  sprintf(utimestr, "%ld", uStartTime);

  strcat(nmeafilename, utimestr);
  strcat(nmeafilename, ".txt");

  strcat(ubxfilename, utimestr);
  strcat(ubxfilename, ".bin");


  // --- end of filenames ---

  FILE* nmeafile = fopen(nmeafilename, "a");
  if (!nmeafile) {
    (*sendMsgPtr)("UBLOX: could not open file for NMEA messages");
  }

  FILE* ubxfile = fopen(ubxfilename, "ab");
  if (!ubxfile) {
    (*sendMsgPtr)("UBLOX: could not open file for UBX messages");

    if(!nmeafile) return;
  }


  runstat->u = 1; //ublox started

  while(ubloxOn) {
    //more stats
    int nmeacount = 0;
    int ubxcount = 0;


    int startPoint = 0;

    /* Read data from ublox port to buffer */ 
    size_t N = fread(nextline, 1, maxBytes, ubxp);
    curTime = getCurrentTime(); //time of read

    if(N < 1) {
      fprintf(stderr, "Ublox: no bytes read\n");
      //break;
    }
    
    linecount++;    
    //printf("------------------------------ read %u bytes -------------------\n", N);


    /* Process the data in buffer */ 

   
    /* Find the rest of NMEA message if last read was incomplete and write to NMEA data file: */ 

    if(partialNMEA>0) {
      for (int i=0; i<(int)N-1; i++) { 
        if(nextline[i] == 0x0d && nextline[i+1] == 0x0a) {
          //printf("end of nmea at index %d\n", i+1);
          strncpy(nmeastr, nextline, i+2);
          nmeastr[i+2] = '\0';
          fprintf(nmeafile, "%s", nmeastr);
          startPoint = i+2;
          partialNMEA = 0;   

          //printf("break at partial nmea, end of nmea at %d\n", i+1);
          break;            
        }
      }     
    }
    
    /* Find the rest of UBX message if last read was incomplete and write to UBX data file:
       partialUBX is the amount of bytes missing from the latest UBX message. 
    */ 

    if(partialUBX>0) {
      fwrite(nextline, 1, partialUBX, ubxfile);
      startPoint = partialUBX;
      partialUBX = 0;
      //printf("wrote rest of UBX, end at %d\n", startPoint);
    }
    

    /* Default data reading loop, first searching the start of nmea or ubx message: */

    for (int i = startPoint; i < (int) N; i++) {
      
      //NMEA found:
      if(nextline[i] == 0x24 && nextline[i+1] == 0x47) { //next part starts with "$G"
        //printf("nmea start at %d, count in buf %d\n", i, nmeacounter);
        
        int endInd = 0;
        
        if(latestStart < i) latestStart = i; 
   
        // search for the end of NMEA message:     
        for (int j = i+1; j < (int) N; j++) {
          if(nextline[j] == 0x0d && nextline[j+1] == 0x0a) { //end of NMEA msg found in buffer, NMEA msg is complete
            //printf("end of nmea at index %d\n", j+1);
            endInd = j+1;
        
            //update stats
            if(j+1 > latestEnd) latestEnd = j+1;
            
            j = N;
          }
        }
        

        if (endInd < i) { //end of NMEA msg not found in buffer
          endInd = N-1;          
          partialNMEA = 1;
          partialNmeas++;
        }        
        
        int msgLen = endInd - i;
        
        strncpy(nmeastr, &nextline[i], msgLen+1);
        //printf("strend: %x %x\n", nmeastr[msgLen-1], nmeastr[msgLen]);
        
        // NMEA messages end with <CR><LF> (=0x0d 0x0a) 
        // if last byte is <CR> then <LF> is the only thing missing
        // and can be added. 

        if(endInd == (int) N && nextline[N-1] == 0x0d) {
          nmeastr[msgLen+1] = 0x0a; 
          nmeastr[msgLen+2] = '\0';
          partialNMEA = 0;
        } else {
          nmeastr[msgLen+1] = '\0';
        }
        
        nmeaTotal++;        

        if((ubcounter %ubPrintRate == 0)) printf("nmea: %s", nmeastr);
        ubcounter++;

        //curTime = getCurrentTime(); 
        fprintf(nmeafile, "%ld,%s", curTime, nmeastr);
        i = endInd;
        nmeacount++;
      } 


      /* UBX message found. All UBX frames start with sync characters 0xB5 0x62. 
         All UBX messages received from uBlox are logged. */
     
      if ((nextline[i] & 0xFF) == 0xb5 && nextline[i+1] == 0x62) {

        struct UBXhead *ubxhead;
        ubxhead = (struct UBXhead*) &nextline[i];
        
        //debug print
        if(ubcounter %ubPrintRate == 0) {
          printf("ubx start at %d: %x %x %x ", i, (nextline[i] & 0xFF), nextline[i], nextline[i+1]);
          printf("struct: %x %x\n", ubxhead->s1, ubxhead->s2);
          printf("ubx class %x, id %x %x\n", nextline[i+2], nextline[i+3], ubxhead->id);
          printf("ubx length %x %x, %u\n", nextline[i+4], nextline[i+5], ubxhead->length);
        } 

        // message length from UBX header
        int umsglen = (int)ubxhead->length + 8;
        

        // if ubx message length is longer than bytes read we have incomplete ubx message
        // mark the amount of missing bytes 

        if (i+umsglen > (int) N) {
          partialUBX = (i+umsglen) - N; //missing bytes         
          umsglen = N-i;
          fprintf(stderr, "partial ubx, %d left\n", partialUBX);
        }


        // write timestamp and ubx msg to ubxfile
        fwrite(&curTime, 1, sizeof(long), ubxfile); 
        fwrite(&nextline[i], 1, umsglen, ubxfile);
        //fprintf(stderr, "wrote ublox to file\n");


        
        // ------- read status from ubx message if requested and status msg received: ---------- 
        // ------- status is read only from complete messages to make things simpler  ----------

        if (readStatus && !partialUBX && ubxhead->msgclass == 0x01 && ubxhead->id == 0x35) { 
          fprintf(stderr, "ublox status msg found, extracting info ...\n");
          //uStatusLen = 512;
          memset(uStatusStr, 0, uStatusLen);
          char ubxStatusMsg[umsglen+1]; //[umsglen] pitäs riittää?
          char finalStatus[8000] = "Ublox: ";

          //int satCount = 0;
          int galileoCount = 0;
          int otherSatCount = 0;
          //int cno40count = 0;
          //int cno20count = 0;


          printf("msglen: %d\n", umsglen);
          memcpy(ubxStatusMsg, &nextline[i], umsglen);
          printf("ubx class %x, id %x\n", ubxStatusMsg[2], ubxStatusMsg[3]);       

          struct UBXplStart *ubxStart;
          ubxStart = (struct UBXplStart*) &ubxStatusMsg[6];

          printf("time: %u, numSvs: %d, version: %x\n", ubxStart->itow , ubxStart->numSvs, ubxStart->version);

          int blockCount = (int) ubxStart->numSvs;

          char satellitesStr[20];
          sprintf(satellitesStr, "%d satellites; ", blockCount);
          strcat(finalStatus, satellitesStr);

          printf("%s\n", finalStatus);
          
          
          int blockStart = ((int) sizeof(struct UBXhead)) + 8; 

          struct UBXinfoblock *ubxinfoblock;
          //int minCno = 0;
          int maxCno = 0;          

          for (int j = 0; j<blockCount; j++) {
            ubxinfoblock = (struct UBXinfoblock *) &ubxStatusMsg[blockStart];
            //ubxinfoblock = (struct UBXinfoblock *) &nextline[i+blockStart]; 
            //printf("block %d: sat %x %x: cno: %x\n", j+1, ubxinfoblock->svId, ubxinfoblock->gnssId, ubxinfoblock->cno);
            //printf("block %d: sat %d gnssId: %d, cno: %d\n", j+1, ubxinfoblock->svId, ubxinfoblock->gnssId, ubxinfoblock->cno);
            
            if(ubxinfoblock->gnssId ) {
              galileoCount++;
            } else {
              otherSatCount++;
            }

            if(ubxinfoblock->cno > maxCno) maxCno = ubxinfoblock->cno;

            if(ubxinfoblock->cno > 39) { //testiä pidemmillä statusviesteillä koska yleensä niillä kaatuu
              char cnoEnough[40];
              sprintf(cnoEnough, "gnssId: %d, svId: %d, C/No: %d;", ubxinfoblock->gnssId, ubxinfoblock->svId, ubxinfoblock->cno);
              strcat(finalStatus, cnoEnough);
            }            

            blockStart = blockStart + 12;                       
          }         
          
          char satStats[50];
          sprintf(satStats, "Galileos: %d, other: %d, max C/No: %d\n", galileoCount, otherSatCount, maxCno);
          strcat(finalStatus, satStats);
          printf("STATUS: len: %lu INFO: %s", strlen(finalStatus), finalStatus);

          strncpy(uStatusStr, finalStatus, (strlen(finalStatus) + 1));
          //printf("status: %s", uStatusStr);
          statusAvailable = 1;
          readStatus = 0;  

          /*
          struct UBXplStart {
            uint32_t itow;
            unsigned char version;
            unsigned char numSvs;    
          };

          struct UBXinfoblock {
            unsigned char gnssId;
            unsigned char svId;
            unsigned char cno;
          };

          */    
          //statusmessage 
          //statusAvailable = 1;
          //readStatus = 0;            
        }
        // ---- 

        ubxcount++;
        i = i+umsglen;
      } //end of UBX processing 
    }

    
    memset(nextline, 0, maxBytes);
    //printf("---------------- ubxcount: %d, nmeacount: %d -------------------\n", ubxcount, nmeacount);
    ubxcount = 0;
    nmeacount = 0;
  }

  // recording stopped, closing files and ports etc. 
  runstat->u = 0;
  fprintf(stderr, "ublox: freeing status ...\n");
  free(uStatusStr);
  fprintf(stderr, "ublox: closing port and files ...\n");
  fclose(ubxp);
  fclose(nmeafile);
  fclose(ubxfile);
  fprintf(stderr, "UBLOX: done reading %d lines. Latest start: %d, latest end: %d\n", linecount, latestStart, latestEnd);
}



/*
int main() {
  startUblox();
  return 0;
}
*/
