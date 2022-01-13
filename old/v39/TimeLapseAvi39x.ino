#include <dummy.h>

/*

  TimeLapseAvi

  ESP32-CAM Video Recorder

  This program records an AVI video on the SD Card of an ESP32-CAM.

  by James Zahary July 20, 2019  TimeLapseAvi23x.ino
     jamzah.plc@gmail.com

  https://github.com/jameszah/ESP32-CAM-Video-Recorder
    jameszah/ESP32-CAM-Video-Recorder is licensed under the
    GNU General Public License v3.0

  ~~~
  Update Sep 15, 2019 TimeLapseAvi39x.ino
  - work-in-progress
  - I'm publishing this as a few people have been asking or working on this
  
  - program now uses both cpu's with cpu 0 taking pictures and queue'ing them
    and a separate task on cpu 1 moving the pictures from the queue and adding
    them to the avi file on the sd card
  - the loop() task on cpu 1 now just handles the ftp system and http server
  - dropped fixed ip and switch to mDNS with name "desklens", which can be typed into
    browser, and also used as wifi name on router
  - small change to ftp to cooperate with WinSCP program
  - fixed bug so Windows would calulcate the correct length (time length) of avi 
  - when queue of frames gets full, it slips every other frame to try to catch up
  - camera is re-configued when changing from UXGA <> VGA to allow for more buffers 
    with the smaller frames
  ~~~

  The is Arduino code, with standard setup for ESP32-CAM
    - Board ESP32 Wrover Module
    - Partition Scheme Huge APP (3MB No OTA)

  This program records an AVI video on the SD Card of an ESP32-CAM.

  It will record realtime video at limited framerates, or timelapses with the full resolution of the ESP32-CAM.
  It is controlled by a web page it serves to stop and start recordings with many parameters, and look through the viewfinder.

  You can control framesize (UXGA, VGA, ...), quality, length, and fps to record, and fps to playback later, etc.

  There is also an ftp server to download the recordings to a PC.

  Instructions:

  The program uses a fixed IP of 192.168.1.188, so you can browse to it from your phone or computer.

  http://192.168.1.188/ -- this gives you the status of the recording in progress and lets you look through the viewfinder

  http://192.168.1.188/stop -- this stops the recording in progress and displays some sample commands to start new recordings

  ftp://192.168.1.188/ -- gives you the ftp server

  The ftp for esp32 seems to not be a full ftp.  The Chrome Browser and the Windows command line ftp's did not work with this, but
  the Raspbarian command line ftp works fine, and an old Windows ftp I have called CoffeeCup Free FTP also works, which is what I have been using.
  You can download at about 450 KB/s -- which is better than having to retreive the SD chip if you camera is up in a tree!

  http://192.168.1.188/start?framesize=VGA&length=1800&interval=250&quality=10&repeat=100&speed=1&gray=0  -- this is a sample to start a new recording

  framesize can be UXGA, SVGA, VGA, CIF (default VGA)
  length is length in seconds of the recording 0..3600 (default 1800)
  interval is the milli-seconds between frames (default 200)
  quality is a number 5..50 for the jpeg  - smaller number is higher quality with bigger and more detailed jpeg (default 10)
  repeat is a number of who many of the same recordings should be made (default 100)
  speed is a factor to speed up realtime for a timelapse recording - 1 is realtime (default 1)
  gray is 1 for a grayscale video (default 0 - color)

  These factors have to be within the limit of the SD chip to receive the data.
  For example, using a LEXAR 300x 32GB microSDHC UHS-I, the following works for me:

  UXGA quality 10,  2 fps (or interval of 500ms)
  SVGA quality 10,  5 fps (200ms)
  VGA  quality 10, 10 fps (100ms)
  CIG  quality 10, 20 fps (50ms)

  If you increase fps, you might have to reduce quality or framesize to keep it from dropping frames as it writes all the data to the SD chip.

  Also, other SD chips will be faster or slower.  I was using a SanDisk 16GB microSDHC "Up to 653X" - which was slower and more unpredictable than the LEXAR ???

  Search for "zzz" to find places to modify the code for:
    1.  Your wifi name and password
    2.  Your preferred ip address (with default gateway, etc)
    3.  Your Timezone for use in filenames
    4.  Defaults for framesize, quality, ... and if the recording should start on reboot of the ESP32 without receiving a command

  Acknowlegements:

  1.  https://robotzero.one/time-lapse-esp32-cameras/
      Timelapse programs for ESP32-CAM version that sends snapshots of screen.
  2.  https://github.com/nailbuster/esp8266FTPServer
      ftp server (slightly modifed to get the directory function working)
  3.  https://github.com/ArduCAM/Arduino/tree/master/ArduCAM/examples/mini
      ArduCAM Mini demo (C)2017 LeeWeb: http://www.ArduCAM.com
      I copied the structure of the avi file, some calculations.

*/


//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"

//#include <WiFi.h>   // redundant
#include <ESPmDNS.h>

#include "ESP32FtpServer.h"
#include <HTTPClient.h>

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP32FtpServer.h to see ftp verbose on serial


// Time
#include "time.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

