# ESP32-CAM-Video-Recorder
Video Recorder for ESP32-CAM with http server for config and ftp server to download video

TimeLapseAvi

  ESP32-CAM Video Recorder

  This program records an MJPEG AVI video on the SD Card of an ESP32-CAM.
  
  by James Zahary July 20, 2019
     jamzah.plc@gmail.com

  https://github.com/jameszah/ESP32-CAM-Video-Recorder
  
  jameszah/ESP32-CAM-Video-Recorder is licensed under the GNU General Public License v3.0

vA1 for arduino-esp32 2.0.2  
v99 for arduino-esp32 1.0.6   
v98 for arduino-esp32 1.0.4   

## Update Jan 12, 2022 - esp32-arduino 2.0.2 and replace ftp with http file transfer

 Version A1 - Jan 1, 2022
  - switch from ftp to http file tranfer
     https://github.com/jameszah/ESPxWebFlMgr/tree/master/esp32_sd_file_manager
  - changes for esp32-arduino 1.06 -> 2.02
  - added link to telegram to download completed avi with one-click
  - delete low-voltage handler (serves no purpose)
  - get rid of touch (not much used i think)

The esp32-arduino 2.0.2 contains some nicer camera code that clears away little glitches that used to pop up.  
It is very clean now!

You can hardcode your ssid and password into settings.h, or put in a 1 character long ssid, and it will use WiFiManager to set your ssid and password.
If you want Telegram bot to work, you still have to hardcode them into settings.h


More commentary coming ... and refinements ... test it before you use it.

Also the newer quicker version (with fewer features) over at https://github.com/jameszah/ESP32-CAM-Video-Recorder-junior   

You can download the videos to your computer or phone with just a click on the file from your browser -- no need for an ftp program anymore!

<img src="./newfm.jpg">  

----


Note line 1413 -- this current version uses the WiFiManager for all WiFi setup.  Change this line if you want to use the previous version of hardcoded ssid and password.

https://github.com/jameszah/ESP32-CAM-Video-Recorder/blob/f90207b74c8ac247ef20f3cde9664b5f80844a02/v99/TimeLapseAvi99x.ino#L1413

## Update Sep 1, 2021 - small update for arduino-esp32 1.0.6

Minimal update for arduino-esp32 1.0.6.

The framesizes numbers changed which I had used on the http url command, so those were updated.   
Add the new frame size HD (1280x720).   
Some updates for the new SSL libraries to keep the telegram interface working.   
You need all the files in the v99 folder.  And Arduino esp32 board library set to 1.0.6.   
You can still use the previous version with the esp32 board library set to 1.0.4.   

<img src="./v99/v99.jpg">   

Lightly tested! 

More frequent updates with better code at these spin-offs of this original project.

https://github.com/jameszah/ESP32-CAM-Video-Recorder-junior   
This simple fast recording and streaming, with http just for information - no controls or ftp.

https://github.com/jameszah/ESP32-CAM-Video-Telegram   
This records and sends a short video to telegram on request or PIR event, using the telegram interface.

## Update Jun 10, 2021 - new program to check out

It records a video and sends it to your Telegram account - no SD card!
Now I just need to do some updates on this one!

https://github.com/jameszah/ESP32-CAM-Video-Telegram

## Update Mar 17, 2021 - arduino-esp32 ver 1.05

It would be best to use arduino-esp32 version 1.04 until I scan through this for update problems.  
1.05 was released in late Feb 2021, has has some new framesizes, aspect rations, and some changes in the WiFi, that need some code changes.

