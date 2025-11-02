/* Main TB control code */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include "jsmn.h"
#include "misc.h"
#include "awinda.h"
#include "realsensecam.h"
#include "b210.h"
#include "ubloxreader.h"
#include "refimureader.h"

#define MAXLINE 65536 //net.core.wmem_default/rmem_default = 212992, rmem_max = 50000000, wmem_max = 1048576 
#define SOCKET_NAME "/tmp/9Lq7BNBnBycd6nxy.socket"

int max(int a, int b) { return ((a > b) ? a : b);}

void sendStatus();

int sensorsOn = 0;
int sensorsStarted = 0;
int remotectrl = 1; //default
int newStatusAvailable = 0;


// int x = 1 when sensor is recording, 0 otherwise


/*
struct SenInfoST {
  int a;
  int u;
  int t;
  int b;
  int r;
};
*/

struct SenInfoST *runStat; //monitor when selected sensors start


struct SenInfoST ssel;
char statusResp[MAXLINE];
pthread_t ctrthread;


void errors(char *str) {
  perror(str);
  exit(1);
}


void initRunStat() {
  runStat = (struct SenInfoST*)malloc(sizeof(struct SenInfoST)); 
  runStat->a = 0;
  runStat->u = 0;
  runStat->t = 0;
  runStat->b = 0;
  runStat->r = 0;
}



int checkSensorStatus() {
  printf("checking sensor status\n");
  if(!sensorsOn) {
    strcpy(statusResp, "Status: No sensors currently recording");
    return 1; 
  }

  char *ubloxStatus;
  char *awindaStatus;
  char *cameraStatus;
  char *usrpStatus;
  char *refImuStatus;
  
  ubloxStatusReq();
  requestUSRPStatus();
  requestRefStatus();

  int maxWait = 10;
  int timeWaited = 0;

  while((!uStatusAvailable() || !usrpStatusAvailable() || !refStatusAvailable()) && sensorsOn) {
  //while(!uStatusAvailable() && sensorsOn) {
    if (timeWaited > maxWait) {
      break;
      //strcpy(statusResp, "Status: cannot read Ublox status");
      //return 1;
    }
    
    printf("ustatusavailable: %d\n",uStatusAvailable()); //, usrpStatusAvailable()); 
    timeWaited++;
    sleep(1);
  }

  //printf("ublox available?\n");

  ubloxStatus = readUbloxStatus();


  if(!uStatusAvailable() || !ubloxStatus || strlen(ubloxStatus) == 0) {
    ubloxStatus = "Ublox: no info"; 
    printf("ublox status not available");
  }

  usrpStatus = readUSRPStatus();

  
  if(!usrpStatusAvailable() || !usrpStatus || strlen(usrpStatus) == 0) {
    usrpStatus = "USRP: no info"; 
    printf("USRP status not available");
  }
  

  refImuStatus = readRefStatus();
  
  if(!refStatusAvailable() || !refImuStatus || strlen(refImuStatus) == 0) {
    refImuStatus = "Ref IMU: no info"; 
    printf("Ref IMU status not available");
  }



  //printf("ubx status from caller: %s, %lu\n", ubloxStatus, strlen(ubloxStatus));
  awindaStatus = readAwindaStatus();
  //printf("awinda status from caller: %s, %lu\n", awindaStatus, strlen(awindaStatus));
  cameraStatus = readRsStatus();
  //printf("RealSense status from caller: %s, %lu\n", cameraStatus, strlen(cameraStatus));
  
  int statLen = (int)(strlen(ubloxStatus) + strlen(awindaStatus) + strlen(cameraStatus) + strlen(usrpStatus) + strlen(refImuStatus)); 

  char statusmsg[statLen+13];
  printf("statlen: %d\n", statLen);
  strcpy(statusmsg, "Status: ");
  strcat(statusmsg, ubloxStatus);
  strcat(statusmsg, "_");
  strcat(statusmsg, awindaStatus);
  strcat(statusmsg, "_");
  strcat(statusmsg, cameraStatus);
  strcat(statusmsg, "_");
  strcat(statusmsg, usrpStatus);
  strcat(statusmsg, "_");
  strcat(statusmsg, refImuStatus);


  printf("infostatus length: %lu\n", strlen(statusmsg));
  printf("status: %s\n", statusmsg);

  strcpy(statusResp, statusmsg);
  printf("status: %s\n", statusResp);

  uStatusReceived();
  fprintf(stderr, "ublox stat received\n");
  usrpStatusReceived();
  fprintf(stderr, "usrp stat received\n");
  refStatusReceived();
  fprintf(stderr, "stat received\n");
  //if(strstr(ubloxStatus, "cno")!= NULL) free(ubloxStatus);
  //else printf("tbsw: not freeing ubxstatus\n");
  
  return 1;
}


