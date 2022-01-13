// mods by James Zahary Dec 28, 2021 https://github.com/jameszah/ESPxWebFlMgr
//                      Jan 12, 2022 - adds dates/times to display
// based on https://github.com/holgerlembke/ESPxWebFlMgr

// inline guard. Did I mention that c/c++ is broken by design?
#ifndef ESPxWebFlMgr_h
#define ESPxWebFlMgr_h

/*
  Changes
    V1.03
     x removed all SPIFFS from ESP32 version, switched fully to LittleFS
     x fixed rename+delete for ESP32+LittleFS (added "/")

    V1.02
     x fixed the way to select the file system by conditional defines

    V1.01
     + added file name progress while uploading
     x fixed error in ZIP file structure (zip.bitflags needs a flag)

    V1.00
     + out of V0.9998...
     + ESP8266: LittleFS is default 
     + javascript: added "msgline();"
     + javascript: added "Loading..." as a not-working-hint to show that Javascript is disabled
     + cleaning up the "/"-stuff (from SPIFF with leading "/" to LittleFS without...)
     + Warning: esp8266 2.7.4 has an error in mime::getContentType(path) for .TXT. Fix line 65 is { kTxtSuffix, kTxt },
     + review of "edit file", moved some stuff to ESPxWebFlMgrWpF.h
*/

#include <Arduino.h>
#include <inttypes.h>

// file system default for esp8266 is LittleFS, for ESP32 it is SPIFFS (no time to check...)

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <FS.h>
  //
  #include <LittleFS.h>
  #define ESPxWebFlMgr_FileSystem LittleFS
  /*
  #include <SPIFFS.h>
  #define ESPxWebFlMgr_FileSystem SPIFFS
  */
#endif

#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <FS.h>
  #include <SD_MMC.h> //jz #include <LittleFS.h>
  #define ESPxWebFlMgr_FileSystem SD_MMC //jz #define ESPxWebFlMgr_FileSystem LittleFS
#endif


#ifndef ESPxWebFlMgr_FileSystem
#pragma message ("ESPxWebFlMgr_FileSystem not defined.")
#endif

/* undefine this to save about 10k code space.
   it requires to put the files from "<library>/filemanager" into the FS. No free lunch.
*/
#define fileManagerServerStaticsInternally

// will show the Edit-Button for every file type, even binary and such.
//#define fileManagerEditEverything

class ESPxWebFlMgr {
  private:
    word _Port ;
#ifdef ESP8266
    ESP8266WebServer * fileManager = NULL;
#endif
#ifdef ESP32
    WebServer * fileManager = NULL;
#endif
    bool _ViewSysFiles = false;
    String _SysFileStartPattern = "/.";
    File fsUploadFile;
    String _backgroundColor = "black";

    void fileManagerNotFound(void);
    String dispIntDotted(size_t i); 
    String dispFileString(size_t fs); 
    String CheckFileNameLengthLimit(String fn);

    // the webpage
    void fileManagerIndexpage(void);
    void fileManagerJS(void);
    void fileManagerCSS(void);
    void fileManagerGetBackGround(void);

    // javascript xmlhttp includes
    String colorline(int i);
    String escapeHTMLcontent(String html);
    void fileManagerFileListInsert(void);
    void fileManagerFileEditorInsert(void);
    boolean allowAccessToThisFile(const String filename);
    void fileManagerCommandExecutor(void);
    void fileManagerReceiverOK(void);
    void fileManagerReceiver(void);

    // Zip-File uncompressed/stored
    void getAllFilesInOneZIP(void);
    int WriteChunk(const char* b, size_t l);

    // helper: fs.h from esp32 and esp8266 don't have a compatible solution
    // for getting a file list from a directory
#ifdef ESP32
#define Dir File
#endif
    File nextFile(Dir &dir);
    File firstFile(Dir &dir);
    // and not to get this data about usage...
    size_t totalBytes(void);
    size_t usedBytes(void);

  public:
    ESPxWebFlMgr(word port);
    virtual ~ESPxWebFlMgr();

    void begin();
    void end();
    virtual void handleClient();

    // This must be called before the webpage is loaded in the browser...
    // must be a valid css color name, see https://en.wikipedia.org/wiki/Web_colors
    void setBackGroundColor(const String backgroundColor);

    void setViewSysFiles(bool vsf);
    bool getViewSysFiles(void);

    void setSysFileStartPattern(String sfsp);
    String getSysFileStartPattern(void);
};

#endif

/*
      History

        -- 2019-07-07
           + Renamed to ESPxWebFlMgr and made it work with esp32 and esp8266
           + separated file manager web page, "build script" to generate it

        -- 2019-07-06
           + "Download all files" creates a zip file from all files and downloads it
           + option to set background color
           - html5 fixes

        -- 2019-07-03
           + Public Release on https://github.com/holgerlembke/ESP8266WebFlMgr


      Things to do

        ?? unify file system access for SPIFFS, LittleFS and SDFS

*/