![image](https://user-images.githubusercontent.com/36938190/111499024-21b61000-8708-11eb-8e6a-5e35f411c694.png)

## Update Jan 06, 2021 - Version 98-WiFiMan

Just the same as Verion 98, but you can configure the ssid and password using WiFiManager.  
Use the normal WiFiManager procedure to set up ssid with 192.168.4.1 etc https://github.com/tzapu/WiFiManager
The only difference is that you have to reboot the esp32 after you have set the ssid/pass for it to work correctly.
Read more about it here https://github.com/tzapu/WiFiManager/issues/1184

And you must use the latest WiFiManager code which supports the ESP32 - currently 2.0.3-alpha which can be installed from Arduino IDE - Manage Libraries.
The WiFiManager could be enhanced to set parameters that cannot be set from hhtp, such as Telegram name and password, but haven't done that yet.


https://github.com/jameszah/ESP32-CAM-Video-Recorder/tree/master/v98-WiFiMan

## Update Sep 29, 2020 - Version 98

Added a feature to automatically delete your oldest day of videos when the SD reaches 90% full.

There are also some compile directives in the settings.h file to cut out entire sections of code like the ftp or telegram, if you need resouces to add your own stuff.


## Update Sep 13, 2020 

Check out a much simpler, somewhat faster version ESP32-CAM-Video_Recorder-junior.

It gets rid of wifi, http, time, ftp, telegram, pir, touch, .... and just records very fast -- pretty much at the full speed of the camera -- with a decent SD card and reasonable quality parameters.

The latest version of junior v07 adds back the streaming video, and series of photos over wifi, and will record at 6 fps UXGA and 24 fps SVGA, but otherwise remains quite uncomplicated.

https://github.com/jameszah/ESP32-CAM-Video-Recorder-junior

## Update Aug 29, 2020 Version 94 - live stream and filter bad jpegs

A couple new things.

I added some code to filter out bad jpegs.  This happens when you have high quality jpeg settings, and then take a movie in bright light, and some of the jpegs exceed the memory allocated, and you get a partial jpeg, which will screw up the index, and break an entire avi.  So there is now code to find those bad jpegs, and get a new frame that is good.  Read more about this here if interested.
https://github.com/espressif/esp32-camera/issues/162

This means it is safe to increase the jpeg quality to 7 (maybe 6?) from the standard setting of 10, or 12 in bright sun.  Lower number is higher quality.  Higher quality will mean more bytes, bigger files, and slower to write.  It is debatable if you want very high quality jpegs if they are flashing along at 24 frames per second.

Another new feature is a streaming video.  There is the single frame on the main status screen so you code see what your camera sees, but the streaming has been much requested to get a better look through the viewfinder, and maybe save the stream.

You set a parameter in settings.h of milli-seconds between updates, and then click on the /stream link on the main web page and it will stream to a new browser window.  Hit "back" to get back to the main page.

To save the video coming to the browser, do the following -- I tried to figure this out before but could not find it on google.  

1.  Click on the /stream on the web page, and you will see the moving picture.
2.  Right-click and select "save image as ...", type a name, and it will start saving the series of jpegs to a file.
3.  You have to stop the save, which as far as I can figure out, you have to select cancel from the menu of the ever increasing file.  But that will delete the file that you have been saving.  So the kludge is to "show in folder" to find the file, then copy the file, then do the cancel.  Then you have to rename that file to x.mjpeg, and then you can play it.  Most programs that play .avi files will not play the .mjpeg, but some will. See VLC below.  Very inelegant.

Another method is to use the VLC media player program.  
1.  Media - Open Network Stream - paste in the url such as http://192.168.1.90/stream
2. Then in the Play menu, switch it to Convert, click on "Dump Raw Input" and type filename ending in .mjpeg, and click Start.  When you are down saving, click the square stop button.  You will get a .mjpeg file which can be played in VLC and some other video or picture players.  An mjpeg does not understand time, so it will play the picture as quick as it can, and you have have to enter new information about the time between frames to get it to play as you want.

VLC seems to have controls there to convert the file to various formats like h264, wmv, etc. but I could not get that to work. 

If anyone has better ways to capture the live stream, or capture and covert it, please let us know in the comments.

I have not really studied how the streaming interacts with the recording.  But 3 fps streaming seems to work fine with 8 fps svga recording.  More work to be done there.  I reduced the http task priorty to below the picture-taking and avi-writing tasks, so the the streaming should slow down before the recording.  It seems the esp32 http server can only handle one client at a time, so you cannot stream to one browser and check the status on another browser. 

Another small feature is that you can now tell the recorder to /stop, and then /start with no parameters, and it will restart according to the original start parameters.  Not only the framesize and interval, but if you set it to record 100 videos, and it has 5 left, after a /stop and /start it will restart at 100 videos.

Also if you have some parameters in EPROM, and your are doing a new compile with changes and you do not want to use those EPROM parameters, there is a setting called MagicNumber in the settings.h.  Just change the MagicNumber, and the program will skip the EPROM settings and write your new settings from settings.h

I changed to default startup settings to VGA 10 frames per second, play at realtime, with no PIR or BOT.

<img src="./old/v94/web_v94.jpg">


## Update Jul 15, 2020 Version 89 - more new stuff

- added some code to save the configuration in eprom, so your device will always reboot to the state set in your most recent /start command.  The previous system just had a hardcoded configuration, and let you /stop and /start in a new configuration, but after a series of squirrel attacks, I moved to the eprom solution.  It always starts where it was, although a movie in progress during the squirrel attack will be lost. You can enable or disable the telegram bot or PIR sensor with a web-page click, and that too will be added to the eprom, so it reboots with or without bot updates or PIR control.  On reboot, it will check your eprom to see if there are parameters, and if so it will use them, or else use the hardcoded parameters from settings.h, and then write those paramters into eprom for next boot.  Change the eprom parameters with the /start command below.

- changed the movie start procedure a little.  It now takes a snapshot at beginning of movie, saves it as a .jpg, and if Telegram bot is enabled sends that same picture to your telegram so you can monitor the activity on your phone, or use that jpg if you want.  It does take several seconds to send the picture to telegram.  And only after that does it start recording the movie.  I attempted to do the telegram send after the movie started recording, but the telegram uses SSL security which needs 50k or more of heap, which would sometimes leave too little heap to work the SD card.  Also the reboot from deepsleep takes a couple seconds, then starting internet takes a couple seconds, then telegram bot takes a couple seconds, and then the movie starts.  So if you want fast-start, then don't use deepsleep, turn off internet, and obviously turn off telegram.

- changed reprogramming the camera a little.  It used to re-set frame-size, quality with every movie, but now it is just when you make changes and at reboot.

- I have switched to quality = 12 which is better for sunny days, and does noticably increase frame-rate.  I can usually get SVGA working at 10 frames per second, which is decent realtime video.

  - http://desklens.local/bot_enable
  - http://desklens.local/bot_disable
  - http://desklens.local/pir_enable
  - http://desklens.local/pir_disable
   
  - http://desklens.local/ ... look through viewfinder and see status
    
  - http://desklens.local/stop
  - http://desklens.local/start  ... with existing or default parameters
  - http://desklens.local/start?framesize=VGA&length=1800&interval=250&quality=10&repeat=100&speed=1&gray=0&pir=0&bot=1

    -  framesize can be UXGA, SVGA, VGA, CIF 
    -  length is length in seconds of the recording
    -  interval is the milli-seconds between frames 
    -  quality is a number 10..50 for the jpeg  - smaller number is higher quality with bigger and more detailed jpeg 
    -  repeat is a number of how many of the same recordings should be made
    -  speed is a factor to speed up realtime for a timelapse recording - 1 is realtime 
    -  gray is 1 for a grayscale video 
    -  pir is 1 to start a 15 seconds movie if pir sensor pulls high, and continue to 10 seconds after pir goes low
    -  bot is 1 will send the opening frame of movie to your Telegram App

  - ftp://desklens.local/ ... use ftp to download or erase old files ... username and password "esp" which is set in the code

The serial debug monitor shows you your ip address, and the web page uses the ip, rather than hoping that you can resolve mDNS everytime.

You just need the files from the /v89 folder. Edit the settings.h file for your wifi and startup configuration.

## Update Jun 30, 2020 Version 86 - some new features

<img src="./old/v86/v86.jpg">

 # Software   
- redo camera scheduler to reduce frame skips with slight delays between frames
- move more processing to separate priority tasks, and remove from idle loop()
- most tasks suspend waiting for events, rather than looping checking for events, ... except ftp which still loops wating for ftp requests
- added a sd card snapshot jpg at beginning of every movie
- added a telegram.org message with opening picture and info about diskspace and rssi to follow camera activity on your computer or phone
- added deepsleep feature to wake on PIR, and then deepsleep after movie is recorded
- added touch sensor on pin12 to enable/disable the pir sensor 
- added more careful setup of difficult pins 12, 13, and 4 - used for SD and re-used for PIR, Touch, and Blinding Disk-Active Light
- added brownout handler to close files on brownout, which didn't work, but at least I can deepsleep to prevent multiple brownout reboots.  (Inside a brownout handler, you have only 300ms and you cannot access wifi, sd, or flash, ... so cannot close files, or send message.)
- re-used pin 4 Blinding Disk-Active Light to blink gently at beginning of movie, and at a Touch - ironically, also turns on during Brownout ;-)
- added several functions to enable / disable pir or bot using internet
   
  - http://desklens.local/bot_enable
  - http://desklens.local/bot_disable
  - http://desklens.local/pir_enable
  - http://desklens.local/pir_disable
    
  - http://desklens.local/ ... look through viewfinder and see status
    
  - http://desklens.local/stop
  - http://desklens.local/start  ... with existing or default parameters
  - http://desklens.local/start?framesize=VGA&length=1800&interval=250&quality=10&repeat=100&speed=1&gray=0 
    - see below or settings.h
     
