// Flash firmware from SD card if available

#include "update_fw.h"
#include <Update.h>


// perform the actual update from a given stream
bool performUpdate(Stream &updateSource, size_t updateSize) 
{
bool success = false;

   if (Update.begin(updateSize)) {      
      size_t written = Update.writeStream(updateSource);
      if (written == updateSize) {
         Serial.println("Written : " + String(written) + " successfully");
      }
      else {
         Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
      }
      if (Update.end()) {
         Serial.println("OTA done!");
         if (Update.isFinished()) {
            Serial.println("Update successfully completed.");
            success = true;
         }
         else {
            Serial.println("Update not finished? Something went wrong!");
         }
      }
      else {
         Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }

   }
   else
   {
      Serial.println("Not enough space to begin OTA");
   }
   return success;
}

// check given FS for valid .bin and perform update if available
bool update_from_sd(fs::FS &fs) 
{
bool success = false; 

   File updateBin = fs.open("/" FIRWARE_FILENAME);
   if (updateBin) 
   {
      if(updateBin.isDirectory()){
         Serial.println("Error, update.bin is not a file");
         updateBin.close();
         return false;
      }

      size_t updateSize = updateBin.size();

      if (updateSize > 0 && updateSize < 2000000 ) {
         Serial.println("Try to start update");
         success = performUpdate(updateBin, updateSize);
      }
      else {
         Serial.printf("Error, file is strange size, %d\n", updateSize);
      }

      updateBin.close();
    
      // whe finished remove the binary from sd card to indicate end of the process
      fs.remove("/" FIRWARE_FILENAME);      
   }
   else {
      Serial.println("Could not find firmware on sd root");
   }
   return success;
}
