/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __FUSIONSOUND_H__
#define __FUSIONSOUND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/interface.h>
#include <fusionsound_build.h>

/*
 * Main interface of FusionSound.
 */
D_DECLARE_INTERFACE( IFusionSound )

/*
 * Interface to a static sound buffer for playback of smaller samples.
 */
D_DECLARE_INTERFACE( IFusionSoundBuffer )

/*
 * Interface to a streaming sound buffer for playback of large files or real time data.
 */
D_DECLARE_INTERFACE( IFusionSoundStream )

/*
 * Interface for advanced playback control.
 */
D_DECLARE_INTERFACE( IFusionSoundPlayback )

/*
 * Interface for rendering music data.
 */
D_DECLARE_INTERFACE( IFusionSoundMusicProvider )

/**********************************************************************************************************************/

/*
 * Checks for a certain FusionSound version.
 * In case of an error a message is returned describing
 * the mismatch.
 */
const char  *FusionSoundCheckVersion (
     unsigned int                            required_major,     /* major version */
     unsigned int                            required_minor,     /* minor version */
     unsigned int                            required_micro      /* micro version */
);

/*
 * Parses the command-line and initializes some variables.
 * You absolutely need to call this before doing anything else.
 * Removes all options used by FusionSound from argv.
 */
DirectResult FusionSoundInit (
     int                                    *argc,               /* pointer to main()'s argc */
     char                                  **argv[]              /* pointer to main()'s argv */
);

/*
 * Sets configuration parameters supported on command line and in
 * config file. Can only be called before FusionSoundCreate but
 * after FusionSoundInit.
 */
DirectResult FusionSoundSetOption (
     const char                             *name,               /* option name */
     const char                             *value               /* option value */
);

/*
 * Creates the main interface.
 */
DirectResult FusionSoundCreate (
     IFusionSound                          **ret_interface       /* pointer to the created interface */
);

/*
 * Prints a description of the result code along with an
 * optional message that is put in front with a colon.
 */
DirectResult FusionSoundError (
     const char                             *msg,                /* optional message */
     DirectResult                            result              /* result code to interpret */
);

/*
 * Returns a string describing result.
 */
const char  *FusionSoundErrorString(
     DirectResult                            result              /* result code to describe */
);

/*
 * Behaves like FusionSoundError, but shuts down the calling
 * application.
 */
DirectResult FusionSoundErrorFatal (
     const char                             *msg,                /* optional message */
     DirectResult                            result              /* result code to interpret */
);

/**********************************************************************************************************************/

#define FS_MAX_FRAMES (0x7fffffff / FS_MAX_CHANNELS / 4)

typedef unsigned int FSTrackID;

/****************
 * IFusionSound *
 ****************/

#define FS_SOUND_DRIVER_INFO_NAME_LENGTH      40
#define FS_SOUND_DRIVER_INFO_VENDOR_LENGTH    60
#define FS_SOUND_DRIVER_INFO_URL_LENGTH      100
#define FS_SOUND_DRIVER_INFO_LICENSE_LENGTH   40

/*
 * Driver information.
 */
typedef struct {
     int                                     major;              /* Major version */
     int                                     minor;              /* Minor version */

     char name[FS_SOUND_DRIVER_INFO_NAME_LENGTH];                /* Driver name */
     char vendor[FS_SOUND_DRIVER_INFO_VENDOR_LENGTH];            /* Driver vendor */
     char url[FS_SOUND_DRIVER_INFO_URL_LENGTH];                  /* Driver URL */
     char license[FS_SOUND_DRIVER_INFO_LICENSE_LENGTH];          /* Driver license */
} FSSoundDriverInfo;

#define FS_SOUND_DEVICE_DESC_NAME_LENGTH     96

/*
 * Description of the sound device.
 */
typedef struct {
     char name[FS_SOUND_DEVICE_DESC_NAME_LENGTH];                /* Device name. */

     FSSoundDriverInfo                       driver;             /* Device driver information */
} FSDeviceDescription;

/*
 * Flags defining which fields of a FSBufferDescription are
 * valid.
 */
typedef enum {
     FSBDF_NONE                            = 0x00000000,         /* None of these. */

     FSBDF_LENGTH                          = 0x00000001,         /* Buffer length is set. */
     FSBDF_CHANNELS                        = 0x00000002,         /* Number of channels is set. */
     FSBDF_SAMPLEFORMAT                    = 0x00000004,         /* Sample format is set. */
     FSBDF_SAMPLERATE                      = 0x00000008,         /* Sample rate is set. */
     FSBDF_CHANNELMODE                     = 0x00000010,         /* Channel mode is set. */

     FSBDF_ALL                             = 0x0000001F          /* All of these. */
} FSBufferDescriptionFlags;

