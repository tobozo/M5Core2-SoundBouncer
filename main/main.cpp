/*\
 *
 * SoundBouncer for M5Stack Core2
 * Project Page: https://github.com/tobozo/M5Core2-SoundBouncer
 *
 * Copyright 2022 tobozo http://github.com/tobozo
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files ("M5Core2-SoundBouncer"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * This demo takes advantage of DMA transfer for both the
 * display and the audio while demonstrating synchronization
 * with incremented visual and auditory rhythms.
 *
 * Libraries dependencies:
 *
 *   - https://github.com/M5Stack/M5GFX
 *   - https://github.com/M5Stack/M5Unified
 *   - https://github.com/marcel-licence/ML_SynthTools
 *   - https://github.com/tobozo/M5Stack-SD-Updater (optional)
 *
 * Credits:
 *
 *   - https://github.com/wararyo/M5Stack_Core2_Sampler
 *
\*/

#include <SD.h>
#include <M5Unified.h>
auto &tft(M5.Display);

#if defined USE_SDUPDATER
  #define SDU_APP_NAME   "SoundBouncer" // app title for the sd-updater lobby screen
  #define SDU_APP_PATH   "/SoundBouncer.bin"     // app binary file name on the SD Card (also displayed on the sd-updater lobby screen)
  #define SDU_APP_AUTHOR "@tobozo"           // app binary author name for the sd-updater lobby screen
  #include <M5StackUpdater.h>
#endif

#if !defined ARDUINO_M5STACK_Core2
  #error "This demo is for M5Stack Core2 only"
#endif

// editable values
#define BULLETS_COUNT 16 // more bullets = bigger sprite
#define BASE_BOUNCES  20 // more bounces = longer animation
#define BULLET_SIZE    5 // bullet radius in pixels, affects sprite size, adjusted for 320x240 with 16 bullets
#define START_NOTE    38 // base note for the lowest bullet
#define END_NOTE      70 // base note for the highest bullet


#include "sampler.h" // sound effects
#include "bouncer.h" // visual effects

// don't burn the screen, quietly go to sleep after animating
void gotoSleep()
{
  while(1) {
    auto brightness = tft.getBrightness();
    if( brightness > 0 ) brightness--;
    else break;
    tft.setBrightness( brightness );
  }
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0); // gpio39 == touch INT
  delay(100);
  tft.sleep();
  tft.waitDisplay();
  esp_deep_sleep_start();
}


void setup()
{
  M5.begin();

  #if defined USE_SDUPDATER
    checkSDUpdater(
      SD,           // filesystem (default=SD)
      MENU_BIN,     // path to binary (default=/menu.bin, empty string=rollback only)
      1000,         // wait delay, (default=0, will be forced to 2000 upon ESP.restart() )
      5             // m5stack=4, m5core2=5
    );
  #endif

  InitI2SSpeakOrMic(MODE_SPK);
  xTaskCreateUniversal( AudioLoop, "audioLoop", 8192, NULL, 1, NULL, 0);
  disableCore0WDT();

  // gradient color palette for bullets, green => yellow => orange => red
  RGBColor heatMapColors[] = { {0, 0xff, 0}, {0xff, 0xff, 0}, {0xff, 0x80, 0}, {0xff, 0, 0} };

  BouncerConfig cfg =
  {
    .display       = &tft,
    .bullets_count = BULLETS_COUNT,
    .base_bounces  = BASE_BOUNCES,
    .bulletsize    = BULLET_SIZE,
    .start_note    = START_NOTE,
    .end_note      = END_NOTE,
    .note_duration = 100, // milliseconds
    .note_veloticy = 75, // byte
    .maskColor     = tft.color565(0xff, 0xff, 0xff), // white
    .strokeColor   = tft.color565(0x00, 0x00, 0x00), // black
    .transColor    = tft.color565(0x00, 0x00, 0xff), // blue
    .heatMapColors = heatMapColors,
    .heatMapSize   = sizeof(heatMapColors)/sizeof(RGBColor),
 };

  Bouncer bouncer( &cfg );
  bouncer.animate(1);
  //bouncer.animate(-1);
  gotoSleep();

}

void loop()
{


}
