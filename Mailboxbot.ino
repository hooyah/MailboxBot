// ESP32 Mailboxbot
//
// uses ESP32 Dev Module, Flash enabled, Minimal SPIFFS (for OTA updates)

#include "config.h"
#include "cam.h"
#include "battery.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "esp_task_wdt.h"
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory
#include <WiFi.h>
#include <WiFiClient.h> 
#include "SD_MMC.h"
#include "fhu_ftp_client.h"
#include "update_fw.h"


// you can pass a FTP timeout and debug mode on the last 2 arguments
//fhu_FTPClient ftp (FTP_SERVER,FTP_USER,FTP_PASS, 5000, 1);
fhu_FTPClient ftp (FTP_SERVER,FTP_USER,FTP_PASS, 10000, 1);


// define the number of bytes you want to access
#define EEPROM_SIZE 1
int pictureNumber = 0;

// times to wake up during the day, specified in minutes from 0:00 (24hformat, eg. 1:30pm => 13:00 => 13*60+30, or TIME_IN_M( 13,30)
const int wake_up_times[] = { TIME_IN_M( 12,00 ), /*TIME_IN_M( 15,00),*/ TIME_IN_M( 18,00) };
const int UTC_offset = TIME_IN_M(13,00);  // timeoffset from UTC to your timezone, in minutes 

// The rtc has a lot of drift over long time periods. In my case, for an 18h sleep interval it is almost 40 minutes off.
// This depends on internal and external factors (ie temperature) so will never be perfect, but just get it in the ball park.
const float rtc_compensation = 1080.0 / 1050.0;  // real time clock multiplication factor for sleep time

