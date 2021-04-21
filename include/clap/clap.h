﻿/*
 * CLAP - CLever Audio Plugin
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Copyright (c) 2014...2021 Alexandre BIQUE <bique.alexandre@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLAP_VERSION_MAKE(Major, Minor, Revision)                              \
   ((((Major)&0xff) << 16) | (((Minor)&0xff) << 8) | ((Revision)&0xff))
#define CLAP_VERSION CLAP_VERSION_MAKE(0, 2, 0)
#define CLAP_VERSION_MAJ(Version) (((Version) >> 16) & 0xff)
#define CLAP_VERSION_MIN(Version) (((Version) >> 8) & 0xff)
#define CLAP_VERSION_REV(Version) ((Version)&0xff)

#if defined _WIN32 || defined __CYGWIN__
#   ifdef __GNUC__
#      define CLAP_EXPORT __attribute__((dllexport))
#   else
#      define CLAP_EXPORT __declspec(dllexport)
#   endif
#else
#   if __GNUC__ >= 4
#      define CLAP_EXPORT __attribute__((visibility("default")))
#   else
#      define CLAP_EXPORT
#   endif
#endif

///////////////////////////
// FORWARD DELCLARATIONS //
///////////////////////////

struct clap_plugin;
struct clap_host;

enum clap_string_size {
   CLAP_ID_SIZE = 128,
   CLAP_NAME_SIZE = 64,
   CLAP_DESC_SIZE = 256,
   CLAP_DISPLAY_SIZE = 64,
   CLAP_TAGS_SIZE = 256,
};

enum clap_log_severity {
   CLAP_LOG_DEBUG = 0,
   CLAP_LOG_INFO = 1,
   CLAP_LOG_WARNING = 2,
   CLAP_LOG_ERROR = 3,
   CLAP_LOG_FATAL = 4,
};

// Description of the plugin
#define CLAP_ATTR_DESCRIPTION "clap/description"
// Product version string
#define CLAP_ATTR_VERSION "clap/version"
// Manufacturer name
#define CLAP_ATTR_MANUFACTURER "clap/manufacturer"
// Url to product
#define CLAP_ATTR_URL "clap/url"
// Url to support page, or mail to support
#define CLAP_ATTR_SUPPORT "clap/support"

// Should be "1" if the plugin supports tunning.
#define CLAP_ATTR_SUPPORTS_TUNING "clap/supports_tuning"

////////////////
// PARAMETERS //
////////////////

union clap_param_value {
   bool    b;
   double  d;
   int64_t i;
};

////////////
// EVENTS //
////////////

enum clap_event_type {
   CLAP_EVENT_NOTE_ON = 0,  // note attribute
   CLAP_EVENT_NOTE_OFF = 1, // note attribute

   CLAP_EVENT_CHOKE = 2, // no attribute

   CLAP_EVENT_PARAM_SET = 3, // param attribute

   /* MIDI Style */
   CLAP_EVENT_CONTROL = 5,  // control attribute
   CLAP_EVENT_PROGRAM = 16, // program attribute
   CLAP_EVENT_MIDI = 6,     // midi attribute

   CLAP_EVENT_PLAY = 12,  // no attribute
   CLAP_EVENT_PAUSE = 13, // no attribute
   CLAP_EVENT_STOP = 14,  // no attribute
};

struct clap_event_param {
   int8_t                 key;
   int8_t                 channel;
   uint32_t               index; // parameter index
   union clap_param_value normalized_value;
   double                 normalized_ramp;
};

/** Note On/Off event. */
struct clap_event_note {
   int8_t key;      // 0..127
   int8_t channel;  // 0..15
   double velocity; // 0..1
};

struct clap_event_control {
   int8_t key;     // 0..127, or -1 to match all keys
   int8_t channel; // 0..15, or -1 to match all channels
   int8_t control; // 0..127
   double value;   // 0..1
};

struct clap_event_midi {
   const uint8_t *buffer; // midi buffer
   uint32_t       size;
};

/**
 * Asks the plugin to load a program.
 * This is analogue to the midi program set:
 * bank msb goes into bank_msb
 * bank lsb goes into bank_lsb
 * program goes into program
 *
 * Clap is not limited to 127.
 *
 * The main advantage of setting a program instead of loading
 * a preset, is that the program should already be in the plugin's
 * memory, and can be set instantly. If the plugin has to load
 * a preset from the filesystem, then parse it, do memory allocation,
 * there is no guarentee that the preset will be loaded in time.
 */
struct clap_event_program {
   int32_t bank_msb; // 0..0x7FFFFFFF
   int32_t bank_lsb; // 0..0x7FFFFFFF
   int32_t program;  // 0..0x7FFFFFFF
};

struct clap_event {
   enum clap_event_type type;
   uint32_t time; // offset from the first sample in the process block

   union {
      struct clap_event_note    note;
      struct clap_event_control control;
      struct clap_event_param   param;
      struct clap_event_midi    midi;
      struct clap_event_program program;
   };
};

struct clap_event_list {
   int (*size)(const struct clap_event_istream *stream);
   const struct clap_event *(*get)(const struct clap_event_istream *stream,
                                   int                              index);
   void (*push_back)(struct clap_event_istream *stream,
                     const struct clap_event *  event);
};

/////////////
// PROCESS //
/////////////

enum clap_process_status {
   /* Processing failed. The output buffer must be discarded. */
   CLAP_PROCESS_ERROR = 0,

   /* Processing succeed. */
   CLAP_PROCESS_CONTINUE = 1,

