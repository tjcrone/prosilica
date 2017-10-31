/* This utility streams Prosilica camera data to disk. It is multithreaded and will stream
 * from any number of cameras at once, limited mostly by disk write speeds and network
 * bandwidth. The goal is to have something flexible that can be run from a Python script.
 * A more descriptive header will happen when this program is complete.
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
#include <pthread.h>
#include <math.h>
#include <PvApi.h>
#include <iostream>
using namespace std;

#define FRAMESCOUNT 1024 // total frame buffers

// camera structure
typedef struct
{
  int           id;
  unsigned long uid;
  tPvHandle     Handle;
  tPvFrame      Frames[FRAMESCOUNT];
  pthread_t     ThHandle;
  char          *outfile;
  FILE*         fhandle;
  bool          acquisitionComplete;
  unsigned long  startSecond;
  unsigned long  startnSecond;
  unsigned long  endSecond;
  unsigned long  endnSecond;
} tCamera;

// session structure
typedef struct
{
  int           Count;
  tCamera*      Cameras;
  int           outfileCount;
  int           err;
  unsigned long startStampHi;
  unsigned long startStampLo;
  bool          startStampSet;
  int		AcquisitionFrameCount;
  int		ExposureValue;
  int		ExposureAutoMax;
  int		GainAutoMax;
  int		actualFramesAcquired;
  float         frameRate;
} tSession;

// global GSession
tSession GSession;

// usage
void ShowUsage()
{
  printf("usage: fileStream -u cameraID -o outfile\n");
  printf("-u\tcamera unique ID (set multiple times for mulitple cameras\n");
  printf("-o\toutput file (must be set as many times as -u)\n");
}

// sleep function (pass milliseconds)
void Sleep(unsigned int time)
{
  struct timespec t,r;
  t.tv_sec    = time / 1000;
  t.tv_nsec   = (time % 1000) * 1000000;
  while(nanosleep(&t,&r)==-1)
    t = r;
}

// wait for cameras (this just waits for the right *number* of cams, not the right cams)
bool WaitForCamera()
{
  int waitCount = 0;
  printf("Waiting for %i camera(s) ...",GSession.Count);
  while(PvCameraCount() < (unsigned int)GSession.Count && waitCount<16)
  {
    Sleep(250);
    waitCount++;
  }
  if(PvCameraCount()>=(unsigned int)GSession.Count)
  {
    printf(" and go.\n");
    return true;
  }
  else
    return false;
}

// end acquisition callback
void CameraEventCB(void* Context,tPvHandle Handle,const tPvCameraEvent* EventList,unsigned long EventListLength)
{
  tCamera* Camera = static_cast<tCamera*>(Context);
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  Camera->endSecond = tp.tv_sec;
  Camera->endnSecond = tp.tv_nsec;
  Camera->acquisitionComplete = true;
}

// frame done callback
void FrameDoneCB(tPvFrame* pFrame)
{
  if(pFrame->Status != ePvErrUnplugged && pFrame->Status != ePvErrCancelled)
  {
    // write real time to file
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    fwrite((unsigned long*)&tp.tv_sec,1,sizeof(unsigned long),(FILE*)pFrame->Context[1]);
    //printf("Sec: %lu\n", (unsigned long)tp.tv_sec);
    fwrite((unsigned long*)&tp.tv_nsec,1,sizeof(long),(FILE*)pFrame->Context[1]);
    //printf("Nsec: %lu\n", (unsigned long)tp.tv_nsec);

    // write camera timestamps to file
    fwrite((unsigned long*)&pFrame->TimestampLo,1,sizeof(long),(FILE*)pFrame->Context[1]);
    //printf("Lo: %lu\n", (unsigned long*)&pFrame->TimestampLo);
    fwrite((unsigned long*)&pFrame->TimestampHi,1,sizeof(long),(FILE*)pFrame->Context[1]);
    //printf("Hi: %lu\n", (unsigned long*)&pFrame->TimestampHi);

    // write out image buffer to file
    fwrite((void*)pFrame->ImageBuffer,pFrame->ImageBufferSize,sizeof(char),(FILE*)pFrame->Context[1]);

    // check frame rate (lastStamp is Context[2]
    //(unsigned int*)pFrame->Context[2] = (unsigned int*)&pFrame->TimestampLo;
    //printf("stamp is %u.\n", (unsigned int*)&pFrame->TimestampLo);
    //cout << (unsigned long)tp.tv_nsec << endl;
    //*(unsigned int*)pFrame->Context[2] = (unsigned int)pFrame->TimestampLo;
    //cout << *(unsigned int*)pFrame->Context[2] << endl;
    //cout << (unsigned int*)pFrame->Context[2] << endl;

    // requeue frame
    PvCaptureQueueFrame((tPvHandle)pFrame->Context[0],pFrame,FrameDoneCB);

    // increment acquired count
    GSession.actualFramesAcquired++;
  }
}

// setup camera 
bool CameraSetup(tCamera& Camera)
{
  // setup event channel
  PvAttrUint32Set(Camera.Handle,"EventsEnable1",0);
  PvAttrEnumSet(Camera.Handle,"EventSelector","AcquisitionEnd");
  PvAttrEnumSet(Camera.Handle,"EventNotification","On");
  PvCameraEventCallbackRegister(Camera.Handle,CameraEventCB,&Camera);
  Camera.acquisitionComplete = 0;

  // change some camera settings (these will come out. all these settings
  // should be loaded from a file before this program is run.)
  //PvAttrEnumSet(Camera.Handle,"SyncOut2Mode","Exposing");
  //PvAttrEnumSet(Camera.Handle,"SyncOut2Invert","Off");
  PvAttrEnumSet(Camera.Handle,"FrameStartTriggerMode","FixedRate");
  PvAttrFloat32Set(Camera.Handle,"FrameRate",GSession.frameRate);
  PvAttrEnumSet(Camera.Handle,"AcquisitionMode","MultiFrame");
  //PvAttrUint32Set(Camera.Handle,"AcquisitionFrameCount",1000);
  PvAttrUint32Set(Camera.Handle,"AcquisitionFrameCount",GSession.AcquisitionFrameCount);
  //PvAttrEnumSet(Camera.Handle,"PixelFormat","Mono8");
  //PvAttrEnumSet(Camera.Handle,"PixelFormat","Mono16");
  PvAttrEnumSet(Camera.Handle,"PixelFormat","Mono12Packed");
  PvAttrUint32Set(Camera.Handle,"Width",1024ul);
  PvAttrUint32Set(Camera.Handle,"Height",1024ul);
  PvAttrUint32Set(Camera.Handle,"ExposureValue",GSession.ExposureValue);
  //PvAttrEnumSet(Camera.Handle,"ExposureMode","Manual");
  PvAttrEnumSet(Camera.Handle,"ExposureMode","Auto");
  PvAttrEnumSet(Camera.Handle,"ExposureAutoAlg","Mean");
  //PvAttrEnumSet(Camera.Handle,"ExposureAutoAlg","FitRange");
  PvAttrUint32Set(Camera.Handle,"ExposureAutoMax", GSession.ExposureAutoMax);
  PvAttrEnumSet(Camera.Handle,"GainMode","Auto");
  PvAttrUint32Set(Camera.Handle,"GainAutoMax", GSession.GainAutoMax);
  PvAttrUint32Set(Camera.Handle,"PacketSize",9000ul);
  //PvAttrUint32Set(Camera.Handle,"StreamBytesPerSecond",124000000ul);
  PvAttrUint32Set(Camera.Handle,"StreamBytesPerSecond",80000000ul);

  return true;
}

// write header functon
bool WriteHeader(tCamera& Camera)
{
  // still need exposure/gain/lens attributes

  unsigned long width = 0;
  unsigned long height = 0;
  unsigned long timeStampFrequency = 0;
  float frameRate;
  unsigned long frameCount = 0;
  char pixelFormat[16];

  // get attributes
  PvAttrUint32Get(Camera.Handle,"Width",&width);
  PvAttrUint32Get(Camera.Handle,"Height",&height);
  PvAttrUint32Get(Camera.Handle,"TimeStampFrequency",&timeStampFrequency);
  PvAttrFloat32Get(Camera.Handle,"FrameRate",&frameRate);
  PvAttrUint32Get(Camera.Handle,"AcquisitionFrameCount",&frameCount);
  PvAttrEnumGet(Camera.Handle,"PixelFormat",pixelFormat,16,NULL);

  // write attributes to file
  fwrite((void*)&width,1,sizeof(long),(FILE*)Camera.fhandle); // 4 bytes
  fwrite((void*)&height,1,sizeof(long),(FILE*)Camera.fhandle); // 4 bytes
  fwrite((void*)&timeStampFrequency,1,sizeof(long),(FILE*)Camera.fhandle); // 4 bytes
  fwrite((void*)&frameRate,1,sizeof(float),(FILE*)Camera.fhandle); // 4 bytes
  fwrite((void*)&frameCount,1,sizeof(long),(FILE*)Camera.fhandle); // 4 bytes
  fwrite((void*)&pixelFormat,1,16,(FILE*)Camera.fhandle); // 16 bytes


  return true;
}

// check camera stats 
unsigned long CheckData(tCamera& Camera)
{
  unsigned long framesDropped;

  // check the camera stats
  PvAttrUint32Get(Camera.Handle,"StatFramesDropped",&framesDropped);
  return framesDropped;
}

// unsetup camera
void CameraUnsetup(tCamera& Camera)
{
  // unsetup event channel
  PvAttrUint32Set(Camera.Handle,"EventsEnable1",0);
  PvCameraEventCallbackUnRegister(Camera.Handle,CameraEventCB);

  // clear queue and close camera
  PvCaptureQueueClear(Camera.Handle);
  PvCameraClose(Camera.Handle);

  // delete allocated buffers
  for(int i=0;i<FRAMESCOUNT;i++)
    delete [] (char*)Camera.Frames[i].ImageBuffer;

  Camera.Handle = NULL;
}

// set up capture
bool startCapture(tCamera& Camera)
{
  // allocate the buffer for each frame and define the file handle
  unsigned long FrameSize = 0;
  PvAttrUint32Get(Camera.Handle,"TotalBytesPerFrame",&FrameSize);

  for(int i=0;i<FRAMESCOUNT;i++)
  {
    Camera.Frames[i].ImageBuffer = new char[FrameSize];
    Camera.Frames[i].ImageBufferSize = FrameSize;
    Camera.Frames[i].Context[0] = Camera.Handle;
    Camera.Frames[i].Context[1] = Camera.fhandle;
    //Camera.Frames[i].Context[2] = &Camera.lastStamp;
    //Camera.Frames[i].Context[3] = &Camera.stampInterval;
  }

  // start capture
  PvCaptureStart(Camera.Handle);
  for (int i=0;i<FRAMESCOUNT;i++)
    PvCaptureQueueFrame(Camera.Handle,&(Camera.Frames[i]),FrameDoneCB);

  return true;
}

// set start time
bool setStart(tCamera& Camera)
{
  unsigned long timeStampHi = 0;
  tPvErr Err;

  // latch to find current camera time
  if(!PvCommandRun(Camera.Handle,"TimeStampValueLatch"))
  {
    // get high time stamp
    Err = PvAttrUint32Get(Camera.Handle,"TimeStampValueHi",&timeStampHi);

    // calculate and set startTimeHi and startTimeLow in GSession
    GSession.startStampHi = timeStampHi+2;
    GSession.startStampLo = 0;
    GSession.startStampSet = 1;
  }
  return true;
}

// start acquisition
bool startAcquisition(tCamera& Camera)
{
  // set PTP start time based on GSession start time
  PvAttrUint32Set(Camera.Handle,"PtpTriggerTimeHi",GSession.startStampHi);
  PvAttrUint32Set(Camera.Handle,"PtpTriggerTimeLo",0);
  
  // set start time
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  Camera.startSecond = tp.tv_sec;
  Camera.startnSecond = tp.tv_nsec;

  // start aquisition
  PvCommandRun(Camera.Handle,"AcquisitionStart");

  return true;
}

// stop camera
void CameraStop(tCamera& Camera)
{
  PvCommandRun(Camera.Handle,"AcquisitionStop");
  PvCaptureEnd(Camera.Handle);
}

// check file size
void checkFile(tCamera& Camera,char* Name)
{
  // get file size
  unsigned long long fileSize;
  fseek(Camera.fhandle, 0L, SEEK_END);
  fileSize = ftell(Camera.fhandle);
  //printf("file size: %llu from %s\n",fileSize, Name);
  
  // get attributes
  unsigned long width = 0;
  unsigned long height = 0;
  unsigned long frameCount = 0;
  char pixelFormat[16];
  PvAttrUint32Get(Camera.Handle,"Width",&width);
  PvAttrUint32Get(Camera.Handle,"Height",&height);
  PvAttrUint32Get(Camera.Handle,"AcquisitionFrameCount",&frameCount);
  PvAttrEnumGet(Camera.Handle,"PixelFormat",pixelFormat,16,NULL);
  
  // calculate expected size
  unsigned long long expectedSize = 0;
  if(strcmp(pixelFormat,"Mono8")==0)
    expectedSize = 8ull * (unsigned long long)width * (unsigned long long)height * (unsigned long long)frameCount / 8ull + (unsigned long long)frameCount * 16ull + 40ull;
  if(strcmp(pixelFormat,"Mono12Packed")==0)
    expectedSize = 12ull * (unsigned long long)width * (unsigned long long)height * (unsigned long long)frameCount / 8ull + (unsigned long long)frameCount * 16ull + 40ull;
  if(strcmp(pixelFormat,"Mono16")==0)
    expectedSize = 16ull * (unsigned long long)width * (unsigned long long)height * (unsigned long long)frameCount / 8ull + (unsigned long long)frameCount * 16ull + 40ull;

  // compare sizes
  if(expectedSize==fileSize)
    printf("File size is correct at %llu bytes.\n",fileSize);
  else
    printf("\n*** Warning ***\nExpected file size is %llu. Actual size is %llu.\n\n",expectedSize,fileSize);

  // print frames acquired
  printf("%i frames acquired.\n",GSession.actualFramesAcquired);
  
}

// calculate rate
void calculateRate(tCamera& Camera)
{
  // get/calculate rate information
  float specifiedRate;
  double actualRate, totalSeconds, rateError;
  PvAttrFloat32Get(Camera.Handle,"FrameRate",&specifiedRate);
  totalSeconds = ((double)Camera.endSecond + (double)Camera.endnSecond/1000000000) - ((double)Camera.startSecond + (double)Camera.startnSecond/1000000000);
  actualRate = (double)GSession.AcquisitionFrameCount/totalSeconds;
  rateError = (actualRate - specifiedRate)/specifiedRate*100;

  // print info 
  printf("Acquisition took approximately %.1f seconds to complete.\n",totalSeconds);
  printf("Approximate frame rate was %.1f frames per second.\n",actualRate);
		  
  // print rate warning if necessary
  if(rateError>2)
    printf("\n*** Warning ***\nFrame rate was approximately %.1f%% higher than expected.\n\n",rateError);
  if(rateError<-2)
    printf("\n*** Warning ***\nFrame rate was approximately %.1f%% lower than expected.\n\n",rateError*(-1));
}

// thread function
void *ThreadFunc(void *pContext)
{
  tCamera* Camera = (tCamera*)pContext;

  // initialize some Camera values
  //Camera->lastStamp = 1;
  //Camera->stampInterval = 2;

  if(!PvCameraOpen(Camera->uid,ePvAccessMaster,&(Camera->Handle)))
  {
    char IP[128];
    char Name[128];

    if(!PvAttrStringGet(Camera->Handle,"DeviceIPAddress",IP,128,NULL) &&
        !PvAttrStringGet(Camera->Handle,"CameraName",Name,128,NULL))
    {

      printf("%u : camera %s (%s) successfully opened\n",Camera->id,IP,Name);

      // open an output file for this thread
      Camera->fhandle = fopen(Camera->outfile,"wb");

      // set start time (only the first camera does this)
      if(Camera->id==1)
        setStart(*Camera);

      // wait for first camera to set start time
      while(!GSession.startStampSet)
        Sleep(100);

      // setup camera
      if(CameraSetup(*Camera))
      {
        // write header information here (through header write function)
        if(WriteHeader(*Camera))
        {
          // start capture
          if(startCapture(*Camera))
          {
            if(startAcquisition(*Camera))
            {
              // wait until acquisition complete callbacks fire
              while(!Camera->acquisitionComplete)
                Sleep(250);

	      // sleep just a bit
	      Sleep(4000);

              // print dropped frames and write to file
              unsigned long framesDropped = CheckData(*Camera);
              printf("%lu frames dropped from %s.\n",framesDropped, Name);
              fwrite((void*)&framesDropped,1,sizeof(unsigned long),(FILE*)Camera->fhandle);

              // check file size
              checkFile(*Camera, Name);

	      // calculate approximate frame rate
	      calculateRate(*Camera);

              // close output file
              fclose(Camera->fhandle);

              // finish up
              CameraStop(*Camera);
              CameraUnsetup(*Camera);
            }
          }
        }
      }
    }
    else
      printf("%u : camera opened but something went wrong\n",Camera->id);

    printf("%u : camera %s (%s) successfully closed\n",Camera->id,IP,Name);
  }
  else
    printf("%u : camera failed to open\n",Camera->id);

  return 0;
} 

// main
int main(int argc, char* argv[])
{
  if(argc>1)
  {
    // initialize camera api
    if(!PvInitialize())
    {
      memset(&GSession,0,sizeof(tSession));

      // initialize needed variables
      int c;

      // count the number of cameras specified so that GSession.Cameras can be created
      GSession.Count = 0;
      while ((c = getopt (argc, argv, "u:o:n:e:r:m:g:")) != -1)
      {
        switch(c)
        {
          case 'u':
            {
              GSession.Count++;
            }
        }
      }

      if(GSession.Count>0)
      {

        // initilaize GSession.Cameras
        GSession.Cameras = new tCamera[GSession.Count];
        memset(GSession.Cameras,0,sizeof(tCamera) * GSession.Count);

        // loop through options again (for real this time)
        GSession.Count = 0;
        GSession.outfileCount = 0;
        optind = 0;
        while ((c = getopt (argc, argv, "u:o:n:e:r:m:g:")) != -1)
        {
          switch(c)
          {
            case 'u':
              {
                GSession.Count++;
                if(optarg)
                  GSession.Cameras[GSession.Count-1].uid = atol(optarg);
                GSession.Cameras[GSession.Count-1].id = GSession.Count;
                break;
              }
            case 'o':
              {
                if(optarg)
                  GSession.outfileCount++;
                GSession.Cameras[GSession.Count-1].outfile = optarg;
                break;
              } 
            case 'n':
              {
                if(optarg)
                  GSession.AcquisitionFrameCount = atol(optarg);
                break;
              } 
            case 'e':
              {
                if(optarg)
                  GSession.ExposureValue = atol(optarg);
                break;
              } 
            case 'r':
              {
                if(optarg)
                  GSession.frameRate = atol(optarg);
                break;
              } 
            case 'm':
              {
                if(optarg)
                  GSession.ExposureAutoMax = atol(optarg);
                break;
              } 
            case 'g':
              {
                if(optarg)
                  GSession.GainAutoMax = atol(optarg);
                break;
              } 
          }
        }

        // check to see of number of cameras equals number of output files
        if(GSession.Count == GSession.outfileCount)
        {
          // initialize startStamps
          GSession.startStampHi = 0;
          GSession.startStampLo = 0;
          GSession.startStampSet = 0;
	    
          // initialize actual frame count
	  GSession.actualFramesAcquired = 0;

          // wait for cameras
          if(WaitForCamera())
          {
            // loop over cameras and spawn threads
            for(int i=0;i<GSession.Count;i++)
            {
              pthread_create(&GSession.Cameras[i].ThHandle,NULL,ThreadFunc,&GSession.Cameras[i]);
            }
            // wait for all threads to terminate
            for(int i=0;i<GSession.Count;i++)
            {
              if(GSession.Cameras[i].ThHandle)
              {
                pthread_join(GSession.Cameras[i].ThHandle,NULL);
              }
            }
            delete [] GSession.Cameras;
          }
          else
            printf(" all cameras not found.\n");
        }
        else
          printf("Number of output files must match the number of cameras specified.\n");
      }
      else
        printf("At least one camera UID must be specified using -u\n");
    }
    else
      printf("Prosilica API failed to initialize.\n");
  }
  else
    ShowUsage();


  //printf("\n");
  //printf("%i\n",sizeof(long long)); // 8 bytes
  //printf("%i\n",sizeof(long)); // 4 bytes
  //printf("%i\n",sizeof(float)); // 4 bytes
  //printf("%i\n",sizeof(int)); // 4 bytes
  

  // uninitialize camera api
  PvUnInitialize();
  return 0;
}
