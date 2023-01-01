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

#include <buffer/ifusionsoundbuffer.h>
#include <core/playback.h>
#include <core/sound_buffer.h>
#include <playback/ifusionsoundplayback.h>

D_DEBUG_DOMAIN( Buffer, "IFusionSoundBuffer", "IFusionSoundBuffer Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IFusionSoundBuffer
 */
typedef struct {
     int              ref;              /* reference counter */

     CoreSound       *core;             /* the core object */
     CoreSoundBuffer *buffer;           /* the buffer object */

     int              length;
     FSChannelMode    mode;
     FSSampleFormat   format;
     int              rate;

     bool             locked;
     int              pos;

     CorePlayback    *looping_playback;

     DirectMutex      lock;
} IFusionSoundBuffer_data;

/**********************************************************************************************************************/

static void
IFusionSoundBuffer_Destruct( IFusionSoundBuffer *thiz )
{
     IFusionSoundBuffer_data *data = thiz->priv;

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->locked)
          fs_buffer_unlock( data->buffer );

     /* Stop and discard looping playback. */
     if (data->looping_playback) {
          fs_playback_stop( data->looping_playback, false );
          fs_playback_unref( data->looping_playback );
     }

     fs_buffer_unref( data->buffer );

     direct_mutex_deinit( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IFusionSoundBuffer_AddRef( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_Release( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IFusionSoundBuffer_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_GetDescription( IFusionSoundBuffer  *thiz,
                                   FSBufferDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags = FSBDF_LENGTH | FSBDF_CHANNELS | FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE |
                       FSBDF_CHANNELMODE;

     ret_desc->length       = data->length;
     ret_desc->channels     = FS_CHANNELS_FOR_MODE( data->mode );
     ret_desc->sampleformat = data->format;
     ret_desc->samplerate   = data->rate;
     ret_desc->channelmode  = data->mode;

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_SetPosition( IFusionSoundBuffer *thiz,
                                int                 position )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (position < 0 || position >= data->length)
          return DR_INVARG;

     data->pos = position;

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_Lock( IFusionSoundBuffer  *thiz,
                         void               **ret_data,
                         int                 *ret_frames,
                         int                 *ret_bytes )
{
     DirectResult  ret;
     void         *lock_data;
     int           lock_bytes;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data)
          return DR_INVARG;

     if (data->locked)
          return DR_LOCKED;

     ret = fs_buffer_lock( data->buffer, data->pos, 0, &lock_data, &lock_bytes );
     if (ret)
          return ret;

     data->locked = true;

     *ret_data = lock_data;

     if (ret_frames)
          *ret_frames = lock_bytes / fs_buffer_bytes( data->buffer );

     if (ret_bytes)
          *ret_bytes = lock_bytes;

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_Unlock( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->locked)
          return DR_OK;

     fs_buffer_unlock( data->buffer );

     data->locked = false;

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_Play( IFusionSoundBuffer *thiz,
                         FSBufferPlayFlags   flags )
{
     DirectResult  ret;
     CorePlayback *playback;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (flags & ~FSPLAY_ALL)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     /* Choose looping playback mode. */
     if (flags & FSPLAY_LOOPING) {
          /* Return an error if a looping playback is already running. */
          if (data->looping_playback) {
               direct_mutex_unlock( &data->lock );
               return DR_BUSY;
          }

          /* Create a playback object. */
          ret = fs_playback_create( data->core, data->buffer, false, &playback );
          if (ret) {
               direct_mutex_unlock( &data->lock );
               return ret;
          }

          /* Set playback direction. */
          if (flags & FSPLAY_REWIND)
               fs_playback_set_pitch( playback, -FS_PITCH_ONE );
          else
               fs_playback_set_pitch( playback, +FS_PITCH_ONE );

          /* Set playback start. */
          fs_playback_set_position( playback, data->pos );

          /* Set looping playback. */
          fs_playback_set_stop( playback, -1 );

          /* Start the playback. */
          ret = fs_playback_start( playback, false );
          if (ret) {
               fs_playback_unref( playback );
               direct_mutex_unlock( &data->lock );
               return ret;
          }

          /* Remember looping playback. */
          data->looping_playback = playback;
     }
     else {
          /* Create a playback object. */
          ret = fs_playback_create( data->core, data->buffer, false, &playback );
          if (ret) {
               direct_mutex_unlock( &data->lock );
               return ret;
          }

          /* Set playback direction. */
          if (flags & FSPLAY_REWIND)
               fs_playback_set_pitch( playback, -FS_PITCH_ONE );
          else
               fs_playback_set_pitch( playback, +FS_PITCH_ONE );

          /* Set playback start. */
          fs_playback_set_position( playback, data->pos );

          /* Set playback end. */
          if (flags & FSPLAY_CYCLE)
               fs_playback_set_stop( playback, data->pos );
          else
               fs_playback_set_stop( playback, 0 );

          /* Start the playback. */
          ret = fs_playback_start( playback, false );
          if (ret) {
               fs_playback_unref( playback );
               direct_mutex_unlock( &data->lock );
               return ret;
          }

          /* Object has a global reference while it's being played and is destroyed when the playback has finished. */
          fs_playback_unref( playback );
     }

     direct_mutex_unlock( &data->lock );

     return ret;
}

static DirectResult
IFusionSoundBuffer_Stop( IFusionSoundBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->lock );

     /* Stop and discard looping playback. */
     if (data->looping_playback) {
          fs_playback_stop( data->looping_playback, false );
          fs_playback_unref( data->looping_playback );
          data->looping_playback = NULL;
     }

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundBuffer_CreatePlayback( IFusionSoundBuffer    *thiz,
                                   IFusionSoundPlayback **ret_interface )
{
     DirectResult          ret;
     CorePlayback         *playback;
     IFusionSoundPlayback *iface;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DR_INVARG;

     /* Create the playback object. */
     ret = fs_playback_create( data->core, data->buffer, true, &playback );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IFusionSoundPlayback );

     ret = IFusionSoundPlayback_Construct( iface, playback, data->length );
     if (ret)
          goto out;

     *ret_interface = iface;

out:
     fs_playback_unref( playback );

     return ret;
}

DirectResult
IFusionSoundBuffer_Construct( IFusionSoundBuffer *thiz,
                              CoreSound          *core,
                              CoreSoundBuffer    *buffer,
                              int                 length,
                              FSChannelMode       mode,
                              FSSampleFormat      format,
                              int                 rate )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundBuffer )

     D_DEBUG_AT( Buffer, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_buffer_ref( buffer );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref    = 1;
     data->core   = core;
     data->buffer = buffer;
     data->length = length;
     data->mode   = mode;
     data->format = format;
     data->rate   = rate;

     direct_recursive_mutex_init( &data->lock );

     thiz->AddRef         = IFusionSoundBuffer_AddRef;
     thiz->Release        = IFusionSoundBuffer_Release;
     thiz->GetDescription = IFusionSoundBuffer_GetDescription;
     thiz->SetPosition    = IFusionSoundBuffer_SetPosition;
     thiz->Lock           = IFusionSoundBuffer_Lock;
     thiz->Unlock         = IFusionSoundBuffer_Unlock;
     thiz->Play           = IFusionSoundBuffer_Play;
     thiz->Stop           = IFusionSoundBuffer_Stop;
     thiz->CreatePlayback = IFusionSoundBuffer_CreatePlayback;

     return DR_OK;
}