- moved many settings to a separate file "settings.h" so you edit that, rather than digging through the main file to set your wifi password, startup defaults, and enable/disable internet, pir, telegram, etc
- not super-elegant code ... still haven't written the avi writer into a nice library
- read comment on rtc_cntl.h below which may or may not be updated in the esp32 board library - links and info below 
- This includes a v1.2 (slight mods) of https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot for the Telegram stuff, plus the ftp and ArduCam mentioned below (major rewrite on ArduCam) 
- You just need the files from the /v86 folder. Edit the settings.h file for your wifi and starup configuration.
   
 # Hardware   
- to use PIR function, put an active high PIR or microwave on pin 13 with a 10k resistor (brown,black,orange) between pin 13 and PIR output to avoid antagonizing sd card
- to use Touch function, put a wire (with optional metal touch point) on pin 12 and touch it to enable/disable pir
- Blinding Disk-Active Light will give little blink during a touch, or when starting a recording
- red led on back with blink with every frame if you have that enabled in settings


Here is the "/" status page

<img src="./old/v86/v86-status.jpg">

And the /stop page to restart with new parameters 

<img src="./old/v86/v86-stop.jpg">

And what it looks like on your phone Telegram App

<img src="./old/v86/v86-Telegram.jpg">

## Update Feb 29, 2020 Sample Hardware for Microwave Camera

