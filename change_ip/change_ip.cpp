/* This utility changes the IP address of an attached camera. Even if 
 * it is unreachable. 
 *
 * Written by Timothy J. Crone
 * tjcrone@gmail.com */

// includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <PvApi.h>

#define MAX_CAMERA_LIST 20

// usage
void ShowUsage()
{
  printf("usage: changeIpAddress -u cameraID -i ipAddress -s subnetMask -g gateway\n");
}

// sleep function
void Sleep(unsigned int time)
{
  struct timespec t,r;

  t.tv_sec    = time / 1000;
  t.tv_nsec   = (time % 1000) * 1000000;

  while(nanosleep(&t,&r)==-1)
    t = r;
}

// wait for camera
void WaitForCamera()
{
  printf("Waiting for camera ...\n");
  //fflush(stdout);
  while(PvCameraCount() < 1)
    Sleep(250);
  printf(" and go.\n");
}

// main
int main(int argc, char* argv[])
{
  if(argc>1)
  {
    // initialize camera api
    if(!PvInitialize())
    {
    //printf("Prosilica API initialized.\n");
      // initialize some variables
      int             c;
      unsigned long   uid = 0;
      tPvIpSettings   IpSettings;
      bool            uSet = false;
      bool            iSet = false;
      bool            sSet = false;
      bool            gSet = false;
      unsigned long   cameraNum = 0;
      tPvCameraInfo   cameraList[MAX_CAMERA_LIST];
      int             rounds = 0;
      tPvErr          Err;
      unsigned long   Mode = 0;

      // loop through options
      //printf("Looping through options.\n");
      while ((c = getopt (argc, argv, "u:i:s:g:")) != -1)
      {
        switch(c)
        {
          case 'u':
            {
              if(optarg){
                uid = atol(optarg);
                uSet = true;
              }
              break;
            }
          case 'i':
            {
              if(optarg){
                IpSettings.PersistentIpAddr = inet_addr(optarg);
                iSet = true;
              }
              break;
            } 
          case 's':
            {
              if(optarg){
                IpSettings.PersistentIpSubnet = inet_addr(optarg);
                sSet = true;
              }
              break;
            } 
          case 'g':
            {
              if(optarg){
                IpSettings.PersistentIpGateway = inet_addr(optarg);
                gSet = true;
              }
              break;
            } 

        }
      }
      //printf("Options set.\n");
      if(uSet && iSet && sSet && gSet)
      {
        printf("Looking for camera ... ");
        while(cameraNum==0 && rounds<10){
          cameraNum = PvCameraList(cameraList,MAX_CAMERA_LIST,NULL);
          cameraNum = cameraNum + PvCameraListUnreachable(cameraList,MAX_CAMERA_LIST,NULL);
          //printf("Attached cameras: %ld\n",cameraNum);
          Sleep(250);
          rounds = rounds+1;
          printf("%i\n",rounds);
        }

        if(cameraNum>0){
          
          printf("done.\n");
          printf("Changing settings ... ");
          
          Mode = ePvIpConfigPersistent;
          //Mode = ePvIpConfigDhcp;
          IpSettings.ConfigMode = (tPvIpConfig)Mode;

          //Mode = ePvIpConfigAutoIp;

          // do a get to fill IpSettings with current values
          //Err = PvCameraIpSettingsGet(uid,&IpSettings);

          //if(IpSettings.ConfigModeSupport & ePvIpConfigPersistent)
          //  printf("FIXED ");
          //if(IpSettings.ConfigModeSupport & ePvIpConfigDhcp)
          //  printf("DHCP ");
          //if(IpSettings.ConfigModeSupport & ePvIpConfigAutoIp)
          //  printf("AutoIP");

          //struct in_addr addr;
          //addr.s_addr = IpSettings.CurrentIpAddress;
          //printf("Current address:\t%s\n",inet_ntoa(addr));

          //char *test;
          //test = *inet_ntop(kkkIpSettings.CurrentIpAddress);

          //printf("%u",inet_ntoa(IpSettings.CurrentIpAddress));

          Err = PvCameraIpSettingsChange(uid,&IpSettings);
          //Err = PvCameraIpSettingsGet(uid,&IpSettings);
          //printf("Settings changed.\n");
          printf("done.\n");
          }
        else
          printf("No cameras found.\n");
      }
      else
      {
        printf("All switches must be specified.\n");
        ShowUsage();
      }
    }
    else
      printf("Prosilica API failed to initialize.\n");
  }
  else
    ShowUsage();

  // uninitialize camera api
  PvUnInitialize();
  return 0;
}
