static const char devname[] = "desklens";         // name of your camera for mDNS, Router, and filenames

// https://sites.google.com/a/usapiens.com/opnode/time-zones  -- find your timezone here
#define TIMEZONE "GMT0BST,M3.5.0/01,M10.5.0/02"             // your timezone  -  this is GMT

// 1 for blink red led with every sd card write, at your frame rate
// 0 for blink only for skipping frames and SOS if camera or sd is broken
#define BlinkWithWrite 1

// EDIT ssid and password
const char* ssid = "jzjzjz";
const char* password = "jzjzjz";

// reboot startup parameters here

int Internet_Enabled = 1;          // set to 0 to shut off all internet activities - wifi, time, http, ftp, telegram
int DeepSleepPir  = 0;             // set to 1 to deepsleep between pir videos
int record_on_reboot = 1;          // set to 1 to record, or 0 to NOT record on reboot
int PIRpin = 13;                   // for active high pir or microwave etc
int PIRenabled = 0;                // 1 is PIR is enable on reboot, will only work if you are not recording

int MagicNumber = 314;             // change this if you are re-compiling and you dont want to use the ESPROM settings
int stream_interval = 333;         // milliseconds between frames delivered during the live stream - 333 is 3 fps

// here are 2 sets of startup parameters -- more down in the stop and restart webpage

// VGA 10 fps for 30 minutes, and repeat, play at real time

int  framesize = 6;                //  10 UXGA, 7 SVGA, 6 VGA, 5 CIF
int  repeat_config = 100;                 //  repaeat same movie this many times
int  xspeed = 1;                   //  playback speed - realtime is 1, or 300 means playpack 30 fps of frames at 10 second per frame ( 30 fps / 0.1 fps ) 
int  gray = 0;                     //  not gray
int  quality = 12;                 //  quality on the 10..50 subscale - 10 is good, 20 is grainy and smaller files, 12 is better in bright sunshine due to clipping
int  capture_interval = 100;       //  milli-seconds between frames
volatile int  total_frames_config = 18000;  //  how many frames - length of movie in ms is total_frames x capture_interval


// UXGA 1 frame every 10 seconds for 60 minutes, and repeat, play at 30 fps or 300 times speed
/*
int  framesize = 10;                //  10 UXGA, 7 SVGA, 6 VGA, 5 CIF
int  repeat_config = 300;                 //  repaeat same movie this many times
int  xspeed = 300;                   //  playback speed - realtime is 1, or 300 means playpack 30 fps of frames at 10 second per frames ( 30 fps / 0.1 fps ) 
int  gray = 0;                     //  not gray
int  quality = 6;                 //  quality on the 10..50 subscale - 10 is good, 20 is grainy and smaller files, 12 is better in bright sunshine due to clipping
int  capture_interval = 10000;       //  milli-seconds between frames
volatile int  total_frames_config = 360;  //  how many frames - length of movie is total_frames x capture_interval
*/

// enable the www.telegram.org BOT - it sends a text and and snapshot to you every time it starts a video
// https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
// I'm using the branch v1.2 from June 2020 - new master introduced late june, but not working for picture and captions, so my v1.2 mods included here
// You need to create a bot, and get its number BOTtoken, and then get your telegram number -- all free at telegram.org
// detailed instructions here https://randomnerdtutorials.com/telegram-control-esp32-esp8266-nodemcu-outputs/

RTC_DATA_ATTR int EnableBOT = 0;  // change to 1 for enable - must fill in your numbers below
#define BOTtoken  "9876543210:qwertyuiopasdfghjklzxcvbnmqwertyuio"   // get your own bot and id at telegram.org 
#define BOTme     "1234567890"