void sendMessageFromSensor(char *msg) {
  fprintf(stderr, "msg: %s\n", msg);
  strcpy(statusResp, msg);
  sendStatus();  
}

void (*sendMsgPtr)(char *msg) = &sendMessageFromSensor;


void stopSensors() {
  printf("stopsensors called\n");
  sensorsOn = 0;  
  stopCamera();
  stopAwinda();
  stopUsrp();
  stopUblox();
  stopRefImu();
}


/* Sensor threads */

void *awindaTh() {
  initAwinda(runStat, sendMsgPtr);
  sleep(1);
  pthread_exit(NULL);
}

void *cameraTh() {
  startCamera(runStat, sendMsgPtr);
  sleep(1);
  pthread_exit(NULL);
}

void *b210Th() {
  startUsrp(runStat, sendMsgPtr);
  sleep(1);
  pthread_exit(NULL);
}

void *ubloxTh() {
  startUblox(runStat, sendMsgPtr);
  sleep(1);
  pthread_exit(NULL);
}


void *refImuTh() {
  startRefImu(runStat, sendMsgPtr);
  sleep(1);
  pthread_exit(NULL);
}


/* ---- */

int startSensors() {
  sensorsOn = 1;
  newStatusAvailable = 1;
  //int thCount = ssel.a + ssel.u + ssel.t + ssel.b;

  int thCount = ssel.a + ssel.u + ssel.t + ssel.b + ssel.r;
  //struct SenInfoST reqSensors = ssel;
    

  printf("thcount: %d\n", thCount);
  pthread_t threads[thCount];
  int err;
  printf("start sensors\n");

  printf("%d, %d, %d, %d\n", ssel.a, ssel.u, ssel.t, ssel.b);

  for (int i = 0; i<thCount; i++) {
    if(ssel.a) {
      err = pthread_create(&threads[i], NULL, awindaTh, NULL);
      if(err) {
        fprintf(stderr, "ERROR, awinda, code %d\n", err);
        exit(-1);
      }
      ssel.a = 0;
    } 

    else if(ssel.u) {
      err = pthread_create(&threads[i], NULL, ubloxTh, NULL);
      if(err) {
        fprintf(stderr, "ERROR, ublox, code %d\n", err);
        exit(-1);
      }
      ssel.u = 0;
    } 
        
    else if(ssel.t) {
      printf("starting realsense thread\n");
      err = pthread_create(&threads[i], NULL, cameraTh, NULL);
      if(err) {
        fprintf(stderr, "ERROR, realsense, code %d\n", err);
        exit(-1);
      }
      ssel.t = 0;
    } 

    else if(ssel.b) {
      err = pthread_create(&threads[i], NULL, b210Th, NULL);
      if(err) {
        fprintf(stderr, "ERROR, usrp, code %d\n", err);
        exit(-1);
      }
      ssel.b = 0;
    } 

    
    else if(ssel.r) {
      err = pthread_create(&threads[i], NULL, refImuTh, NULL);
      if(err) {
        fprintf(stderr, "ERROR, reference, code %d\n", err);
        exit(-1);
      }
      ssel.r = 0;
    } 

    else {
      fprintf(stderr, "unknown sensor or no sensors selected\n");
    }
  }

  int sensorsWaited = 0;

  while(sensorsOn && sensorsWaited < 500) {
    sleep(2); 
    printf("runstat: u: %d, b: %d, t: %d, a: %d\n", runStat->u, runStat->b, runStat->t, runStat->a);
    printf("thcount: %d, stats: %d\n", thCount, (runStat->a + runStat->u + runStat->t + runStat->b + runStat->r)); 
    if(thCount == (runStat->a + runStat->u + runStat->t + runStat->b + runStat->r)) {
      fprintf(stderr, "selected sensors started\n");
      strcpy(statusResp, "Sensors started");
      sendStatus();
      break;  
    }
    sensorsWaited++;
  }
        


  /* wait for sensor threads to stop and join them */  
  
  for(int i = 0; i<thCount; i++) {
    printf("join %d\n", i);
    pthread_join(threads[i], NULL);
    printf("done thread %d\n", i);
  }

  sleep(2);
  printf("done joining\n");
  return 0;
}


void *comm() {
  startSensors();
  //sensorsOn = 0;
  fprintf(stderr, "all done\n");
  pthread_exit(NULL);    
}



