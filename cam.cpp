#include "config.h"
#include "esp_camera.h"
#include "Arduino.h"

// Pin definition for CAMERA_MODEL_AI_THINKER
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

camera_fb_t *cam_capture()
{
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    //config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.frame_size = FRAMESIZE_XGA; 
    //config.jpeg_quality = 10;
    config.jpeg_quality = 32;
    config.fb_count = 2;
   //config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  digitalWrite(GPIO_NUM_33, 0); // power up the camera
  delay(500); // give it a moment

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return NULL;
  }

  sensor_t * sensor = esp_camera_sensor_get();
  if(sensor) {
    //sensor->set_brightness(sensor, 0);     // -2 to 2
    //sensor->set_contrast(sensor, 0);       // -2 to 2
    //sensor->set_saturation(sensor, 0);     // -2 to 2
    //sensor->set_special_effect(sensor, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    //sensor->set_whitebal(sensor, 1);       // 0 = disable , 1 = enable
    //sensor->set_awb_gain(sensor, 1);       // 0 = disable , 1 = enable
    sensor->set_wb_mode(sensor, 2);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    sensor->set_exposure_ctrl(sensor, 1);  // 0 = disable , 1 = enable
    //sensor->set_aec2(sensor, 0);           // 0 = disable , 1 = enable
    //sensor->set_ae_level(sensor, 0);       // -2 to 2
    sensor->set_aec_value(sensor, 300);    // 0 to 1200
    sensor->set_gain_ctrl(sensor, 0);      // 0 = disable , 1 = enable
    //sensor->set_agc_gain(sensor, 0);       // 0 to 30
    //sensor->set_gainceiling(sensor, (gainceiling_t)0);  // 0 to 6
    //sensor->set_bpc(sensor, 0);            // 0 = disable , 1 = enable
    //sensor->set_wpc(sensor, 1);            // 0 = disable , 1 = enable
    //sensor->set_raw_gma(sensor, 1);        // 0 = disable , 1 = enable
    sensor->set_lenc(sensor, 1);           // 0 = disable , 1 = enable
    sensor->set_hmirror(sensor, 0);        // 0 = disable , 1 = enable
    sensor->set_vflip(sensor, 1);          // 0 = disable , 1 = enable
    //sensor->set_dcw(sensor, 1);            // 0 = disable , 1 = enable
    //sensor->set_colorbar(sensor, 0);       // 0 = disable , 1 = enable
  }else
    Serial.println("couldn't configure sensor");

  digitalWrite(GPIO_NUM_4, HIGH);  // switch on the flash
  delay(1000);
 
  // take a picture 
  camera_fb_t *fb = esp_camera_fb_get(); 
  digitalWrite(GPIO_NUM_4, LOW);  // switch off the flash

  //esp_camera_deinit();  //NB this destroys the image data
  return fb;
}