/*
 * Flags defining which fields of a FSStreamDescription are
 * valid.
 */
typedef enum {
     FSSDF_NONE                            = 0x00000000,         /* None of these. */

     FSSDF_BUFFERSIZE                      = 0x00000001,         /* Ring buffer size is set. */
     FSSDF_CHANNELS                        = 0x00000002,         /* Number of channels is set. */
     FSSDF_SAMPLEFORMAT                    = 0x00000004,         /* Sample format is set. */
     FSSDF_SAMPLERATE                      = 0x00000008,         /* Sample rate is set. */
     FSSDF_PREBUFFER                       = 0x00000010,         /* Prebuffer amount is set. */
     FSSDF_CHANNELMODE                     = 0x00000020,         /* Channel mode is set. */

     FSSDF_ALL                             = 0x0000003F          /* All of these. */
} FSStreamDescriptionFlags;

/*
 * Encodes sample format constants in the following way (bit 31 - 0):
 *
 * 0000:0000 | 0000:0dcc | cccc:cbbb | bbbb:aaaa
 *
 * a) sampleformat index
 * b) total bits per sample
 * c) effective sound bits per sample (i.e. depth)
 * d) signed sample format
 */
#define FS_SAMPLEFORMAT(index,bits,depth,is_signed) \
     ( ((index     & 0x0F)      ) |                 \
       ((bits      & 0x7F) <<  4) |                 \
       ((depth     & 0x7F) << 11) |                 \
       ((is_signed & 0x01) << 18) )

/*
 * Sample format.
 *
 * 16, 24 and 32 bit samples are always stored in native endian.
 * Always access sample buffers like arrays of 8, 16 or 32 bit
 * integers depending on the sample format, unless the data is
 * written with endianness considered. This does not excuse the
 * endian conversion that might be needed when reading data from
 * files.
 */
typedef enum {
     /* Unknown or invalid format */
     FSSF_UNKNOWN = 0x00000000,

     /* Unsigned 8 bit */
     FSSF_U8      = FS_SAMPLEFORMAT( 0,  8,  8, 0 ),

     /* Signed 16 bit */
     FSSF_S16     = FS_SAMPLEFORMAT( 1, 16, 16, 1 ),

     /* Signed 24 bit */
     FSSF_S24     = FS_SAMPLEFORMAT( 2, 24, 24, 1 ),

     /* Signed 32 bit */
     FSSF_S32     = FS_SAMPLEFORMAT( 3, 32, 32, 1 ),

     /* Floating-point 32 bit */
     FSSF_FLOAT   = FS_SAMPLEFORMAT( 4, 32, 32, 1 )
} FSSampleFormat;

/* Number of sampleformats defined. */
#define FS_NUM_SAMPLEFORMATS                 5

/* These macros extract information about the sample format. */
#define FS_SAMPLEFORMAT_INDEX(fmt)            ((fmt) & 0x0000000F)

#define FS_BITS_PER_SAMPLE(fmt)              (((fmt) & 0x000007F0) >>  4)

#define FS_BYTES_PER_SAMPLE(fmt)             (((fmt) & 0x000007F0) >>  7)

#define FS_SAMPLEFORMAT_DEPTH(fmt)           (((fmt) & 0x0003f800) >> 11)

#define FS_SIGNED_SAMPLEFORMAT(fmt)          (((fmt) & 0x00040000) != 0)

/*
 * Encodes channel mode constants in the following way (bit 31 - 0):
 *
 * 0000:0000 | 0000:0000 | 0000:0000 | dccb:aaaa
 *
 * a) number of channels per frame
 * b) center channel present
 * c) number of rear channels
 * d) LFE channel present
 */
#define FS_CHANNELMODE(num,center,rears,lfe) \
     ( ((num    & 0x3F)     ) |              \
       ((center & 0x01) << 4) |              \
       ((rears  & 0x03) << 5) |              \
       ((lfe    & 0x01) << 7) )              \

/*
 * Channel mode.
 */