   /* Processing succeed, but no more processing is required, until next event.
    */
   CLAP_PROCESS_SLEEP = 2,
};

struct clap_audio_buffer {
   // if data is null, then assume that the input has the value 0 for each
   // samples. data[i] for channel i buffer
   float **data;
   int32_t channel_count;
};

struct clap_transport {
   bool is_playing;
   bool is_recording;
   bool is_looping;

   double tempo; // tempo in bpm

   double song_pos;  // position in beats
   double bar_start; // start pos of the current bar
   double loop_start;
   double loop_end;

   int16_t tsig_num;   // time signature numerator
   int16_t tsig_denom; // time signature denominator

   int64_t steady_time; // the steady time in samples
};

struct clap_process {
   int32_t frames_count; // number of frame to process

   struct clap_transport transport;

   /* audio ports */
   const struct clap_audio_buffer *audio_in;
   const struct clap_audio_buffer *audio_out;
   int                             audio_in_count;
   int                             audio_out_count;

   /* events */
   const struct clap_event_list *in_events;
   struct clap_event_list *      out_events;
};

//////////
// HOST //
//////////

struct clap_host {
   int32_t clap_version; // initialized to CLAP_VERSION

   void *host_data; // reserved pointer for the host

   /* Name and version could be attributes but it is convenient to
    * have it when debugging. Also they are mandatory. */
   char name[CLAP_NAME_SIZE];    // plugin name, eg: "BitwigStudio"
   char version[CLAP_NAME_SIZE]; // the plugin version, eg: "1.3.14"

   /* Returns the size of the original string. If this is larger than size, then
    * the function did not have enough space to copy all the data.
    * [thread-safe] */
   int32_t (*get_attribute)(struct clap_host *host,
                            const char *      attr,
                            char *            buffer,
                            int32_t           size);

   /* Log a message through the host.
    * [thread-safe] */
   void (*log)(struct clap_host *     host,
               struct clap_plugin *   plugin,
               enum clap_log_severity severity,
               const char *           msg);

   /* Query an extension.
    * [thread-safe] */
   const void *(*extension)(struct clap_host *host, const char *extention_id);
};

////////////
// PLUGIN //
////////////

/* bitfield
 * This gives an hint to the host what the plugin might do. */
enum clap_plugin_type {
   /* Instruments can play notes, and generate audio */
   CLAP_PLUGIN_INSTRUMENT = (1 << 0),

   /* Audio effects, process audio input and produces audio.
    * Exemple: delay, reverb, compressor. */
   CLAP_PLUGIN_AUDIO_EFFECT = (1 << 1),

   /* Event effects, takes events as input and produces events.
    * Exemple: arpegiator */
   CLAP_PLUGIN_EVENT_EFFECT = (1 << 2), // can be seen as midi effect

   /* Analyze audio and/or events, and produces analysis results */
   CLAP_PLUGIN_ANALYZER = (1 << 3),
};

struct clap_plugin {
   int32_t clap_version; // initialized to CLAP_VERSION

   void *plugin_data; // reserved pointer for the plugin

   /* The 3 following strings are here because:
    * - they are mandatory
    * - it is convenient when you debug, to be able to see
    *   the plugin name, id and version by displaying
    *   the structure.
    */
   char name[CLAP_NAME_SIZE];    // plugin name, eg: "Diva"
   char id[CLAP_ID_SIZE];        // plugin id, eg: "com.u-he.diva"
   char version[CLAP_NAME_SIZE]; // the plugin version, eg: "1.3.2"

   enum clap_plugin_type plugin_type;

   /* Free the plugin and its resources.
    * It is not required to deactivate the plugin prior to this call. */
   void (*destroy)(struct clap_plugin *plugin);

   /* Copy at most size of the attribute's value into buffer.
    * This function must place a '\0' byte at the end of the string.
    * Returns the size of the original string or 0 if there is no
    * value for this attributes.
    * [thread-safe] */
   int32_t (*get_attribute)(struct clap_plugin *plugin,
                            const char *        attr,
                            char *              buffer,
                            int32_t             size);

   /* activation/deactivation
    * [main-thread] */
   bool (*activate)(struct clap_plugin *plugin, int sample_rate);
   void (*deactivate)(struct clap_plugin *plugin);

   /* process audio, events, ...
    * [audio-thread] */
   enum clap_process_status (*process)(struct clap_plugin * plugin,
                                       struct clap_process *process);

   /* query an extension
    * [thread-safe] */
   const void *(*extension)(struct clap_plugin *plugin, const char *id);
};

/* This interface is the entry point of the dynamic library.
 *
 * Every methods must be thread-safe. */
struct clap_plugin_entry {
   void (*init)(const char *plugin_path);
   void (*deinit)(void);

   /* Get the number of plugins available.
    * [thread-safe] */
   int32_t (*get_plugin_count)(void);

   /* Create a clap_plugin by its index.
    * Valid indexes are from 0 to get_plugin_count() - 1.
    * Returns null in case of error.
    * [thread-safe] */
   struct clap_plugin *(*create_plugin_by_index)(struct clap_host *host,
                                                 int32_t           index);

   /* Create a clap_plugin by its plugin_id.
    * Returns null in case of error.
    * [thread-safe] */
   struct clap_plugin *(*create_plugin_by_id)(struct clap_host *host,
                                              const char *      plugin_id);
};

/* Entry point */
CLAP_EXPORT extern const struct clap_plugin_entry *clap_plugin_entry;

#ifdef __cplusplus
}
#endif