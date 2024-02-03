
#include <AsyncTelegram2.h>
#include <time.h>
#include <WiFi.h>    
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <WiFiClient.h>
#include <SSLClient.h>
#include <WiFiClientSecure.h>
#include "tg_certificate.h"
#include "esp_camera.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#define FILESYSTEM SPIFFS
#define CAMERA_MODEL_ESP_EYE 
#include "camera_pins.h"
#include <driver/i2s.h>
#include <SPIFFS.h>


#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
#define VIDEO_RECORDING 21  
#define TOTAL_FRAMES 22	   
#define AVIOFFSET 252

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid =  "";
const char* password = "";
String chatId = "";
WiFiClientSecure mClient;

//Telegram Parameters
const char* token = "";
TBMessage msg;
TBMessage lastValidUser;
AsyncTelegram2 myBot(mClient);


File idxfile;
File videoRecFile;
const char filename[] = "/recording16.avi";
const int headerSize = 44;
volatile bool videoPending = false;

uint32_t filePadding = 0;

unsigned long startms;
unsigned long movi_size = 0;
unsigned long jpeg_size = 0;

uint16_t frame_cnt = 0;
unsigned long elapsedms;

camera_fb_t * fb_q[1];

uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63};    // "00dc"
uint8_t avi1_buf[4] = {0x41, 0x56, 0x49, 0x31};    // "AVI1"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31};    // "idx1"


const byte avi_header[AVIOFFSET] =      // This is the AVI file header.  Some of these values get overwritten.
{
  0x52, 0x49, 0x46, 0x46,  // 0x00 "RIFF"
  0x00, 0x00, 0x00, 0x00,  // 0x04           Total file size less 8 bytes [gets updated later] 
  0x41, 0x56, 0x49, 0x20,  // 0x08 "AVI "

  0x4C, 0x49, 0x53, 0x54,  // 0x0C "LIST"
  0x44, 0x00, 0x00, 0x00,  // 0x10 68        Structure length
  0x68, 0x64, 0x72, 0x6C,  // 0x04 "hdrl"

  0x61, 0x76, 0x69, 0x68,  // 0x08 "avih"    fcc
  0x38, 0x00, 0x00, 0x00,  // 0x0C 56        Structure length
  0x40, 0x0D, 0x03, 0x00,  // 0x20 200000    dwMicroSecPerFrame     [based on FRAME_INTERVAL] 
  0x00, 0x00, 0x00, 0x00,  // 0x24           dwMaxBytesPerSec       [gets updated later] 
  0x00, 0x00, 0x00, 0x00,  // 0x28 0         dwPaddingGranularity
  0x10, 0x00, 0x00, 0x00,  // 0x2C 0x10      dwFlags - AVIF_HASINDEX set.
  0x00, 0x00, 0x00, 0x00,  // 0x30           dwTotalFrames          [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x34 0         dwInitialFrames (used for interleaved files only)
  0x01, 0x00, 0x00, 0x00,  // 0x38 1         dwStreams (just video)
  0x00, 0x00, 0x00, 0x00,  // 0x3C 0         dwSuggestedBufferSize
  0x20, 0x03, 0x00, 0x00,  // 0x40 800       dwWidth - 800 (S-VGA)  [based on FRAMESIZE] 
  0x58, 0x02, 0x00, 0x00,  // 0x44 600       dwHeight - 600 (S-VGA) [based on FRAMESIZE]      
  0x00, 0x00, 0x00, 0x00,  // 0x48           dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x4C           dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x50           dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x54           dwReserved

  0x4C, 0x49, 0x53, 0x54,  // 0x58 "LIST"
  0x84, 0x00, 0x00, 0x00,  // 0x5C 144
  0x73, 0x74, 0x72, 0x6C,  // 0x60 "strl"

  0x73, 0x74, 0x72, 0x68,  // 0x64 "strh"    Stream header
  0x30, 0x00, 0x00, 0x00,  // 0x68  48       Structure length
  0x76, 0x69, 0x64, 0x73,  // 0x6C "vids"    fccType - video stream
  0x4D, 0x4A, 0x50, 0x47,  // 0x70 "MJPG"    fccHandler - Codec
  0x00, 0x00, 0x00, 0x00,  // 0x74           dwFlags - not set
  0x00, 0x00,              // 0x78           wPriority - not set
  0x00, 0x00,              // 0x7A           wLanguage - not set
  0x00, 0x00, 0x00, 0x00,  // 0x7C           dwInitialFrames
  0x01, 0x00, 0x00, 0x00,  // 0x80 1         dwScale
  0x05, 0x00, 0x00, 0x00,  // 0x84 5         dwRate (frames per second)         [based on FRAME_INTERVAL]         
  0x00, 0x00, 0x00, 0x00,  // 0x88           dwStart               
  0x00, 0x00, 0x00, 0x00,  // 0x8C           dwLength (frame count)             [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x90           dwSuggestedBufferSize
  0x00, 0x00, 0x00, 0x00,  // 0x94           dwQuality
  0x00, 0x00, 0x00, 0x00,  // 0x98           dwSampleSize

  0x73, 0x74, 0x72, 0x66,  // 0x9C "strf"    Stream format header
  0x28, 0x00, 0x00, 0x00,  // 0xA0 40        Structure length
  0x28, 0x00, 0x00, 0x00,  // 0xA4 40        BITMAPINFOHEADER length (same as above)
  0x20, 0x03, 0x00, 0x00,  // 0xA8 800       Width                  [based on FRAMESIZE] 
  0x58, 0x02, 0x00, 0x00,  // 0xAC 600       Height                 [based on FRAMESIZE] 
  0x01, 0x00,              // 0xB0 1         Planes  
  0x18, 0x00,              // 0xB2 24        Bit count (bit depth once uncompressed)                   
  0x4D, 0x4A, 0x50, 0x47,  // 0xB4 "MJPG"    Compression 
  0x00, 0x00, 0x04, 0x00,  // 0xB8 262144    
  0x00, 0x00, 0x00, 0x00,  // 0xBC           X pixels per metre 
  0x00, 0x00, 0x00, 0x00,  // 0xC0           Y pixels per metre
  0x00, 0x00, 0x00, 0x00,  // 0xC4           Colour indices used  
  0x00, 0x00, 0x00, 0x00,  // 0xC8           Colours considered important (0 all important).


  0x49, 0x4E, 0x46, 0x4F, // 0xCB  Info
  0x1C, 0x00, 0x00, 0x00, // 0xD0 28         Structure length
  0x70, 0x61, 0x75, 0x6c, // 0xD4 
  0x2e, 0x77, 0x2e, 0x69, // 0xD8 
  0x62, 0x62, 0x6f, 0x74, // 0xDC 
  0x73, 0x6f, 0x6e, 0x40, // 0xE0 
  0x67, 0x6d, 0x61, 0x69, // 0xE4 
  0x6c, 0x2e, 0x63, 0x6f, // 0xE8 
  0x6d, 0x00, 0x00, 0x00, // 0xEC 

  0x4C, 0x49, 0x53, 0x54, // 0xF0 "LIST"
  0x00, 0x00, 0x00, 0x00, // 0xF4           Total size of frames        [gets updated later]
  0x6D, 0x6F, 0x76, 0x69  // 0xF8 "movi"
};