This is a bit of hardware to set up a camera recording to SD Card whenever something moves, as seen through a microwave device, and adds a led so you can see when the camera sees you!

<img src="./demo_fritz3.jpg">

<img src="./demo4.jpg">

<img src="./deom5.jpg">

## Update Feb 25, 2020 Sample Hardware for PIR Camera

This is a bit of hardware to set up a camera recording to SD Card whenever something moves.

<img src="./demo_fritz2.jpg">(my microUSB adapter is different from the one in the library)

<img src="./demo1.jpg">

<img src="./demo2.jpg">

<img src="./demo3.jpg">

## Update Feb 26, 2020 TimeLapseAvi60x.ino

New version # 60
- moved from 4 bit SD access to 1 bit, which frees up gpio pins 4, 12, and 13
- the Blinding Disk-Access Light is now OFF, without soldering or tape
- pin 12 can now be used for a PIR or switch - pull it high to start a 15 second video, continuing until 10 seconds after it goes low
- to use PIR or switch, the machine must not be recording, so edit "record_on_reboot" to 0, or use web to stop recording
- if you want no internet, just leave the fake ssid and password, and it will date your PIR recordings as 1970, but keep all your PIR clips timed and dated after 1970, which is better than just numbering them
- default startup is VGA, 10 fps, quailty 10, 30 minutes long, playback realtime, repeat 100 times, and it starts automatically after a reboot -- this is actually a little aggressive for my LEXAR 300x 32GB microSDHC UHS-I, which will usually keep up with 10 fps, but will sometimes start skipping
- BlinkWithWrite #define of 1 will blink the little red led with every frame, or #define 0 will just blink SOS if the camera or sd card are broken, or if you are skipping frames because sd cannot keep up
- also implemented dates and times in ftp which had been mysteriously missing.  The "Date Modified" on Windows should now be the correct time the file was completed on the ESP32.  The "Date Created" will be the time you ftp'ed it.  And the time in the file name is the time the file was started recording on the ESP32.
- also note that the file names of the PIR files are all just "L15" (the original creation of the file), but the files themselves will be as long as the PIR had activity.  I haven't updated the filename for the eventual length.
- you just need the 3 files from the /v60 folder
- I'll rewrite this intro with v60 instructions ... at some point

