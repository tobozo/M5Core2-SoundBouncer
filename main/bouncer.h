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
\*/

#pragma once

#include "sampler.h"


struct Point
{
  int32_t x, y;
};


struct Bullet
{
  uint8_t note;
  uint16_t color;
  int16_t bounces;
  Point origin, pos, last;
  int8_t dir, lastdir;
  float radius, hdist, fulldistance;
  unsigned long playing_since;
};


struct BouncerConfig
{
  // must be set those before calling init()
  M5GFX *display;
  const size_t bullets_count;
  const uint16_t base_bounces;
  int bulletsize;

  const uint16_t start_note;
  const uint16_t end_note;

  const uint16_t note_duration;// = 100; // ms
  const uint8_t  note_veloticy;
  const uint16_t maskColor;
  const uint16_t strokeColor;
  const uint16_t transColor;
  RGBColor *heatMapColors;
  size_t heatMapSize;

  // will be set by init()
  LGFX_Sprite *sprite;
  Bullet *bullets;

  float yoffset;
  float maxfulldist;
  float pixeloffset;
  float anglestart; // -M_PI/4.0 = -45 degrees

  uint16_t strokelen;
  uint16_t pixeldistance;
  uint16_t boxwidth;
  uint16_t boxheight;

  uint8_t strokewidth;

  Point origin;
  Point baseline;
  Point spriteCoords;

  void init()
  {
    assert( bullets_count > 0 );
    assert( display );
    bullets = new Bullet[bullets_count];
    // bullets will bounce inside a box tilted at -45 degrees
    anglestart        = float(-M_PI/4.0); // -45 degrees
    // deduce some dynamic values (radius, distance, offset) from bullet size
    strokewidth       = bulletsize/2;                    // border width for the tilted half square
    int bulletslength = bullets_count*bulletsize;        // distance between first and last bullet
    pixeldistance     = bulletslength*2;                 // double side length, deduced from bullets length, will be used to calculate box width
    pixeloffset       = float(pixeldistance)/16.0*7.0;   // distance between bottom side and first bullet ( arbitrary 7/16th )
    strokelen         = pixeloffset+bulletslength;       // distance between bottom side and last bullet
    float maxypos     = pixeloffset+(bullets_count-1)*2; // highest bullet (used to calculate box height)
    // box width/height is speculated from the previous values to be as small as possible
    boxwidth          = sqrt( pixeldistance*pixeldistance + pixeldistance*pixeldistance ) + (bulletsize)*2 + 2;    // largest horizontal (X) distance (sprite)
    boxheight         = (maxypos - ( sin(anglestart)*pixeloffset ) )+ bulletsize + 2;                          // largest vertical (Y) distance (sprite)
    origin            = { boxwidth/2, boxheight };                                                             // point of origin for drawing  (sprite)
    float ylow        = origin.y - ( cos(anglestart)*pixeloffset ) + strokewidth + 1;                          // lowest Y bullet coord when at -45 degrees (sprite)
    yoffset           = (boxheight - ylow) - strokewidth;                                                      // used for bullets, tilted square (both sprite and tft)
    spriteCoords      = { display->width()/2 - boxwidth/2, display->height()/2 - (boxheight+int(yoffset))/2 }; // sprite upper left coords (TFT)
    baseline          = { spriteCoords.x+origin.x, spriteCoords.y+origin.y+int(yoffset) };                     // tilted square corner coords (TFT)
    sprite            = new LGFX_Sprite( display );
    Serial.printf("Calculated sprite size: %d x %d\n", boxwidth, boxheight );
  }
};


struct Bouncer
{
  // constructor
  Bouncer( BouncerConfig *_cfg ) : cfg(_cfg) { init(); }
  // settings holder
  BouncerConfig *cfg;