long current_millis;
long last_capture_millis = 0;
static esp_err_t cam_err;
static esp_err_t card_err;
char strftime_buf[64];
int file_number = 0;
bool internet_connected = false;
struct tm timeinfo;
time_t now;

char *filename ;
char *stream ;
int newfile = 0;
int frames_so_far = 0;
FILE *myfile;
long bp;
long ap;
long bw;
long aw;
long totalp;
long totalw;
float avgp;
float avgw;
int overtime_count = 0;

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// GLOBALS
#define BUFFSIZE 512

// global variable used by these pieces

char str[20];
uint16_t n;
uint8_t buf[BUFFSIZE];

static int i = 0;
uint8_t temp = 0, temp_last = 0;
unsigned long fileposition = 0;
uint16_t frame_cnt = 0;
uint16_t remnant = 0;
uint32_t length = 0;
uint32_t startms;
uint32_t elapsedms;
uint32_t uVideoLen = 0;
bool is_header = false;
long bigdelta = 0;
int other_cpu_active = 0;
int skipping = 0;
int skipped = 0;

int fb_max = 12;

camera_fb_t * fb_q[30];
int fb_in = 0;
int fb_out = 0;

camera_fb_t * fb = NULL;

FILE *avifile = NULL;
FILE *idxfile = NULL;

//
//
// EDIT ssid and password
//
// zzz
const char* ssid = "Cable314";
const char* password = "ABC1234ABC";


// these are just declarations -- look below to edit defaults

int capture_interval = 200; // microseconds between captures
int total_frames = 300;     // default updated below
int recording = 0;          // turned off until start of setup
int framesize = 6;          // vga
int repeat = 100;           // capture 100 videos
int quality = 10;
int xspeed = 1;
int xlength = 3;
int gray = 0;
int new_config = 0;

#define AVIOFFSET 240 // AVI main header length

unsigned long movi_size = 0;
unsigned long jpeg_size = 0;
unsigned long idx_offset = 0;

uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t   dc_buf[4] = {0x30, 0x30, 0x64, 0x63};    // "00dc"
uint8_t avi1_buf[4] = {0x41, 0x56, 0x49, 0x31};    // "AVI1"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31};    // "idx1"

uint8_t  vga_w[2] = {0x80, 0x02}; // 640
uint8_t  vga_h[2] = {0xE0, 0x01}; // 480
uint8_t  cif_w[2] = {0x90, 0x01}; // 400
uint8_t  cif_h[2] = {0x28, 0x01}; // 296
uint8_t svga_w[2] = {0x20, 0x03}; //
uint8_t svga_h[2] = {0x58, 0x02}; //
uint8_t uxga_w[2] = {0x40, 0x06}; // 1600
uint8_t uxga_h[2] = {0xB0, 0x04}; // 1200


