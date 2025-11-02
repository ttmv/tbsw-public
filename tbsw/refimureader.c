/* Reference IMU sensor component code */ 

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "misc.h"
#include "refimureader.h"



int referenceOn = 0;

//status vars
char *refStatusStr = NULL;
int rStatusRequested = 0;
int rStatusAvailable = 0;



void stopRefImu() {
  if(referenceOn) fprintf(stderr, "stopping reference logging\n");
  else fprintf(stderr, "Reference IMU not logging\n");
  referenceOn = 0;
}


/* Status request handling functions */ 

void requestRefStatus() {
  fprintf(stderr, "Ref imu status requested, ref on: %d\n", referenceOn);

  if(!referenceOn) {
    rStatusAvailable = 1;
  }
  else {
    rStatusRequested = 1;
  }
}


int refStatusAvailable() {
  printf("referece status available? %d\n", rStatusAvailable);
  return rStatusAvailable;
}


char *readRefStatus() {
  printf("ref status called\n");
  if (!referenceOn) return "Ref IMU is not on\n";
  return refStatusStr;
}

void refStatusReceived() {
  fprintf(stderr,"ref received\n");
  //if(refStatusStr)
  //  strcpy(refStatusStr, "");

  rStatusAvailable = 0;
}


void initRefStatus() {
  refStatusStr = malloc(80*sizeof(char));
  strcpy(refStatusStr, "Ref IMU: status not available\n");
}


void createStatus(char *info) {
  printf("creating ref status\n");
  char tempStatusStr[180];
  sprintf(tempStatusStr, "Ref IMU: %s at %ld\n", info, getCurrentTime());
  printf("temp status: %s", tempStatusStr);
  strcpy(refStatusStr, tempStatusStr);
  printf("ref status: %s", refStatusStr);
  rStatusAvailable = 1;
  //rStatusRequested = 0;
  fprintf(stderr, "ref imu status ready at %ld\n", getCurrentTime());
  printf("ref status ready\n");
}



/*  */ 
void startRefImu(struct SenInfoST *runStat, void (*sendMsgPtr)(char *msg)) {
  // KVH imu stream can be read from some ttyUSB device, but it's not always the same
  // script is run to find the correct device and configure it
  system("/home/tbuser/koodit/sensors/threaded/scripts/refimusettings.sh");
  sleep(1);

  //long curTime;

  fprintf(stderr, "Initializing ref imu\n");

  int maxBytes = 1024;
  char nextLine[maxBytes];


  char portInfoFile[50] = "/home/tbuser/koodit/sensors/threaded/refport.txt";
  FILE *pifp = fopen(portInfoFile, "r");

  if(!pifp) {
    fprintf(stderr, "REF: could not open port info file\n");
    (*sendMsgPtr)("REFERENCE: could not open port for reading");
    return;
  }

  char refPort[12];
  fscanf(pifp, "%s", refPort);
  fclose(pifp);

  printf("found port %s\n", refPort);

  FILE* refFp = fopen(refPort, "rb");

  if(!refFp) {
    printf("REF: could not open ref port\n");
    (*sendMsgPtr)("REFERENCE: could not open port for reading");
    return;
  }

  char *datapath = SENSORDATAPATH;
  char reffilename[54]; //= "../../sensordata/refdata-";
  strcpy(reffilename, datapath);
  strcat(reffilename, "refdata-");

  char reftimestr[14];
  long refStartTime = getCurrentTime();  

  sprintf(reftimestr, "%ld", refStartTime);

  strcat(reffilename, reftimestr);
  strcat(reffilename, ".bin");

  FILE* reffile = fopen(reffilename, "ab");

  if(!reffile) {
    fprintf(stderr, "REF: cannot open file to write\n");
    (*sendMsgPtr)("REFERENCE: could not open file to write");
    return;
  }

  fprintf(stderr, "REF: recording to file %s\n", reffilename);


  int readErrors = 0;
  int totalReadErrors = 0;
  size_t totalBytes = 0;

  initRefStatus();
  referenceOn = 1;
  runStat->r = 1;

  while(referenceOn) {
    size_t N = fread(nextLine, 1, maxBytes, refFp);
    //printf("ref: %zu bytes read\n", N);
    

    if(N < 1) {
      if(rStatusRequested) {
        printf("statusreq process");
        rStatusRequested = 0;
        createStatus("read error");
      }
      //fprintf(stderr, "KVH: no bytes read\n");
      //break;
      readErrors++;
      if (readErrors > 10) {
        (*sendMsgPtr)("REFERENCE: reading errors, rerunning settings ...");
        // Sometimes it forgers settings for some reason and no data is read, 
        // run settings script to change them back 
        system("/home/tbuser/koodit/sensors/threaded/scripts/refimusettings.sh");
        totalReadErrors += readErrors;
        readErrors = 0;
        sleep(3);
      }
      if (totalReadErrors > 200) {
        (*sendMsgPtr)("REFERENCE: cannot read data");
        break; //didn't work
      }
    } 

    else {
      //printf("ref bytes read, statusreq: %d\n", rStatusRequested);
      if(rStatusRequested) {
        printf("statusreq process");
        rStatusRequested = 0;
        createStatus("recording");        
      }

      totalBytes += N;    
      fwrite(nextLine, 1, N, reffile);
      memset(nextLine, 0, maxBytes);
    }
  }

  runStat->r = 0;
  printf("Ref: %d readErrors, %d total, %zu bytes read\n", readErrors, totalReadErrors, totalBytes);
  printf("Ref: reading done, closing files...\n");

  if(refStatusStr) {
    free(refStatusStr);
  }  

  if(refFp) {
    fclose(refFp);
  }

  if(reffile) {
    fclose(reffile);
    printf("Ref: port closed, all done\n");
  }
}


/*
int main() {
  startReference();
  return 0;
}*/

