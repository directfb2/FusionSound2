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

#include <core/playback.h>
#include <playback/ifusionsoundplayback.h>

D_DEBUG_DOMAIN( Playback, "IFusionSoundPlayback", "IFusionSoundPlayback Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IFusionSoundPlayback
 */
typedef struct {
     int              ref;       /* reference counter */

     CorePlayback    *playback; /* the playback object */

     bool             stream;
     int              length;

     Reaction         reaction;

     float            volume;
     float            pan;
     int              pitch;
     int              dir;

     DirectMutex      lock;
     DirectWaitQueue  wait;
} IFusionSoundPlayback_data;

/**********************************************************************************************************************/

static void
IFusionSoundPlayback_Destruct( IFusionSoundPlayback *thiz )
{
     IFusionSoundPlayback_data *data = thiz->priv;

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     fs_playback_detach( data->playback, &data->reaction );
     if (!data->stream)
          fs_playback_stop( data->playback, false );
     fs_playback_unref( data->playback );

     direct_waitqueue_deinit( &data->wait );
     direct_mutex_deinit( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IFusionSoundPlayback_AddRef( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionSoundPlayback_Release( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IFusionSoundPlayback_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionSoundPlayback_Start( IFusionSoundPlayback *thiz,
                            int                   start,
                            int                   stop )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %d -> %d )\n", __FUNCTION__, thiz, start, stop );

     if (data->stream)
          return DR_UNSUPPORTED;

     if (start < 0 || start >= data->length)
          return DR_INVARG;

     if (stop >= data->length)
          return DR_INVARG;

     direct_mutex_lock( &data->lock );

     fs_playback_set_position( data->playback, start );
     fs_playback_set_stop( data->playback, stop );
     fs_playback_start( data->playback, false );

     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionSoundPlayback_Stop( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     return fs_playback_stop( data->playback, false );
}

static DirectResult
IFusionSoundPlayback_Continue( IFusionSoundPlayback *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     return fs_playback_start( data->playback, false );
}

static DirectResult
IFusionSoundPlayback_Wait( IFusionSoundPlayback *thiz )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->lock );

     for (;;) {
          CorePlaybackStatus status;

          ret = fs_playback_get_status( data->playback, &status, NULL );
          if (ret)
               break;

          if (status & CPS_PLAYING) {
               if (status & CPS_LOOPING) {
                    ret = DR_UNSUPPORTED;
                    break;
               }
               else {
                    direct_waitqueue_wait( &data->wait, &data->lock );
               }
          }
          else
               break;
     }

     direct_mutex_unlock( &data->lock );

     return ret;
}

static DirectResult
IFusionSoundPlayback_GetStatus( IFusionSoundPlayback *thiz,
                                bool                 *running,
                                int                  *ret_position )
{
     DirectResult       ret;
     int                position;
     CorePlaybackStatus status;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_playback_get_status( data->playback, &status, &position );
     if (ret)
          return ret;

     if (running)
          *running = status & CPS_PLAYING;

     if (ret_position)
          *ret_position = position;

     return DR_OK;
}

static DirectResult
UpdateVolume( IFusionSoundPlayback_data *data )
{
                      /*    L     R     C    Rl    Rr   LFE */
     float levels[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

     if (data->pan != 0.0f) {
          if (data->pan < 0.0f)
               levels[1] = levels[4] = 1.0f + data->pan; /* right */
          else if (data->pan > 0.0f)
               levels[0] = levels[3] = 1.0f - data->pan; /* left */
     }

     if (data->volume != 1.0f) {
          int i;
          for (i = 0; i < 6; i++) {
               levels[i] *= data->volume;
               if (levels[i] > 64.0f)
                    levels[i] = 64.0f;
          }
     }

     return fs_playback_set_volume( data->playback, levels );
}

static DirectResult
IFusionSoundPlayback_SetVolume( IFusionSoundPlayback *thiz,
                                float                 level )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %.3f )\n", __FUNCTION__, thiz, level );

     if (level < 0.0f)
          return DR_INVARG;

     if (level > 64.0f)
          return DR_UNSUPPORTED;

     data->volume = level;

     return UpdateVolume( data );
}

static DirectResult
IFusionSoundPlayback_SetPan( IFusionSoundPlayback *thiz,
                             float                 value )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %.3f )\n", __FUNCTION__, thiz, value );

     if (value < -1.0f || value > 1.0f)
          return DR_INVARG;

     data->pan = value;

     return UpdateVolume( data );
}

