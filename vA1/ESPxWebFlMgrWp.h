// mods by James Zahary Dec 28, 2021 https://github.com/jameszah/ESPxWebFlMgr
//                      Jan 12, 2022 - adds dates/times to display
// based on https://github.com/holgerlembke/ESPxWebFlMgr

// inline guard. Did I mention that c/c++ is broken by design?
#ifndef ESPxWebFlMgrWp_h
#define ESPxWebFlMgrWp_h

// this file has been created by makeESPxWebFlMgrWp\do.cmd

//*****************************************************************************************************
static const char ESPxWebFlMgrWpindexpage[] PROGMEM = R"==x==(
<!DOCTYPE html>
<html  lang="en">
  <head>
    <title>FileManager</title>
    <meta charset="utf-8"/>
    <link rel="stylesheet" type="text/css" href="/bg.css">
    <link rel="stylesheet" type="text/css" href="/fm.css">
    <script src="/fm.js"></script>  
    <script src="/gzipper.js"></script>  
  </head>
  <body class="background">
    <div id="gc">
        <div class="o1">&nbsp;</div>
        <div class="o2">&nbsp;</div>
        <div class="o3" id="o3">&nbsp;</div>
        <div class="o4">&nbsp;</div>

        <div class="m1">
            <div class="s11">&nbsp;</div>
            <div class="s12">
            <div class="s13 background">&nbsp;</div>
            </div>
        </div>
        <div class="m2" ondrop="dropHandler(event);" ondragover="dragOverHandler(event);">
          File<br />
          Drop<br />
          Zone<br />
        </div>
        <div class="m3">
            <div class="s31">&nbsp;</div>
            <div class="s32">
            <div class="s33 background">&nbsp;</div>
            </div>
        </div>

        <div class="u1">&nbsp;</div>
        <div class="u2" onclick="downloadall();">Download all files</div>
        <div class="u3" id="msg">Loading...</div>
        <div class="u4">&nbsp;</div>
        <div class="c" id="fi">
          File list should appear here.
        </div>
    </div>
  </body>
</html>  

  )==x==";

static const char ESPxWebFlMgrWpjavascript[] PROGMEM = R"==x==(

function compressurlfile(source) {
  msgline("Fetching file...");
  var request = new XMLHttpRequest();
  request.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      var data = this.responseText;
      var gzip = require('gzip-js'), options = { level: 9, name: source, timestamp: parseInt(Date.now() / 1000, 10) };
      var out = gzip.zip(data, options);
      var bout = new Uint8Array(out); // out is 16 bits...

      msgline("Sending compressed file...");
      var sendback = new XMLHttpRequest();
      sendback.onreadystatechange = function () {
        var DONE = this.DONE || 4;
        if (this.readyState === DONE) {
          getfileinsert();
        }
      };
      sendback.open('POST', '/r');
      var formdata = new FormData();
      var blob = new Blob([bout], { type: "application/octet-binary" });
      formdata.append(source + '.gz', blob, source + '.gz');
      sendback.send(formdata);
    }
  };
  request.open('GET', source, true);
  request.send(null);
}

var subdir;

function getfileinsert() {
  msgline("Fetching files infos...");
  subdir = '/';
  var request = new XMLHttpRequest();
  request.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      var res = this.responseText.split("##");
      document.getElementById('fi').innerHTML = res[0];
      document.getElementById("o3").innerHTML = res[1];
      msgline("");
    }
  };
  request.open('GET', '/i', true);  
  request.send(null);
}

function getfileinsert2(strddd) {
  msgline("Fetching files infos...");
  subdir = strddd;
  var request = new XMLHttpRequest();
  request.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      var res = this.responseText.split("##");
      document.getElementById('fi').innerHTML = res[0];
      document.getElementById("o3").innerHTML = res[1];
      msgline("");
    }
  };
  request.open('GET', '/i?subdir=' + strddd, true);  // must send the subdir variable to get that folder //jz
  request.send(null);
}
function executecommand(command) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      getfileinsert2(subdir);
    }
  };
  xhr.open('GET', '/c?' + command, true);
  xhr.send(null);
}

function downloadfile(filename) {
  window.location.href = "/c?dwn=" + filename;
}

function opendirectory(filename) {
  
  getfileinsert2(filename);
}

function deletefile(filename) {
  if (confirm("Really delete " + filename)) {
    msgline("Refresh when done deleting..."); //jz msgline("Please wait. Delete in progress...");
    executecommand("del=" + filename);
  }
}