int selectOp(int c) {
  printf("%d %c\n", c, c);
  char *status;
  switch (c) {
    case 104: //'h'
      printf("runstat: u: %d, b: %d, t: %d, a: %d\n", runStat->u, runStat->b, runStat->t, runStat->a);
      status = "Start sensors: s, stop sensors: e";
      break;
    case 105: //'i' (info), request status info once
      printf("sensor status info requested\n");
      return checkSensorStatus();  
    case 115: //'s'
      if(!sensorsStarted) {
        sensorsStarted = 1;
        int err = pthread_create(&ctrthread, NULL, comm, NULL);

        if(err) {
          fprintf(stderr, "ERROR, code %d\n", err);
          exit(-1);
        }
        status = "starting sensors";
      } else {
        status = "sensors already started";
      }
      break; 
  
    case 101: //'e'
      if(sensorsStarted) {
        printf("stopping sensors\n");
        stopSensors();
        pthread_join(ctrthread, NULL);
        sensorsStarted = 0;
        printf("stopped\n");
        status = "sensors stopped";
      } else {
        status = "no sensors running";
      }
      break;
    case 111: //'o', just don't override conf status
      printf("110\n");
      return 1;  
    case 113: //'q'
      if(sensorsStarted) {
        printf("stop sensors before quitting\n");
        break;
      } else {
        printf("quitting\n");
        return 0;
      }
    default: 
      printf("sensorsOn: %d\n", sensorsOn);
      //status = "unknown command";
      break;
  }

  strcpy(statusResp, status);
  printf("%s\n", status);
  return 1;
}


// control loop for cli mode

int mainloop() {
  printf("sw started from cli\n");
  printf("Start sensors: s, stop sensors: e, quit: q, sensorsStarted: any\n");
  int c = 0;
  sensorsStarted = 0;
  int opstat;

  for (;;) {

    // sensor selection. 1=selected, 0=not selected
    ssel.a = 1;
    ssel.u = 1;
    ssel.t = 1;
    ssel.b = 1;
    ssel.r = 1;          

    //read from user
    c = getc(stdin);
    printf("read %c %d\n", c, c);
    opstat = selectOp(c);
    printf("INFO: %s\n", statusResp);
    if (opstat == 0) {
      printf("stopping main loop\n");
      break;
    }
  }  

  return 0;
}


// Socket code 

int gsockfd = -1;

void sendStatus() {
  if(gsockfd < 0) {
    fprintf(stderr, "TestBed: no connection\n");
    return;
  }

  int nresp;

  if ( (nresp = write(gsockfd, statusResp, strlen(statusResp))) < 0) {
    if (errno != EWOULDBLOCK) errors("write error to socket");
  } else {
    fprintf(stderr, "wrote %d bytes to socket\n",  nresp);
  }
}


