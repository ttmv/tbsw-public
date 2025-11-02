#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "misc.h"


long getCurrentTime() {
  struct timeval t;
  if(gettimeofday(&t, NULL)==0) {
    return t.tv_sec * 1000 + t.tv_usec / 1000;
  } else {
    printf("error\n"); 
    return -1;
  }
}

/*
char *createFilename(char *sensorname) {
  int namelen = strlen(sensorname);
  char filename[namelen + 13 + 4];
  char timestr[13];
  startTime = getCurrentTime();  
  sprintf(timestr, "%ld", startTime);
  strcat(filename, sensorname);
  strcat(filename, "-");
  strcat(filename, timestr);
  strcat(filename, ".csv");

  return filename;
}
*/
