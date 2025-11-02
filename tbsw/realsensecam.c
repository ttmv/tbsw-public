/* 
  Realsense T265 sensor component code
  Code based on multiple examples from RealSense 
  
  T265 component code saves frames read from the T265 camera to .bag file. 
  In addition timestamps from the camera are read and saved to separate timestamp file 
  with timestamps of frame arrival  
*/

#include <librealsense2/rs.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_frame.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "misc.h"
#include "realsensecam.h"

int realsenseOn = 0;
long rsLocalTime = 0;
char *rsStatusStr = NULL;

void stopCamera() {
  printf("stopping camera\n");
  realsenseOn = 0;
}


/*
  Respond to status request query. If some frames have been read 
  the timestamp of the latest frame is known already.
*/ 

char *readRsStatus() {
  printf("rs status called\n");
  //return "Camera status\n"; 
  if (!realsenseOn) return "RealSense is not on\n";
    
  char tempRsStatus[80];
  sprintf(tempRsStatus, "RealSense: recording at %ld\n", rsLocalTime);
  strcpy(rsStatusStr, tempRsStatus);

  //return "RealSense: recording\n";   
  return rsStatusStr;
}



int startCamera(struct SenInfoST *runstat, void (*sendMsgPtr)(char *msg)) {
  int verboseMode = 1;
  rsStatusStr = malloc(80);
  realsenseOn = 1;
  rs2_error* e = 0;
  const char* err; 

  // Create filenames for camera data and timestamps
  char *datapath = SENSORDATAPATH;

  char rsfilename[50]; //= "../../sensordata/T265-";
  char rsTimestamps[55]; //= "../../sensordata/T265times-";
  strcpy(rsfilename, datapath);
  strcpy(rsTimestamps, datapath);

  strcat(rsfilename, "T265-");
  strcat(rsTimestamps, "T265times-");

  long rsStartTime = getCurrentTime(); 
  char rstimestr[14];

   
  sprintf(rstimestr, "%ld", rsStartTime);
  strcat(rsfilename, rstimestr);
  strcat(rsTimestamps, rstimestr);
  strcat(rsfilename, ".bag");
  strcat(rsTimestamps, ".csv");

  rs2_context* ctx = rs2_create_context(RS2_API_VERSION, &e);
  rs2_pipeline* pipeline =  rs2_create_pipeline(ctx, &e);
  rs2_config* config = rs2_create_config(&e);

  /* Enable streams. Pose and FishEyes used currently, acc & gyro available */

  //rs2_config_enable_stream(config, RS2_STREAM_ACCEL, -1, 0, 0, FORMAT, 0, &e);
  //rs2_config_enable_stream(config, RS2_STREAM_GYRO, -1, 0, 0, FORMAT, 0, &e);
  rs2_config_enable_stream(config, RS2_STREAM_POSE, -1, 0, 0, RS2_FORMAT_6DOF, 0, &e);
  rs2_config_enable_stream(config, RS2_STREAM_FISHEYE, 1, 0, 0, RS2_FORMAT_Y8, 0, &e);
  rs2_config_enable_stream(config, RS2_STREAM_FISHEYE, 2, 0, 0, RS2_FORMAT_Y8, 0, &e);

  fprintf(stderr, "REALSENSE: file to write %s, timestamps: %s\n", rsfilename, rsTimestamps);
  FILE *timesFp = fopen(rsTimestamps, "a");


  if(!timesFp) { 
    printf("cannot open timestamp file\n");
  } else {
    fprintf(timesFp, "Frame type;System Time;Camera Time\n");
  }

  rs2_config_enable_record_to_file(config, rsfilename, &e);
  if(e) {
    const char* err = rs2_get_error_message(e);
    printf("RS error %s\n", err);
  }

  rs2_pipeline_profile* pipeline_profile = rs2_pipeline_start_with_config(pipeline, config, &e);
  if (e) {
    //printf("REALSENSE: The connected device doesn't support this streaming!\n");
    err = rs2_get_error_message(e);
    fprintf(stderr, "RS error %s\n", err);
    //TODO send better error msg to gui
    
    (*sendMsgPtr)("Error starting RealSense, check logs");
    return EXIT_FAILURE;
  }

  printf("Realsense is working\n");
  int videocounter = 0;
  int posecounter = 0;
  char *frameType;

  runstat->t = 1;  

  while(realsenseOn) {
    rs2_frame* frames = rs2_pipeline_wait_for_frames(pipeline, RS2_DEFAULT_TIMEOUT, &e);
    rs2_pose pose;

    int num_of_frames = rs2_embedded_frames_count(frames, &e);

    for (int i = 0; i < num_of_frames; ++i) {
      rs2_frame* frame = rs2_extract_frame(frames, i, &e);

      // frame timestamp: time frame was recorded by the camera
      // rsLocalTime: system time when the frame was read by the TBSW. 

      rs2_time_t frameTimestamp = rs2_get_frame_timestamp(frame, &e);
      rsLocalTime = getCurrentTime();
	
        
      int width = rs2_get_frame_width(frame, &e);
      int height = rs2_get_frame_height(frame, &e);
      int datasize = rs2_get_frame_data_size(frame, &e);
      if(rs2_is_frame_extendable_to(frame, RS2_EXTENSION_POSE_FRAME, &e) != 0) { 
        frameType = "pose";         
        rs2_pose_frame_get_pose_data(frame, &pose, &e);
        posecounter++; 
          
        if((posecounter % 2000 == 0) && verboseMode) {
          printf("REALSENSE: pose frame %d; ", posecounter);
          printf("velocity: %f, %f, %f; ", pose.velocity.x, pose.velocity.y, pose.velocity.z); 
          printf("acceleration: %f, %f, %f; ", pose.acceleration.x, pose.acceleration.y, pose.acceleration.z);
          printf("rotation: %f, %f, %f, %f; ", pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w);
          printf("\n"); 
        }
      } 

      if(rs2_is_frame_extendable_to(frame, RS2_EXTENSION_VIDEO_FRAME, &e) != 0) {
        frameType = "video";
        videocounter++;
        //printf("REALSENSE: video frame; ");
      }

      if (timesFp) {        
        fprintf(timesFp, "%s;%ld;%f\n", frameType, rsLocalTime, frameTimestamp);
      }	
        
      if((videocounter % 1000 == 0) && verboseMode) {
        printf("REALSENSE: %d videoframes and %d pose frames, last:\n", videocounter, posecounter);
        printf("REALSENSE: time: %f, frame width: %d, height: %d, data size: %d\n", frameTimestamp, width, height, datasize);
      }
        
      //printf("REALSENSE: time: %f, frame width: %d, height: %d, data size: %d\n", timestamp, width, height, datasize);
      //fprintf(fp, "%f, frame width: %d, height: %d, data size: %d\n", timestamp, width, height, datasize);
      rs2_release_frame(frame);
      //sleep(1);
    }
      
    rs2_release_frame(frames);
  }    

  runstat->t = 0;

  // Close the timestamp file
  fclose(timesFp);

  // Stop the pipeline streaming
  rs2_pipeline_stop(pipeline, &e);
  //check_error(e);

  

  // Release resources
  rs2_delete_pipeline_profile(pipeline_profile);
  rs2_delete_config(config);
  rs2_delete_pipeline(pipeline);
  rs2_delete_context(ctx);
  if (rsStatusStr) free(rsStatusStr);
  
  fprintf(stderr, "camera stopped at %ld\n", getCurrentTime());
  return EXIT_SUCCESS;
}