static DirectResult
IFusionSoundPlayback_SetPitch( IFusionSoundPlayback *thiz,
                               float                 value )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %.3f )\n", __FUNCTION__, thiz, value );

     if (value < 0.0f)
          return DR_INVARG;

     if (value > 64.0f)
          return DR_UNSUPPORTED;

     data->pitch = value * FS_PITCH_ONE + 0.5f;

     fs_playback_set_pitch( data->playback, data->pitch * data->dir );

     return DR_OK;
}

static DirectResult
IFusionSoundPlayback_SetDirection( IFusionSoundPlayback *thiz,
                                   FSPlaybackDirection   direction )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %d )\n", __FUNCTION__, thiz, direction );

     switch (direction) {
          case FSPD_FORWARD:
          case FSPD_BACKWARD:
               data->dir = direction;
               break;
          default:
               return DR_INVARG;
     }

     fs_playback_set_pitch( data->playback, data->pitch * data->dir );

     return DR_OK;
}

static DirectResult
IFusionSoundPlayback_SetDownmixLevels( IFusionSoundPlayback *thiz,
                                       float                 center,
                                       float                 rear )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p, %.3f, %.3f )\n", __FUNCTION__, thiz, center, rear );

     if (center < 0.0f || center > 1.0f)
          return DR_INVARG;

     if (rear < 0.0f || rear > 1.0f)
          return DR_INVARG;

     ret = fs_playback_set_downmix( data->playback, center, rear );
     if (ret)
          return ret;

     return UpdateVolume( data );
}

static ReactionResult
IFusionSoundPlayback_React( const void *msg_data,
                            void       *ctx )
{
     const CorePlaybackNotification *notification = msg_data;
     IFusionSoundPlayback_data      *data         = ctx;

     D_DEBUG_AT( Playback, "%s( %p, %p )\n", __FUNCTION__, notification, data );

     if (notification->flags & CPNF_START)
          D_DEBUG_AT( Playback, "  -> playback started at position %d\n", notification->pos );

     if (notification->flags & CPNF_STOP)
          D_DEBUG_AT( Playback, "  -> playback stopped at position %d\n", notification->pos );

     if (notification->flags & CPNF_ADVANCE)
          D_DEBUG_AT( Playback, "  -> playback advanced to position %d\n", notification->pos );

     if (notification->flags & (CPNF_START | CPNF_STOP))
          direct_waitqueue_broadcast( &data->wait );

     return RS_OK;
}

DirectResult
IFusionSoundPlayback_Construct( IFusionSoundPlayback *thiz,
                                CorePlayback         *playback,
                                int                   length )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundPlayback )

     D_DEBUG_AT( Playback, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_playback_ref( playback );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Attach listener to the playback. */
     ret = fs_playback_attach( playback, IFusionSoundPlayback_React, data, &data->reaction );
     if (ret) {
          fs_playback_unref( playback );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref      = 1;
     data->playback = playback;
     data->stream   = (length < 0);
     data->length   = length;
     data->volume   = 1.0f;
     data->pitch    = FS_PITCH_ONE;
     data->dir      = 1;

     direct_recursive_mutex_init( &data->lock );
     direct_waitqueue_init( &data->wait );

     thiz->AddRef           = IFusionSoundPlayback_AddRef;
     thiz->Release          = IFusionSoundPlayback_Release;
     thiz->Start            = IFusionSoundPlayback_Start;
     thiz->Stop             = IFusionSoundPlayback_Stop;
     thiz->Continue         = IFusionSoundPlayback_Continue;
     thiz->Wait             = IFusionSoundPlayback_Wait;
     thiz->GetStatus        = IFusionSoundPlayback_GetStatus;
     thiz->SetVolume        = IFusionSoundPlayback_SetVolume;
     thiz->SetPan           = IFusionSoundPlayback_SetPan;
     thiz->SetPitch         = IFusionSoundPlayback_SetPitch;
     thiz->SetDirection     = IFusionSoundPlayback_SetDirection;
     thiz->SetDownmixLevels = IFusionSoundPlayback_SetDownmixLevels;

     return DR_OK;
}