function renamefile(filename) {
  var newname = prompt("new name for " + filename, filename);
  if (newname != null) {
    msgline("Refresh when done renaming ..."); //jz msgline("Please wait. Rename in progress...");
    executecommand("ren=" + filename + "&new=" + newname);
  }
}

var editxhr;

function editfile(filename) {
  msgline("Please wait. Creating editor...");

  editxhr = new XMLHttpRequest();
  editxhr.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      document.getElementById('fi').innerHTML = editxhr.responseText;
      document.getElementById("o3").innerHTML = "Edit " + filename;
      msgline("");
    }
  };
  editxhr.open('GET', '/e?edit=' + filename, true);
  editxhr.send(null);
}

function sved(filename) {
  var content = document.getElementById('tect').value;
  // utf-8
  content = unescape(encodeURIComponent(content));

  var xhr = new XMLHttpRequest();

  xhr.open("POST", "/r", true);

  var boundary = '-----whatever';
  xhr.setRequestHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  var body = "" +
    '--' + boundary + '\r\n' +
    'Content-Disposition: form-data; name="uploadfile"; filename="' + filename + '"' + '\r\n' +
    'Content-Type: text/plain' + '\r\n' +
    '' + '\r\n' +
    content + '\r\n' +
    '--' + boundary + '--\r\n' +        // \r\n fixes upload delay in ESP8266WebServer
    '';

  // ajax does not do xhr.setRequestHeader("Content-length", body.length);

  xhr.onreadystatechange = function () {
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      getfileinsert();
    }
  }

  xhr.send(body);
}

function abed() {
  getfileinsert();
}

var uploaddone = true; // hlpr for multiple file uploads

function uploadFile(file, islast) {
  uploaddone = false;
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    // console.log(xhr.status);
    var DONE = this.DONE || 4;
    if (this.readyState === DONE) {
      if (islast) {
        getfileinsert2(subdir);
        console.log('last file');
      }
      uploaddone = true;
    }
  };
  xhr.open('POST', '/r');
  var formdata = new FormData();
  //var newname = subdir + '/' + file.name;  //jz didnt work, so do it in c++
  //file.name = newname;
  formdata.append('uploadfile', file);
  // not sure why, but with that the upload to esp32 is stable.
  formdata.append('dummy', 'dummy');
  xhr.send(formdata);
}

var globaldropfilelisthlpr = null; // read-only-list, no shift()
var transferitem = 0;
var uploadFileProzessorhndlr = null;

function uploadFileProzessor() {
    if (uploaddone) {
        if (transferitem==globaldropfilelisthlpr.length) {
            clearInterval(uploadFileProzessorhndlr);
        } else {
            var file = globaldropfilelisthlpr[transferitem];
            msgline("Please wait. Transferring file "+file.name+"...");
            console.log('process file ' + file.name);
            transferitem++;
            uploadFile(file,transferitem==globaldropfilelisthlpr.length);
        }
    }
}

/*
function dropHandlerALT(ev) {
  console.log('File(s) dropped');

  document.getElementById('msg').innerHTML = "Please wait. Transferring file...";

  // Prevent default behavior (Prevent file from being opened)
  ev.preventDefault();

  if (ev.dataTransfer.items) {
    // Use DataTransferItemList interface to access the file(s)
    for (var i = 0; i < ev.dataTransfer.items.length; i++) {
      // If dropped items aren't files, reject them
      if (ev.dataTransfer.items[i].kind === 'file') {
        var file = ev.dataTransfer.items[i].getAsFile();
        uploadFile(file);
        console.log('.1. file[' + i + '].name = ' + file.name);
      }
    }
  } else {
    // Use DataTransfer interface to access the file(s)
    for (var i = 0; i < ev.dataTransfer.files.length; i++) {
      console.log('.2. file[' + i + '].name = ' + ev.dataTransfer.files[i].name);
    }
  }
}
*/

function dropHandler(ev) {
  console.log('File(s) dropped');
  
  globaldropfilelisthlpr = ev.dataTransfer;
  transferitem = 0;

  msgline("Please wait. Transferring file...");

  // Prevent default behavior (Prevent file from being opened)
  ev.preventDefault();

  if (ev.dataTransfer.items) {
      var data = ev.dataTransfer;
      globaldropfilelisthlpr = data.files;
      uploadFileProzessorhndlr = setInterval(uploadFileProzessor,1000);
      console.log('Init upload list.');
  } else {
    // Use DataTransfer interface to access the file(s)
    for (var i = 0; i < ev.dataTransfer.files.length; i++) {
      console.log('.2. file[' + i + '].name = ' + ev.dataTransfer.files[i].name);
    }
  }
}

