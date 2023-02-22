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

#include <config.h>
#include <direct/stream.h>
#include <direct/thread.h>
#include <fusionsound_util.h>
#include <media/ifusionsoundmusicprovider.h>

D_DEBUG_DOMAIN( MusicProvider_WAVE, "MusicProvider/WAVE", "WAVE Music Provider" );

static DirectResult Probe    ( IFusionSoundMusicProvider_ProbeContext *ctx );

static DirectResult Construct( IFusionSoundMusicProvider              *thiz,
                               const char                             *filename,
                               DirectStream                           *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, WAVE )

/**********************************************************************************************************************/

typedef struct {
     int                           ref;                     /* reference counter */

     DirectStream                 *stream;

     u16                           channels;
     FSSampleFormat                sampleformat;
     u32                           samplerate;

     int                           framesize;               /* bytes per frame */
     u32                           headsize;                /* size of headers */
     u32                           datasize;                /* size of PCM data */

     FSTrackDescription            desc;

     FSMusicProviderPlaybackFlags  flags;

     DirectThread                 *thread;
     DirectMutex                   lock;
     DirectWaitQueue               cond;

     FSMusicProviderStatus         status;
     int                           finished;
     bool                          seeked;

     void                         *buf;

     struct {
          IFusionSoundStream      *stream;
          IFusionSoundBuffer      *buffer;
          FSSampleFormat           sampleformat;
          FSChannelMode            mode;
          int                      length;
     } dest;

     FMBufferCallback              buffer_callback;
     void                         *buffer_callback_context;
} IFusionSoundMusicProvider_WAVE_data;

/**********************************************************************************************************************/

typedef struct {
#ifdef WORDS_BIGENDIAN
     s8 c;
     u8 b;
     u8 a;
#else
     u8 a;
     u8 b;
     s8 c;
#endif
} __attribute__((packed)) s24;

static __inline__ int
getsamp( u8             *buf,
         const int       i,
         FSSampleFormat  f )
{
     switch (f) {
          case FSSF_U8:
               return (buf[i] ^ 0x80) << 22;

          case FSSF_S16:
#ifdef WORDS_BIGENDIAN
               return BSWAP16( ((u16*) buf)[i] ) << 14;
#else
               return ((s16*) buf)[i] << 14;
#endif

          case FSSF_S24:
#ifdef WORDS_BIGENDIAN
               return (buf[i*3] << 22) | (buf[i*3+1] << 14) | (buf[i*3+2] << 6);
#else
               return (buf[i*3+2] << 22) | (buf[i*3+1] << 14) | (buf[i*3] << 6);
#endif

          case FSSF_S32:
#ifdef WORDS_BIGENDIAN
               return BSWAP32( ((u32*) buf)[i] ) >> 2;
#else
               return ((s32*) buf)[i] >> 2;
#endif
          default:
               break;
     }

     return 0;
}

static __inline__ u8 *
putsamp( u8             *dst,
         FSSampleFormat  f,
         int             s )
{
     switch (f) {
          case FSSF_U8:
               *dst = (s >> 22) ^ 0x80;
               dst++;
               break;

          case FSSF_S16:
               *((s16*) dst) = s >> 14;
               dst += 2;
               break;

          case FSSF_S24:
               ((s24*) dst)->c = s >> 22;
               ((s24*) dst)->b = s >> 14;
               ((s24*) dst)->a = s >>  6;
               dst += 3;
               break;

          case FSSF_S32:
               *((s32*) dst) = s << 2;
               dst += 4;
               break;

          case FSSF_FLOAT:
               *((float*) dst) = (float) s / (1 << 29);
               dst += 4;
               break;

          default:
               break;
     }

     return dst;
}

