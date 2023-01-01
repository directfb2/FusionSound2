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

#include <buffer/ifusionsoundstream.h>
#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>
#include <direct/memcpy.h>
#include <playback/ifusionsoundplayback.h>

D_DEBUG_DOMAIN( Stream, "IFusionSoundStream", "IFusionSoundStream Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IFusionSoundStream
 */
typedef struct {
     int                   ref;                /* reference counter */

     CoreSound            *core;               /* the core object */
     CoreSoundBuffer      *buffer;             /* the buffer object */

     CorePlayback         *streaming_playback;

     int                   buffersize;
     FSChannelMode         mode;
     FSSampleFormat        format;
     int                   rate;
     int                   prebuffer;

     Reaction              reaction;

     DirectMutex           lock;
     DirectWaitQueue       wait;

     bool                  playing;
     int                   pos_write;
     int                   pos_read;
     int                   filled;
     int                   pending;

     IFusionSoundPlayback *playback;
} IFusionSoundStream_data;

/**********************************************************************************************************************/

static void
IFusionSoundStream_Destruct( IFusionSoundStream *thiz )
{
     IFusionSoundStream_data *data = thiz->priv;

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->playback)
          data->playback->Release( data->playback );

     fs_playback_detach( data->streaming_playback, &data->reaction );
     fs_playback_stop( data->streaming_playback, true );
     fs_playback_unref( data->streaming_playback );

     fs_buffer_unref( data->buffer );

     direct_waitqueue_deinit( &data->wait );
     direct_mutex_deinit( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IFusionSoundStream_AddRef( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionSoundStream_Release( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IFusionSoundStream_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_GetDescription( IFusionSoundStream  *thiz,
                                   FSStreamDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags = FSSDF_BUFFERSIZE | FSSDF_CHANNELS | FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE | FSSDF_PREBUFFER |
                       FSSDF_CHANNELMODE;

     ret_desc->buffersize   = data->buffersize;
     ret_desc->channels     = FS_CHANNELS_FOR_MODE( data->mode );
     ret_desc->sampleformat = data->format;
     ret_desc->samplerate   = data->rate;
     ret_desc->prebuffer    = data->prebuffer;
     ret_desc->channelmode  = data->mode;

     return DR_OK;
}

static DirectResult
IFusionSoundStream_Write( IFusionSoundStream *thiz,
                          const void         *sample_data,
                          int                 length )
{
     DirectResult ret = DR_OK;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (!sample_data || length < 1)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     data->pending = length;

     while (data->pending) {
          int   num, size;
          void *lock_data;
          int   lock_bytes;
          int   bytes = 0;

          D_DEBUG_AT( Stream, "  -> length %d, read pos %d, write pos %d, filled %d/%d (%splaying)\n", data->pending,
                      data->pos_read, data->pos_write, data->filled, data->buffersize, data->playing ? "" : "not " );

          D_ASSERT( data->filled <= data->buffersize );

          /* Wait for at least one free sample. */
          while (data->filled == data->buffersize) {
               direct_waitqueue_wait( &data->wait, &data->lock );

               /* Drop() could have been called while waiting. */
               if (!data->pending)
                    goto out;
          }

          /* Calculate the number of free samples in the buffer. */
          num = data->buffersize - data->filled;

          /* Do not write more than requested. */
          if (num > data->pending)
               num = data->pending;

          size = num;

          /* Fill free space with automatic wrap around to the beginning. */
          while (size) {
               length = MIN( size, data->buffersize - data->pos_write );

               /* Write data. */
               ret = fs_buffer_lock( data->buffer, data->pos_write, length, &lock_data, &lock_bytes );
               if (ret)
                    goto out;

               direct_memcpy( lock_data, sample_data + bytes, lock_bytes );

               fs_buffer_unlock( data->buffer );

               /* Update parameters. */
               size -= length;
               bytes += lock_bytes;

               /* Update write position. */
               data->pos_write += length;

               /* Handle wrap around. */
               if (data->pos_write == data->buffersize)
                    data->pos_write = 0;

               /* Set new stop position. */
               ret = fs_playback_set_stop( data->streaming_playback, data->pos_write );
               if (ret)
                    goto out;

               /* (Re)enable playback if the buffer is empty. */
               fs_playback_enable( data->streaming_playback );

               /* Update fill level. */
               data->filled += length;
          }

          /* (Re)start if playback is stopped. */
          if (!data->playing && data->prebuffer >= 0 && data->filled >= data->prebuffer) {
               D_DEBUG_AT( Stream, "  -> starting playback\n" );

               fs_playback_start( data->streaming_playback, true );
          }

          /* Update input sample data (sample data that has not yet been written). */
          sample_data += bytes;

          /* Update amount of pending data. */
          if (data->pending)
               data->pending -= num;
     }

out:
     direct_mutex_unlock( &data->lock );

     return ret;
}

static DirectResult
IFusionSoundStream_Wait( IFusionSoundStream *thiz,
                         int                 length )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (length < 0 || length > data->buffersize)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     while (true) {
          if (length) {
               int num;

               /* Calculate the number of free samples in the buffer. */
               num = data->buffersize - data->filled;

               if (num >= length)
                    break;
          }
          else if (!data->playing)
               break;

          direct_waitqueue_wait( &data->wait, &data->lock );
     }

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_GetStatus( IFusionSoundStream *thiz,
                              int                *filled,
                              int                *total,
                              int                *read_position,
                              int                *write_position,
                              bool               *playing )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->lock );

     if (filled)
          *filled = data->filled;

     if (total)
          *total = data->buffersize;

     if (read_position)
          *read_position = data->pos_read;

     if (write_position)
          *write_position = data->pos_write;

     if (playing)
          *playing = data->playing;

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_Flush( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     /* Stop playback. */
     fs_playback_stop( data->streaming_playback, true );

     direct_mutex_lock( &data->lock );

     while (data->playing) {
          direct_waitqueue_wait( &data->wait, &data->lock );
     }

     /* Reset the buffer. */
     data->pos_write = data->pos_read;
     data->filled    = 0;

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_Drop( IFusionSoundStream *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->lock );

     /* Discard pending data. */
     data->pending = 0;

     direct_waitqueue_broadcast( &data->wait );

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_GetPresentationDelay( IFusionSoundStream *thiz,
                                         int                *ret_delay )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_delay)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     *ret_delay = fs_core_output_delay( data->core ) + (data->filled + data->pending) * 1000 / data->rate;

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundStream_GetPlayback( IFusionSoundStream    *thiz,
                                IFusionSoundPlayback **ret_interface )
{
     DirectResult          ret;
     IFusionSoundPlayback *iface;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DR_INVARG;

     if (!data->playback) {
          DIRECT_ALLOCATE_INTERFACE( iface, IFusionSoundPlayback );

          ret = IFusionSoundPlayback_Construct( iface, data->streaming_playback, -1 );
          if (ret)
               return ret;

          data->playback = iface;
     }

     data->playback->AddRef( data->playback );

     *ret_interface = data->playback;

     return DR_OK;
}

static DirectResult
IFusionSoundStream_Access( IFusionSoundStream  *thiz,
                           void               **ret_data,
                           int                 *ret_frames )
{
     DirectResult ret;
     int          bytes;
     int          length;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data || !ret_frames)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     D_DEBUG_AT( Stream, "  -> read pos %d, write pos %d, filled %d/%d (%splaying)\n",
                 data->pos_read, data->pos_write, data->filled, data->buffersize, data->playing ? "" : "not " );

     D_ASSERT( data->filled <= data->buffersize );

     /* Wait for at least one free sample. */
     while (data->filled == data->buffersize) {
          direct_waitqueue_wait( &data->wait, &data->lock );
     }

     /* Calculate the number of free samples in the buffer. */
     length = data->buffersize - data->filled;

     if (length > data->buffersize - data->pos_write)
          length = data->buffersize - data->pos_write;

     ret = fs_buffer_lock( data->buffer, data->pos_write, length, ret_data, &bytes );

     *ret_frames = ret ? 0 : length;

     direct_mutex_unlock( &data->lock );

     return ret;
}

static DirectResult
IFusionSoundStream_Commit( IFusionSoundStream  *thiz,
                           int                  length )
{
     DirectResult ret = DR_OK;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     if (length < 0)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     if (length > data->buffersize - data->filled) {
          ret = DR_INVARG;
          goto out;
     }

     D_DEBUG_AT( Stream, "  -> length %d, read pos %d, write pos %d, filled %d/%d (%splaying)\n", length,
                 data->pos_read, data->pos_write, data->filled, data->buffersize, data->playing ? "" : "not " );

     fs_buffer_unlock( data->buffer );

     if (length) {
          /* Update write position. */
          data->pos_write += length;

          /* Handle wrap around. */
          if (data->pos_write == data->buffersize)
               data->pos_write = 0;

          /* Set new stop position. */
          ret = fs_playback_set_stop( data->streaming_playback, data->pos_write );
          if (ret)
               goto out;

          /* (Re)enable playback if the buffer is empty. */
          fs_playback_enable( data->streaming_playback );

          /* Update fill level. */
          data->filled += length;

          /* (Re)start if playback is stopped. */
          if (!data->playing && data->prebuffer >= 0 && data->filled >= data->prebuffer) {
               D_DEBUG_AT( Stream, "  -> starting playback\n" );

               fs_playback_start( data->streaming_playback, true );
          }
     }

out:
     direct_mutex_unlock( &data->lock );

     return ret;
}

static ReactionResult
IFusionSoundStream_React( const void *msg_data,
                          void       *ctx )
{
     const CorePlaybackNotification *notification = msg_data;
     IFusionSoundStream_data        *data         = ctx;

     D_DEBUG_AT( Stream, "%s( %p, %p )\n", __FUNCTION__, notification, data );

     if (notification->flags & CPNF_START) {
          D_DEBUG_AT( Stream, "  -> playback started at position %d\n", notification->pos );

          data->playing = true;

          return RS_OK;
     }

     direct_mutex_lock( &data->lock );

     if (notification->flags & CPNF_ADVANCE) {
          D_DEBUG_AT( Stream, "  -> playback advanced by %d from position %d to position %d\n",
                      notification->num, data->pos_read, notification->pos );

          D_ASSERT( data->filled >= notification->num );

          data->filled -= notification->num;
     }

     data->pos_read = notification->pos;

     if (notification->flags & CPNF_STOP) {
          D_DEBUG_AT( Stream, "  -> playback stopped at position %d\n", notification->pos );

          data->playing = false;
     }

     direct_waitqueue_broadcast( &data->wait );

     direct_mutex_unlock( &data->lock );

     return RS_OK;
}

DirectResult
IFusionSoundStream_Construct( IFusionSoundStream *thiz,
                              CoreSound          *core,
                              CoreSoundBuffer    *buffer,
                              int                 buffersize,
                              FSChannelMode       mode,
                              FSSampleFormat      format,
                              int                 rate,
                              int                 prebuffer )
{
     DirectResult  ret;
     CorePlayback *playback;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundStream )

     D_DEBUG_AT( Stream, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_buffer_ref( buffer );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Create a playback object. */
     ret = fs_playback_create( core, buffer, true, &playback );
     if (ret) {
          fs_buffer_unref( buffer );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Attach listener to the playback. */
     ret = fs_playback_attach( playback, IFusionSoundStream_React, data, &data->reaction );
     if (ret) {
          fs_buffer_unref( buffer );
          fs_playback_unref( playback );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Disable playback. */
     fs_playback_stop( playback, true );

     data->ref                = 1;
     data->core               = core;
     data->buffer             = buffer;
     data->streaming_playback = playback;
     data->buffersize         = buffersize;
     data->mode               = mode;
     data->format             = format;
     data->rate               = rate;
     data->prebuffer          = prebuffer;

     direct_recursive_mutex_init( &data->lock );
     direct_waitqueue_init( &data->wait );

     thiz->AddRef               = IFusionSoundStream_AddRef;
     thiz->Release              = IFusionSoundStream_Release;
     thiz->GetDescription       = IFusionSoundStream_GetDescription;
     thiz->Write                = IFusionSoundStream_Write;
     thiz->Wait                 = IFusionSoundStream_Wait;
     thiz->GetStatus            = IFusionSoundStream_GetStatus;
     thiz->Flush                = IFusionSoundStream_Flush;
     thiz->Drop                 = IFusionSoundStream_Drop;
     thiz->GetPresentationDelay = IFusionSoundStream_GetPresentationDelay;
     thiz->GetPlayback          = IFusionSoundStream_GetPlayback;
     thiz->Access               = IFusionSoundStream_Access;
     thiz->Commit               = IFusionSoundStream_Commit;

     return DR_OK;
}