  void init()
  {
    assert( cfg );
    assert( cfg->bullets_count > 0 );

    cfg->init();

    // init bullets position
    for( int i=0; i<cfg->bullets_count; i++ ) {
      uint8_t note   = map( i, 0, cfg->bullets_count-1, cfg->start_note, cfg->end_note );
      uint16_t color = getHeatMapColor( 0, cfg->bullets_count-1, i, cfg->heatMapColors, cfg->heatMapSize, 16 );
      float radius   = mapf( i, 0, cfg->bullets_count-1, cfg->pixeloffset, cfg->pixeldistance );
      // calculate bullet coordinates
      Point coords = {
        .x = cfg->origin.x + int(sin(cfg->anglestart)*float(radius) ),
        .y = int(cfg->origin.y - (cos(cfg->anglestart)*float(radius) )) + int( cfg->yoffset )
      };
      // calculate horizontal distances
      int16_t bounces    = cfg->base_bounces+i;
      float hdist        = (cfg->origin.x-coords.x)*2.0; // one bounce, distance in pixels
      float fulldistance = bounces*hdist;           // all bounces, distance in pixels
      //float amplitude    = cfg->pixeloffset+i*2;         // vertical bounce amplitude

      cfg->bullets[i] = {
        .note          = note,
        .color         = color,
        .bounces       = bounces,
        .origin        = coords,
        .pos           = coords,
        .last          = coords,
        .dir           = 1,
        .lastdir       = -1,
        .radius        = radius,
        .hdist         = hdist,
        .fulldistance  = fulldistance,
        //.amplitude     = amplitude,
        .playing_since = 0
      };
      // memoize the highest distance for the animation timing
      cfg->maxfulldist = max( cfg->maxfulldist, fulldistance );
    }

    // clear screen
    cfg->display->fillScreen( cfg->maskColor );
    // debug
    // cfg->display->drawRect( cfg->spriteCoords.x-1, cfg->spriteCoords.y-1, cfg->boxwidth+2, cfg->boxheight+2, TFT_BLUE);
    // create sprite
    cfg->sprite->setPsram( false );  // using psram glitches the audio, let's make sure it won't re-enable itself
    cfg->sprite->setColorDepth( 8 );
    if( !cfg->sprite->createSprite( cfg->boxwidth, cfg->boxheight ) ) {
      cfg->display->print("Error, not enough memory to create sprite");
      return;
    }
    // pre-fill sprite
    cfg->sprite->fillSprite( cfg->transColor );

    // draw persistent lines first (will be partially overlapped by the sprite)
    for( int h=0;h<cfg->strokewidth;h++) {
      cfg->display->drawLine( cfg->baseline.x, cfg->baseline.y+h, cfg->baseline.x+cfg->strokelen, cfg->baseline.y-cfg->strokelen+h, cfg->strokeColor );
      cfg->display->drawLine( cfg->baseline.x, cfg->baseline.y+h, cfg->baseline.x-cfg->strokelen, cfg->baseline.y-cfg->strokelen+h, cfg->strokeColor );
    }
  }


  void animate( int8_t direction = 1 )
  {
    // evaluate total animation time
    unsigned long now      = millis();
    unsigned long duration = ((cfg->bullets_count-1)*cfg->base_bounces)*100;
    unsigned long end      = now + duration;

    // start time based animation
    while( now <= end ) {
      // get position in the current timeline
      int64_t pos = end-now;
      // translate to max full distance progress
      float d =
        (direction == 1 )
           ? mapf( pos, duration, 0, 0, cfg->maxfulldist )
           : mapf( pos, 0, duration, 0, cfg->maxfulldist )
      ;
      updateCoords( d );
      handleNotes( now );
      delay(1);
      drawItems();
      // update timer
      now = millis();
    }

    // last
    for( int i=0; i<cfg->bullets_count; i++ ) {
      SendNoteOn(cfg->bullets[i].note, cfg->note_veloticy/2, 1);
      delay(2);
    }
    //resetCoords();
    //drawItems();
    delay(500);
    for( int i=0; i<cfg->bullets_count; i++ ) {
      SendNoteOff(cfg->bullets[i].note, cfg->note_veloticy/2, 1);
    }
    delay(1000);
  }


  void resetCoords()
  {
    for( size_t i=0; i<cfg->bullets_count; i++ ) {
      cfg->bullets[i].last.x = cfg->bullets[i].pos.x;
      cfg->bullets[i].last.y = cfg->bullets[i].pos.y;
      cfg->bullets[i].pos.x  = (cfg->bullets[i].dir>0) ? cfg->bullets[i].origin.x : cfg->bullets[i].origin.x + cfg->bullets[i].hdist;
      cfg->bullets[i].pos.y  = cfg->bullets[i].origin.y;
    }
  }


  void updateCoord( size_t i, float d )
  {
    const float posx      = mapf( d, 0, cfg->maxfulldist, 0, cfg->bullets[i].fulldistance );
    const int16_t slotnum = posx/cfg->bullets[i].hdist;
    const int8_t slotdir  = (slotnum%2==0) ? 1 : -1;
    const float posinslot = fmod(posx, cfg->bullets[i].hdist);
    const float xpos      = (slotdir>0) ? posinslot : cfg->bullets[i].hdist-posinslot;
    const float angle     = mapf( posinslot, 0, cfg->bullets[i].hdist, 0, PI );
    const float ypos      = (-sin(angle)*cfg->bullets[i].radius)*.5;

    cfg->bullets[i].last.x = cfg->bullets[i].pos.x;
    cfg->bullets[i].last.y = cfg->bullets[i].pos.y;
    cfg->bullets[i].pos.x  = cfg->bullets[i].origin.x+xpos;
    cfg->bullets[i].pos.y  = cfg->bullets[i].origin.y+ypos;
    cfg->bullets[i].dir    = slotdir;
  }


  void updateCoords( float d )
  {
    for( size_t i=0; i<cfg->bullets_count; i++ ) {
      updateCoord( i, d );
    }
  }


