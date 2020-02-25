# ESP32-CAM-Video-Recorder
Video Recorder for ESP32-CAM with http server for config and ftp server to download video

TimeLapseAvi

  ESP32-CAM Video Recorder

  This program records an MJPEG AVI video on the SD Card of an ESP32-CAM.
  
  by James Zahary July 20, 2019
     jamzah.plc@gmail.com

  https://github.com/jameszah/ESP32-CAM-Video-Recorder
    jameszah/ESP32-CAM-Video-Recorder is licensed under the
    GNU General Public License v3.0


##Update Feb 24, 2020 TimeLapseAvi59x.ino

New version # 59
- moved from 4 bit SD access to 1 bit, which frees up gpio pins
- the Blinding Disk-Access Light is now OFF, without soldering or tape
- pin 12 is now a PIR or switch - hold it high to start a 15 second video, continuing until 10 seconds after it goes low
- to use PIR or switch, the machine must not be recording, so edit "record_on_reboot" to 0, or use web to stop recording
- if you want no internet, just leave the fake ssid and password, and it will mark your PIR recording dated 1970
- I'll rewrite this intro with v59 instructions ... at some point
 
##Update Feb 18, 2020
- Check out https://github.com/s60sc/ESP32-CAM_MJPEG2SD

It is a similar program, but makes a ".mjpeg" file rather than an ".avi" file.

It will also give you the live-stream through the camera on your 
browser, it will play the videos for you through the browser, and 
it will record based on a PIR or other sensor that grounds a pin.

And it solves the "Blinding Disk-Active Light" without any soldering or tape.

Reference by amirjak over in the "Issues" section.

I will be borrowing a few of these good ideas in days to come!


Link here: https://github.com/s60sc/ESP32-CAM_MJPEG2SD

##Update Oct 15, 2019
- Make sure you are using esp32 board library 1.03 or better
- 1.02 has major wifi problems !!!


##Update Sep 15, 2019 TimeLapseAvi39x.ino
  - work-in-progress
  - I'm publishing this as a few people have been asking or working on this
  
  - program now uses both cores with core 0 taking pictures and queueing them
    for a separate task on core 1 writing them to the avi file on the sd card
  - the loop() task on core 1 now just handles the ftp system and http server
  - dropped fixed ip and switch to mDNS with name "desklens", which can be typed into
    browser, and also used as wifi name on router
  - small change to ftp to cooperate with WinSCP program
  - fixed bug so Windows would calulcate the correct length (time length) of avi 
  - when queue of frames gets full, it slips every other frame to try to catch up
  - camera is re-configued when changing from UXGA <> VGA to allow for more buffers 
    with the smaller frames
    
    You just need the 3 files in the /v23 folder for July version, which takes a picture and stores it
    or the 3 files in the /v39 folder for the current which adds the queueing system to get better
    frame rates, and keep recording if there is a small delay on file system.
 
##Original July 2019 Intro

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
  
http://192.168.1.222/start?framesize=VGA&length=1800&interval=250&quality=10&repeat=100&speed=1&gray=0  -- this is a sample to start a new recording

- framesize can be UXGA, SVGA, VGA, CIF (default VGA)
- length is length in seconds of the recording 0..3600 (default 1800)
- interval is the milli-seconds between frames (default 200)
- quality is a number 5..50 for the jpeg  - smaller number is higher quality with bigger and more detailed jpeg (default 10)
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

Also, other SD chips will be faster or slower.  
I was using a SanDisk 16GB microSDHC "Up to 653X" - which was slower and more unpredictable than the LEXAR ???

Search for "zzz" to find places to modify the code for:
1.  Your wifi name and password
2.  Your preferred ip address (with default gateway, etc)
3.  Your Timezone for use in filenames
4.  Defaults for framesize, quality, ... and if the recording should start on reboot of the ESP32 without receiving a command
  
Sample videos produced by the program in the /sample-output folder -- it is not GoPro quality, but then GoPro's don't cost $10.

While not necessay, following is how I dealt with the "Flash" led on the front of the ESP32-CAm chip.

Picture below shows my solution to the "Flash" led, aka "the Blinding Disk-Active light".  The led turns on whenever you are are writing data to the SD chip, which is normally after you have taken the picture, so you don't need the flash on any more!  

Quick de-solder of the collector on the top of the J3Y transistor just above the led, then put in some tape to keep it clear -- you can solder it back later if you want to use it.

<img src="./de-solder.png">

<img src="./de-solder3.png">