static void
wave_mix_audio( u8             *buf,
                u8             *dst,
                int             frames,
                FSSampleFormat  sf,
                FSSampleFormat  df,
                int             channels,
                FSChannelMode   mode )
{
                /* L  C  R Rl Rr LFE */
     int c[6]  = { 0, 0, 0, 0, 0,  0 };
     int bytes = FS_BYTES_PER_SAMPLE( sf ) * channels;

#define clip(s)                 \
     if ((s) >= (1 << 29))      \
          (s) = (1 << 29) - 1;  \
     else if ((s) < -(1 << 29)) \
          (s) = -(1 << 29);     \

     while (frames--) {
          int s;

          switch (channels) {
               case 1:
                    c[0] =
                    c[2] = getsamp( buf, 0, sf );
                    break;
               case 2:
                    c[0] = getsamp( buf, 0, sf );
                    c[2] = getsamp( buf, 1, sf );
                    break;
               case 3:
                    c[0] = getsamp( buf, 0, sf );
                    c[1] = getsamp( buf, 1, sf );
                    c[2] = getsamp( buf, 2, sf );
                    break;
               case 4:
                    c[0] = getsamp( buf, 0, sf );
                    c[2] = getsamp( buf, 1, sf );
                    c[3] = getsamp( buf, 2, sf );
                    c[4] = getsamp( buf, 3, sf );
                    break;
               default:
                    c[0] = getsamp( buf, 0, sf );
                    c[1] = getsamp( buf, 1, sf );
                    c[2] = getsamp( buf, 2, sf );
                    c[3] = getsamp( buf, 3, sf );
                    c[4] = getsamp( buf, 4, sf );
                    if (channels > 5)
                         c[5] = getsamp( buf, 5, sf );
                    break;
          }

          buf += bytes;

          switch (mode) {
               case FSCM_MONO:
                    s = c[0] + c[2];
                    if (channels > 2) {
                         int sum = (c[1] << 1) + c[3] + c[4];
                         s += sum - (sum >> 2);
                         s >>= 1;
                         clip( s );
                    }
                    else {
                         s >>= 1;
                    }
                    dst = putsamp( dst, df, s );
                    break;
               case FSCM_STEREO:
               case FSCM_STEREO21:
                    s = c[0];
                    if (channels > 2) {
                         int sum = c[1] + c[3];
                         s += sum - (sum >> 2);
                         clip( s );
                    }
                    dst = putsamp( dst, df, s );
                    s = c[2];
                    if (channels > 2) {
                         int sum = c[1] + c[4];
                         s += sum - (sum >> 2);
                         clip( s );
                    }
                    dst = putsamp( dst, df, s );
                    if (FS_MODE_HAS_LFE( mode ))
                         dst = putsamp( dst, df, c[5] );
                    break;
               case FSCM_STEREO30:
               case FSCM_STEREO31:
                    s = c[0] + (c[3] - (c[3] >> 2));
                    clip( s );
                    dst = putsamp( dst, df, s );
                    if (channels == 2 || channels == 4)
                         dst = putsamp( dst, df, (c[0] + c[2]) >> 1 );
                    else
                         dst = putsamp( dst, df, c[1] );
                    s = c[2] + (c[4] - (c[4] >> 2));
                    clip( s );
                    dst = putsamp( dst, df, s );
                    if (FS_MODE_HAS_LFE( mode ))
                         dst = putsamp( dst, df, c[5] );
                    break;
               default:
                    if (FS_MODE_HAS_CENTER( mode )) {
                         dst = putsamp( dst, df, c[0] );
                         if (channels == 2 || channels == 4)
                              dst = putsamp( dst, df, (c[0] + c[2]) >> 1 );
                         else
                              dst = putsamp( dst, df, c[1] );
                         dst = putsamp( dst, df, c[2] );
                    }
                    else {
                         c[0] += c[1] - (c[1] >> 2);
                         c[2] += c[1] - (c[1] >> 2);
                         clip( c[0] );
                         clip( c[2] );
                         dst = putsamp( dst, df, c[0] );
                         dst = putsamp( dst, df, c[2] );
                    }
                    if (FS_MODE_NUM_REARS( mode ) == 1) {
                         s = (c[3] + c[4]) >> 1;
                         dst = putsamp( dst, df, s );
                    }
                    else {
                         dst = putsamp( dst, df, c[3] );
                         dst = putsamp( dst, df, c[4] );
                    }
                    if (FS_MODE_HAS_LFE( mode ))
                         dst = putsamp( dst, df, c[5] );
                    break;
          }
     }

#undef clip
}

/**********************************************************************************************************************/

static void
WAVE_Stop( IFusionSoundMusicProvider_WAVE_data *data,
           bool                                 now )
{
     data->status = FMSTATE_STOP;

     if (data->thread) {
          if (!direct_thread_is_joined( data->thread )) {
               if (now) {
                    direct_thread_cancel( data->thread );
                    direct_thread_join( data->thread );
               }
               else {
                    /* Mutex must already be locked. */
                    direct_mutex_unlock( &data->lock );
                    direct_thread_join( data->thread );
                    direct_mutex_lock( &data->lock );
               }
          }
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }
}

