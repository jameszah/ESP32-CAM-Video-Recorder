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

  Version 89 - Jul 13, 2020
   - bot/pir enable/diable on web
   - less re-progs of camera
   - store settings in eprom, so it reboots back to where it was
   - more pictures before movie starts to stablize exposure

  Version 86 - Jun 30, 2020
    - redo camera scheduler to reduce frame skips with slight delays between frames
    - move more processing to separate priority tasks, and remove from idle loop()
    - most tasks suspened waiting for events, rather than loopong checking for events, ... except ftp which still loops wating for ftp requests
    - added a sd card snapshot jpg at beginning of every movie
    - added a telegram.org message with opening picture and info about diskspace and rssi to follow activity on camera on your computer or phone
    - added deepsleep feature to wake on PIR, and then deepsleep after movie is recorded
    - added touch sensor on pin12 to enable/disable the pir sensor
    - added more careful setup of difficult pins 12, 13, and 4 - used for SD and re-used for PIR, Touch, and Blinding Disk-Active Light
    - added brownout handler to close files on brownout, which didn't work, but at least I can deepsleep to prevent multiple brownout reboots
      - inside a brownout handler, you have only 300ms and you cannot access wifi, sd, or flash, ... so cannot close files, or send message
    - re-used pin 4 Blinding Disk-Active Light to blink gently at beginning of movie, and at a Touch - ironically, also turns on during Brownout ;-)
    - added several functions to enable / disable pir or bot using internet
                  http://desklens.local/bot_enable
                  http://desklens.local/bot_disable
                  http://desklens.local/pir_enable
                  http://desklens.local/pir_disable
    - moved many settings to a separate file "settings.h" so you edit that, rather than digging through the main file to set your wifi password, startup defaults,
      and enable/disable internet, pir, telegram, etc
    - not super-elegant code ... still haven't written the avi writer into a nice library
    - read comment on rtc_cntl.h below which may or may not be updated in the esp32 board library - links and info below
  Hardware
    - to use PIR function, put an active high PIR or microwave on pin 12 with a 10k resistor (brown,black,orange) to avoid antagonizing sd card
    - to use Touch function, put a wire (with optional metal touch point) on pin 13 and touch it to enable/disable pir
    - Blinding Disk-Active Light will give little blink during a touch, or when starting a recording
    - red led on back with blink with every frame if you have that enabled in settings

*/

/*
Using library ESP32 at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\ESP32 
Using library EEPROM at version 1.0.3 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\EEPROM 
Using library WiFi at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\WiFi 
Using library WiFiClientSecure at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\WiFiClientSecure 
Using library ArduinoJson at version 6.15.2 in folder: C:\Users\James\Documents\Arduino\libraries\ArduinoJson 
Using library ESPmDNS at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\ESPmDNS 
Using library SD_MMC at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\SD_MMC 
Using library FS at version 1.0 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\FS 
Using library HTTPClient at version 1.2 in folder: C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries\HTTPClient 
*/


static const char vernum[] = "v89";

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  edit parameters for wifi name, startup parameters in the local file settings.h
#include "settings.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int count_avi = 0;
int count_cam = 0;
int count_ftp = 0;
int count_ftp2 = 0;
int count_loop = 0;
int  new_config = 5;         // this system abandoned !
int  xlength = total_frames_config * capture_interval / 1000;
int repeat = repeat_config;  // repeat_config declared in settings
int total_frames = total_frames_config;
int recording = 0;
int PIRstatus = 0;
int PIRrecording = 0;
int ready = 0;

// eprom stuff v87

#include <EEPROM.h>

struct eprom_data {
  int eprom_good;
  int Internet_Enabled;
  int DeepSleepPir;
  int record_on_reboot;
  int PIRpin;
  int PIRenabled;
  int  framesize;
  int  repeat;
  int  xspeed;
  int  gray;
  int  quality;
  int  capture_interval;
  int  total_frames;
  int  xlength;
  int EnableBOT;

};



//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "UniversalTelegramBot.h"

WiFiClientSecure client;

UniversalTelegramBot bot(BOTtoken, client);

int diskspeed = 0;
char fname[100];
int send_a_telegram = 0;
int Wait_for_bot = 0;

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
char strftime_buf2[12];

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
unsigned long nothing_cam = 0;
unsigned long nothing_avi = 0;

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
  0x76, 0x38, 0x39, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// AviWriterTask runs on cpu 1 to write the avi file
//

TaskHandle_t CameraTask, AviWriterTask, FtpTask;
SemaphoreHandle_t baton;
int counter = 0;