const int avi_header[AVIOFFSET] PROGMEM = {
  0x52, 0x49, 0x46, 0x46, 0xD8, 0x01, 0x0E, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4C, 0x49, 0x53, 0x54,
  0xD0, 0x00, 0x00, 0x00, 0x68, 0x64, 0x72, 0x6C, 0x61, 0x76, 0x69, 0x68, 0x38, 0x00, 0x00, 0x00,
  0xA0, 0x86, 0x01, 0x00, 0x80, 0x66, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x84, 0x00, 0x00, 0x00,
  0x73, 0x74, 0x72, 0x6C, 0x73, 0x74, 0x72, 0x68, 0x30, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x73,
  0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x66,
  0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00,
  0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x46, 0x4F,
  0x10, 0x00, 0x00, 0x00, 0x6A, 0x61, 0x6D, 0x65, 0x73, 0x7A, 0x61, 0x68, 0x61, 0x72, 0x79, 0x20,
  0x76, 0x33, 0x39, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// AviWriterTask runs on cpu 1 to write the avi file
//

TaskHandle_t CameraTask, AviWriterTask;
SemaphoreHandle_t baton;
int counter = 0;

void codeForAviWriterTask( void * parameter )
{

  print_stats("AviWriterTask runs on Core: ");

  for (;;) {
    make_avi();
    delay(1);
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// CameraTask runs on cpu 0 to take pictures and drop them in a queue
//

void codeForCameraTask( void * parameter )
{

  print_stats("CameraTask runs on Core: ");

  for (;;) {

    if (other_cpu_active == 1 ) {
      current_millis = millis();
      if (current_millis - last_capture_millis > capture_interval) {

        last_capture_millis = millis();

        xSemaphoreTake( baton, portMAX_DELAY );

        if  ( ( (fb_in + fb_max - fb_out) % fb_max) + 1 == fb_max ) {
          xSemaphoreGive( baton );

          Serial.print(" S ");  // the queue is full
          skipped++;
          skipping = 1;

          //Serial.print(" Q: "); Serial.println( (fb_in + fb_max - fb_out) % fb_max );
          //Serial.print(fb_in); Serial.print(" / "); Serial.print(fb_out); Serial.print(" / "); Serial.println(fb_max);
          //delay(1);

        } if (skipping > 0 ) {

          if (skipping % 2 == 0) {  // skip every other frame until queue is cleared

            frames_so_far = frames_so_far + 1;
            frame_cnt++;

            fb_in = (fb_in + 1) % fb_max;
            bp = millis();
            fb_q[fb_in] = esp_camera_fb_get();
            totalp = totalp - bp + millis();

          } else {
            //Serial.print(((fb_in + fb_max - fb_out) % fb_max));  Serial.print("-s ");  // skip an extra frame to empty the queue
            skipped++;
          }
          skipping = skipping + 1;
          if (((fb_in + fb_max - fb_out) % fb_max) == 0 ) {
            skipping = 0;
            Serial.print(" == ");
          }

          xSemaphoreGive( baton );

        } else {

          skipping = 0;
          frames_so_far = frames_so_far + 1;
          frame_cnt++;

          fb_in = (fb_in + 1) % fb_max;
          bp = millis();
          fb_q[fb_in] = esp_camera_fb_get();
          totalp = totalp - bp + millis();
          xSemaphoreGive( baton );

        }


      } else {
        //delay(5);     // waiting to take next picture
      }
    } else {
      //delay(50);  // big delay if not recording
    }
    delay(1);
  }
}


//
// Writes an uint32_t in Big Endian at current file position
//
static void inline print_quartet(unsigned long i, FILE * fd)
{
  uint8_t x[1];

  x[0] = i % 0x100;
  size_t i1_err = fwrite(x , 1, 1, fd);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i2_err = fwrite(x , 1, 1, fd);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i3_err = fwrite(x , 1, 1, fd);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i4_err = fwrite(x , 1, 1, fd);
}


void startCameraServer();
httpd_handle_t camera_httpd = NULL;

char the_page[3000];

char localip[20];
WiFiEventId_t eventID;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// setup() runs on cpu 1
//

void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector  // creates other problems

  Serial.begin(115200);

  Serial.setDebugOutput(true);

  // zzz
  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.println("ESP-CAM Video Recorder v39\n");
  Serial.println(" http://desklens.local - to access the camera\n");
  Serial.println("-------------------------------------");

  print_stats("Begin setup Core: ");

  pinMode(33, OUTPUT);    // little red led on back of chip

  digitalWrite(33, LOW);           // turn on the red LED on the back of chip


  eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.disconnected.reason);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("*** connected/disconnected issue!   WiFi disconnected ???...");
      WiFi.disconnect();
    } else {
      Serial.println("*** WiFi disconnected ???...");
    }
  }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);


  if (init_wifi()) { // Connected to WiFi
    internet_connected = true;
    Serial.println("Internet connected");
    sprintf(localip, "%s", WiFi.localIP().toString().c_str());

    if (!MDNS.begin("desklens")) {
      Serial.println("Error setting up MDNS responder!");
    } else {
      Serial.println("mDNS responder started");
    }

    init_time();
    time(&now);

    //setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);

    // zzz
    setenv("TZ", "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00", 1);  // mountain time zone
    tzset();
    delay(1000);
    time(&now);
    Serial.print("After timezone : "); Serial.println(ctime(&now));
  }

  baton = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    codeForCameraTask,
    "CameraTask",
    10000,
    NULL,
    1,
    &CameraTask,
    0);

  delay(500);

  xTaskCreatePinnedToCore(
    codeForAviWriterTask,
    "AviWriterTask",
    10000,
    NULL,
    2,
    &AviWriterTask,
    1);

  delay(500);

  print_stats("After Task 1 Core: ");

  if (psramFound()) {
  } else {
    Serial.println("paraFound wrong - major fail");
    major_fail();
  }

  // SD camera init
  card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    major_fail();
    return;
  }

  print_stats("After SD init Core: ");

  startCameraServer();

  print_stats("After Server init Core: ");

  // zzz username and password for ftp server

  ftpSrv.begin("esp", "esp");

  print_stats("After ftp init Core: ");

  digitalWrite(33, HIGH);

  //
  //  startup defaults  -- EDIT HERE
  //  zzz

  framesize = 10;  // uxga
  repeat = 100;    // 100 files
  xspeed = 30;     // 30x playback speed
  gray = 0;        // not gray
  quality = 10;    // 10 on the 0..64 scale, or 10..50 subscale
  capture_interval = 1000; // 1000 ms or 1 second
  total_frames = 1800;     // 1800 frames or 60 x 30 = 30 minutes
  xlength = total_frames * capture_interval / 1000;

  new_config = 5;  // 5 means we have not configured the camera
                   // 1 setup as vga, 2 setup as uxga
                   // 3 move from uxga -> vga
                   // 4 move from vga -> uxga

  newfile = 0;    // no file is open  // don't fiddle with this!
  recording = 0;  // we are NOT recording

  config_camera();

  recording = 1;  // we are recording


  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// print_stats to keep track of memory during debugging
//

void print_stats(char *the_message) {

  Serial.print(the_message);  Serial.println(xPortGetCoreID());
  Serial.print(" Free Heap: "); Serial.print(ESP.getFreeHeap());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  printf(" Himem is %dKiB, Himem free %dKiB, ", (int)ESP.getPsramSize() / 1024, (int)ESP.getFreePsram() / 1024);
  printf("Flash is %dKiB, Sketch is %dKiB \n", (int)ESP.getFlashChipSize() / 1024, (int)ESP.getSketchSize() / 1024);

  Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);
  Serial.println(" ");
}