static void *
WAVEStream( DirectThread *thread,
            void         *arg )
{
     IFusionSoundMusicProvider_WAVE_data *data  = arg;
     int                                  count = data->dest.length * data->framesize;

     while (data->status == FMSTATE_PLAY) {
          DirectResult    ret;
          int             bytes;
          int             frames;
          void           *dst;
          unsigned int    len = 0;
          struct timeval  tv  = { 0, 1000 };

          direct_mutex_lock( &data->lock );

          if (data->status != FMSTATE_PLAY) {
               direct_mutex_unlock( &data->lock );
               break;
          }

          if (data->seeked) {
               data->dest.stream->Flush( data->dest.stream );
               data->seeked = false;
          }

          /* Direct copy. */
          if (!data->buf) {
               ret = data->dest.stream->Access( data->dest.stream, &dst, &frames );
               if (ret) {
                    direct_mutex_unlock( &data->lock );
                    continue;
               }
               bytes = frames * data->framesize;
          }
          else {
               dst   = data->buf;
               bytes = count;
          }

          ret = direct_stream_wait( data->stream, bytes, &tv );
          if (ret != DR_TIMEOUT) {
               ret = direct_stream_read( data->stream, bytes, dst, &len );
               len /= data->framesize;
          }

          if (!data->buf)
               data->dest.stream->Commit( data->dest.stream, len );

          if (ret) {
               if (ret == DR_EOF) {
                    if (data->flags & FMPLAY_LOOPING) {
                         direct_stream_seek( data->stream, data->headsize );
                    }
                    else {
                         data->finished = true;
                         data->status   = FMSTATE_FINISHED;
                         direct_waitqueue_broadcast( &data->cond );
                    }
               }
               direct_mutex_unlock( &data->lock );
               continue;
          }

          direct_mutex_unlock( &data->lock );

          if (len < 1)
               continue;

          if (data->buf) {
               unsigned int pos = 0;

               /* Converting to output format. */
               while (pos < len) {
                    if (data->dest.stream->Access( data->dest.stream, &dst, &frames ))
                         break;

                    if (frames > len - pos)
                         frames = len - pos;

                    wave_mix_audio( data->buf + pos * data->framesize, dst, frames, data->sampleformat,
                                    data->dest.sampleformat, data->channels, data->dest.mode );

                    data->dest.stream->Commit( data->dest.stream, frames );

                    pos += frames;
               }
          }
          else {
               data->dest.stream->Wait( data->dest.stream, 1 );
          }
     }

     return NULL;
}