typedef enum {
     /* Unknown or invalid mode */
     FSCM_UNKNOWN         = 0x00000000,

     /* 1 Channel (Mono) */
     FSCM_MONO            = FS_CHANNELMODE( 1, 0, 0, 0 ),

     /* 2 Channels (Left Right) */
     FSCM_STEREO          = FS_CHANNELMODE( 2, 0, 0, 0 ),

     /* 3 Channels (Left Right Subwoofer) */
     FSCM_STEREO21        = FS_CHANNELMODE( 3, 0, 0, 1 ),

     /* 3 Channels (Left Center Right) */
     FSCM_STEREO30        = FS_CHANNELMODE( 3, 1, 0, 0 ),

     /* 4 Channels (Left Center Right Subwoofer) */
     FSCM_STEREO31        = FS_CHANNELMODE( 3, 1, 0, 1 ),

     /* 3 Channels (Left Right Rear) */
     FSCM_SURROUND30      = FS_CHANNELMODE( 3, 0, 1, 0 ),

     /* 4 Channels (Left Right Rear Subwoofer) */
     FSCM_SURROUND31      = FS_CHANNELMODE( 4, 0, 1, 1 ),

     /* 4 Channels (Left Right RearLeft RearRight) */
     FSCM_SURROUND40_2F2R = FS_CHANNELMODE( 4, 0, 2, 0 ),

     /* 5 Channels (Left Right RearLeft RearRight Subwoofer) */
     FSCM_SURROUND41_2F2R = FS_CHANNELMODE( 5, 0, 2, 1 ),

     /* 4 Channels (Left Center Right Rear) */
     FSCM_SURROUND40_3F1R = FS_CHANNELMODE( 4, 1, 1, 0 ),

     /* 5 Channels (Left Center Right Rear Subwoofer) */
     FSCM_SURROUND41_3F1R = FS_CHANNELMODE( 5, 1, 1, 1 ),

     /* 5 Channels (Left Center Right RearLeft RearRight) */
     FSCM_SURROUND50      = FS_CHANNELMODE( 5, 1, 2, 0 ),

     /* 6 Channels (Left Center Right RearLeft RearRight Subwoofer) */
     FSCM_SURROUND51      = FS_CHANNELMODE( 6, 1, 2, 1 ),
} FSChannelMode;

/* Number of channelmodes defined. */
#define FS_NUM_CHANNELMODES                  13

/* These macros extract information about the channel mode. */
#define FS_CHANNELS_FOR_MODE(mode)            ((mode) & 0x0000000F)

#define FS_MODE_HAS_CENTER(mode)             (((mode) & 0x00000010) != 0)

#define FS_MODE_NUM_REARS(mode)              (((mode) & 0x00000060) >> 5)

#define FS_MODE_HAS_LFE(mode)                (((mode) & 0x00000080) != 0)

/*
 * Description of the static sound buffer that is to be created.
 */
typedef struct {
     FSBufferDescriptionFlags                flags;              /* Field validation. */

     int                                     length;             /* Buffer length specified in number of samples
                                                                    per channel. */
     int                                     channels;           /* Number of channels. */
     FSSampleFormat                          sampleformat;       /* Format of each sample. */
     int                                     samplerate;         /* Number of samples per second (per channel). */
     FSChannelMode                           channelmode;        /* Channel mode (overrides channels). */
} FSBufferDescription;

/*
 * Description of the streaming sound buffer that is to be created.
 */
typedef struct {
     FSStreamDescriptionFlags                flags;              /* Field validation. */

     int                                     buffersize;         /* Ring buffer size specified in a number of samples
                                                                    per channel. */
     int                                     channels;           /* Number of channels. */
     FSSampleFormat                          sampleformat;       /* Format of each sample. */
     int                                     samplerate;         /* Number of samples per second (per channel). */
     int                                     prebuffer;          /* Samples to buffer before starting the playback.
                                                                    A negative value disables auto start of playback. */
     FSChannelMode                           channelmode;        /* Channel mode (overrides channels). */
} FSStreamDescription;

/*
 * IFusionSound is the main interface. It can be retrieved by a
 * call to FusionSoundCreate().
 *
 * Static sound buffers for smaller samples like sound effects in
 * games or audible feedback in UIs are created by calling
 * CreateBuffer(). They can be played several times with an
 * unlimited number of concurrent playbacks. Playback can be
 * started in looping mode. Other per-playback control includes
 * pan value, volume level and pitch.
 *
 * Streaming sound buffers for large or compressed files and for
 * streaming of real time sound data are created by calling
 * CreateStream(). There's only one single playback that
 * automatically starts when data is written to the ring buffer
 * for the first time. If the buffer underruns, the playback
 * automatically stops and continues when the ring buffer is
 * written to again.
 */