function dragOverHandler(ev) {
  console.log('File(s) in drop zone');

  // Prevent default behavior (Prevent file from being opened)
  ev.preventDefault();
}

function msgline(msg) {
  document.getElementById('msg').innerHTML = msg;
}

function downloadall() {
  msgline("Sending all files in one zip.");
  window.location.href = "/c?za=all";
  msgline("");
}

//->
window.onload = getfileinsert;

  )==x==";


//*****************************************************************************************************
static const char ESPxWebFlMgrWpcss[] PROGMEM = R"==g==(

div {
  margin: 1px;
  padding: 0px;
  font-family: 'Segoe UI', Verdana, sans-serif;
}

#gc {
  display: grid;
  grid-template-columns: 80px 25% auto 30px;
  grid-template-rows: 20px 30px auto 30px 20px;
  grid-template-areas: "o1 o2 o3 o4" "m1 c c c" "m2 c c c" "m3 c c c" "u1 u2 u3 u4";
}

.o1 {
  grid-area: o1;
  background-color: #9999CC;
  border-top-left-radius: 20px;
  margin-bottom: 0px;
}

.o2 {
  grid-area: o2;
  background-color: #9999FF;
  margin-bottom: 0px;
}

.o3 {
  grid-area: o3;
  background-color: #CC99CC;
  margin-bottom: 0px;
  white-space: nowrap;
}

.o4 {
  grid-area: o4;
  background-color: #CC6699;
  border-radius: 0 10px 10px 0;
  margin-bottom: 0px;
}

.m1 {
  grid-area: m1;
  margin-top: 0px;
  background-color: #9999CC;
  display: grid;
  grid-template-columns: 60px 20px;
  grid-template-rows: 20px;
  grid-template-areas: "s11 s12";  
}

.s12 {
  margin: 0px;
  background-color: #9999CC;
}

.s13 {
  margin: 0px;
  border-top-left-radius: 20px;
  height: 30px;
}

.m2 {
  display: flex;
  justify-content: center; 
  align-items: center;
  grid-area: m2;
  background-color: #CC6699;
  width: 60px;
}

.m3 {
  grid-area: m3;
  margin-bottom: 0px;
  background-color: #9999CC;
  display: grid;
  grid-template-columns: 60px 20px;
  grid-template-rows: 20px;
  grid-template-areas: "s31 s32";  
}

.s32 {
  margin: 0px;
  background-color: #9999CC;
}

.s33 {
  margin: 0px;
  border-bottom-left-radius: 20px;
  height: 30px;
}

.u1 {
  grid-area: u1;
  background-color: #9999CC;
  border-bottom-left-radius: 20px;
  margin-top: 0px;
}

.u2 {
  grid-area: u2;
  cursor: pointer;
  background-color: #CC6666;
  margin-top: 0px;
  padding-left: 10px;
  vertical-align: middle;
  font-size: 80%;
}

.u2:hover {
  background-color: #9999FF;
  color: white;
}

.u3 {
  grid-area: u3;
  padding-left: 10px;
  background-color: #FF9966;
  font-size: 80%;
  margin-top: 0px;
}

.u4 {
  grid-area: u4;
  background-color: #FF9900;
  border-radius: 0 10px 10px 0;
  margin-top: 0px;
}

.c {
  grid-area: c;
}

#fi .b {
  background-color: Transparent;
  border: 1px solid #9999FF;  
  border-radius: 1px;
  padding: 0px;
  width: 30px;
  cursor: pointer;
}

#fi .b:hover {
  background-color: #9999FF;
  color: white;
}

.cc {
  width: min-content;
  margin: 10px 0px;
}

.gc div {
  padding: 1px;  
}

.ccg {
    height: 1.5em;
  background-color: #A5A5FF;
}

.ccu {
    height: 1.5em;
  background-color: #FE9A00;
}

.ccd {
    height: 1.5em;
  background-color: #e8e2d8;
}

.ccl {
  border-radius: 5px 0 0 5px;
  cursor: pointer;
}

.ccl:hover {
  border-radius: 5px 0 0 5px;
  color: white;
  cursor: pointer;
}

.ccr {
  border-radius: 0 5px 5px 0;
}

.cct {
  text-align: right;
}

.ccz {
  text-align: right;
}

.gc {
  display: grid;
  grid-template-columns: repeat(4, max-content);  
}  
  )==g==";


#endif