static void *
WAVEBuffer( DirectThread *thread,
            void         *arg )
{
     IFusionSoundMusicProvider_WAVE_data *data  = arg;
     size_t                               count = data->dest.length * data->framesize;

     while (data->status == FMSTATE_PLAY) {
          DirectResult    ret;
          int             bytes;
          int             frames;
          void           *dst;
          unsigned int    len = 0;
          struct timeval  tv  = { 0, 1000 };

          direct_mutex_lock( &data->lock );

          if (data->status != FMSTATE_PLAY) {
               direct_mutex_unlock( &data->lock );
               break;
          }

          /* Direct copy. */
          if (!data->buf) {
               ret = data->dest.buffer->Lock( data->dest.buffer, &dst, NULL, &bytes );
               if (ret) {
                    D_DERROR( ret, "MusicProvider/WAVE: Could not lock buffer!\n" );
                    direct_mutex_unlock( &data->lock );
                    break;
               }
          }
          else {
               dst   = data->buf;
               bytes = count;
          }

          ret = direct_stream_wait( data->stream, bytes, &tv );
          if (ret != DR_TIMEOUT) {
               ret = direct_stream_read( data->stream, bytes, dst, &len );
               len /= data->framesize;
          }

          if (!data->buf)
               data->dest.buffer->Unlock( data->dest.buffer );

          if (ret) {
               if (ret == DR_EOF) {
                    if (data->flags & FMPLAY_LOOPING) {
                         direct_stream_seek( data->stream, data->headsize );
                    }
                    else {
                         data->finished = true;
                         data->status = FMSTATE_FINISHED;
                         direct_waitqueue_broadcast( &data->cond );
                    }
               }
               direct_mutex_unlock( &data->lock );
               continue;
          }

          direct_mutex_unlock( &data->lock );

          if (len < 1)
               continue;

          if (data->buf) {
               /* Converting to output format. */
               while (len > 0) {
                    ret = data->dest.buffer->Lock( data->dest.buffer, &dst, &frames, NULL );
                    if (ret) {
                         D_DERROR( ret, "MusicProvider/WAVE: Could not lock buffer!\n" );
                         break;
                    }

                    if (frames > len)
                         frames = len;

                    wave_mix_audio( data->buf, dst, frames, data->sampleformat,
                                    data->dest.sampleformat, data->channels, data->dest.mode );

                    data->dest.buffer->Unlock( data->dest.buffer );

                    len -= frames;

                    if (data->buffer_callback) {
                         if (data->buffer_callback( frames, data->buffer_callback_context )) {
                              data->status = FMSTATE_STOP;
                              direct_waitqueue_broadcast( &data->cond );
                              break;
                         }
                    }
               }
          }
          else {
               if (data->buffer_callback) {
                    if (data->buffer_callback( len, data->buffer_callback_context )) {
                         data->status = FMSTATE_STOP;
                         direct_waitqueue_broadcast( &data->cond );
                    }
               }
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

static void
IFusionSoundMusicProvider_WAVE_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_WAVE_data *data = thiz->priv;

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     WAVE_Stop( data, true );

     direct_stream_destroy( data->stream );

     direct_waitqueue_deinit( &data->cond );
     direct_mutex_deinit( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IFusionSoundMusicProvider_WAVE_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IFusionSoundMusicProvider_WAVE_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                FSMusicProviderCapabilities *ret_caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_caps)
          return DR_INVARG;

     *ret_caps = FMCAPS_BASIC;
     if (direct_stream_seekable( data->stream ))
          *ret_caps |= FMCAPS_SEEK;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                    FSTrackDescription        *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DR_INVARG;

     *ret_desc = data->desc;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                     FSStreamDescription       *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags        = FSSDF_BUFFERSIZE | FSSDF_CHANNELS | FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
     ret_desc->buffersize   = data->samplerate / 10;
     ret_desc->channels     = data->channels;
     ret_desc->sampleformat = data->sampleformat;
     ret_desc->samplerate   = data->samplerate;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                     FSBufferDescription       *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags        = FSBDF_LENGTH | FSBDF_CHANNELS | FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE;
     ret_desc->length       = MIN( data->datasize / data->framesize, FS_MAX_FRAMES );
     ret_desc->channels     = data->channels;
     ret_desc->sampleformat = data->sampleformat;
     ret_desc->samplerate   = data->samplerate;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_PlayToStream( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundStream        *destination )
{
     FSStreamDescription desc;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!destination)
          return DR_INVARG;

     if (destination == data->dest.stream)
          return DR_OK;

     destination->GetDescription( destination, &desc );

     if (desc.samplerate != data->samplerate)
          return DR_UNSUPPORTED;

     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
          case FSSF_FLOAT:
               break;
          default:
               return DR_UNSUPPORTED;
     }

     if (desc.channels > 6)
          return DR_UNSUPPORTED;

     switch (desc.channelmode) {
          case FSCM_MONO:
          case FSCM_STEREO:
          case FSCM_STEREO21:
          case FSCM_STEREO30:
          case FSCM_STEREO31:
          case FSCM_SURROUND30:
          case FSCM_SURROUND31:
          case FSCM_SURROUND40_2F2R:
          case FSCM_SURROUND41_2F2R:
          case FSCM_SURROUND40_3F1R:
          case FSCM_SURROUND41_3F1R:
          case FSCM_SURROUND50:
          case FSCM_SURROUND51:
               break;
          default:
               return DR_UNSUPPORTED;
     }

     direct_mutex_lock( &data->lock );

     WAVE_Stop( data, false );

     if (desc.sampleformat != data->sampleformat || desc.channelmode != fs_mode_for_channels( data->channels )) {
          data->buf = D_MALLOC( desc.buffersize * data->channels * FS_BYTES_PER_SAMPLE( data->sampleformat ) );
          if (!data->buf) {
               direct_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }

     /* Increase the sound stream reference counter. */
     destination->AddRef( destination );

     data->dest.stream       = destination;
     data->dest.sampleformat = desc.sampleformat;
     data->dest.mode         = desc.channelmode;
     data->dest.length       = desc.buffersize;

     if (data->finished) {
          direct_stream_seek( data->stream, data->headsize );
          data->finished = false;
     }

     data->status = FMSTATE_PLAY;

     direct_waitqueue_broadcast( &data->cond );

     data->thread = direct_thread_create( DTT_DEFAULT, WAVEStream, data, "WAVE Stream" );

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundBuffer        *destination,
                                             FMBufferCallback           callback,
                                             void                      *ctx )
{
     FSBufferDescription desc;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!destination)
          return DR_INVARG;

     if (destination == data->dest.buffer)
          return DR_OK;

     destination->GetDescription( destination, &desc );

     if (desc.samplerate != data->samplerate)
          return DR_UNSUPPORTED;

     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
          case FSSF_FLOAT:
               break;
          default:
               return DR_UNSUPPORTED;
     }

     if (desc.channels > 6)
          return DR_UNSUPPORTED;

     switch (desc.channelmode) {
          case FSCM_MONO:
          case FSCM_STEREO:
          case FSCM_STEREO21:
          case FSCM_STEREO30:
          case FSCM_STEREO31:
          case FSCM_SURROUND30:
          case FSCM_SURROUND31:
          case FSCM_SURROUND40_2F2R:
          case FSCM_SURROUND41_2F2R:
          case FSCM_SURROUND40_3F1R:
          case FSCM_SURROUND41_3F1R:
          case FSCM_SURROUND50:
          case FSCM_SURROUND51:
               break;
          default:
               return DR_UNSUPPORTED;
     }

     direct_mutex_lock( &data->lock );

     WAVE_Stop( data, false );

     if (desc.sampleformat != data->sampleformat || desc.channelmode != fs_mode_for_channels( data->channels )) {
          data->buf = D_MALLOC( desc.length * data->channels * FS_BYTES_PER_SAMPLE( data->sampleformat ) );
          if (!data->buf) {
               direct_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }

     /* Increase the sound buffer reference counter. */
     destination->AddRef( destination );

     data->dest.buffer             = destination;
     data->dest.sampleformat       = desc.sampleformat;
     data->dest.mode               = desc.channelmode;
     data->dest.length             = desc.length;
     data->buffer_callback         = callback;
     data->buffer_callback_context = ctx;

     if (data->finished) {
          direct_stream_seek( data->stream, data->headsize );
          data->finished = false;
     }

     data->status = FMSTATE_PLAY;

     direct_waitqueue_broadcast( &data->cond );

     data->thread = direct_thread_create( DTT_DEFAULT, WAVEBuffer, data, "WAVE Buffer" );

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->lock );

     WAVE_Stop( data, false );

     direct_waitqueue_broadcast( &data->cond );

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetStatus( IFusionSoundMusicProvider *thiz,
                                          FSMusicProviderStatus     *ret_status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_status)
          return DR_INVARG;

     *ret_status = data->status;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_SeekTo( IFusionSoundMusicProvider *thiz,
                                       double                     seconds )
{
     DirectResult ret;
     unsigned int offset;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (seconds < 0.0)
          return DR_INVARG;

     offset = data->samplerate * seconds;
     offset = offset * data->framesize;

     if (data->datasize && offset > data->datasize)
          return DR_UNSUPPORTED;

     offset += data->headsize;

     direct_mutex_lock( &data->lock );

     ret = direct_stream_seek( data->stream, offset );
     if (ret == DR_OK) {
          data->seeked   = true;
          data->finished = false;
     }

     direct_mutex_unlock( &data->lock );

     return ret;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetPos( IFusionSoundMusicProvider *thiz,
                                       double                    *ret_seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_seconds)
          return DR_INVARG;

     *ret_seconds = (double) direct_stream_offset( data->stream ) / (data->samplerate * data->framesize);

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_GetLength( IFusionSoundMusicProvider *thiz,
                                          double                    *ret_seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_seconds)
          return DR_INVARG;

     *ret_seconds = (double) data->datasize / (data->samplerate * data->framesize);

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                 FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (flags & ~FMPLAY_LOOPING)
          return DR_UNSUPPORTED;

     if (flags & FMPLAY_LOOPING && !direct_stream_seekable( data->stream ))
          return DR_UNSUPPORTED;

     data->flags = flags;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_WAVE_WaitStatus( IFusionSoundMusicProvider *thiz,
                                           FSMusicProviderStatus      mask,
                                           unsigned int               timeout )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     if (!mask || mask & ~FMSTATE_ALL)
          return DR_INVARG;

     if (timeout) {
          long long s;

          s = direct_clock_get_abs_micros() + timeout * 1000ll;

          while (direct_mutex_trylock( &data->lock )) {
               usleep( 1000 );
               if (direct_clock_get_abs_micros() >= s)
                    return DR_TIMEOUT;
          }

          while (!(data->status & mask)) {
               ret = direct_waitqueue_wait_timeout( &data->cond, &data->lock, timeout * 1000ll );
               if (ret) {
                    direct_mutex_unlock( &data->lock );
                    return ret;
               }
          }
     }
     else {
          direct_mutex_lock( &data->lock );

          while (!(data->status & mask))
               direct_waitqueue_wait( &data->cond, &data->lock );
     }

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header, "RIFF", 4 ) && !memcmp( ctx->header + 8, "WAVEfmt ", 8 ))
          return DR_OK;

     return DR_UNSUPPORTED;
}

static DirectResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream )
{
     DirectResult  ret = DR_UNSUPPORTED;
     char          id[4];
     u32           size;
     u32           fmt_size;
     u16           compression;
     u32           byterate;
     u16           blockalign;
     u16           bitspersample;
     u32           data_size;
     const char   *fmt;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_WAVE )

     D_DEBUG_AT( MusicProvider_WAVE, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref    = 1;
     data->stream = direct_stream_dup( stream );

#define wave_read( buf, bytes ) {                                         \
     unsigned int len;                                                    \
     direct_stream_wait( stream, bytes, NULL );                           \
     if (direct_stream_read( stream, bytes, buf, &len ) || len < (bytes)) \
          goto error;                                                     \
}

     /* 4 bytes: ChunkID */
     wave_read( id, 4 );
     if (id[0] != 'R' || id[1] != 'I' || id[2] != 'F' || id[3] != 'F') {
          D_ERROR( "MusicProvider/WAVE: No RIFF header found!\n" );
          goto error;
     }

     /* 4 bytes: ChunkSize */
     wave_read( &size, 4 );

     /* 4 bytes: WaveID */
     wave_read( id, 4 );
     if (id[0] != 'W' || id[1] != 'A' || id[2] != 'V' || id[3] != 'E') {
          D_ERROR( "MusicProvider/WAVE: No WAVE header found!\n" );
          goto error;
     }

     /* 4 bytes: Format ChunkID */
     wave_read( id, 4 );
     if (id[0] != 'f' || id[1] != 'm' || id[2] != 't' || id[3] != ' ') {
          D_ERROR( "MusicProvider/WAVE: No fmt header found!\n" );
          goto error;
     }

     /* 4 bytes: Format ChunkSize */
     wave_read( &fmt_size, 4 );
