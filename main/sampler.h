/*\
 *
 * SoundBouncer for M5Stack Core2
 * Project Page:
 *
 *   - https://github.com/tobozo/M5Core2-SoundBouncer
 *
 * Reuses some code from:
 *
 *   - https://github.com/wararyo/M5Stack_Core2_Sampler (see LICENCE.sampler file)
 *
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





#if defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE) // m5stack classic/fire

  #define SAMPLE_RATE     44100
  #define SAMPLE_BUFFER_SIZE 128
  #define MAX_SOUND 8

  void sendSoundData( int16_t* dataI, size_t bytes_written )
  {
    M5.Speaker.playRAW(  dataI, SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false);
  }


  void initSound()
  {
    M5.Speaker.begin();
    if (!M5.Speaker.isEnabled()) {
      M5.Display.print("Speaker not found...");
      for (;;) { delay(1); }
      return;
    }
    M5.Speaker.setVolume(48);
  }


  void SendNoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channnel)
  {
    // TODO: use wav data with fadein / fadeout
    /// tone data (8bit unsigned wav)
    // const uint8_t wavdata[64] = { 132,138,143,154,151,139,138,140,144,147,147,147,151,159,184,194,203,222,228,227,210,202,197,181,172,169,177,178,172,151,141,131,107,96,87,77,73,66,42,28,17,10,15,25,55,68,76,82,80,74,61,66,79,107,109,103,81,73,86,94,99,112,121,129 };
    // static uint8_t channel_num = 0;
    // M5.Speaker.tone(255+(noteNo-START_NOTE)*64, (channel_num++%MAX_SOUND), true, wavdata, sizeof(wavdata));

    M5.Speaker.tone( 255+(noteNo-START_NOTE)*64, 50 );
  }

  void SendNoteOff(uint8_t noteNo,  uint8_t velocity, uint8_t channnel)
  {
    // nothing to do, M5Unified Speaker manages the timer
  }

#endif // defined( ARDUINO_M5Stack_Core_ESP32 ) || defined( ARDUINO_M5STACK_FIRE) // m5stack classic/fire




#if defined ARDUINO_M5STACK_Core2 // M5Stack Core2

  #include <driver/i2s.h>

  // sox -V -r 44100 -n -b 16 -c 1 blip.wav synth 1 sin 500 vol -8dB
  // xxd -i blip.wav > blip.h
  #include "blip.h"
  // xxd -i pop.wav > pop.h
  #include "pop.h"

  #define CONFIG_I2S_BCK_PIN     12
  #define CONFIG_I2S_LRCK_PIN     0
  #define CONFIG_I2S_DATA_PIN     2
  #define CONFIG_I2S_DATA_IN_PIN 34

  #define Speak_I2S_NUMBER I2S_NUM_0

  #define MODE_MIC    0
  #define MODE_SPK    1

  #define MAX_SOUND   12

  #define DATA_SIZE 1024
  #define SAMPLE_RATE 44100
  #define SAMPLE_BUFFER_SIZE 64

  // handle I2S migration across SDK versions
  #ifdef ESP_ARDUINO_VERSION_VAL
    #if defined ESP_ARDUINO_VERSION && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
      #define I2C_COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S
    #else
      #define I2C_COMM_FORMAT I2S_COMM_FORMAT_I2S
    #endif
  #else
    #define I2C_COMM_FORMAT I2S_COMM_FORMAT_I2S
  #endif


  i2s_config_t getSpeakerConfig(int mode)
  {
    static i2s_config_t i2s_config = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER),
      .sample_rate          = SAMPLE_RATE,
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = I2C_COMM_FORMAT,
      .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count        = 8,
      .dma_buf_len          = SAMPLE_BUFFER_SIZE,
    };
    if (mode == MODE_MIC) {
      i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    } else {
      i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
      i2s_config.use_apll = false;
      i2s_config.tx_desc_auto_clear = true;
    }
    return i2s_config;
  }


  i2s_pin_config_t getTxPinConfig()
  {
    static i2s_pin_config_t tx_pin_config;
    tx_pin_config.bck_io_num   = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num    = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num  = CONFIG_I2S_DATA_IN_PIN;
    return tx_pin_config;
  }


  bool InitI2SSpeakOrMic(int mode)
  {
    esp_err_t err = ESP_OK;
    i2s_config_t i2s_config = getSpeakerConfig(mode);
    i2s_pin_config_t tx_pin_config = getTxPinConfig();
    err += i2s_driver_install(Speak_I2S_NUMBER, &i2s_config, 0, NULL);
    err += i2s_set_pin(Speak_I2S_NUMBER, &tx_pin_config);
    return true;
  }


  void sendSoundData( int16_t* dataI, size_t bytes_written )
  {
    i2s_write(Speak_I2S_NUMBER, (const unsigned char *)dataI, 2 * SAMPLE_BUFFER_SIZE, &bytes_written, portMAX_DELAY);
  }


  void AudioLoop(void *pvParameters);


  void initSound()
  {
    InitI2SSpeakOrMic(MODE_SPK); // Note: I2S MUST be started before SD, or weird things happen
    xTaskCreateUniversal( AudioLoop, "audioLoop", 8192, NULL, 1, NULL, 0);
    disableCore0WDT();
  }


  static float globalVolume = 0.5f;
  static uint32_t audioProcessTime = 0; // プロファイリング用 一回のオーディオ処理にかかる時間


  enum SampleAdsr
  {
    attack,
    decay,
    sustain,
    release,
  };


  struct Sample
  {
    const int16_t *sample;
    uint32_t length;
    uint8_t root;
    uint32_t loopStart;
    uint32_t loopEnd;

    bool adsrEnabled;
    float attack;
    float decay;
    float sustain;
    float release;
  };


  struct SamplePlayer
  {
    SamplePlayer(Sample *sample, uint8_t noteNo, float volume)
      : sample{sample}, noteNo{noteNo}, volume{volume}, createdAt{micros()} {}
    SamplePlayer()
      : sample{nullptr}, noteNo{60}, volume{1.0f}, playing{false}, createdAt{micros()} {}
    Sample *sample;
    uint8_t noteNo;
    float volume;
    bool playing = true;
    unsigned long createdAt = 0;
    float pitchBend = 0;
    uint32_t pos = 0;
    float pos_f = 0.0f;
    bool released = false;
    float adsrGain = 0.0f;
    enum SampleAdsr adsrState = SampleAdsr::attack;
  };


  #define PCM_HEADER_SIZE 44 // size of the PCM header in wav files, data starts just after that


  static Sample blipSample =
  {
    .sample      = (int16_t*)&blip[PCM_HEADER_SIZE],
    .length      = (blip_len-PCM_HEADER_SIZE)/2,
    .root        = 50,
    .loopStart   = 0,
    .loopEnd     = (blip_len-PCM_HEADER_SIZE)/2 - 1,
    .adsrEnabled = true,
    .attack      = 1.0f,
    .decay       = 0.987f,
    .sustain     = 0.1,
    .release     = 0.948885f
  };

  /*

  static Sample popSample =
  {
    .sample      = (int16_t*)&pop[PCM_HEADER_SIZE],
    .length      = (pop_len-PCM_HEADER_SIZE)/2,
    .root        = 60,
    .loopStart   = 0,
    .loopEnd     = (pop_len-PCM_HEADER_SIZE)/2 - 1,
    .adsrEnabled = true,
    .attack      = 0.75f,
    .decay       = 0.9888f,
    .sustain     = 0.1f,
    .release     = 0.948885f
  };
  */

  SamplePlayer players[MAX_SOUND] = {SamplePlayer()};

  float PitchFromNoteNo(float noteNo, float root)
  {
    float delta = noteNo - root;
    float f = ((pow(2.0f, delta / 12.0f)));
    return f;
  }


  void UpdateAdsr(SamplePlayer *player)
  {
    Sample *sample = player->sample;
    if(player->released) player->adsrState = release;

    switch (player->adsrState) {
    case attack:
      player->adsrGain += sample->attack;
      if (player->adsrGain >= 1.0f) {
        player->adsrGain = 1.0f;
        player->adsrState = decay;
      }
      break;
    case decay:
      player->adsrGain = (player->adsrGain - sample->sustain) * sample->decay + sample->sustain;
      if ((player->adsrGain - sample->sustain) < 0.01f) {
        player->adsrState = sustain;
        player->adsrGain = sample->sustain;
      }
      break;
    case sustain:
      break;
    case release:
      player->adsrGain *= sample->release;
      if (player->adsrGain < 0.01f) {
        player->adsrGain = 0;
        player->playing = false;
      }
      break;
    }
  }


  void SendNoteOn(uint8_t noteNo, uint8_t velocity, uint8_t channnel) {
    uint8_t oldestPlayerId = 0;
    for(uint8_t i = 0;i < MAX_SOUND;i++) {
      if(players[i].playing == false) {
        players[i] = SamplePlayer(&blipSample, noteNo, velocity / 127.0f);
        return;
      } else {
        if(players[i].createdAt < players[oldestPlayerId].createdAt) oldestPlayerId = i;
      }
    }
    // 全てのPlayerが再生中だった時には、最も昔に発音されたPlayerを停止する
    players[oldestPlayerId] = SamplePlayer(&blipSample, noteNo, velocity / 127.0f);
  }


  void SendNoteOff(uint8_t noteNo,  uint8_t velocity, uint8_t channnel) {
    for(uint8_t i = 0;i < MAX_SOUND;i++) {
      if(players[i].playing == true && players[i].noteNo == noteNo) {
        players[i].released = true;
      }
    }
  }


  void AudioLoop(void *pvParameters)
  {
    while (true)
    {
      float data[SAMPLE_BUFFER_SIZE] = {0.0f};

      unsigned long startTime = micros();

      // 波形を生成
      for (uint8_t i = 0; i < MAX_SOUND; i++)
      {
        SamplePlayer *player = &players[i];
        if(player->playing == false) continue;
        Sample *sample = player->sample;
        if(sample->adsrEnabled) UpdateAdsr(player);
        if(player->playing == false) continue;

        float pitch = PitchFromNoteNo(player->noteNo, player->sample->root);

        for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
        {
          if (player->pos >= sample->length)
          {
            player->playing = false;
            break;
          }
          else
          {
            // 波形を読み込む
            float val = sample->sample[player->pos];
            if(sample->adsrEnabled) val *= player->adsrGain;
            val *= player->volume / 32767.0f;
            data[n] += val;

            // 次のサンプルへ移動
            int32_t pitch_u = pitch;
            player->pos_f += pitch - pitch_u;
            player->pos += pitch_u;
            int posI = player->pos_f;
            player->pos += posI;
            player->pos_f -= posI;

            // ループポイントが設定されている場合はループする
            if(sample->adsrEnabled && player->released == false && player->pos >= sample->loopEnd)
              player->pos -= (sample->loopEnd - sample->loopStart);
          }
        }
      }

      int16_t dataI[SAMPLE_BUFFER_SIZE];
      const float multiplier = globalVolume * 32767.0f; // 32767 = int16_tがとりうる最大値
      for (uint8_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        dataI[i] = int16_t(data[i] * multiplier);
      }

      unsigned long endTime = micros();
      audioProcessTime = endTime - startTime;

      static size_t bytes_written = 0;
      sendSoundData( dataI, bytes_written );
      //i2s_write(Speak_I2S_NUMBER, (const unsigned char *)dataI, 2 * SAMPLE_BUFFER_SIZE, &bytes_written, portMAX_DELAY);
    }
  }

#endif // defined ARDUINO_M5STACK_Core2