//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
void major_fail() {

  for  (int i = 0;  i < 10; i++) {
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);

    delay(1000);

    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(500);
    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(500);
    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(500);

    delay(1000);
  }

  ESP.restart();        

}


bool init_wifi()
{
  int connAttempts = 0;
  WiFi.mode(WIFI_STA);

  WiFi.setHostname("desklens");
  WiFi.printDiag(Serial);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts > 10) {
      Serial.println("Cannot connect - try again");
      WiFi.begin(ssid, password);
      WiFi.printDiag(Serial);
    }
    if (connAttempts > 20) {
      Serial.println("Cannot connect - fail");
      major_fail();
      return false;
      WiFi.printDiag(Serial);
      major_fail();
      return false;
    }

    connAttempts++;
  }

  WiFi.printDiag(Serial);
  return true;

  /*
    //  this is the fixed ip stuff that does not work with with router
    // zzz
    // Set your Static IP address
    IPAddress local_IP(192, 168, 1, 225);
    //IPAddress local_IP(192, 169, 1, 225);

    // Set your Gateway IP address
    IPAddress gateway(192, 168, 1, 254);
    //IPAddress gateway(192, 169, 1, 1);

    IPAddress subnet(255, 255, 0, 0);
    IPAddress primaryDNS(8, 8, 8, 8); // optional
    IPAddress secondaryDNS(8, 8, 4, 4); // optional

    WiFi.mode(WIFI_STA);

    WiFi.setHostname("ESP32CAM225");  // does not seem to do anything with my wifi router ???

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    major_fail();
    }

    WiFi.printDiag(Serial);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts > 10) {
      Serial.println("Cannot connect");
      WiFi.printDiag(Serial);
      major_fail();
      return false;
    }
    connAttempts++;
    }
    return true;

  */
}

void init_time()
{

  do_time();

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_setservername(1, "time.windows.com");
  sntp_setservername(2, "time.nist.gov");

  sntp_init();

  // wait for time to be set
  time_t now = 0;
  timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.printf("Waiting for system time to be set... (%d/%d) -- %d\n", retry, retry_count, timeinfo.tm_year);
    delay(2000);
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.println(ctime(&now));
  }

  if (timeinfo.tm_year < (2016 - 1900)) {
    major_fail();
  }
}

static esp_err_t init_sdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 10,
  };
  sdmmc_card_t *card;

  Serial.println("Mounting SD card...");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    Serial.println("SD card mount successfully!");
  }  else  {
    Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    major_fail();
  }

  Serial.print("SD_MMC Begin: "); Serial.println(SD_MMC.begin());   // required by ftp system ??
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Make the avi move in 4 pieces
//
// make_avi() called in every loop, which calls below, depending on conditions
//   start_avi() - open the file and write headers
//   another_pic_avi() - write one more frame of movie
//   end_avi() - write the final parameters and close the file



void make_avi( ) {

  // we are recording, but no file is open

  if (newfile == 0 && recording == 1) {                                     // open the file

    digitalWrite(33, HIGH);
    newfile = 1;
    start_avi();

  } else {

    // we have a file open, but not recording

    if (newfile == 1 && recording == 0) {                                  // got command to close file

      digitalWrite(33, LOW);
      end_avi();

      Serial.println("Done capture due to command");

      frames_so_far = total_frames;

      newfile = 0;    // file is closed
      recording = 0;  // DO NOT start another recording

    } else {

      if (newfile == 1 && recording == 1) {                            // regular recording

        if (frames_so_far >= total_frames)  {                                // we are done the recording

          Serial.println("Done capture for total frames!");

          digitalWrite(33, LOW);                                                       // close the file
          end_avi();

          frames_so_far = 0;
          newfile = 0;          // file is closed

          if (repeat > 0) {
            recording = 1;        // start another recording
            repeat = repeat - 1;
          } else {
            recording = 0;
          }

        } else if ((millis() - startms) > (total_frames * capture_interval)) {

          Serial.println (" "); Serial.println("Done capture for time");
          Serial.print("Time Elapsed: "); Serial.print(millis() - startms); Serial.print(" Frames: "); Serial.println(frame_cnt);
          Serial.print("Config:       "); Serial.print(total_frames * capture_interval ) ; Serial.print(" (");
          Serial.print(total_frames); Serial.print(" x "); Serial.print(capture_interval);  Serial.println(")");

          digitalWrite(33, LOW);                                                       // close the file

          end_avi();

          frames_so_far = 0;
          newfile = 0;          // file is closed
          if (repeat > 0) {
            recording = 1;        // start another recording
            repeat = repeat - 1;
          } else {
            recording = 0;
          }

        } else  {                                                            // regular

          another_save_avi();

        }
      }
    }
  }
}