D_DEFINE_INTERFACE( IFusionSound,

   /** Hardware capabilities **/

     /*
      * Get a description of the sound device.
      */
     DirectResult (*GetDeviceDescription) (
          IFusionSound                      *thiz,
          FSDeviceDescription               *ret_desc
     );

   /** Buffers **/

     /*
      * Create a static sound buffer.
      *
      * This requires 'desc' to have at least the length being
      * set.
      * Default values for sample rate, sample format and number
      * of channels depend on device configuration.
      */
     DirectResult (*CreateBuffer) (
          IFusionSound                      *thiz,
          const FSBufferDescription         *desc,
          IFusionSoundBuffer               **ret_interface
     );

     /*
      * Create a streaming sound buffer.
      *
      * If 'desc' is NULL, all default values will be used.
      * Default values for sample rate, sample format and number
      * of channels depend on device configuration, the
      * ring buffer length defaults to 1/5 seconds.
      */
     DirectResult (*CreateStream) (
          IFusionSound                      *thiz,
          const FSStreamDescription         *desc,
          IFusionSoundStream               **ret_interface
     );

     /*
      * Create a music provider.
      */
     DirectResult (*CreateMusicProvider) (
          IFusionSound                      *thiz,
          const char                        *filename,
          IFusionSoundMusicProvider        **ret_interface
     );

   /** Volume Control **/

     /*
      * Get master volume level (that applies to all playbacks).
      */
     DirectResult (*GetMasterVolume) (
          IFusionSound                      *thiz,
          float                             *ret_level
     );

     /*
      * Set master volume level (that applies to all playbacks).
      *
      * The level is a linear factor ranging from 0.0f to 1.0f.
      */
     DirectResult (*SetMasterVolume) (
          IFusionSound                      *thiz,
          float                              level
     );

     /*
      * Get local volume level (that applies to the playbacks
      * created by the current process).
      */
     DirectResult (*GetLocalVolume) (
          IFusionSound                      *thiz,
          float                             *ret_level
     );

     /*
      * Set local volume level (that applies to the playbacks
      * created by the current process).
      *
      * The level is a linear factor ranging from 0.0f to 1.0f.
      */
     DirectResult (*SetLocalVolume) (
          IFusionSound                      *thiz,
          float                              level
     );

   /** Misc **/

     /*
      * Suspend, no other calls are allowed until Resume()
      * has been called.
      */
     DirectResult (*Suspend) (
          IFusionSound                      *thiz
     );

     /*
      * Resume, only to be called after Suspend().
      */
     DirectResult (*Resume) (
          IFusionSound                      *thiz
     );

   /** Monitoring **/

     /*
      * Get the actual volume level produced on master output.
      */
     DirectResult (*GetMasterFeedback) (
          IFusionSound                      *thiz,
          float                             *ret_left,
          float                             *ret_right
     );
)

/**********************
 * IFusionSoundBuffer *
 **********************/

/*
 * Flags for simple playback.
 */
typedef enum {
     FSPLAY_NOFX                           = 0x00000000,         /* No effects are applied. */

     FSPLAY_LOOPING                        = 0x00000001,         /* Playback will continue at the beginning of the
                                                                    buffer as soon as the end is reached. There's no gap
                                                                    produced by concatenation. Only one looping playback
                                                                    at a time is supported by the simple playback. */
     FSPLAY_CYCLE                          = 0x00000002,         /* Play the whole buffer for one cycle, wrapping at the
                                                                    end. */
     FSPLAY_REWIND                         = 0x00000004,         /* Play reversing sample order. */

     FSPLAY_ALL                            = 0x00000007          /* All of these. */
} FSBufferPlayFlags;

/*
 * IFusionSoundBuffer represents a static block of sample data.
 *
 * Data access is simply provided by Lock() and Unlock().
 *
 * There are two ways of playback.
 *
 * Simple playback is provided by this interface. It includes an
 * unlimited number of non-looping playbacks plus one looping
 * playback at a time. To start the looping playback with Play()
 * use the FSPLAY_LOOPING playback flag. It will stop when the
 * interface is destroyed or Stop() is called.
 *
 * Advanced playback is provided by an extra interface called
 * IFusionSoundPlayback which is created by CreatePlayback().
 * It includes live control over pan, volume, pitch and provides
 * versatile playback commands.
 */