void resetVideoResources(void){
  filePadding = 0;
  startms;
  movi_size = 0;
  jpeg_size = 0;

   if(SPIFFS.remove(filename)){
        Serial.println("Previous Video Deleted!");
  }
   if(SPIFFS.remove("/idx.txt")){
        Serial.println("/idx.txt Deleted!");
  }
   videoRecFile = SPIFFS.open(filename, FILE_WRITE);
   writeAviHeader(videoRecFile);
   videoRecFile.close();  
}

void SPIFFSInit(){

  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS initialisation failed!");
    while(1) yield();
  }
  SPIFFS.format();

  Serial.println("File  test ");

  videoRecFile = SPIFFS.open(filename, FILE_WRITE);
  if(!videoRecFile){
    Serial.println("videoRecFile is not available!");
  }

  writeAviHeader(videoRecFile);
  videoRecFile.close();  
  
}
void send_the_video(TBMessage &msg,
                  AsyncTelegram2::DocumentType fileType,
                  const char* filename,
                  const char* caption = nullptr ) {

     File file3 = FILESYSTEM.open("/recording16.avi", "r");
        Serial.println("Size of Video is  "+ file3.size());
  if (file3) {
    myBot.sendDocument(msg, file3,file3.size(), fileType,"/recording16.avi", caption);
    file3.close();
  }else{
    Serial.println("ERROR SENDING VIDEO");
  }
}