void remoteloop(int sockfd) {
  printf("new client connected\n");
  gsockfd = sockfd;
  int readInd = 0;
  int maxfdp1, val, stdineof;
  int n;
  fd_set rset, wset;

  char fr[MAXLINE];
  
  //pointers to correct parts of buffers  
  char *friptr, *froptr;  

  //set sockfd and stdout to be nonblocing
  val = fcntl(gsockfd, F_GETFL, 0);
  fcntl(gsockfd, F_SETFL, val | O_NONBLOCK);

  val = fcntl(STDOUT_FILENO, F_GETFL, 0);
  fcntl(STDOUT_FILENO, F_SETFL, val | O_NONBLOCK);

  //set pointers to point to the beginning of buffers
  friptr = froptr = fr;

  stdineof = 0;
  maxfdp1 = max(max(STDIN_FILENO, STDOUT_FILENO), gsockfd) + 1;  

  int loopcounter = 0;

  for(;;) {    
    //init readset, writeset
    FD_ZERO(&rset);
    FD_ZERO(&wset);

   // printf("loop %d\n", loopcounter);
    loopcounter++;

    //there is space left to read from socket
    if (friptr < &fr[MAXLINE]) {
      FD_SET(gsockfd, &rset); /* read from socket */
     // printf("space left to read from socket\n");
    }    

    select(maxfdp1, &rset, &wset, NULL, NULL);

    if (FD_ISSET(gsockfd, &rset)) {      
      if ((n = read(gsockfd, friptr, &fr[MAXLINE] - friptr)) < 0) {
        if (errno != EWOULDBLOCK) errors("read error on socket");        
      } 

      else if(n == 0) {
        fprintf(stderr, "EOF on socket\n");
        if (stdineof) 
          return; 
        else {
          errors("str_cli: server terminated prematurely");
        }
      } 

      else {
        //fprintf(stderr, "read %d bytes from socket\n", n);
        //fprintf(stderr, "%s\n", fr);
        fprintf(stderr, "received: %s, op %c\n",friptr, fr[readInd]);

        // start sensors command, parsing selected sensors 
        if (fr[readInd] == 's') {
          if (n>=9) printf("sensor selection:\n");
          
          for (int i = 2; i<11; i=i+2) {
            printf("%c", fr[readInd + i]);
          }
          printf("\n");
          //a,u,t,b,r
          ssel.a = fr[readInd + 2] - 48;
          ssel.u = fr[readInd + 4] - 48;
          ssel.t = fr[readInd + 6] - 48;
          ssel.b = fr[readInd + 8] - 48;
          ssel.r = fr[readInd + 10] - 48;          
        }

        selectOp(fr[readInd]);
       
        // user conf:

        if (fr[readInd] == 'o' && fr[readInd + 1] == 'p' && fr[readInd + 2] == 't') {
          //printf("user conf, %zu bytes\n");
          int conflen = n - 4;
          char confJSON[conflen];
          strncpy(confJSON, &fr[readInd + 4], conflen);
          printf("json: %s\n", confJSON);          

          double freq = -1.0;
          double rate = -1.0;
          double gain = -1.0;
          double bandwidth = -1.0;

          jsmn_parser p;
          jsmntok_t t[10]; 
          jsmn_init(&p);
          int r = jsmn_parse(&p, confJSON, conflen, t, 10);
          printf("%d tokens\n", r);
          
          int toklen;
          char *leftover;


          if (r==9) { //correct amount of tokens
          

            if (t[1].type == 3 && t[1].size == 1 && t[2].type == 4) {
              toklen = t[2].end - t[2].start;
              char tokVal[toklen + 1];
              strncpy(tokVal, &confJSON[t[2].start], toklen);
              tokVal[toklen] = '\0';
              freq = strtod(tokVal, &leftover);
            }

            if (t[3].type == 3 && t[3].size == 1 && t[4].type == 4) {
              toklen = t[4].end - t[4].start;
              char tokVal[toklen + 1];
              strncpy(tokVal, &confJSON[t[4].start], toklen);
              tokVal[toklen] = '\0';
              rate = strtod(tokVal, &leftover);
            }

            if (t[5].type == 3 && t[5].size == 1 && t[6].type == 4) {
              toklen = t[6].end - t[6].start;
              char tokVal[toklen + 1];
              strncpy(tokVal, &confJSON[t[6].start], toklen);
              tokVal[toklen] = '\0';
              printf("%s ", tokVal);
              gain = strtod(tokVal, &leftover);
            }

            if (t[7].type == 3 && t[7].size == 1 && t[8].type == 4) {
              toklen = t[8].end - t[8].start;
              char tokVal[toklen + 1];
              strncpy(tokVal, &confJSON[t[8].start], toklen);
              tokVal[toklen] = '\0';
              bandwidth = strtod(tokVal, &leftover);
            }

          }  

          if (changeUsrpSettings(freq, rate, gain, bandwidth)) {
            strcpy(statusResp, "USRP configured");
          }  
        } 

        printf("%d, %d, %d, %d, %d\n", ssel.a, ssel.u, ssel.t, ssel.b, ssel.r);

        //selectOp(fr[readInd]);
        friptr += n; /* # just read */
        readInd += n;
        //FD_SET(STDOUT_FILENO, &wset); /* try and write below */
        FD_SET(gsockfd, &wset);
      }
    }

    if (FD_ISSET(gsockfd, &wset)) {
      sendStatus();       
    }
  }

  return;
}



int handleConnections() {
  socklen_t clilen;
  int sockfd, newsockfd;
  //char buffer[256];
  struct sockaddr_un serv_name, cli_name;
  //int n;
  int pid;

  unlink(SOCKET_NAME);
  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
     errors("ERROR opening socket");
  }

  memset(&serv_name, 0, sizeof(struct sockaddr_un));

  serv_name.sun_family = AF_UNIX;
  strncpy(serv_name.sun_path, SOCKET_NAME, sizeof(serv_name.sun_path) - 1);

  if( bind(sockfd, (const struct sockaddr *) &serv_name, sizeof(struct sockaddr_un)) < 0) {
    errors("bind");
  }

  if(listen(sockfd,5) < 0) {
    errors("listen");
  }

  clilen = sizeof(cli_name);
  
  printf("server started\n");

  for(;;) {
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_name, &clilen);
    if (newsockfd < 0) {
      errors("ERROR on accept");
    }

    pid = fork();

    if (pid < 0) {
      errors("ERROR on fork");
    }

    if (pid == 0)  {
      close(sockfd);
      remoteloop(newsockfd);
      exit(0);
    }

    else close(newsockfd);
  }

  unlink(SOCKET_NAME);
  return 0;
}


int main(int argc, char *argv[]) {
  initRunStat();
  if(argc > 1 && (strcmp(argv[1], "loc") ==0)) {
    remotectrl = 0;   
    mainloop();
  } else {
    remotectrl = 1;
    handleConnections();
  }

  return 0;
}