Also, someone did an instructables.com explanation and video about the Sep 15, 2019 version, which is not bad.  
I am refered to as "The Team"   :-)

https://www.instructables.com/id/Video-Capture-Using-the-ESP32-CAM-Board/

Other general advice about the ESP32-CAM.
- put a capacitor between the +5 and Ground to prevent the frequent "brownout" problems.  I saw 220 microFarad recommended somewhere, and it works good.
- keep the antenna part of the chip - the part with the squiggly line - above and away from your circuit board or any wires.  The internet speed will improve dramatically with just 1 or 2 mm of extra space.
 
## Update Feb 18, 2020
- Check out https://github.com/s60sc/ESP32-CAM_MJPEG2SD

It is a similar program, but makes a ".mjpeg" file rather than an ".avi" file.

It will also give you the live-stream through the camera to your 
browser, it will play the videos for you through the browser, and 
it will record based on a PIR or other sensor that grounds a pin.

And it solves the "Blinding Disk-Active Light" without any soldering or tape.

Reference by amirjak over in the "Issues" section.

I will be borrowing a few of these good ideas in days to come!


## Update Oct 15, 2019
- Make sure you are using esp32 board library 1.03 or better
- 1.02 has major wifi problems !!!


## Update Sep 15, 2019 TimeLapseAvi39x.ino
"work-in-progress"  I'm publishing this as a few people have been asking or working on this.
- program now uses both cores with core 0 taking pictures and queueing them for a separate task on core 1 writing them to the avi file on the sd card
- the loop() task on core 1 now just handles the ftp system and http server
- dropped fixed ip and switch to mDNS with name "desklens", which can be typed into browser, and also used as wifi name on router
- small change to ftp to cooperate with WinSCP program
- fixed bug so Windows would calulcate the correct length (time length) of avi
- when queue of frames gets full, it skips every other frame to try to catch up
- camera is re-configued when changing from UXGA <> VGA to allow for more buffers with the smaller frames
    
    You just need the 3 files in the /v23 folder for July version, which takes a picture and stores it
    or the 3 files in the /v39 folder for the current which adds the queueing system to get better
    frame rates, and keep recording if there is a small delay on file system.
 