#ifdef WORDS_BIGENDIAN
     fmt_size = BSWAP32( fmt_size );
#endif
     if (fmt_size < 16) {
          D_ERROR( "MusicProvider/WAVE: Invalid fmt header size %u!\n", fmt_size );
          goto error;
     }

     /* 2 bytes: FormatTag */
     wave_read( &compression, 2 );
#ifdef WORDS_BIGENDIAN
     compression = BSWAP16( compression );
#endif
     if (compression != 1) {
          D_ERROR( "MusicProvider/WAVE: Unsupported compression %u!\n", compression );
          goto error;
     }

     /* 2 bytes: Channels */
     wave_read( &data->channels, 2 );
#ifdef WORDS_BIGENDIAN
     data->channels = BSWAP16( data->channels );
#endif
     if (data->channels < 1 || data->channels > FS_MAX_CHANNELS) {
          D_ERROR( "MusicProvider/WAVE: Invalid number of channels %u!\n", data->channels );
          goto error;
     }

     /* 4 bytes: SamplesPerSec */
     wave_read( &data->samplerate, 4 );
#ifdef WORDS_BIGENDIAN
     data->samplerate = BSWAP32( data->samplerate );
#endif
     if (data->samplerate < 1000) {
          D_ERROR( "MusicProvider/WAVE: Unsupported frequency %uHz!\n", data->samplerate );
          goto error;
     }

     /* 4 bytes: AvgBytesPerSec */
     wave_read( &byterate, 4 );