void codeForAviWriterTask( void * parameter )
{
  uint32_t ulNotifiedValue;
  Serial.print("aviwriter, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  for (;;) {
    ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (ulNotifiedValue-- > 0)  {
      make_avi();
      count_avi++;
      delay(1);
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// FtpTask runs on cpu 0 to respond to ftp
//
void codeForFtpTask( void * parameter )
{
  uint32_t ulNotifiedValue;
  Serial.print("ftp, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  for (;;) {
    ftpSrv.handleFTP();
    count_ftp++;
    delay(1);

  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// CameraTask runs on cpu 1 to take pictures and drop them in a queue
//

void codeForCameraTask( void * parameter )
{
  int pic_delay = 0;
  int next = 0;
  long next_run_time = 0;
  Serial.print("camera, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  for (;;) {

    if (other_cpu_active == 1 ) {
      current_millis = millis();
      count_cam++;
      xSemaphoreTake( baton, portMAX_DELAY );

      int q_size = (fb_in + fb_max - fb_out) % fb_max ;

      if ( q_size + 1 == fb_max) {
        xSemaphoreGive( baton );

        Serial.print(" Queue Full, Skipping ... ");  // the queue is full
        skipped++; skipped++;
        skipping = 1;
        next = 3 * capture_interval;

      } else {
        frames_so_far++;
        frame_cnt++;

        fb_in = (fb_in + 1) % fb_max;
        bp = millis();
        fb_q[fb_in] = esp_camera_fb_get();
        totalp = totalp - bp + millis();
        pic_delay = millis() - current_millis;
        xSemaphoreGive( baton );
        last_capture_millis = millis();

        if (q_size == 0) {
          if (skipping == 1) {
            Serial.println(" Queue cleared. ");
            skipping = 0;
          }
          next = capture_interval - pic_delay;
          if (next < 2) next = 2;
        } else if (q_size < 2 ) {
          next = capture_interval - pic_delay;
          if (next < 2) next = 2;
        } else if (q_size < 4 ) {
          next =  capture_interval ;
        } else {
          next = 2 * capture_interval;
          skipped++;
          Serial.print(((fb_in + fb_max - fb_out) % fb_max));
        }
      }

      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(AviWriterTask, &xHigherPriorityTaskWoken);

      delay(next);
      next_run_time = millis() + next;
    } else {
      next_run_time = millis() + capture_interval;
      delay(capture_interval);
    }
  }
  //delay(1);
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

char the_page[4000];

char localip[20];
WiFiEventId_t eventID;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"

long TouchDeBounce = 0;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  PIR_ISR - interupt handler for PIR  - starts or extends a video
//
static void IRAM_ATTR PIR_ISR(void* arg) {

  PIRstatus = digitalRead(PIRpin) + digitalRead(PIRpin) + digitalRead(PIRpin) ;
  //Serial.print("PIR Interupt>> "); Serial.println(PIRstatus);

  //do_blink_short();
  if (PIRenabled == 1) {
    if (PIRstatus == 3) {
      if (PIRrecording == 1) {
        // keep recording for 15 more seconds

        if ( (millis() - startms) > (total_frames * capture_interval - 5000)  ) {

          total_frames = total_frames + 10000 / capture_interval ;
          //Serial.print("PIR frames = "); Serial.println(total_frames);
          Serial.print("#");
          //Serial.println("Add another 10 seconds");
        }

      } else {

        if ( recording == 0 && newfile == 0) {

          //start a pir recording with current parameters, except no repeat and 15 seconds
          Serial.println("Start a PIR");
          PIRrecording = 1;
          repeat = 0;
          total_frames = 15000 / capture_interval;
          startms = millis();
          Serial.print("PIR frames = "); Serial.println(total_frames);
          xlength = total_frames * capture_interval / 1000;
          recording = 1;
          BaseType_t xHigherPriorityTaskWoken = pdFALSE;
          vTaskNotifyGiveFromISR(AviWriterTask, &xHigherPriorityTaskWoken);
          do_blink();
        }
      }
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// get_touch5 - handler for capactive touch sensor - enable/disable pir
//

void get_touch5 () {
  // capacitive touch sensor pin 12 == T5

  //int x = (touchRead(T5) +  touchRead(T5) +  touchRead(T5)  ) / 3;
  int x = touchRead(T5);

  //Serial.print("TOUCH Interupt>> "); Serial.println(x);

  if ( x < 29 ) {

    if (PIRenabled == 1 ) {
      if (millis() - TouchDeBounce > 1000 ) {

        PIRenabled = 0;
        TouchDeBounce = millis();
        Serial.println("\nPIR Disabled\n");
        do_blink();

      }
    } else {
      if (millis() - TouchDeBounce > 1000 ) {

        PIRenabled = 1;
        TouchDeBounce = millis();
        Serial.println("PIR Enabled.");
        PIRstatus = digitalRead(PIRpin) + digitalRead(PIRpin) + digitalRead(PIRpin) ;
        if (PIRstatus == 3) {
          BaseType_t xHigherPriorityTaskWoken = pdFALSE;
          vTaskNotifyGiveFromISR(AviWriterTask, &xHigherPriorityTaskWoken);
        }
        do_blink();
      }
    }
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// setup some interupts during reboot
//

static void setupinterrupts() {

  pinMode(PIRpin, INPUT_PULLDOWN);

  Serial.print("PIRpin = ");
  for (int i = 0; i < 5; i++) {
    Serial.print( digitalRead(PIRpin) ); Serial.print(", ");
  }
  Serial.println(" ");

  esp_err_t err = gpio_isr_handler_add((gpio_num_t)PIRpin, &PIR_ISR, NULL);

  if (err != ESP_OK) Serial.printf("gpio_isr_handler_add failed (%x)", err);
  gpio_set_intr_type((gpio_num_t)PIRpin, GPIO_INTR_ANYEDGE);

  touchAttachInterrupt(T5, get_touch5, 30);
  Serial.print("Touch T5 = ");
  for (int i = 0; i < 5; i++) {
    Serial.print( touchRead(T5) ); Serial.print(", ");
  }
  Serial.println(" ");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// blink functions - which turn on/off Blinding Disk-Active Light ... gently
//

hw_timer_t * timer = NULL;

// shut off the Blinding Disk-Active Light
void IRAM_ATTR onTimer() {
  ledcWrite( 5, 0);
}

// blink on the Blinding Disk-Active Light for 100 ms, 1/256th intensity
void do_blink() {
  //Serial.println("<<<*** BLINK ***>>>");
  // timer 3, 80 million / 80000 = 1 millisecond, 100 ms
  timer = timerBegin(3, 8000, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 100, false);
  timerAlarmEnable(timer);

  // pwm channel 5, 5000 freq, 8 bit resolution, dutycycle 7, gpio 4

  ledcSetup(5, 5000, 8 );
  ledcAttachPin(4, 5);
  ledcWrite( 5, 7);
}

void do_blink_short() {
  //Serial.println("<<<*** blink ***>>>");
  // timer 3, 80 million / 80000 = 1 millisecond, 20 ms
  timer = timerBegin(3, 8000, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 20, false);
  timerAlarmEnable(timer);

  // pwm channel 5, 5000 freq, 8 bit resolution, dutycycle 1, gpio 4

  ledcSetup(5, 5000, 8 );
  ledcAttachPin(4, 5);
  ledcWrite( 5, 1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// save photos and send to telegram stuff
//

uint8_t* fb_buffer;
size_t fb_length;
int currentByte;

bool isMoreDataAvailable() {
  return (fb_length - currentByte);
}

uint8_t getNextByte() {
  currentByte++;
  return (fb_buffer[currentByte - 1]);
}

void Send_text_telegram() {

  time(&now);
  const char *strdate = ctime(&now);

  int tot = SD_MMC.totalBytes() / (1024 * 1024);
  int use = SD_MMC.usedBytes() / (1024 * 1024);
  long rssi = WiFi.RSSI();

  const char msg[] PROGMEM = R"rawliteral(
  ESP32-CAM Video Recorder %s
 %s %s  
 Used %d MB / %d MB, Rssi %d

 %s
  )rawliteral";

  sprintf(the_page, msg, vernum, devname, localip, use, tot, rssi, fname);

  if (EnableBOT) bot.sendMessage(BOTme, the_page, ""); // "MarkdownV2");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// print_ram - debugging function for show heap total and in tasks, loops through priority tasks
//

void print_ram() {
  Serial.println("cam / avi / ftp / ftp2 / loop ");
  Serial.print(count_cam); Serial.print(" / ");
  Serial.print(count_avi); Serial.print(" / ");
  Serial.print(count_ftp); Serial.print(" / ");
  Serial.print(count_ftp2); Serial.print(" / ");
  Serial.print(count_loop); Serial.println("  ");

  Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  Serial.printf("SPIRam Total heap   %d, SPIRam Free Heap   %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  //Serial.printf(" Flash Size %d, Flash Speed %d\n",ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

  if (ready) {
    Serial.println("Avi Writer / Camera / Ftp ");
    Serial.print  (uxTaskGetStackHighWaterMark(AviWriterTask));
    Serial.print  (" / "); Serial.print  (uxTaskGetStackHighWaterMark(CameraTask));
    Serial.print  (" / "); Serial.println(uxTaskGetStackHighWaterMark(FtpTask));
  }


  //Serial.printf( "Task Name\tStatus\tPrio\tHWM\tTask\tAffinity\n");
  // char stats_buffer[1024];
  //vTaskList(stats_buffer);
  // vTaskGetRunTimeStats(stats_buffer);
  // Serial.printf("%s\n\n", stats_buffer);
  Serial.println("----");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// save_photo_dated - just save one picture as a jpg and optioning send to telegram
//

static esp_err_t save_photo_dated()
{

  Serial.println("Taking a picture for file ...");
  camera_fb_t *fb = esp_camera_fb_get();

  time(&now);
  localtime_r(&now, &timeinfo);

  //delay(2000);

  strftime(strftime_buf2, sizeof(strftime_buf2), "/%Y%m%d", &timeinfo);
  SD_MMC.mkdir(strftime_buf2);

  strftime(strftime_buf, sizeof(strftime_buf), "%F %H.%M.%S", &timeinfo);

  char fname[130];

  if (framesize == 6) {
    sprintf(fname, "/sdcard%s/%s %s vga_Q%d_I%d_L%d_S%d.jpg", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 7) {
    sprintf(fname, "/sdcard%s/%s %s svga_Q%d_I%d_L%d_S%d.jpg", strftime_buf2, devname,  strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 10) {
    sprintf(fname, "/sdcard%s/%s %s uxga_Q%d_I%d_L%d_S%d.jpg", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else  if (framesize == 5) {
    sprintf(fname, "/sdcard%s/%s %s cif_Q%d_I%d_L%d_S%d.jpg", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else {
    Serial.println("Wrong framesize");
  }

  FILE *file = fopen(fname, "w");
  //file = fopen(fname, "w");
  if (file != NULL)  {
    size_t err = fwrite(fb->buf, 1, fb->len, file);
    Serial.printf("File saved: %s\n", fname);
  }  else  {
    Serial.println("Could not open file");
  }
  fclose(file);
  /////

  /////
  if (EnableBOT == 1 && Internet_Enabled == 1) {
    time(&now);
    const char *strdate = ctime(&now);

    int tot = SD_MMC.totalBytes() / (1024 * 1024);
    int use = SD_MMC.usedBytes() / (1024 * 1024);
    long rssi = WiFi.RSSI();

    const char msg[] PROGMEM = R"rawliteral(
  ESP32-CAM Video Recorder %s
 %s %s  
 Used %d MB / %d MB, Rssi %d
 %s
  )rawliteral";

    sprintf(the_page, msg, vernum, devname, localip, use, tot, rssi, strdate);  //fname

    Serial.println("Taking a picture for telegram...");
    //camera_fb_t *fb = esp_camera_fb_get();

    currentByte = 0;
    fb_length = fb->len;
    fb_buffer = fb->buf;


    Serial.print("Sending Photo Telegram, bytes: "); Serial.println(fb_length);

    //Serial.print("\nSend_photo heap before: "); Serial.println(ESP.getFreeHeap());

    String sent = bot.sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                  "image/jpeg", the_page, BOTme, fb_length,
                  isMoreDataAvailable, getNextByte, nullptr, nullptr);

    //Serial.print("\nSend_photo heap after : "); Serial.println(ESP.getFreeHeap());

    if (sent.length() > 1) {
      Serial.println("\nPhoto telegram was successfully sent "); // Serial.print(sent); Serial.println("<");

    } else {
      Serial.print("\nPhoto telegram failed >");
      Serial.print(sent); Serial.println("<");
    }

  }
  //esp_camera_fb_return(fb);
  /////
  esp_camera_fb_return(fb);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// send_photo_telegram - send an opening frame to telegram to see on phone or computer
//
// - this often fails if run at same time as a recording due to heap
// - the telegram ssl connection needs about 60k, the the sd write fuctions needs heap as well
//

static esp_err_t send_photo_telegram()
{

  time(&now);
  const char *strdate = ctime(&now);

  int tot = SD_MMC.totalBytes() / (1024 * 1024);
  int use = SD_MMC.usedBytes() / (1024 * 1024);
  long rssi = WiFi.RSSI();

  const char msg[] PROGMEM = R"rawliteral(
  ESP32-CAM Video Recorder %s
 %s %s  
 Used %d MB / %d MB, Rssi %d
 %s
  )rawliteral";

  sprintf(the_page, msg, vernum, devname, localip, use, tot, rssi, strdate);  //fname

  Serial.println("Taking a picture for telegram...");
  camera_fb_t *fb = esp_camera_fb_get();

  currentByte = 0;
  fb_length = fb->len;
  fb_buffer = fb->buf;


  Serial.print("Sending Photo Telegram, bytes: "); Serial.println(fb_length);

  //Serial.print("\nSend_photo heap before: "); Serial.println(ESP.getFreeHeap());

  String sent = bot.sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                "image/jpeg", the_page, BOTme, fb_length,
                isMoreDataAvailable, getNextByte, nullptr, nullptr);

  //Serial.print("\nSend_photo heap after : "); Serial.println(ESP.getFreeHeap());

  if (sent.length() > 1) {
    Serial.println("\nPhoto telegram was successfully sent "); // Serial.print(sent); Serial.println("<");

  } else {
    Serial.print("\nPhoto telegram failed >");
    Serial.print(sent); Serial.println("<");
  }


  esp_camera_fb_return(fb);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// setup() runs on cpu 1
//

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "soc/soc.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"

#include "esp_task_wdt.h"

#ifdef CONFIG_BROWNOUT_DET_LVL
#define BROWNOUT_DET_LVL CONFIG_BROWNOUT_DET_LVL
#else
#define BROWNOUT_DET_LVL 5
#endif //CONFIG_BROWNOUT_DET_LVL

#define CONFIG_BROWNOUT_DET_LVL_SEL_5 1

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// low_voltage_save  - runs during brownout just before system brownout handler
//
//  - turns on Blinding Disk-Active light and deepsleeps at the end
//  - not the correct action if you are using a non-protected lipo battery, but does
//    prevent multiple reboots on a weak battery, and alets the human with the bright led
//
//  - mostly included as information, as it was a lot of work and didn't ultimately work to close
//    the avi files

void IRAM_ATTR low_voltage_save(void *arg) {
  Serial.print("\nJZ low voltage handler\nStarting at ");
  long start_of_inter = millis();
  Serial.println(start_of_inter);
  time(&now);
  const char *strdate = ctime(&now);
  Serial.println(strdate);

  //recording = 0;
  //Serial.println("\nstopping recording");

  Serial.print("low volt, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //save WatchDog register
  Serial.print("\nBrown regsiter was (in hex)"); Serial.println(brown_reg_temp, HEX);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  print_ram();

  //esp_cpu_stall ( !xPortGetCoreID () );

  vTaskDelay ( 200000 / portTICK_PERIOD_MS );  // does not work
  Serial.println("slept 200 seconds - does not work!");

  Serial.println("3 seconds to close files - does not work");
  delay(3000);

  for  (int i = 0;  i < 1000000; i++) {
    Serial.print(millis() - start_of_inter); Serial.print(" ms, i = "); Serial.println(i);
    if ( millis() - start_of_inter > 250 &&  millis() - start_of_inter < 255) {
      Serial.println("250 ms passed - try to extend before 300ms wdt -- does not work");

      esp_task_wdt_reset();
    }
    if ( millis() - start_of_inter > 280) {
      Serial.println("280 ms passed - deepsleep ");

      pinMode(4, OUTPUT);               // Blinding Disk-Avtive Light
      digitalWrite(4, HIGH);             // turn ON

      esp_deep_sleep_start();
    }
  }

  Serial.println("... switching to system shutdown ...");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// print_wakeup_reason - display message after deepsleep wakeup
//

RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void do_eprom_read() {

  eprom_data ed;

  long x = millis();
  EEPROM.begin(200);
  EEPROM.get(0, ed);
  Serial.println("Get took " + String(millis() - x));

  if (ed.eprom_good == 271) {
    Serial.println("Good settings in the EPROM ");

    Internet_Enabled = ed.Internet_Enabled; Serial.print("Internet_Enabled "); Serial.println(Internet_Enabled );
    DeepSleepPir  = ed.DeepSleepPir; Serial.print("DeepSleepPir "); Serial.println(DeepSleepPir );
    record_on_reboot = ed.record_on_reboot; Serial.print("record_on_reboot "); Serial.println(record_on_reboot );
    PIRpin = ed.PIRpin; Serial.print("PIRpin "); Serial.println(PIRpin );
    PIRenabled = ed.PIRenabled; Serial.print("PIRenabled "); Serial.println(PIRenabled );
    framesize = ed.framesize; Serial.print("framesize "); Serial.println(framesize );
    repeat_config = ed.repeat; Serial.print("repeat_config "); Serial.println(repeat_config );
    repeat = ed.repeat;
    xspeed = ed.xspeed; Serial.print("xspeed "); Serial.println(xspeed );
    gray = ed.gray; Serial.print("gray "); Serial.println(gray );
    quality = ed.quality; Serial.print("quality "); Serial.println(quality );
    capture_interval = ed.capture_interval; Serial.print("capture_interval "); Serial.println(capture_interval );
    total_frames = ed.total_frames;
    total_frames_config = ed.total_frames; Serial.print("total_frames_config "); Serial.println(total_frames_config );
    xlength = ed.xlength; Serial.print("xlength "); Serial.println(xlength );
    EnableBOT = ed.EnableBOT; Serial.print("EnableBOT "); Serial.println(EnableBOT );
  } else {
    Serial.println("No settings in EPROM - putting in hardcoded settings ");
    do_eprom_write();
  }
}


void do_eprom_write() {

  eprom_data ed;

  Serial.println("Write settings in the EPROM ");
  ed.eprom_good = 271;
  ed.Internet_Enabled = Internet_Enabled;
  ed.DeepSleepPir  = DeepSleepPir;
  ed.record_on_reboot = record_on_reboot;
  ed.PIRpin = PIRpin;
  ed.PIRenabled = PIRenabled;
  ed.framesize = framesize;
  ed.repeat = repeat_config;
  ed.xspeed = xspeed;
  ed.gray = gray;
  ed.quality = quality;
  ed.capture_interval = capture_interval;
  ed.total_frames = total_frames_config;
  ed.xlength = xlength;
  ed.EnableBOT = EnableBOT;

  Serial.println("Writing to EPROM ...");

  long x = millis();
  EEPROM.begin(200);
  EEPROM.put(0, ed);
  EEPROM.commit();
  EEPROM.end();

  Serial.println("Put took " + String(millis() - x) + " ms, bytes = " + String(sizeof(ed)));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  setup()  - the Arduino setup
//

// You may have to edit rtc_cntl.h ... according to this link -- doesn't seem to be included in esp32 libraries as of Jun 2020
// https://github.com/espressif/esp-idf/commit/17bd6e8faba15812780d21e6e3db08fb26dd7033#diff-5e22dcf9fc6087d1585c7b2e434c0932
// https://github.com/espressif/esp-idf/pull/4532
// C:\Users\James\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\tools\sdk\include\driver\driver -- approximate path
// #include "driver/rtc_cntl.h"
// ... or i'll just include it with this program
#include "rtc_cntl.h"

void setup() {

  Serial.begin(115200);
  Serial.println("\n\n---");
  //Serial.println("delay 5 seconds"); delay(5000);

  esp_err_t xx = rtc_isr_register(low_voltage_save, NULL, RTC_CNTL_BROWN_OUT_INT_ENA_M);  // see 10 lines up if you get an error here!

  rtc_gpio_hold_dis(GPIO_NUM_33);
  pinMode(33, OUTPUT);             // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip

  rtc_gpio_hold_dis(GPIO_NUM_4);
  pinMode(4, OUTPUT);               // Blinding Disk-Avtive Light
  digitalWrite(4, LOW);             // turn off

  Serial.setDebugOutput(true);
  Serial.print("setup, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  // zzz
  Serial.println("                                    ");
  Serial.println("-------------------------------------");
  Serial.printf("ESP-CAM Video Recorder %s\n", vernum);
  Serial.printf(" http://%s.local - to access the camera\n", devname);
  Serial.println("-------------------------------------");

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  print_wakeup_reason();

  do_eprom_read();
  repeat = repeat_config;
  total_frames = total_frames_config;

  if (!psramFound()) {
    Serial.println("paraFound wrong - major fail");
    major_fail();
  }

  if (Internet_Enabled) {
    Serial.println("Starting wifi ...");
    if (init_wifi()) { // Connected to WiFi
      internet_connected = true;
    } else {
      Serial.println("Internet skipped");
      internet_connected = false;
    }
  }
  //plm print_ram();  delay(1000);
  Serial.println("Starting sd card ...");

  // SD camera init
  card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    major_fail();
    return;
  }
  
  //plm print_ram();  delay(2000);
  Serial.println("Starting server ...");

  if (Internet_Enabled) startCameraServer();

  // zzz username and password for ftp server

  //plm print_ram();  delay(2000);
  Serial.println("Starting ftp ...");

  if (Internet_Enabled) ftpSrv.begin("esp", "esp");

  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

  //plm print_ram();  delay(2000);
  Serial.println("Starting tasks ...");

  baton = xSemaphoreCreateMutex();  // baton controls access to camera and frame queue

  xTaskCreatePinnedToCore(
    codeForCameraTask,
    "CameraTask",
    1024,        // heap available for this task
    NULL,
    2,           // prio 2 - higher number is higher priio
    &CameraTask,
    1);          // core 1

  delay(20);

  xTaskCreatePinnedToCore(
    codeForAviWriterTask,
    "AviWriterTask",
    3072,       // heap
    NULL,
    3,          // prio 3
    &AviWriterTask,
    1);         // on cpu 1 - same as ftp http

  delay(20);


  if (Internet_Enabled) {
    xTaskCreatePinnedToCore(
      codeForFtpTask,
      "FtpTask",
      4096,       // heap
      NULL,
      4,          // prio higher than 1
      &FtpTask,
      0);         // on cpu 0

    delay(20);
  }
  //plm print_ram();  delay(2000);
  Serial.println("Starting camera ...");

  recording = 0;  // we are NOT recording
  config_camera();

  setupinterrupts();

  newfile = 0;    // no file is open  // don't fiddle with this!

  recording = record_on_reboot;

  //plm print_ram();  delay(2000);

  ready = 1;
  digitalWrite(33, HIGH);         // red light turns off when setup is complete

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  xTaskNotifyGive(AviWriterTask);

  delay(1000);
  //plm print_ram();  delay(2000);
}


//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
void major_fail() {

  Serial.println(" ");

  for  (int i = 0;  i < 10; i++) {                 // 10 loops or about 100 seconds then reboot
    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);   delay(150);
      digitalWrite(33, HIGH);  delay(150);
    }

    delay(1000);

    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);  delay(500);
      digitalWrite(33, HIGH); delay(500);
    }

    delay(1000);
    Serial.print("Major Fail  "); Serial.print(i); Serial.print(" / "); Serial.println(10);
  }

  ESP.restart();
}


bool init_wifi()
{
  int connAttempts = 0;

  Serial.println(" Disable brownout");
  uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //save WatchDog register
  Serial.print("\nBrown regsiter was (in hex)"); Serial.println(brown_reg_temp, HEX);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(devname);
  //WiFi.printDiag(Serial);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED ) {
    delay(1000);
    Serial.print(".");
    if (connAttempts == 20 ) {
      Serial.println("Cannot connect - try again");
      WiFi.begin(ssid, password);
    }
    if (connAttempts == 30) {
      Serial.println("Cannot connect - fail");

      WiFi.printDiag(Serial);
      return false;
    }
    connAttempts++;
  }

  Serial.println("\nInternet connected");

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
  const int retry_count = 15;
  delay(1000);
  time(&now);
  localtime_r(&now, &timeinfo);

  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.printf("Waiting for system time to be set... (%d/%d) -- %d\n", retry, retry_count, timeinfo.tm_year);
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  Serial.print("Local time: "); Serial.println(ctime(&now));
  sprintf(localip, "%s", WiFi.localIP().toString().c_str());

  Serial.println(" Enable brownout");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp); //enable brownout detector
  return true;

}


static esp_err_t init_sdcard()
{

  //pinMode(12, PULLUP);
  pinMode(13, PULLUP);
  //pinMode(4, OUTPUT);

  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;                       // using 1 bit mode
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  diskspeed = host.max_freq_khz;
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;                                   // using 1 bit mode
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 8,
  };

  sdmmc_card_t *card;

  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    Serial.println("SD card mount successfully!");
  }  else  {
    Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    Serial.println("Try again...");
    delay(5000);
    diskspeed = 400;
    host.max_freq_khz = SDMMC_FREQ_PROBING;
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
      Serial.println("SD card mount successfully SLOW SLOW SLOW");
    } else {
      Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
      major_fail();
    }
  }
  sdmmc_card_print_info(stdout, card);
  Serial.print("SD_MMC Begin: "); Serial.println(SD_MMC.begin());   // required by ftp system ??

  //pinMode(13, PULLDOWN);
  //pinMode(13, INPUT_PULLDOWN);
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

  if (PIRenabled == 1) {
    PIRstatus = digitalRead(PIRpin) + digitalRead(PIRpin) + digitalRead(PIRpin) ;
    //Serial.println(millis());
    if (DeepSleepPir == 1 && millis() < 15000 ) {
      //DeepSleepPir = 0;
      PIRstatus  = 3;
    }
    //Serial.print("Mak>> "); Serial.println(PIRstatus);
    if (PIRstatus == 3) {

      if (PIRrecording == 1) {
        // keep recording for 15 more seconds
        if ( (millis() - startms) > (total_frames * capture_interval - 5000)  ) {

          total_frames = total_frames + 10000 / capture_interval ;
          //Serial.print("Make PIR frames = "); Serial.println(total_frames);
          Serial.print("@");
          //Serial.println("Add another 10 seconds");
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
  }

  // we are recording, but no file is open

  if (newfile == 0 && recording == 1) {                                     // open the file

    digitalWrite(33, HIGH);
    newfile = 1;

    if (EnableBOT == 1 && Internet_Enabled == 1) {           //  if BOT is enabled wait to send it ... could be several seconds (5 or 10)
      //89 config_camera();
      send_a_telegram = 1;
      Wait_for_bot = 1;

      while (Wait_for_bot == 1) {
        delay(1000);
        Serial.print("z");                      // serial monitor will shows these "z" mixed with "*" from telegram sender
      }
    }
    Serial.println(" ");
    start_avi();                                 // now start the avi

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

        if ((millis() - startms) > (total_frames * capture_interval)) {  // time is up, even though we have not done all the frames

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
            xTaskNotifyGive(AviWriterTask);
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

    fb_max = 6;           //74.5 from 7                      // for vga and uxga
    config.jpeg_quality = 6;  //74.5 from 7
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
  ss->set_brightness(ss, 1);  //up the blightness just a bit
  ss->set_saturation(ss, -2); //lower the saturation


  for (int j = 0; j < 5; j++) {
    do_fb();  // start the camera ... warm it up
    delay(2);
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//


static esp_err_t start_avi() {

  Serial.println("Starting an avi ");

  //plm print_ram();

  //89 config_camera();

  time(&now);
  localtime_r(&now, &timeinfo);

  strftime(strftime_buf2, sizeof(strftime_buf2), "/%Y%m%d", &timeinfo);
  SD_MMC.mkdir(strftime_buf2);

  strftime(strftime_buf, sizeof(strftime_buf), "%F %H.%M.%S", &timeinfo);

  if (framesize == 6) {
    sprintf(fname, "/sdcard%s/%s %s vga_Q%d_I%d_L%d_S%d.avi", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 7) {
    sprintf(fname, "/sdcard%s/%s %s svga_Q%d_I%d_L%d_S%d.avi", strftime_buf2, devname,  strftime_buf, quality, capture_interval, xlength, xspeed);
  } else if (framesize == 10) {
    sprintf(fname, "/sdcard%s/%s %s uxga_Q%d_I%d_L%d_S%d.avi", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else  if (framesize == 5) {
    sprintf(fname, "/sdcard%s/%s %s cif_Q%d_I%d_L%d_S%d.avi", strftime_buf2, devname, strftime_buf, quality, capture_interval, xlength, xspeed);
  } else {
    Serial.println("Wrong framesize");
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
    nothing_avi++;

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

    //bw = millis();

    int time_to_give_up = 0;
    while (ESP.getFreeHeap() < 35000) {
      Serial.print(time_to_give_up); Serial.print(" Low on heap "); Serial.print(ESP.getFreeHeap());
      Serial.print(" frame q = "); Serial.println((fb_in + fb_max - fb_out) % fb_max);
      if (time_to_give_up++ == 50) break;
      delay(100 + 5 * time_to_give_up);
    }

    size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);

    time_to_give_up = 0;
    while (err != fb_q[fb_out]->len) {
      Serial.print("Error on avi write: err = "); Serial.print(err);
      Serial.print(" len = "); Serial.println(fb_q[fb_out]->len);
      time_to_give_up++;
      if (time_to_give_up == 10) major_fail();
      Serial.print(time_to_give_up); Serial.print(" Low on heap !!! "); Serial.println(ESP.getFreeHeap());

      delay(1000);
      size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);

    }

    //totalw = totalw + millis() - bw;

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

    totalw = totalw + millis() - bw;

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
  Serial.print("Average picture time (ms) "); Serial.println( 1.0 * totalp / frame_cnt);
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

  //Serial.print("Pic, len="); Serial.println(fb->len);

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

    MDNS.begin(devname);
    sprintf(localip, "%s", WiFi.localIP().toString().c_str());
  }

}

////////////////////////////////////////////////////////////////////////////////////
//
// some globals for the loop()
//

long wakeup;
long last_wakeup = 0;
int first = 1;

void loop()
{
  if (first) {
    Serial.print("the loop, core ");  Serial.print(xPortGetCoreID());
    Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));
    //vTaskPrioritySet( NULL, 2 );
    //print_ram();
    first = 0;
  }
  if (DeepSleepPir) {
    if (recording == 0 && PIRenabled == 1) {

      delay(10000);                                     //   wait 10 seoonds for another event before sleep

      if (recording == 0 && PIRenabled == 1) {

        Serial.println("Going to sleep now");

        pinMode(4, OUTPUT);
        digitalWrite(4, LOW);
        rtc_gpio_hold_en(GPIO_NUM_4);
        digitalWrite(33, HIGH);
        //rtc_gpio_hold_en(GPIO_NUM_33);

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);
        delay(500);
        esp_deep_sleep_start();
      }
    }
  }
  count_loop++;
  wakeup = millis();
  if (wakeup - last_wakeup > (13  * 60 * 1000) ) {       // 13 minutes
    last_wakeup = millis();
    do_time();

    //plm print_ram();
  }

  if (send_a_telegram == 1) {        // send the telegram after flag set, using the general heap
    send_a_telegram = 0;
    if (EnableBOT == 1 && Internet_Enabled == 1) {  // just double-check
      save_photo_dated();
      //send_photo_telegram();
      Wait_for_bot = 0;
    }
  }

  delay(1000);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//

static esp_err_t capture_handler(httpd_req_t *req) {

  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char fname[100];
  xSemaphoreTake( baton, portMAX_DELAY );

  Serial.print("capture, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

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

  do_stop();
  //do_stop("Stopping previous recording");
  xTaskNotifyGive(AviWriterTask);
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;

}

void do_status();  // down below

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t pir_en_handler(httpd_req_t *req) {

  Serial.print("http pir_en, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  PIRenabled = 1;
  do_eprom_write();
  do_status();
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t pir_dis_handler(httpd_req_t *req) {

  Serial.print("http pir_dis, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  PIRenabled = 0;
  do_eprom_write();
  do_status();
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t bot_en_handler(httpd_req_t *req) {

  Serial.print("http bot_en, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  EnableBOT = 1;
  do_eprom_write();
  do_status();
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t bot_dis_handler(httpd_req_t *req) {

  Serial.print("http bot_dis, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  EnableBOT = 0;
  do_eprom_write();
  do_status();
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t start_handler(httpd_req_t *req) {

  esp_err_t res = ESP_OK;

  char  buf[120];
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
    int new_framesize = s->status.framesize;
    int new_quality = s->status.quality;
    int new_repeat = repeat_config; //v87
    int new_xspeed = xspeed;
    int new_xlength = capture_interval * total_frames_config / 1000;    // xlength; v88
    int new_gray = gray;
    int new_bot = EnableBOT;
    int new_pir = PIRenabled;


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
            new_xlength = x;
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
          if (x >= 1 && x <= 10000) {
            new_xspeed = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => speed=%s", param);
        }

        if (httpd_query_key_value(buf, "gray", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x == 0 || x == 1 ) {
            new_gray = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => gray=%s", param);
        }

        if (httpd_query_key_value(buf, "pir", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x == 0 || x == 1 ) {
            new_pir = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => gray=%s", param);
        }

        if (httpd_query_key_value(buf, "bot", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x == 0 || x  == 1 ) {
            new_bot = x;
          }

          ESP_LOGI(TAG, "Found URL query parameter => gray=%s", param);
        }

        if (httpd_query_key_value(buf, "interval", param, sizeof(param)) == ESP_OK) {

          int x = atoi(param);
          if (x >= 1 && x <= 300000) {  //  300,000 ms = 5 min
            new_interval = x;
          }
          ESP_LOGI(TAG, "Found URL query parameter => interval=%s", param);
        }
      }
    }

    framesize = new_framesize;
    capture_interval = new_interval;
    xlength = new_xlength;
    total_frames = new_xlength * 1000 / capture_interval;
    total_frames_config = total_frames;
    repeat = new_repeat;
    repeat_config = new_repeat;
    quality = new_quality;
    xspeed = new_xspeed;
    gray = new_gray;
    EnableBOT = new_bot;
    PIRenabled = new_pir;

    config_camera();

    do_eprom_write();

    do_start();
    httpd_resp_send(req, the_page, strlen(the_page));


    recording = 1;
    xTaskNotifyGive(AviWriterTask);

    return ESP_OK;
  }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
void do_start() {
  const char the_message[] = "Starting a new AVI";

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
 <h3><font color="red">%s</font></h3><br>
 
 Recording = %d (1 is active)<br>
 Capture Interval = %d ms<br>
 Length = %d seconds<br>
 Quality = %d (10 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d<br>
 Speed = %d<br>
 Gray = %d<br>
 PIR = %d<br>
 BOT = %d<br><br>

<br>


</body>
</html>)rawliteral";


  sprintf(the_page, msg, devname, devname, vernum, the_message, recording, capture_interval, capture_interval * total_frames / 1000, quality, framesize, repeat, xspeed, gray, PIRenabled, EnableBOT);
  //Serial.println(strlen(msg));

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
void do_stop() {
  const char the_message[] = "Stopping previous recording";
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
 <h3><font color="red">%s</font></h3><br>
 <h3><a href="http://%s/">http://%s/</a></h3>
<br><a href="http://%s/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0&pir=1&bot=1">http://%s/start?framesize=VGA&length=1800&interval=100&quality=10&repeat=100&speed=1&gray=0&pir=1&bot=1</a> 
<br><a href="http://%s/start?framesize=VGA&length=1800&interval=500&quality=10&repeat=300&speed=15&gray=0&pir=1&bot=1">VGA 2 fps, for 30 minutes repeat, 15x playback</a>
<br><a href="http://%s/start?framesize=UXGA&length=1800&interval=1000&quality=12&repeat=100&speed=30&gray=0&bot=1&pir=0">UXGA 1 sec per frame, for 30 minutes repeat, 30x playback, with bot</a>
<br><a href="http://%s/start?framesize=UXGA&length=1800&interval=500&quality=12&repeat=100&speed=15&gray=0&pir=0&bot=1">UXGA 2 fps for 30 minutes repeat, 15x playback</a>
<br><a href="http://%s/start?framesize=UXGA&length=3600&interval=10000&quality=12&repeat=300&speed=300&pir=0&bot=1">UXGA 10 sec per frame for 1 hour x300 repeat, Q12</a>
<br><a href="http://%s/start?framesize=SVGA&length=600&interval=100&quality=12&repeat=900&speed=2&gray=0&pir=1&bot=1">SVGA 10fps for 10 min x2 repeat, with pir and bot</a>
<br><a href="http://%s/start?framesize=UXGA&length=7200&interval=30000&quality=12&repeat=100&speed=900&gray=0&pir=0&bot=1">UXGA 30 sec per frame for 2 hours repeat</a>

<br>
</body>
</html>)rawliteral";

  sprintf(the_page, msg, devname, devname, vernum, the_message, localip, localip, localip, localip, localip, localip, localip, localip, localip, localip);

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
void do_status() {
  const char the_message[] = "Status";
  //Serial.print("do_status "); Serial.println(the_message);

  elapsedms = millis() - startms;

  uint32_t ms_per_frame = 0;
  int avg_frame_wrt = 0;
  if (frame_cnt > 0) {
    ms_per_frame = elapsedms / frame_cnt;
    avg_frame_wrt = totalw / frame_cnt ;
  }
  time(&now);
  const char *strdate = ctime(&now);

  int tot = SD_MMC.totalBytes() / (1024 * 1024);
  int use = SD_MMC.usedBytes() / (1024 * 1024);
  long rssi = WiFi.RSSI();

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%s ESP32-CAM Video Recorder</title>
</head>
<body>
<h1>%s<br>ESP32-CAM Video Recorder %s <br><font color="red">%s</font></h1><br>

 Used / Total SD Space <font color="red"> %d MB / %d MB</font>, Rssi %d, SD speed %d<br>
 Recording = %d, PIR Active = %d, PIR Enabled = %d, BOT Enabled = %d<br>
 Filename %s <br>
 <br>
 Frame %d of %d, Skipped %d<br>
 Capture Interval = %d ms, Actual Interval = %d ms, Avg Write time = <font color="red">%d ms</font>, 
 <br><br>Length = %d seconds, Quality = %d (10 best to 50 worst)<br>
 Framesize = %d (10 UXGA, 7 SVGA, 6 VGA, 5 CIF)<br>
 Repeat = %d, Playback Speed = %d, Gray = %d<br>
 <br>
 <h3><a href="http://%s/">http://%s/</a></h3>
 <a href="http://%s/pir_enable">pir_enable</a>
 <a href="http://%s/pir_disable">pir_disable</a><br>
 <a href="http://%s/bot_enable">bot_enable</a>
 <a href="http://%s/bot_disable">bot_disable</a>
 
 <h3><a href="http://%s/stop">http://%s/stop<font color="red"> ... and restart.  You must be stopped before restart or PIR</font></a></h3>
 <h3><a href="ftp://%s/">ftp://%s/ ... Username: esp, Password: esp</a></h3>
 <br>

<br><div id="image-container"></div>
<script>
document.addEventListener('DOMContentLoaded', function() {
  var c = document.location.origin;
  const ic = document.getElementById('image-container');  
  var i = 1;
  
  var timing = 5000; // time between snapshots for multiple shots

  function loop() {
    ic.insertAdjacentHTML('beforeend','<img src="'+`${c}/capture?_cb=${Date.now()}`+'">')
    ic.insertAdjacentHTML('beforeend','<br>')
    ic.insertAdjacentHTML('beforeend',Date())
    ic.insertAdjacentHTML('beforeend','<br>')
    
    i = i + 1;
    if ( i <= 1 ) {             // change to 3,4,5 for more snapshots 
      window.setTimeout(loop, timing);
    }
  }
  loop();
  
})
</script><br>
</body>
</html>)rawliteral";

  //Serial.print(strlen(msg)); Serial.print(" ");

  sprintf(the_page, msg, devname, devname, vernum, strdate, use, tot, rssi, diskspeed, recording, PIRrecording, PIRenabled, EnableBOT, fname,
          frames_so_far, total_frames, skipped, capture_interval, ms_per_frame, avg_frame_wrt, capture_interval * total_frames / 1000,
          quality, framesize, repeat, xspeed, gray, localip, localip, localip, localip, localip, localip, localip, localip, localip, localip, localip);

  //Serial.println(strlen(the_page));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//
static esp_err_t index_handler(httpd_req_t *req) {
  Serial.print("http index, core ");  Serial.print(xPortGetCoreID());
  Serial.print(", priority = "); Serial.println(uxTaskPriorityGet(NULL));

  do_status();
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  //Serial.print("Default task prio: "); Serial.println(config.task_priority);
  //config.task_priority = 6;
  //config.core_id = 0;
  Serial.print("http task prio: "); Serial.println(config.task_priority);
  //Serial.print("http task core: "); Serial.println(config.core_id);

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


  httpd_uri_t file_pir_en = {
    .uri       = "/pir_enable",
    .method    = HTTP_GET,
    .handler   = pir_en_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t file_pir_dis = {
    .uri       = "/pir_disable",
    .method    = HTTP_GET,
    .handler   = pir_dis_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t file_bot_en = {
    .uri       = "/bot_enable",
    .method    = HTTP_GET,
    .handler   = bot_en_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t file_bot_dis = {
    .uri       = "/bot_disable",
    .method    = HTTP_GET,
    .handler   = bot_dis_handler,
    .user_ctx  = NULL
  };


  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &file_start);
    httpd_register_uri_handler(camera_httpd, &file_stop);
    httpd_register_uri_handler(camera_httpd, &file_pir_en);
    httpd_register_uri_handler(camera_httpd, &file_pir_dis);
    httpd_register_uri_handler(camera_httpd, &file_bot_en);
    httpd_register_uri_handler(camera_httpd, &file_bot_dis);
  }

  Serial.println("Camera http started");
}