##  Original July 2019 Intro

  Acknowlegements:

  1.  https://robotzero.one/time-lapse-esp32-cameras/
      Timelapse programs for ESP32-CAM version that sends snapshots of screen.
  2.  https://github.com/nailbuster/esp8266FTPServer
      ftp server (slightly modifed to get the directory function working)
  3.  https://github.com/ArduCAM/Arduino/tree/master/ArduCAM/examples/mini
      ArduCAM Mini demo (C)2017 LeeWeb: http://www.ArduCAM.com
      I copied the structure of the avi file, some calculations.

  The is Arduino code, with standard setup for ESP32-CAM
    - Board ESP32 Wrover Module
    - Partition Scheme Huge APP (3MB No OTA)
    
  This program records an AVI video on the SD Card of an ESP32-CAM.
  
  It will record realtime video at limited framerates, or timelapses with the full resolution of the ESP32-CAM.
  It is controlled by a web page it serves to stop and start recordings with many parameters, and look through the viewfinder.
  
  You can control framesize (UXGA, VGA, ...), quality, length, and fps to record, and fps to playback later, etc.

  There is also an ftp server to download the recordings to a PC.

  Instructions:

  The program uses a fixed IP of 192.168.1.222, so you can browse to it from your phone or computer.
  
  http://192.168.1.222/ -- this gives you the status of the recording in progress and lets you look through the viewfinder

  http://192.168.1.222/stop -- this stops the recording in progress and displays some sample commands to start new recordings

  ftp://192.168.1.222/ -- gives you the ftp server

  The ftp for esp32 seems to not be a full ftp.  The Chrome Browser and the Windows command line ftp's did not work with this, but
  the Raspbarian command line ftp works fine, and an old Windows ftp I have called CoffeeCup Free FTP also works, which is what I have been using.
  You can download at about 450 KB/s -- which is better than having to retreive the SD chip if you camera is up in a tree!
  
http://192.168.1.222/start?framesize=VGA&length=1800&interval=250&quality=10&repeat=100&speed=1&gray=0  
-- this is a sample to start a new recording

- framesize can be UXGA, SVGA, VGA, CIF (default VGA)
- length is length in seconds of the recording 0..3600 (default 1800)
- interval is the milli-seconds between frames (default 200)
- quality is a number 10..50 for the jpeg  - smaller number is higher quality with bigger and more detailed jpeg (default 10)
- repeat is a number of who many of the same recordings should be made (default 100)
- speed is a factor to speed up realtime for a timelapse recording - 1 is realtime (default 1)
- gray is 1 for a grayscale video (default 0 - color)

These factors have to be within the limit of the SD chip to receive the data.
For example, using a LEXAR 300x 32GB microSDHC UHS-I, the following works for me:

- UXGA quality 10,  2 fps (or interval of 500ms)
- SVGA quality 10,  5 fps (200ms)
- VGA  quality 10, 10 fps (100ms)
- CIF  quality 10, 20 fps (50ms)

If you increase fps, you might have to reduce quality or framesize to keep it from dropping frames as it writes all the data to the SD chip.

Also, other SD chips will be faster or slower.  I was using a SanDisk 16GB microSDHC "Up to 653X" - which was slower and more unpredictable than the LEXAR ???

Search for "zzz" to find places to modify the code for:
1.  Your wifi name and password
2.  Your preferred ip address (with default gateway, etc)
3.  Your Timezone for use in filenames
4.  Defaults for framesize, quality, ... and if the recording should start on reboot of the ESP32 without receiving a command
  
Sample videos produced by the program in the /sample-output folder -- it is not GoPro quality, but then GoPro's don't cost $10.

While not necessay, following is how I dealt with the "Flash" led on the front of the ESP32-CAM chip.

Picture below shows my solution to the "Flash" led, aka "the Blinding Disk-Active light".  The led turns on whenever you are are writing data to the SD chip, which is normally after you have taken the picture, so you don't need the flash on any more!  

Quick de-solder of the collector on the top of the J3Y transistor just above the led, then put in some tape to keep it clear -- you can solder it back later if you want to use it.

<img src="./de-solder.png">

<img src="./de-solder3.png">