  void handleNote( size_t i, unsigned long now )
  {
    // trigger note on direction change
    if( cfg->bullets[i].dir != cfg->bullets[i].lastdir ) {
      cfg->bullets[i].lastdir = cfg->bullets[i].dir;
      SendNoteOn(cfg->bullets[i].note, cfg->note_veloticy, 1);
      cfg->bullets[i].playing_since = now;
    }
    // silence after note duration complete
    if( cfg->bullets[i].playing_since !=0 && cfg->bullets[i].playing_since + cfg->note_duration < now ) {
      SendNoteOff(cfg->bullets[i].note, cfg->note_veloticy, 1);
      cfg->bullets[i].playing_since = 0; // reset note timer
    }
  }


  void handleNotes( unsigned long now )
  {
    for( size_t i=0; i<cfg->bullets_count; i++ ) {
      handleNote( i, now );
    }
  }


  void drawItems()
  {
    cfg->sprite->fillSprite( cfg->transColor );
    // add clear mask to last known coords for lines/bullets
    for( int i=0; i<cfg->bullets_count; i++ ) {
      if( i>0 ) {
        cfg->sprite->drawLine( cfg->bullets[i-1].last.x, cfg->bullets[i-1].last.y, cfg->bullets[i].last.x, cfg->bullets[i].last.y, cfg->maskColor );
        cfg->sprite->drawLine( cfg->bullets[i].last.x, cfg->bullets[i].last.y, cfg->bullets[i-1].last.x, cfg->bullets[i-1].last.y, cfg->maskColor );
      }
      cfg->sprite->fillCircle( cfg->bullets[i].last.x, cfg->bullets[i].last.y, cfg->bulletsize+1, cfg->maskColor );
    }
    // draw base lines
    for( int h=0;h<cfg->strokewidth;h++) {
      int ly = cfg->origin.y+cfg->yoffset+h;
      cfg->sprite->drawLine( cfg->origin.x, ly, cfg->origin.x+cfg->strokelen, ly-cfg->strokelen, cfg->strokeColor );
      cfg->sprite->drawLine( cfg->origin.x, ly, cfg->origin.x-cfg->strokelen, ly-cfg->strokelen, cfg->strokeColor );
    }
    // draw lines between bullets
    for( int i=1; i<cfg->bullets_count; i++ ) {
      cfg->sprite->drawLine( cfg->bullets[i-1].pos.x, cfg->bullets[i-1].pos.y, cfg->bullets[i].pos.x, cfg->bullets[i].pos.y, cfg->strokeColor );
    }
    // draw bullets
    for( int i=0; i<cfg->bullets_count; i++ ) {
      cfg->sprite->drawCircle( cfg->bullets[i].pos.x, cfg->bullets[i].pos.y, cfg->bulletsize+1, cfg->strokeColor );
      cfg->sprite->fillCircle( cfg->bullets[i].pos.x, cfg->bullets[i].pos.y, cfg->bulletsize, cfg->bullets[i].color );
    }
    cfg->sprite->pushSprite( cfg->spriteCoords.x, cfg->spriteCoords.y, cfg->transColor );
  }

  uint16_t getHeatMapColor( int minimum, int maximum, int value, RGBColor *colors, size_t palette_size, uint8_t bit_depth=16 )
  {
    float indexFloat = float(value-minimum) / float(maximum-minimum) * float(palette_size-1);
    int paletteIndex = int(indexFloat/1);
    float distance   = indexFloat - float(paletteIndex);
    if( distance < std::numeric_limits<float>::epsilon() ) {
      return cfg->display->color565( colors[paletteIndex].r, colors[paletteIndex].g, colors[paletteIndex].b );
    } else {
      uint8_t r1 = colors[paletteIndex].r;
      uint8_t g1 = colors[paletteIndex].g;
      uint8_t b1 = colors[paletteIndex].b;
      uint8_t r2 = colors[paletteIndex+1].r;
      uint8_t g2 = colors[paletteIndex+1].g;
      uint8_t b2 = colors[paletteIndex+1].b;
      switch( bit_depth ) {
        case 24: return cfg->display->color888( uint8_t(r1 + distance*float(r2-r1)), uint8_t(g1 + distance*float(g2-g1)), uint8_t(b1 + distance*float(b2-b1)) );
        case 16: return cfg->display->color565( uint8_t(r1 + distance*float(r2-r1)), uint8_t(g1 + distance*float(g2-g1)), uint8_t(b1 + distance*float(b2-b1)) );
        case 8:  return cfg->display->color332( uint8_t(r1 + distance*float(r2-r1)), uint8_t(g1 + distance*float(g2-g1)), uint8_t(b1 + distance*float(b2-b1)) );
        // TODO: calc grayscale
        case 1:  return ( uint8_t(r1 + distance*float(r2-r1)) & uint8_t(g1 + distance*float(g2-g1)) & uint8_t(b1 + distance*float(b2-b1)) ) > 0x7f ? 0x01 : 0x00;
        default: return 0;
      }
    }
  }


  // map() for float/double
  double mapf(double val, double in_min, double in_max, double out_min, double out_max)
  {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }


};