D_DEFINE_INTERFACE( IFusionSoundBuffer,

   /** Retrieving information **/

     /*
      * Get a description of the buffer.
      */
     DirectResult (*GetDescription) (
          IFusionSoundBuffer                *thiz,
          FSBufferDescription               *ret_desc
     );

   /** Positioning **/

     /*
      * Set the buffer position indicator (in frames) affecting
      * subsequent playback and lock for access.
      */
     DirectResult (*SetPosition) (
          IFusionSoundBuffer                *thiz,
          int                                position
     );

   /** Access **/

     /*
      * Lock a buffer to access its data.
      *
      * Optionally returns the amount of available frames or
      * bytes at the current position.
      */
     DirectResult (*Lock) (
          IFusionSoundBuffer                *thiz,
          void                             **ret_data,
          int                               *ret_frames,
          int                               *ret_bytes
     );

     /*
      * Unlock a buffer.
      */
     DirectResult (*Unlock) (
          IFusionSoundBuffer                *thiz
     );

   /** Simple playback **/

     /*
      * Start playing the buffer at the specified position.
      *
      * There's no limited number of concurrent playbacks, but
      * the simple playback only provides one looping playback
      * at a time.
      */
     DirectResult (*Play) (
          IFusionSoundBuffer                *thiz,
          FSBufferPlayFlags                  flags
     );

     /*
      * Stop looping playback.
      *
      * This method is for the one concurrently looping playback
      * that is provided by the simple playback.
      */
     DirectResult (*Stop) (
          IFusionSoundBuffer                *thiz
     );

   /** Advanced playback **/

     /*
      * Retrieve advanced playback control interface.
      *
      * Each playback instance represents one concurrent
      * playback of the buffer.
      */
     DirectResult (*CreatePlayback) (
          IFusionSoundBuffer                *thiz,
          IFusionSoundPlayback             **ret_interface
     );
)

/**********************
 * IFusionSoundStream *
 **********************/

/*
 * IFusionSoundStream represents a ring buffer for streamed
 * playback which fairly maps to writing to a sound device.
 * Use it for easy porting of applications that use exclusive
 * access to a sound device.
 *
 * Writing to the ring buffer triggers the playback if it's not
 * already running. The method Write() can be called with an
 * arbitrary number of samples. It returns after all samples
 * have been written to the ring buffer and sleeps while the
 * ring buffer is full. Blocking writes are perfect for accurate
 * filling of the buffer, which keeps the ring buffer as full as
 * possible using a very small block size (depending on
 * sample rate, playback pitch and underlying hardware).
 *
 * Waiting for a specific amount of free space in the ring buffer
 * is provided by Wait(). It can be used to avoid blocking of
 * Write() or to finish playback before destroying the interface.
 *
 * Status information includes the amount of filled and total
 * space in the ring buffer, along with the current read and
 * write position. It can be retrieved by calling GetStatus()
 * at any time without blocking.
 */