//
// Writes an uint32_t in Big Endian at current file position
//
static void inline print_quartet(unsigned long i, File  fd)
{
  uint8_t x[1];

  x[0] = i % 0x100;
  size_t i1_err = fd.write(x , 1);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i2_err = fd.write(x , 1);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i3_err = fd.write(x , 1);
  i = i >> 8;  x[0] = i % 0x100;
  size_t i4_err =fd.write(x , 1);
}
void writeAviHeader(File avifile) {
  int i = 0;
  uint8_t buf[512];

  for ( i = 0; i < AVIOFFSET; i++)
  {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  size_t err = avifile.write(buf,AVIOFFSET);
}

void writeAviFrame(camera_fb_t * fb_q2) {

  jpeg_size = fb_q2->len;
  movi_size += jpeg_size;

  // Calculate if a padding byte is required (frame chunks need to be an even number of bytes).
  uint8_t paddingByte = jpeg_size & 0x00000001;
  uint32_t frameSize =jpeg_size + paddingByte;  

  videoRecFile.write(dc_buf,4);
  print_quartet(frameSize, videoRecFile);
  //Write Frame
  videoRecFile.write(fb_q2->buf,fb_q2->len);

  videoRecFile.seek((frameSize - 6) * -1,SeekEnd);
  videoRecFile.write(avi1_buf,4);
 // unsigned long fileSizeIs = videoRecFile.size(); cambio abajo mal

  videoRecFile.seek(0,SeekEnd);

  if(paddingByte > 0)
  {
    videoRecFile.write(zero_buf,paddingByte);
  }

  uint32_t frameOffset = videoRecFile.size() - AVIOFFSET;
  print_quartet(frameOffset,idxfile);
  print_quartet((frameSize - paddingByte),idxfile);

  filePadding+=paddingByte;
}

// ----------------------------------------------------------------------------------
//  Add the idx1 (frame index) chunk to the end of the file.  
// ----------------------------------------------------------------------------------

void writeIdx1Chunk()
{
  // The idx1 chunk consists of:
  // +--- 1 per file ----------------------------------------------------------------+ 
  // | fcc         FOURCC 'idx1'                                                     |
  // | cb          DWORD  length not including first 8 bytes                         |
  // | +--- 1 per frame -----------------------------------------------------------+ |
  // | | dwChunkId DWORD  '00dc' StreamID = 00, Type = dc (compressed video frame) | |
  // | | dwFlags   DWORD  '0000'  dwFlags - none set                               | | 
  // | | dwOffset  DWORD   Offset from movi for this frame                         | |
  // | | dwSize    DWORD   Size of this frame                                      | |
  // | +---------------------------------------------------------------------------+ | 
  // +-------------------------------------------------------------------------------+  
  // The offset & size of each frame are read from the idx1.tmp file that we created
  // earlier when adding each frame to the main file.
  // 

  // Write the idx1 header to the file
   videoRecFile.write(idx1_buf,4);


  // Write the chunk size to the file.
  print_quartet((uint32_t)frame_cnt * 16, videoRecFile);  

  idxfile.close();
  idxfile = SPIFFS.open("/idx.txt","r");

  // We need to read the idx1 file back in, so move the read head to the start of the idx1 file.
  idxfile.seek(0,SeekSet);
  
  // For each frame, write a sub chunk to the AVI file (offset & size are read from the idx file)
  uint8_t readBuffer[8];
  for (uint32_t x = 0; x < frame_cnt; x++)
  {
    // Read the offset & size from the idx file.
     idxfile.read(readBuffer, 8);
    
    // Write the subchunk header 00dc
     videoRecFile.write(dc_buf,  4);

    // Write the subchunk flags
     videoRecFile.write(zero_buf, 4);

    // Write the offset & size
     videoRecFile.write(readBuffer ,8);

  }
  // Close the idx1 file.
   idxfile.close();
}
void writeAviFooter(File avifile,unsigned long sizeFile) {
    unsigned long current_end = 0;

    current_end =  sizeFile;
    frame_cnt = TOTAL_FRAMES;

    elapsedms = ((millis()) - startms);
    unsigned long fileDuration = (elapsedms) / 1000UL;

   float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * 1;

   printf("fRealFPS  = %f \n",fRealFPS);


    //Modify the MJPEG header from the beginning of the file, overwriting various placeholders
    avifile.seek( 4,SeekSet);
    print_quartet(movi_size + AVIOFFSET - 8  + 16 * frame_cnt + 8 * frame_cnt + filePadding + 8, avifile);


    unsigned long max_bytes_per_sec = movi_size / fileDuration;

    avifile.seek(0x24,SeekSet);
    print_quartet(max_bytes_per_sec, avifile);

    avifile.seek( 0x30,SeekSet);
    print_quartet(frame_cnt, avifile);

    avifile.seek( 0x8c,SeekSet);
    print_quartet(frame_cnt, avifile); 


    avifile.seek( 0xF4,SeekSet);
    print_quartet(movi_size + frame_cnt * 8 + filePadding, avifile);

    avifile.seek(0,SeekEnd);

    // Add the idx1 section to the end of the AVI file
    writeIdx1Chunk();
    avifile.close();

}

void captureFramesAndCreateVideo(void *arg) {
    idxfile = SPIFFS.open("/idx.txt",FILE_APPEND);
    videoRecFile = SPIFFS.open(filename, FILE_APPEND);
    digitalWrite(VIDEO_RECORDING, HIGH);

    long inicio = millis();
    startms = millis();

      for (int i = 0; i < TOTAL_FRAMES; i++) { 
        fb_q[0] = esp_camera_fb_get();
        if (!(fb_q[0])){
              Serial.println("Camera capture failed in video");
        }else{
              Serial.println("Camera capture ok in Video");
        }
          writeAviFrame(fb_q[0]);
          esp_camera_fb_return(fb_q[0]);
      }
    
    unsigned long sizeF = videoRecFile.size();
    writeAviFooter(videoRecFile,sizeF);
    videoPending = true;
    digitalWrite(VIDEO_RECORDING, LOW);
    vTaskDelete(NULL);  
}



size_t sendPicture(TBMessage& msg) {

  // Take picture with Camera and send to Telegram
  digitalWrite(LED_GPIO_NUM, HIGH);
  delay(100);
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(LED_GPIO_NUM, LOW);
  if (!fb) {
    Serial.println("Camera capture failed");
    return 0;
  }
  size_t len = fb->len;
  myBot.sendPhoto(msg, fb->buf, fb->len);

  // Clear buffer
  esp_camera_fb_return(fb);
  return len;
}

void camaraConfig(){
  camera_config_t config;
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_SVGA;//800x600 Resolution
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;//CAMERA_FB_IN_PSRAM;                               // for vga and uxga
  config.jpeg_quality = 50;
  config.fb_count =2;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 50;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_SVGA;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }
#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
     return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_SVGA);

  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif
}