static esp_err_t config_camera() {

  camera_config_t config;

  Serial.println("config camera");

  if (new_config > 2) {

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (new_config == 3) {

      config.frame_size = FRAMESIZE_VGA;
      fb_max = 20;  // from 12
      new_config = 1;
    } else {
      config.frame_size = FRAMESIZE_UXGA;
      fb_max = 4;
      new_config = 2;
    }

    config.jpeg_quality = 5;
    config.fb_count = fb_max;

    print_stats("Before deinit() runs on Core: ");

    esp_camera_deinit();

    print_stats("After deinit() runs on Core: ");

    // camera init
    cam_err = esp_camera_init(&config);
    if (cam_err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", cam_err);
      major_fail();
    }

    print_stats("After the new init runs on Core: ");

    delay(500);
  }

  sensor_t * ss = esp_camera_sensor_get();
  ss->set_quality(ss, quality);
  ss->set_framesize(ss, (framesize_t)framesize);
  if (gray == 1) {
    ss->set_special_effect(ss, 2);  // 0 regular, 2 grayscale
  } else {
    ss->set_special_effect(ss, 0);  // 0 regular, 2 grayscale
  }

  //Serial.println("after the sensor stuff");

  for (int j = 0; j < 5; j++) {
    do_fb();  // start the camera ... warm it up
    delay(20);
  }

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//

static esp_err_t start_avi() {

  Serial.println("Starting an avi ");

  config_camera();

  time(&now);
  localtime_r(&now, &timeinfo);

  strftime(strftime_buf, sizeof(strftime_buf), "%F_%H.%M.%S", &timeinfo);

  char fname[100];

  if (framesize == 6) {
    sprintf(fname, "/sdcard/%s_vga_Q%d_I%d_L%d_S%d.avi", strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 7) {
    sprintf(fname, "/sdcard/%s_svga_Q%d_I%d_L%d_S%d.avi", strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 10) {
    sprintf(fname, "/sdcard/%s_uxga_Q%d_I%d_L%d_S%d.avi", strftime_buf, quality, capture_interval, xlength, xspeed);
  } else  if (framesize == 5) {
    sprintf(fname, "/sdcard/%s_cif_Q%d_I%d_L%d_S%d.avi", strftime_buf, quality, capture_interval, xlength, xspeed);
  } else {
    Serial.println("Wrong framesize");
    sprintf(fname, "/sdcard/%s_xxx_Q%d_I%d_L%d_S%d.avi", strftime_buf, quality, capture_interval, xlength, xspeed);
  }

  Serial.print("\nFile name will be >"); Serial.print(fname); Serial.println("<");

  avifile = fopen(fname, "w");
  idxfile = fopen("/sdcard/idx.tmp", "w");

  if (avifile != NULL)  {

    //Serial.printf("File open: %s\n", fname);

  }  else  {
    Serial.println("Could not open file");
    major_fail();
  }

  if (idxfile != NULL)  {

    //Serial.printf("File open: %s\n", "/sdcard/idx.tmp");

  }  else  {
    Serial.println("Could not open file");
    major_fail();
  }


  for ( i = 0; i < AVIOFFSET; i++)
  {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  size_t err = fwrite(buf, 1, AVIOFFSET, avifile);

  if (framesize == 6) {

    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(vga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(vga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(vga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(vga_h, 1, 2, avifile);

  } else if (framesize == 10) {

    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(uxga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(uxga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(uxga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(uxga_h, 1, 2, avifile);

  } else if (framesize == 7) {

    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(svga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(svga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(svga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(svga_h, 1, 2, avifile);

  }  else if (framesize == 5) {

    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(cif_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(cif_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(cif_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(cif_h, 1, 2, avifile);
  }

  fseek(avifile, AVIOFFSET, SEEK_SET);

  Serial.print(F("\nRecording "));
  Serial.print(total_frames);
  Serial.println(F(" video frames ...\n"));

  startms = millis();
  bigdelta = millis();
  totalp = 0;
  totalw = 0;
  overtime_count = 0;
  jpeg_size = 0;
  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;


  frame_cnt = 0;
  frames_so_far = 0;

  skipping = 0;
  skipped = 0;

  newfile = 1;

  other_cpu_active = 1;

} // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi runs on cpu 1, saves another frame to the avi file
//
//  the "baton" semaphore makes sure that only one cpu is using the camera subsystem at a time
//

static esp_err_t another_save_avi() {

  xSemaphoreTake( baton, portMAX_DELAY );

  if (fb_in == fb_out) {        // nothing to do

    xSemaphoreGive( baton );

  } else {

    //if ( (fb_in + fb_max - fb_out) % fb_max > 3) {  // more than 1 in queue ??
    //Serial.print(millis()); Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);
    //}

    fb_out = (fb_out + 1) % fb_max;

    int fblen;
    fblen = fb_q[fb_out]->len;

    //xSemaphoreGive( baton );

    digitalWrite(33, LOW);

    jpeg_size = fblen;
    movi_size += jpeg_size;
    uVideoLen += jpeg_size;

    bw = millis();
    size_t dc_err = fwrite(dc_buf, 1, 4, avifile);
    size_t ze_err = fwrite(zero_buf, 1, 4, avifile);

    //bw = millis();
    size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);
    if (err == 0 ) {
      Serial.println("Error on avi write");
      major_fail();
    }

    //xSemaphoreTake( baton, portMAX_DELAY );
    esp_camera_fb_return(fb_q[fb_out]);     // release that buffer back to the camera system
    xSemaphoreGive( baton );

    remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

    print_quartet(idx_offset, idxfile);
    print_quartet(jpeg_size, idxfile);

    idx_offset = idx_offset + jpeg_size + remnant + 8;

    jpeg_size = jpeg_size + remnant;
    movi_size = movi_size + remnant;
    if (remnant > 0) {
      size_t rem_err = fwrite(zero_buf, 1, remnant, avifile);
    }

    fileposition = ftell (avifile);       // Here, we are at end of chunk (after padding)
    fseek(avifile, fileposition - jpeg_size - 4, SEEK_SET);    // Here we are the the 4-bytes blank placeholder

    print_quartet(jpeg_size, avifile);    // Overwrite placeholder with actual frame size (without padding)

    fileposition = ftell (avifile);

    fseek(avifile, fileposition + 6, SEEK_SET);    // Here is the FOURCC "JFIF" (JPEG header)
    // Overwrite "JFIF" (still images) with more appropriate "AVI1"

    size_t av_err = fwrite(avi1_buf, 1, 4, avifile);

    fileposition = ftell (avifile);
    fseek(avifile, fileposition + jpeg_size - 10 , SEEK_SET);
    //Serial.println("Write done");
    totalw = totalw + millis() - bw;

    //if (((fb_in + fb_max - fb_out) % fb_max) > 0 ) {
    //  Serial.print(((fb_in + fb_max - fb_out) % fb_max)); Serial.print(" ");
    //}

    digitalWrite(33, HIGH);
  }
} // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi runs on cpu 1, empties the queue of frames, writes the index, and closes the files
//

static esp_err_t end_avi() {

  unsigned long current_end = 0;

  other_cpu_active = 0 ;

  Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);

  for (int i = 0; i < fb_max; i++) {           // clear the queue
    another_save_avi();
  }

  Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);

  current_end = ftell (avifile);

  Serial.println("End of avi - closing the files");

  elapsedms = millis() - startms;
  float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * xspeed;
  float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
  uint8_t iAttainedFPS = round(fRealFPS);
  uint32_t us_per_frame = round(fmicroseconds_per_frame);


  //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

  fseek(avifile, 4 , SEEK_SET);
  print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

  fseek(avifile, 0x20 , SEEK_SET);
  print_quartet(us_per_frame, avifile);

  unsigned long max_bytes_per_sec = movi_size * iAttainedFPS / frame_cnt;

  fseek(avifile, 0x24 , SEEK_SET);
  print_quartet(max_bytes_per_sec, avifile);

  fseek(avifile, 0x30 , SEEK_SET);
  print_quartet(frame_cnt, avifile);

  fseek(avifile, 0x8c , SEEK_SET);
  print_quartet(frame_cnt, avifile);

  fseek(avifile, 0x84 , SEEK_SET);
  print_quartet((int)iAttainedFPS, avifile);

  fseek(avifile, 0xe8 , SEEK_SET);
  print_quartet(movi_size + frame_cnt * 8 + 4, avifile);

  Serial.println(F("\n*** Video recorded and saved ***\n"));
  Serial.print(F("Recorded "));
  Serial.print(elapsedms / 1000);
  Serial.print(F("s in "));
  Serial.print(frame_cnt);
  Serial.print(F(" frames\nFile size is "));
  Serial.print(movi_size + 12 * frame_cnt + 4);
  Serial.print(F(" bytes\nActual FPS is "));
  Serial.print(fRealFPS, 2);
  Serial.print(F("\nMax data rate is "));
  Serial.print(max_bytes_per_sec);
  Serial.print(F(" byte/s\nFrame duration is "));  Serial.print(us_per_frame);  Serial.println(F(" us"));
  Serial.print(F("Average frame length is "));  Serial.print(uVideoLen / frame_cnt);  Serial.println(F(" bytes"));
  Serial.print("Average picture time (ms) "); Serial.println( totalp / frame_cnt );
  Serial.print("Average write time (ms) "); Serial.println( totalw / frame_cnt );
  Serial.print("Frames Skipped % ");  Serial.println( 100.0 * skipped / frame_cnt, 1 );

  Serial.println("Writing the index");

  fseek(avifile, current_end, SEEK_SET);

  fclose(idxfile);

  size_t i1_err = fwrite(idx1_buf, 1, 4, avifile);

  print_quartet(frame_cnt * 16, avifile);

  idxfile = fopen("/sdcard/idx.tmp", "r");

  if (idxfile != NULL)  {

    //Serial.printf("File open: %s\n", "/sdcard/idx.tmp");

  }  else  {
    Serial.println("Could not open file");
    //major_fail();
  }

  char * AteBytes;
  AteBytes = (char*) malloc (8);

  for (int i = 0; i < frame_cnt; i++) {

    size_t res = fread ( AteBytes, 1, 8, idxfile);
    size_t i1_err = fwrite(dc_buf, 1, 4, avifile);
    size_t i2_err = fwrite(zero_buf, 1, 4, avifile);
    size_t i3_err = fwrite(AteBytes, 1, 8, avifile);

  }

  free(AteBytes);

  fclose(idxfile);
  fclose(avifile);

  Serial.println("---");
  //WiFi.printDiag(Serial);

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  do_fb - just takes a picture and discards it
//

static esp_err_t do_fb() {
  xSemaphoreTake( baton, portMAX_DELAY );
  camera_fb_t * fb = esp_camera_fb_get();

  Serial.print("Pic, len="); Serial.println(fb->len);

  esp_camera_fb_return(fb);
  xSemaphoreGive( baton );
}

void do_time() {

  int numberOfNetworks = WiFi.scanNetworks();

  Serial.print("Number of networks found: ");
  Serial.println(numberOfNetworks);

}

////////////////////////////////////////////////////////////////////////////////////
//
// some globals for the loop()
//

long wakeup;
long last_wakeup = 0;


void loop()
{

  if (WiFi.status() != WL_CONNECTED) {
    init_wifi();
    Serial.println("***** WiFi reconnect *****");
  }

  wakeup = millis();
  if (wakeup - last_wakeup > (10 * 60 * 1000) ) {       // 10 minutes
    last_wakeup = millis();

    print_stats("Wakeup in loop() Core: ");
  }

  ftpSrv.handleFTP();
  delay(10);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

static esp_err_t capture_handler(httpd_req_t *req) {

  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char fname[100];
  xSemaphoreTake( baton, portMAX_DELAY );
  fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    xSemaphoreGive( baton );
    return ESP_FAIL;
  }

  file_number++;

  sprintf(fname, "inline; filename=capture_%d.jpg", file_number);

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", fname);

  size_t out_len, out_width, out_height;
  size_t fb_len = 0;
  fb_len = fb->len;
  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  xSemaphoreGive( baton );
  return res;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
static esp_err_t stop_handler(httpd_req_t *req) {

  esp_err_t res = ESP_OK;

  recording = 0;
  Serial.println("stopping recording");

  do_stop("Stopping previous recording");

  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
static esp_err_t start_handler(httpd_req_t *req) {

  esp_err_t res = ESP_OK;

  char  buf[80];
  size_t buf_len;
  char  new_res[20];

  if (recording == 1) {
    const char* resp = "You must Stop recording, before starting a new one.  Start over ...";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
    return res;

  } else {
    //recording = 1;
    Serial.println("starting recording");

    sensor_t * s = esp_camera_sensor_get();

    int new_interval = capture_interval;
    int new_length = capture_interval * total_frames;

    int new_framesize = s->status.framesize;
    int new_quality = s->status.quality;
    int new_repeat = 0;
    int new_xspeed = 1;
    int new_xlength = 3;
    int new_gray = 0;

    Serial.println("");
    Serial.println("Current Parameters :");
    Serial.print("  Capture Interval = "); Serial.print(capture_interval);  Serial.println(" ms");
    Serial.print("  Length = "); Serial.print(capture_interval * total_frames / 1000); Serial.println(" s");
    Serial.print("  Quality = "); Serial.println(new_quality);
    Serial.print("  Framesize = "); Serial.println(new_framesize);
    Serial.print("  Repeat = "); Serial.println(repeat);
    Serial.print("  Speed = "); Serial.println(xspeed);

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        ESP_LOGI(TAG, "Found URL query => %s", buf);
        char param[32];
        /* Get value of expected key from query string */
        Serial.println(" ... parameters");
        if (httpd_query_key_value(buf, "length", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 5 && x <= 3600 * 24 ) {   // 5 sec to 24 hours
            new_length = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => length=%s", param);

        }
        if (httpd_query_key_value(buf, "repeat", param, sizeof(param)) == ESP_OK) {
          int x = atoi(param);
          if (x >= 0  ) {
            new_repeat = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => repeat=%s", param);
        }
        if (httpd_query_key_value(buf, "framesize", new_res, sizeof(new_res)) == ESP_OK) {
          if (strcmp(new_res, "UXGA") == 0) {
            new_framesize = 10;
          } else if (strcmp(new_res, "SVGA") == 0) {
            new_framesize = 7;
          } else if (strcmp(new_res, "VGA") == 0) {
            new_framesize = 6;
          } else if (strcmp(new_res, "CIF") == 0) {
            new_framesize = 5;
          } else {
            Serial.println("Only UXGA, SVGA, VGA, and CIF are valid!");

          }
          ESP_LOGI(TAG, "Found URL query parameter => framesize=%s", new_res);
        }
        if (httpd_query_key_value(buf, "quality", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 5 && x <= 50) {
            new_quality = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => quality=%s", param);
        }

        if (httpd_query_key_value(buf, "speed", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 1 && x <= 100) {
            new_xspeed = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => speed=%s", param);
        }

        if (httpd_query_key_value(buf, "gray", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x == 1 ) {
            new_gray = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => gray=%s", param);
        }

        if (httpd_query_key_value(buf, "interval", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 1 && x <= 108000) {  //  108,000 ms = 30 min
            new_interval = x;
          }
          ESP_LOGI(TAG, "Found URL query parameter => interval=%s", param);
        }
      }
    }

    framesize = new_framesize;
    capture_interval = new_interval;
    xlength = new_length;
    total_frames = new_length * 1000 / capture_interval;
    repeat = new_repeat;
    quality = new_quality;
    xspeed = new_xspeed;
    gray = new_gray;

    if ((new_config == 1) && (framesize > 6)) {
      new_config = 4;
      Serial.println("from VGA to UXGA");
    } else if ((new_config == 2) && (framesize < 7)) {
      new_config = 3;
      Serial.println("from UXGA to VGA");
    }


    do_start("Starting a new AVI");
    httpd_resp_send(req, the_page, strlen(the_page));



    recording = 1;
    return ESP_OK;
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_start(char *the_message) {

  Serial.print("do_start "); Serial.println(the_message);

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>ESP32-CAM Video Recorder v39</h1><br>
 <h2>Message is <font color="red">%s</font></h2><br>
 Recording = %d (1 is active)<br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (5 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Speed = %d<br>
 Gray = %d<br><br>

<br>
<br><div id="image-container"></div>

</body>
</html>)rawliteral";


  sprintf(the_page, msg, the_message, recording, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray);
  Serial.println(strlen(msg));

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_stop(char *the_message) {

  Serial.print("do_stop "); Serial.println(the_message);


  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>ESP32-CAM Video Recorder v39</h1><br>
 <h2>Message is <font color="red">%s</font></h2><br>
 <h2><a href="http://%s/">http://%s/</a></h2><br>
   Information and viewfinder<br><br>
 <h2><a href="http://%s/start?framesize=VGA&length=1800&interval=83&quality=10&repeat=100&speed=1&gray=0">http://%s/start?framesize=VGA&length=1800&interval=83&quality=10&repeat=100&speed=1&gray=0</a></h2><br>
   VGA 12 fps - VGA 640x480, video of 1800 seconds (30 min), picture every 83 ms, jpeg quality 10, repeat for 100 more of the same and play back at 1x actual fps, and don't make it grayscale<br><br>      
<h2><a href="http://%s/start?framesize=UXGA&length=1800&interval=1000&quality=10&repeat=100&speed=30&gray=0">UXGA 1 sec per frame, for 30 minutes repeat, 30x playback</a></h2><br>
<h2><a href="http://%s/start?framesize=UXGA&length=1800&interval=500&quality=10&repeat=100&speed=1&gray=0">UXGA 2 fps for 30 minutes repeat</a></h2><br>
<h2><a href="http://%s/start?framesize=CIF&length=1800&interval=42&quality=10&repeat=100&speed=1&gray=0">CIF 24 fps second for 30 minutes repeat</a></h2><br>

<br>
<br><div id="image-container"></div>

</body>
</html>)rawliteral";


  sprintf(the_page, msg, the_message, localip, localip, localip, localip, localip, localip, localip);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_status(char *the_message) {

  Serial.print("do_status "); Serial.println(the_message);

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>ESP32-CAM Video Recorder v39<br><font color="red">%s</font></h1><br>
 <h2>Message is <font color="red">%s</font></h2><br>
 Recording = %d (1 is active)<br>
 Frame %d of %d, Skipped %d<br><br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (5 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Playback Speed = %d<br>
 Gray = %d<br><br>
 Commands as follows for your ESP's ip address:<br><br>
 <h2><a href="http://%s/">http://%s/</a></h2><br>
   Information and viewfinder<br><br>
 <h2><a href="http://%s/stop">http://%s/stop ... and restart</a></h2><br>
   You must "stop" before starting with new parameters<br><br>
 <br>
 <h2><a href="ftp://%s/">ftp://%s/</a></h2><br>
 Username: esp, Password: esp ... to download the files<br><br>
 Red LED on back of ESP will flash with every frame, and flash SOS if camera or sd card not working.<br>

<br>
Check camera position with the frames below every 5 seconds for 5 pictures  <br>
Refresh page for more.<br>
<br><div id="image-container"></div>
<script>
document.addEventListener('DOMContentLoaded', function() {
  var c = document.location.origin;
  const ic = document.getElementById('image-container');  
  var i = 1;
  
  var timing = 5000;

  function loop() {
    ic.insertAdjacentHTML('beforeend','<img src="'+`${c}/capture?_cb=${Date.now()}`+'">')
    i = i + 1;
    if ( i < 6 ) {
      window.setTimeout(loop, timing);
    }
  }
  loop();
  
})
</script><br>
</body>
</html>)rawliteral";

  time(&now);
  const char *strdate = ctime(&now);


  sprintf(the_page,  msg, strdate, the_message, recording, frames_so_far, total_frames, skipped, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray, localip, localip, localip, localip, localip, localip);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
static esp_err_t index_handler(httpd_req_t *req) {

  print_stats("Index Handler  Core: ");

  do_status("Refresh Status");
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t file_stop = {
    .uri       = "/stop",
    .method    = HTTP_GET,
    .handler   = stop_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t file_start = {
    .uri       = "/start",
    .method    = HTTP_GET,
    .handler   = start_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &file_start);
    httpd_register_uri_handler(camera_httpd, &file_stop);
  }
}
