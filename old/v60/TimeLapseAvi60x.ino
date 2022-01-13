

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

  The is Arduino code, with standard setup for ESP32-CAM
    - Board ESP32 Wrover Module
    - Partition Scheme Huge APP (3MB No OTA)

*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  edit these parameters for your needs

static const char vernum[] = "v60";               // version 60 Feb 26, 2020 - added dates/times to ftp
                                                  // version 59 Feb 23, 2020 - switch to 1 bit system, and pir
static const char devname[] = "desklens";         // name of your camera for mDNS, Router, and filenames


#define TIMEZONE "GMT0BST,M3.5.0/01,M10.5.0/02"             // your timezone  -  this is GMT


// 1 for blink red led with every sd card write, at your frame rate
// 0 for blink only for skipping frames and SOS if camera or sd is broken
#define BlinkWithWrite 0

// EDIT ssid and password
const char ssid[] = "wifiRouter";           // your wireless network name (SSID)
const char password[] = "wifipassword";  // your Wi-Fi network password

// startup defaults for first recording

// here are the recording options from the "retart web page" 
// VGA 10 fps for 30 min, repeat, realtime                   http://192.168.0.117/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0
// VGA 2 fps, for 30 minutes repeat, 30x playback            http://192.168.0.117/start?framesize=VGA&length=1800&interval=500&quality=10&repeat=300&speed=30&gray=0
// UXGA 1 sec per frame, for 30 minutes repeat, 30x playback http://192.168.0.117/start?framesize=UXGA&length=1800&interval=1000&quality=10&repeat=100&speed=30&gray=0
// UXGA 2 fps for 30 minutes repeat, 15x playback            http://192.168.0.117/start?framesize=UXGA&length=1800&interval=500&quality=10&repeat=100&speed=30&gray=0
// CIF 20 fps second for 30 minutes repeat                   http://192.168.0.117/start?framesize=CIF&length=1800&interval=50&quality=10&repeat=100&speed=1&gray=0

// reboot startup parameters here

int record_on_reboot = 1;          // set to 1 to record, or 0 to NOT record on reboot
int  framesize = 6;                // vga  (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)
int  repeat = 100;                 // 100 files
int  xspeed = 1;                   // 1x playback speed (realtime is 1)
int  gray = 0;                     // not gray
int  quality = 10;                 // 10 on the 0..64 scale, or 10..50 subscale - 10 is good, 20 is grainy and smaller files
int  capture_interval = 100;       // 100 ms or 10 frames per second
int  total_frames = 18000;         // 18000 frames = 10 fps * 60 seconds * 30 minutes = half hour

int PIRpin = 12;  

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int  new_config = 5;         // this system abandoned !
int  xlength = total_frames * capture_interval / 1000;
int recording = 0;
int PIRstatus = 0;
int PIRrecording = 0;
int ready = 0;

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"

#include <ESPmDNS.h>

#include "ESP32FtpServer.h"
#include <HTTPClient.h>

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP32FtpServer.h to see ftp verbose on serial

// Time
#include "time.h"

// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <SD_MMC.h>

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
uint8_t svga_w[2] = {0x20, 0x03}; // 800
uint8_t svga_h[2] = {0x58, 0x02}; // 600
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
  0x76, 0x36, 0x30, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
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

  for (;;) {
    if (ready) {
      make_avi();
    }
    delay(1);
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// CameraTask runs on cpu 0 to take pictures and drop them in a queue
//

void codeForCameraTask( void * parameter )
{

  for (;;) {

    if (other_cpu_active == 1 ) {
      current_millis = millis();
      if (current_millis - last_capture_millis > capture_interval) {

        last_capture_millis = millis();

        xSemaphoreTake( baton, portMAX_DELAY );

        if  ( ( (fb_in + fb_max - fb_out) % fb_max) + 1 == fb_max ) {
          xSemaphoreGive( baton );

          Serial.print(" Queue Full, Skipping ... ");  // the queue is full
          skipped++;
          skipping = 1;

        }

        if (skipping > 0 ) {


          if (!BlinkWithWrite) {
            digitalWrite(33, LOW);
          }

          if (skipping % 2 == 0) {  // skip every other frame until queue is cleared

            frames_so_far = frames_so_far + 1;
            frame_cnt++;

            fb_in = (fb_in + 1) % fb_max;
            bp = millis();
            fb_q[fb_in] = esp_camera_fb_get();
            totalp = totalp - bp + millis();

          } else {
            Serial.print(((fb_in + fb_max - fb_out) % fb_max));  // skip an extra frame to empty the queue
            skipped++;
          }
          skipping = skipping + 1;
          if (((fb_in + fb_max - fb_out) % fb_max) == 0 ) {
            skipping = 0;
            Serial.println(" Queue cleared. ");
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
      }
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
  Serial.printf("ESP-CAM Video Recorder %s\n", vernum);
  Serial.printf(" http://%s.local - to access the camera\n", devname);
  Serial.println("-------------------------------------");

  pinMode(33, OUTPUT);    // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip

  if (init_wifi()) { // Connected to WiFi
    internet_connected = true;
  }

  if (!psramFound()) {
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

  startCameraServer();

  // zzz username and password for ftp server

  ftpSrv.begin("esp", "esp");

  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

  digitalWrite(33, HIGH);         // red light turns off when setup is complete

  baton = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    codeForCameraTask,
    "CameraTask",
    10000,
    NULL,
    1,
    &CameraTask,
    0);

  delay(50);

  xTaskCreatePinnedToCore(
    codeForAviWriterTask,
    "AviWriterTask",
    10000,
    NULL,
    2,
    &AviWriterTask,
    1);

  delay(50);


  recording = 0;  // we are NOT recording
  config_camera();


  pinMode(4, OUTPUT);                 // using 1 bit mode, shut off the Blinding Disk-Active Light
  digitalWrite(4, LOW);

  pinMode(PIRpin, INPUT_PULLDOWN);    // or PULLDOWN for active high

  newfile = 0;    // no file is open  // don't fiddle with this!

  recording = record_on_reboot;
 
  ready = 1;

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");


}


//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
void major_fail() {

  Serial.println(" ");

  for  (int i = 0;  i < 10; i++) {                 // 10 loops or about 100 seconds then reboot
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
    Serial.print("Major Fail  "); Serial.print(i); Serial.print(" / "); Serial.println(10);
  }

  ESP.restart();

}


bool init_wifi()
{
  int connAttempts = 0;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.setHostname(devname);
  //WiFi.printDiag(Serial);
  WiFi.begin(ssid, password);
  delay(1000);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts == 10) {
      Serial.println("Cannot connect - try again");
      WiFi.begin(ssid, password);
      WiFi.printDiag(Serial);
    }
    if (connAttempts == 20) {
      Serial.println("Cannot connect - fail");

      WiFi.printDiag(Serial);
      return false;
    }
    connAttempts++;
  }

  Serial.println("Internet connected");

  //WiFi.printDiag(Serial);

  if (!MDNS.begin(devname)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.printf("mDNS responder started '%s'\n", devname);
  }

  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE, 1);  // mountain time zone from #define at top
  tzset();

  time_t now ;
  timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  delay(1000);
  time(&now);
  localtime_r(&now, &timeinfo);

  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.printf("Waiting for system time to be set... (%d/%d) -- %d\n", retry, retry_count, timeinfo.tm_year);
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  Serial.println(ctime(&now));
  sprintf(localip, "%s", WiFi.localIP().toString().c_str());

  return true;

}


static esp_err_t init_sdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;                       // using 1 bit mode
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;                                   // using 1 bit mode
  //Serial.print("Slot config width should be 4 width:  "); Serial.println(slot_config.width);
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
  };

  //pinMode(4, OUTPUT);                 // using 1 bit mode, shut off the Blinding Disk-Active Light
  //digitalWrite(4, LOW);

  sdmmc_card_t *card;

  Serial.println("Mounting SD card...");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    Serial.println("SD card mount successfully!");
  }  else  {
    Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    major_fail();
  }
  sdmmc_card_print_info(stdout, card);
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


  PIRstatus = digitalRead(PIRpin);


  if (PIRstatus == 1) {

    if (PIRrecording == 1) {
      // keep recording for 15 more seconds
      if ( (millis() - startms) > (total_frames * capture_interval - 5000)  ) {

        total_frames = total_frames + 10000 / capture_interval ;
        Serial.println("Add another 10 seconds");
      }

    } else {

      if ( recording == 0 && newfile == 0) {

        //start a pir recording with current parameters, except no repeat and 15 seconds
        Serial.println("Start a PIR");
        PIRrecording = 1;
        repeat = 0;
        total_frames = 15000 / capture_interval;
        xlength = total_frames * capture_interval / 1000;
        recording = 1;
      }
    }
  }


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
      PIRrecording = 0;

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
            PIRrecording = 0;
          }

        } else if ((millis() - startms) > (total_frames * capture_interval)) {  // time is up, even though we have not done all the frames

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
            PIRrecording = 0;
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

  //Serial.println("config camera");

  if (new_config == 5) {

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

    config.frame_size = FRAMESIZE_UXGA;

    fb_max = 7;                                 // for vga and uxga
    config.jpeg_quality = 8;
    config.fb_count = fb_max + 1;

    // camera init
    cam_err = esp_camera_init(&config);
    if (cam_err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", cam_err);
      major_fail();
    }

    new_config = 2;

  }

  delay(100);

  sensor_t * ss = esp_camera_sensor_get();
  ss->set_quality(ss, quality);
  ss->set_framesize(ss, (framesize_t)framesize);
  if (gray == 1) {
    ss->set_special_effect(ss, 2);  // 0 regular, 2 grayscale
  } else {
    ss->set_special_effect(ss, 0);  // 0 regular, 2 grayscale
  }

  for (int j = 0; j < 3; j++) {
    do_fb();  // start the camera ... warm it up
    delay(1);
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
    sprintf(fname, "/sdcard/%s_%s_vga_Q%d_I%d_L%d_S%d.avi", devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 7) {
    sprintf(fname, "/sdcard/%s_%s_svga_Q%d_I%d_L%d_S%d.avi", devname,  strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 10) {
    sprintf(fname, "/sdcard/%s_%s_uxga_Q%d_I%d_L%d_S%d.avi", devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else  if (framesize == 5) {
    sprintf(fname, "/sdcard/%s_%s_cif_Q%d_I%d_L%d_S%d.avi", devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else {
    Serial.println("Wrong framesize");
    sprintf(fname, "/sdcard/%s_%s_xxx_Q%d_I%d_L%d_S%d.avi", devname, strftime_buf, quality, capture_interval, xlength, xspeed);
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

    fb_out = (fb_out + 1) % fb_max;

    int fblen;
    fblen = fb_q[fb_out]->len;

    //xSemaphoreGive( baton );

    if (BlinkWithWrite) {
      digitalWrite(33, LOW);
    }

    jpeg_size = fblen;
    movi_size += jpeg_size;
    uVideoLen += jpeg_size;

    bw = millis();
    size_t dc_err = fwrite(dc_buf, 1, 4, avifile);
    size_t ze_err = fwrite(zero_buf, 1, 4, avifile);

    bw = millis();
    size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);
    if (err == 0 ) {
      Serial.println("Error on avi write");
      major_fail();
    }
    totalw = totalw + millis() - bw;

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
    //41 totalw = totalw + millis() - bw;

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

  other_cpu_active = 0 ;  // shuts down the picture taking program

  //Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);

  for (int i = 0; i < fb_max; i++) {           // clear the queue
    another_save_avi();
  }

  //Serial.print(" Write Q: "); Serial.print((fb_in + fb_max - fb_out) % fb_max); Serial.print(" in/out  "); Serial.print(fb_in); Serial.print(" / "); Serial.println(fb_out);

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
  Serial.print("Average write time (ms)   "); Serial.println( totalw / frame_cnt );
  Serial.print("Frames Skipped % ");  Serial.println( 100.0 * skipped / total_frames, 1 );

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
  int xx = remove("/sdcard/idx.tmp");

  Serial.println("---");

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

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("***** WiFi reconnect *****");
    WiFi.reconnect();
    delay(5000);

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("***** WiFi rerestart *****");
      init_wifi();
    }

    sprintf(localip, "%s", WiFi.localIP().toString().c_str());
  }

}

////////////////////////////////////////////////////////////////////////////////////
//
// some globals for the loop()
//

long wakeup;
long last_wakeup = 0;


void loop()
{

  wakeup = millis();
  if (wakeup - last_wakeup > (14 * 60 * 1000) ) {       // 14 minutes
    last_wakeup = millis();

    do_time();
  }

  ftpSrv.handleFTP();

  //delay(1);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//

static esp_err_t capture_handler(httpd_req_t *req) {

  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char fname[100];
  //xSemaphoreTake( baton, portMAX_DELAY );
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
  //xSemaphoreGive( baton );
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

    /*
        Serial.println("");
        Serial.println("Current Parameters :");
        Serial.print("  Capture Interval = "); Serial.print(capture_interval);  Serial.println(" ms");
        Serial.print("  Length = "); Serial.print(capture_interval * total_frames / 1000); Serial.println(" s");
        Serial.print("  Quality = "); Serial.println(new_quality);
        Serial.print("  Framesize = "); Serial.println(new_framesize);
        Serial.print("  Repeat = "); Serial.println(repeat);
        Serial.print("  Speed = "); Serial.println(xspeed);
    */

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
      if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        ESP_LOGI(TAG, "Found URL query => %s", buf);
        char param[32];
        /* Get value of expected key from query string */
        //Serial.println(" ... parameters");
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
          if (x >= 10 && x <= 50) {                 // MINIMUM QUALITY 10 to save memory
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
          if (x >= 1 && x <= 180000) {  //  180,000 ms = 3 min
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
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder %s </h1><br>
 <h3>Message is <font color="red">%s</font></h3><br>
 Recording = %d (1 is active)<br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (10 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Speed = %d<br>
 Gray = %d<br><br>

<br>
<br><div id="image-container"></div>

</body>
</html>)rawliteral";


  sprintf(the_page, msg, devname, devname, vernum, the_message, recording, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray);
  //Serial.println(strlen(msg));

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
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder %s </h1><br>
 <h3>Message is <font color="red">%s</font></h3><br>
 <h3><a href="http://%s/">http://%s/</a></h3>
 <h3><a href="http://%s/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0">http://%s/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0</a></h3> 
<h3><a href="http://%s/start?framesize=VGA&length=1800&interval=500&quality=10&repeat=300&speed=30&gray=0">VGA 2 fps, for 30 minutes repeat, 30x playback</a></h3>
<h3><a href="http://%s/start?framesize=UXGA&length=1800&interval=1000&quality=10&repeat=100&speed=30&gray=0">UXGA 1 sec per frame, for 30 minutes repeat, 30x playback</a></h3>
<h3><a href="http://%s/start?framesize=UXGA&length=1800&interval=500&quality=10&repeat=100&speed=30&gray=0">UXGA 2 fps for 30 minutes repeat, 15x playback</a></h3>
<h3><a href="http://%s/start?framesize=CIF&length=1800&interval=50&quality=10&repeat=100&speed=1&gray=0">CIF 20 fps second for 30 minutes repeat</a></h3>
<br>
</body>
</html>)rawliteral";

  sprintf(the_page, msg, devname, devname, vernum, the_message, localip, localip, localip, localip, localip, localip, localip, localip);

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
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder %s <br><font color="red">%s</font></h1><br>
 <h3>Message is <font color="red">%s</font></h3><br>
 Total SD Space is %d MB, Used SD Space is %d MB<br>
 Recording = %d (1 is active)<br>
 Frame %d of %d, Skipped %d<br><br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (10 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Playback Speed = %d<br>
 Gray = %d<br><br>
 Commands as follows for your ESP's ip address:<br>
 <h3><a href="http://%s/">http://%s/</a></h3>
 <h3><a href="http://%s/stop">http://%s/stop<font color="red"> ... and restart<br>You must be stopped before restart or PIR</font></a></h3>
 <h3><a href="ftp://%s/">ftp://%s/</a></h3>
 Username: esp, Password: esp ... to download the files<br><br>
 Red LED on back of ESP will flash with every frame (or skipped frames), and flash SOS if camera or sd card not working.<br>

<br>
Check camera position with the frame below <br>
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
    ic.insertAdjacentHTML('beforeend','<br>')
    ic.insertAdjacentHTML('beforeend',Date())
    ic.insertAdjacentHTML('beforeend','<br>')
    
    i = i + 1;
    if ( i < 2 ) {
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

  //Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  //Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

  int tot = SD_MMC.totalBytes() / (1024 * 1024);
  int use = SD_MMC.usedBytes() / (1024 * 1024);

  //Serial.print(strlen(msg)); Serial.print(" ");

  sprintf(the_page, msg, devname, devname, vernum, strdate, the_message, tot, use, recording, frames_so_far, total_frames, skipped, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray, localip, localip, localip, localip, localip, localip);

  //Serial.println(strlen(the_page));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t index_handler(httpd_req_t *req) {

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

  Serial.println("Camera http started");
}