void sendDocument(TBMessage &msg,
                  AsyncTelegram2::DocumentType fileType,
                  const char* filename,
                  const char* caption = nullptr )
  {

  File file3 = FILESYSTEM.open("/recording16.avi", "r");
  if (file3) {
    myBot.sendDocument(msg, file3, file3.size(), fileType, file3.name(), caption);
    file3.close();
  }
  else {
    Serial.println("ERROR SENDING THE DOCUMENT");
  }
}

void setup() {

  Serial.begin(9600);
  SPIFFSInit();
  pinMode(LED_GPIO_NUM, OUTPUT);
  pinMode(VIDEO_RECORDING, OUTPUT);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Sync time with NTP
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  mClient.setCACert(telegram_cert);
  myBot.setUpdateTime(2000);
  myBot.setTelegramToken(token);
  myBot.begin() ? Serial.println("OK") : Serial.println("NOK");

  camaraConfig();
}


void loop(){

    if (myBot.getNewMessage(msg)) {
      String msgText = msg.text;
      String chat_id = String(msg.chatId);
      if (chat_id != chatId){
        myBot.sendMessage(msg, "Unauthorized user"); 
      }else{
          lastValidUser = msg;
          if (msgText.equals("/photo")) {      
             delay(10); 
             sendPicture(msg);
          }else if (msgText.equals("/video")) {  
          myBot.sendMessage(msg, "Video Recording Started");
          xTaskCreate(captureFramesAndCreateVideo, "captureFramesAndCreateVideo", 10000, NULL, 1, NULL); 
        }else if (msgText.equals("/start")) {     
            String from_name = msg.sender.firstName;
            String welcome = "Welcome 😄👋 , " + from_name + "\n";
            welcome += "Use following commands to control TWL iCAM \n";
            welcome += "/photo : Take Photo\n";
            welcome += "/video : Video Record\n";
            myBot.sendMessage(msg, welcome);            
        }    
      }
  }
  if(videoPending){
    videoPending = false;
    send_the_video(lastValidUser, AsyncTelegram2::DocumentType::VIDEO, "" );
    resetVideoResources();
  }
}