D_DEFINE_INTERFACE( IFusionSoundStream,

   /** Retrieving information **/

     /*
      * Get a description of the stream.
      */
     DirectResult (*GetDescription) (
          IFusionSoundStream                *thiz,
          FSStreamDescription               *ret_desc
     );

   /** Ring buffer **/

     /*
      * Write the sample data into the ring buffer.
      *
      * The length specifies the number of samples per channel.
      * If the ring buffer gets full, the method blocks until
      * it can write more data.
      */
     DirectResult (*Write) (
          IFusionSoundStream                *thiz,
          const void                        *sample_data,
          int                                length
     );

     /*
      * Wait for a specified amount of free ring buffer space.
      *
      * This method blocks until there's free space of at least
      * the specified length (number of samples per channel).
      * Specifying a length of zero waits until playback has
      * finished.
      */
     DirectResult (*Wait) (
          IFusionSoundStream                *thiz,
          int                                length
     );

     /*
      * Query ring buffer status.
      *
      * Returns the number of samples the ring buffer is filled
      * with, the total number of samples that can be stored, the
      * current read and write position, and whether the stream
      * is playing.
      */
     DirectResult (*GetStatus) (
          IFusionSoundStream                *thiz,
          int                               *filled,
          int                               *total,
          int                               *read_position,
          int                               *write_position,
          bool                              *playing
     );

     /*
      * Flush the ring buffer.
      *
      * This method stops the playback immediately and discards
      * any buffered data.
      */
     DirectResult (*Flush) (
          IFusionSoundStream                *thiz
     );

     /*
      * Drop pending data.
      *
      * This method discards all pending input data, causing
      * Write() to return as soon as possible.
      */
     DirectResult (*Drop) (
          IFusionSoundStream                *thiz
     );

   /** Timing **/

     /*
      * Query the presentation delay.
      *
      * Returns the amount of time in milliseconds that passes
      * until the last sample stored in the buffer is audible.
      * This includes any buffered data (by hardware or driver)
      * as well as the ring buffer status of the stream. Even if
      * the stream is not playing (due to pre-buffering), this
      * method behaves as if the playback has just been started.
      */
     DirectResult (*GetPresentationDelay) (
          IFusionSoundStream                *thiz,
          int                               *ret_delay
     );

   /** Playback control **/

     /*
      * Retrieve advanced playback control interface.
      *
      * The returned interface provides advanced control over
      * the playback of the stream. This includes volume, pitch
      * and pan settings as well as manual starting, pausing or
      * stopping of the playback.
      */
     DirectResult (*GetPlayback) (
          IFusionSoundStream                *thiz,
          IFusionSoundPlayback             **ret_interface
     );

   /** Direct memory access **/

     /*
      * Access the ring buffer to fill it with data.
      *
      * This method returns a pointer to the current write
      * position and the amount of available space in frames.
      * If the ring buffer is full, the method blocks until
      * there is space available.
      * After filling the ring buffer, call Commit() to submit
      * the samples to the stream.
      */
     DirectResult (*Access) (
          IFusionSoundStream                *thiz,
          void                             **ret_data,
          int                               *ret_frames
     );

     /*
      * Commit written data of size 'length' to the stream.
      */
     DirectResult (*Commit) (
          IFusionSoundStream                *thiz,
          int                                length
     );
)

/************************
 * IFusionSoundPlayback *
 ************************/

/*
 * Direction of a playback.
 */
typedef enum {
     FSPD_FORWARD                          =  1,                 /* Forward. */
     FSPD_BACKWARD                         = -1,                 /* Backward. */
} FSPlaybackDirection;

/*
 * IFusionSoundPlayback represents one concurrent playback and
 * provides full control over the internal processing of samples.
 *
 * Commands control the playback.
 * This includes starting the playback at any position with an
 * optional stop position. A value of zero causes the playback
 * to stop at the end and a negative value puts the playback in
 * looping mode. If the playback is already running, Start()
 * does seeking and updates the stop position.
 * Other methods provide pausing, stopping and waiting for the
 * playback to end.
 *
 * Information provided by GetStatus() includes the current
 * position and whether the playback is running.
 *
 * Parameters provide live control over volume, pan, pitch and
 * direction of the playback.
 */
D_DEFINE_INTERFACE( IFusionSoundPlayback,

   /** Commands **/

     /*
      * Start playback of the buffer.
      *
      * This method is only supported for playback of a buffer.
      * For stream playbacks use Continue().
      * The 'start' position specifies the sample at which the
      * playback is going to start.
      * The 'stop' position specifies the sample after the last
      * sample being played. A value of zero causes the playback
      * to stop after the last sample in the buffer.
      * A negative value means unlimited playback (looping).
      * This method can be used for seeking if the playback is
      * already running.
      */
     DirectResult (*Start) (
          IFusionSoundPlayback              *thiz,
          int                                start,
          int                                stop
     );

     /*
      * Stop playback of the buffer.
      *
      * This method stops a running playback. The playback can
      * be continued by calling Continue() or restarted using
      * Start().
      */
     DirectResult (*Stop) (
          IFusionSoundPlayback              *thiz
     );

     /*
      * Continue playback of the buffer or start playback of a
      * stream (playback that no longer runs).
      *
      * This method is used to continue a playback that is no
      * longer in progress. Playback will begin at the position
      * where it stopped, either explicitly by Stop() or by
      * reaching the stop position.
      * If the playback has never been started, it uses the
      * default start and stop position which means non-looping
      * playback from the beginning to the end.
      * It returns without an error if the playback is running.
      */
     DirectResult (*Continue) (
          IFusionSoundPlayback              *thiz
     );

     /*
      * Wait until playback of the buffer has finished.
      *
      * This method will block as long as the playback is
      * running. If the playback is in looping mode the method
      * returns immediately with an error.
      */
     DirectResult (*Wait) (
          IFusionSoundPlayback              *thiz
     );

   /** Information **/

     /*
      * Get the current playback status.
      *
      * This method can be used to check if the playback is
      * running. It also returns the current playback position
      * or the position where Continue() would start playing.
      */
     DirectResult (*GetStatus) (
          IFusionSoundPlayback              *thiz,
          bool                              *running,
          int                               *ret_position
     );

   /** Parameters **/

     /*
      * Set volume level.
      *
      * The volume level is a linear factor being 1.0f by default
      * and can vary from 0.0f to 64.0f.
      */
     DirectResult (*SetVolume) (
          IFusionSoundPlayback              *thiz,
          float                              level
     );

     /*
      * Set panning value.
      *
      * The panning value ranges from -1.0f (left) to 1.0f (right).
      */
     DirectResult (*SetPan) (
          IFusionSoundPlayback              *thiz,
          float                              value
     );

     /*
      * Set pitch value.
      *
      * The pitch value is a linear factor being 1.0f by default
      * and can vary from 0.0f to 64.0f.
      */
     DirectResult (*SetPitch) (
          IFusionSoundPlayback              *thiz,
          float                              value
     );

     /*
      * Set the direction of the playback.
      */
     DirectResult (*SetDirection) (
          IFusionSoundPlayback              *thiz,
          FSPlaybackDirection                direction
     );

     /*
      * Set the volume levels for downmixing.
      *
      * Set the levels used for downmixing the center and rear
      * channels of a multichannel buffer (more than 2 channels).
      * Levels are linear factors ranging from 0.0f to 1.0f and
      * being 0.707f (-3 dB) by default.
      */
     DirectResult (*SetDownmixLevels) (
          IFusionSoundPlayback              *thiz,
          float                              center,
          float                              rear
     );
)