#ifdef WORDS_BIGENDIAN
     byterate = BSWAP32( byterate );
#endif

     /* 2 bytes: BlockAlign */
     wave_read( &blockalign, 2 );
#ifdef WORDS_BIGENDIAN
     blockalign = BSWAP16( blockalign );
#endif

     /* 2 bytes: BitsPerSample */
     wave_read( &bitspersample, 2 );
#ifdef WORDS_BIGENDIAN
     bitspersample = BSWAP16( bitspersample );
#endif
     if (bitspersample !=  8 && bitspersample != 16 && bitspersample != 24 && bitspersample != 32) {
          D_ERROR( "MusicProvider/WAVE: Unsupported bits per sample %u!\n", bitspersample );
          goto error;
     }

     if (byterate != (data->samplerate * data->channels * bitspersample >> 3)) {
          D_ERROR( "MusicProvider/WAVE: Invalid byterate %u!\n", byterate );
          goto error;
     }

     if (blockalign != (data->channels * bitspersample >> 3)) {
          D_ERROR( "MusicProvider/WAVE: Invalid sample frame size %u!\n", blockalign );
          goto error;
     }

     /* Skip remaining bytes. */
     if (fmt_size > 16) {
          char tmp[fmt_size - 16];
          wave_read( tmp, fmt_size - 16 );
     }

     while (1) {
          /* 4 bytes: Data ChunkID */
          wave_read( id, 4 );

          /* 4 bytes: Data ChunkSize */
          wave_read( &data_size, 4 );
#ifdef WORDS_BIGENDIAN
          data_size = BSWAP32( data_size );
#endif

          if (id[0] != 'd' || id[1] != 'a' || id[2] != 't' || id[3] != 'a') {
               D_DEBUG_AT( MusicProvider_WAVE, "  -> expected 'data', got '%c%c%c%c'!\n", id[0], id[1], id[2], id[3] );

               if (data_size) {
                    char tmp[data_size];
                    wave_read( tmp, data_size );
               }
          }
          else
               break;
     }

#undef wave_read

     switch (bitspersample) {
          case 8:
               data->sampleformat = FSSF_U8;
               break;
          case 16:
               data->sampleformat = FSSF_S16;
               break;
          case 24:
               data->sampleformat = FSSF_S24;
               break;
          case 32:
               data->sampleformat = FSSF_S32;
               break;
          default:
               break;
     }

     data->framesize = data->channels * FS_BYTES_PER_SAMPLE( data->sampleformat );
     data->headsize  = fmt_size + 28;
     data->datasize  = data_size;

     size = direct_stream_length( data->stream );
     if (size) {
          if (data->datasize)
               data->datasize = MIN( data->datasize, size - data->headsize );
          else
               data->datasize = size - data->headsize;
     }

     switch (data->sampleformat) {
          case FSSF_U8:    fmt = "u8";    break;
          case FSSF_S16:   fmt = "s16le"; break;
          case FSSF_S24:   fmt = "s24le"; break;
          case FSSF_S32:   fmt = "s32le"; break;
          case FSSF_FLOAT: fmt = "f32le"; break;
          default:         fmt = "\0";    break;
     }

     snprintf( data->desc.encoding, FS_TRACK_DESC_ENCODING_LENGTH, "pcm_%s", fmt );

     data->desc.bitrate = data->samplerate * data->channels * FS_BITS_PER_SAMPLE( data->sampleformat );

     direct_recursive_mutex_init( &data->lock );
     direct_waitqueue_init( &data->cond );

     data->status = FMSTATE_STOP;

     thiz->AddRef               = IFusionSoundMusicProvider_WAVE_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_WAVE_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_WAVE_GetCapabilities;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_WAVE_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_WAVE_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_WAVE_GetBufferDescription;
     thiz->PlayToStream         = IFusionSoundMusicProvider_WAVE_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_WAVE_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_WAVE_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_WAVE_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_WAVE_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_WAVE_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_WAVE_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_WAVE_SetPlaybackFlags;
     thiz->WaitStatus           = IFusionSoundMusicProvider_WAVE_WaitStatus;

     return DR_OK;

error:
     direct_stream_destroy( stream );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}