void setup() {

  bool fw_updated = false;    // has the firmware been updated along the way?
  int  next_wakeup = 6*60;    // next wakeup in minutes (default in case we cannot get the current time)

  unsigned long start_time = millis();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  // activate the watchdog timer in case anything bad happens in the field
  esp_task_wdt_init(120, true);   // reboot after a 120s lockup
  esp_task_wdt_add(NULL);         // add current thread to the WDT watch list
  
  // power up the camera
  rtc_gpio_hold_dis(GPIO_NUM_33); // undo the pin holds from a previous run
  rtc_gpio_hold_dis(GPIO_NUM_4);           
  pinMode(GPIO_NUM_33, OUTPUT);
  pinMode(GPIO_NUM_4, OUTPUT); // flash led
  digitalWrite(GPIO_NUM_33, 1); // power down the camera for now
  
  Serial.begin(115200);
  

  //#######################
  // Check the battery voltage
  float batv = measure_battery_voltage();


  //#########################
  // Take a picture
  
  camera_fb_t *fb = cam_capture();
  if(!fb)
    Serial.println("Camera capture failed");

  
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1;


  //#########################
  // Connect to Wifi

  //if(fb)
  {
    WiFi.begin( WIFI_SSID, WIFI_PASS );
    Serial.print("Connecting Wifi");
    for(int c = 0; c < 100 && WiFi.status() != WL_CONNECTED; ++c) {
        Serial.print(".");
        delay(500);
    }
  
    // ########################
    // upload image to ftp
    
    if(WiFi.status() == WL_CONNECTED)
    {
      Serial.println("");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
  
      ftp.OpenConnection();
      String log;
      if(fb)
      {
        // Create the new file and send the image
        ftp.InitFile("Type I");
        String filename = "mailbox." + String(pictureNumber) +".jpg";
        ftp.NewFile(filename.c_str());
        ftp.WriteData( fb->buf, fb->len );
        ftp.CloseFile();
        esp_camera_fb_return(fb); 

        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  
        // get time from file written
        char ftime[128];
        memset(ftime, 0, sizeof(ftime));
        ftp.GetLastModifiedTime(filename.c_str(), ftime);
        String timestamp(ftime);
        int tm_hrs = atoi(timestamp.substring(8, 10).c_str());
        int tm_mins= atoi(timestamp.substring(10, 12).c_str());
  
        log += timestamp.substring(0, 4)+"-"+timestamp.substring(4, 6)+"-"+timestamp.substring(6, 8)+
                     " " + timestamp.substring(8, 10)+":"+timestamp.substring(10, 12) +
                     "  -  " + filename + " - bat:" + String(batv, 2)+"V (wakeup:0x" + String(wakeup_reason, HEX) + ")\n";

        // calculate how long we will sleep for this time around
        if(ftime[0]) // only if we got a valid time
        {
          int tm_now = (TIME_IN_M(tm_hrs, tm_mins)+UTC_offset)%(24*60);
          int tm_next = wake_up_times[0] + 24*60;
          for(uint t=0; t < (sizeof(wake_up_times)/sizeof(wake_up_times[0])); t++) 
            if( wake_up_times[t] > tm_now + 30) { // find the next wake time that's at least 30 minutes away 
              tm_next = wake_up_times[t];
              break;
            }

          next_wakeup = tm_next - tm_now;
          log += "sleeping for " + String(next_wakeup/60) + "h " + String(next_wakeup%60) + "m\n";
        }  
        
        //Serial.printf("filetime=%s\n", log.c_str());
        EEPROM.write(0, pictureNumber);
        EEPROM.commit();
      }
      else
        log += "couldn't take picture\n";
      


      // --------------------------
      // check for fw update
      
      if(SD_MMC.begin("/sdcard", true))  // one wire interface
      {
        // fix LED pin
        pinMode(GPIO_NUM_4, OUTPUT);
        digitalWrite(GPIO_NUM_4, LOW);

        // check ftp for updated fw
        if(SD_MMC.cardType() != CARD_NONE)
        {
          long szRemote = ftp.GetFileSize(FIRWARE_FILENAME);
          if(szRemote > 0) // file present on ftp
          {
            // delete file on sd if it exists
            SD_MMC.remove("/" FIRWARE_FILENAME);
            File outfile = SD_MMC.open("/" FIRWARE_FILENAME, FILE_WRITE);
            if(outfile)
            {    
              ftp.InitFile("Type I");
              bool res = ftp.DownloadFileToStream(FIRWARE_FILENAME, outfile);
              outfile.flush(); // contrary to popular opinion, apparently this needs to be flushed before size() returns anything but 0
              long szLocal = outfile.size(); 
              outfile.close();
              Serial.printf("download.  %d Bytes remote - %d Bytes local\n", szRemote, szLocal);
              if(szLocal==szRemote) {
                log += "new firmware downloaded\n";
                ftp.DeleteFile(FIRWARE_FILENAME);   // delete remote image
              }else {
                log += "firmware download failed\n";
                SD_MMC.remove("/" FIRWARE_FILENAME);    // don't leave this around if it's not the correct size        
              }
            }
          }

          // try to flash firmware from SD, nothing happens if there is no file          
          fw_updated = update_from_sd(SD_MMC);
          if(fw_updated)
              log += "firmware successfully flashed\n";
        }
           
      }
      else
        log += "card mount failed\n";

      SD_MMC.end();

      // append to log
      ftp.InitFile("Type A");
      ftp.AppendFile("log.txt");
      //ftp.Write(log.c_str());
      uint8_t* logstr = (uint8_t*)log.c_str();
      ftp.WriteData(logstr, strlen((char*)logstr));
      ftp.CloseFile();
  
      ftp.CloseConnection();
      //Serial.printf("Saved file to: %s\n", filename.c_str());
    }

  }



  digitalWrite(GPIO_NUM_33, 1); // power down the camera
  
  // hold the off state of the LED pin during sleep
  rtc_gpio_hold_en(GPIO_NUM_4);
  rtc_gpio_hold_en(GPIO_NUM_33);
  
  unsigned long elapsed = millis() - start_time;
  Serial.printf("time elapsed %.1fs\n Going to sleep now", elapsed / 1000.0f);
  Serial.flush();

  if(fw_updated) {
    ESP.restart();
  }

  next_wakeup *= rtc_compensation;
  esp_sleep_enable_timer_wakeup(next_wakeup * 60ULL * 1000 * 1000); 
  //esp_sleep_enable_timer_wakeup(4ULL * 60 * 60 * 1000 * 1000); // sleep 4hrs
  //esp_sleep_enable_timer_wakeup(60ULL * 60 * 1000 * 1000); // sleep 60 mins
  esp_deep_sleep_start();
}

void loop() {
  // we'll never get here
}