/*****************************
 * IFusionSoundMusicProvider *
 *****************************/

/*
 * The music provider capabilities.
 */
typedef enum {
     FMCAPS_BASIC                          = 0x00000000,         /* basic ops (play, stop) */
     FMCAPS_SEEK                           = 0x00000001,         /* supports seek to a position */
     FMCAPS_RESAMPLE                       = 0x00000002,         /* supports audio resampling */
     FMCAPS_HALFRATE                       = 0x00000004          /* supports decoding at half original rate */
} FSMusicProviderCapabilities;

#define FS_TRACK_DESC_ARTIST_LENGTH           32
#define FS_TRACK_DESC_TITLE_LENGTH           125
#define FS_TRACK_DESC_ALBUM_LENGTH           125
#define FS_TRACK_DESC_GENRE_LENGTH            32
#define FS_TRACK_DESC_ENCODING_LENGTH         32

/*
 * Information about of a track.
 */
typedef struct {
     char  artist[FS_TRACK_DESC_ARTIST_LENGTH];                  /* artist */
     char  title[FS_TRACK_DESC_TITLE_LENGTH];                    /* title */
     char  album[FS_TRACK_DESC_ALBUM_LENGTH];                    /* album */

     short                                   year;               /* year */

     char  genre[FS_TRACK_DESC_GENRE_LENGTH];                    /* genre */
     char  encoding[FS_TRACK_DESC_ENCODING_LENGTH];              /* encoding (e.g. "mp3") */

     int                                     bitrate;            /* amount of bits per second */
     float                                   replaygain;         /* ReplayGain level (1.0 by default) */
     float                                   replaygain_album;   /* album ReplayGain level */
} FSTrackDescription;

/*
 * Status of a music provider.
 */
typedef enum {
     FMSTATE_UNKNOWN                       = 0x00000000,         /* unknown status */

     FMSTATE_PLAY                          = 0x00000001,         /* playing */
     FMSTATE_STOP                          = 0x00000002,         /* playback was stopped */
     FMSTATE_FINISHED                      = 0x00000004,         /* playback is finished */

     FMSTATE_ALL                           = 0x00000007          /* all of these */
} FSMusicProviderStatus;

/*
 * Flags controlling playback mode of a music provider.
 */
typedef enum {
     FMPLAY_NOFX                           = 0x000000000,        /* normal playback */
     FMPLAY_LOOPING                        = 0x000000001,        /* automatically restart playback when end-of-stream is
                                                                    reached */
} FSMusicProviderPlaybackFlags;


/*
 * Return value of buffer write callback.
 */
typedef enum {
     FMBCR_OK                              = 0x00000000,         /* Continue. */
     FMBCR_BREAK                           = 0x00000001,         /* Stop loading. */
} FMBufferCallbackResult;

/*
 * Called after each buffer write.
 */
typedef FMBufferCallbackResult (*FMBufferCallback) (
     int                                     length,
     void                                   *ctx
);

/*
 * Called for each available track.
 */
typedef DirectEnumerationResult (*FSTrackCallback) (
     FSTrackID                               track_id,
     FSTrackDescription                      desc,
     void                                   *ctx
);

/*
 * IFusionSoundMusicProvider is the music provider interface.
 */
D_DEFINE_INTERFACE( IFusionSoundMusicProvider,

   /** Retrieving information **/

     /*
      * Retrieve information about the music provider's
      * capabilities.
      */
     DirectResult (*GetCapabilities) (
          IFusionSoundMusicProvider         *thiz,
          FSMusicProviderCapabilities       *ret_caps
     );

     /*
      * Enumerate all tracks contained in the file.
      *
      * Calls the given callback for all available tracks.
      * The callback is passed the track id that can be used
      * to select a track for playback using SelectTrack().
      */
     DirectResult (*EnumTracks) (
          IFusionSoundMusicProvider         *thiz,
          FSTrackCallback                    callback,
          void                              *ctx
     );

     /*
      * Get the unique ID of the current track.
      */
     DirectResult (*GetTrackID) (
          IFusionSoundMusicProvider         *thiz,
          FSTrackID                         *ret_track_id
     );

     /*
      * Get a description of the current track.
      */
     DirectResult (*GetTrackDescription) (
          IFusionSoundMusicProvider         *thiz,
          FSTrackDescription                *ret_desc
     );

     /*
      * Get a stream description that best matches the music
      * contained in the file.
      */
     DirectResult (*GetStreamDescription) (
          IFusionSoundMusicProvider         *thiz,
          FSStreamDescription               *ret_desc
     );

     /*
      * Get a buffer description that best matches the music
      * contained in the file.
      */
     DirectResult (*GetBufferDescription) (
          IFusionSoundMusicProvider         *thiz,
          FSBufferDescription               *ret_desc
     );

   /** Playback **/

     /*
      * Select a track by its unique ID.
      */
     DirectResult (*SelectTrack) (
          IFusionSoundMusicProvider         *thiz,
          FSTrackID                          track_id
     );

     /*
      * Play the selected track by rendering it to the
      * destination stream.
      */
     DirectResult (*PlayToStream) (
          IFusionSoundMusicProvider         *thiz,
          IFusionSoundStream                *destination
     );

     /*
      * Play the selected track by rendering it to the
      * destination buffer.
      *
      * Optionally a callback can be registered that is called
      * after each buffer write.
      * The callback is passed the number of samples per channel
      * actually written to the destination buffer.
      */
     DirectResult (*PlayToBuffer) (
          IFusionSoundMusicProvider         *thiz,
          IFusionSoundBuffer                *destination,
          FMBufferCallback                   callback,
          void                              *ctx
     );

     /*
      * Stop playback.
      */
     DirectResult (*Stop) (
          IFusionSoundMusicProvider         *thiz
     );

     /*
      * Get playback status.
      */
     DirectResult (*GetStatus) (
          IFusionSoundMusicProvider         *thiz,
          FSMusicProviderStatus             *ret_status
     );

   /** Media Control **/

     /*
      * Seek to a position within the current track.
      */
     DirectResult (*SeekTo) (
          IFusionSoundMusicProvider         *thiz,
          double                             seconds
     );

     /*
      * Get current position within the current track.
      */
     DirectResult (*GetPos) (
          IFusionSoundMusicProvider         *thiz,
          double                            *ret_seconds
     );

     /*
      * Get the length of the current track.
      */
     DirectResult (*GetLength) (
          IFusionSoundMusicProvider         *thiz,
          double                            *ret_seconds
     );

   /** Advanced Playback **/

     /*
      * Set the flags controlling playback mode.
      */
     DirectResult (*SetPlaybackFlags) (
          IFusionSoundMusicProvider         *thiz,
          FSMusicProviderPlaybackFlags       flags
     );

     /*
      * Wait for playback status.
      *
      * This method blocks until playback reaches one of the
      * states specified in 'mask' or until 'timeout' expires
      * if the specified 'timeout' in milliseconds is non-zero.
      */
     DirectResult (*WaitStatus) (
          IFusionSoundMusicProvider         *thiz,
          FSMusicProviderStatus              mask,
          unsigned int                       timeout
     );
)

#ifdef __cplusplus
}
#endif

#endif
