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

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>
#include <core/sound_device.h>

D_DEBUG_DOMAIN( CoreSound_Playback, "CoreSound/Playback", "FusionSound Core Playback" );

/**********************************************************************************************************************/

struct __FS_CorePlayback {
     FusionObject     object;

     FusionSkirmish   lock;

     CoreSound       *core;
     CoreSoundBuffer *buffer;
     bool             notify;

     bool             disabled;  /* playback disabled */
     bool             running;   /* playback position */
     int              position;  /* playback position */
     int              stop;      /* stop position */
     int              pitch;     /* multiplier for sample rate */

     __fsf            center;    /* downmixing level for center channel */
     __fsf            rear;      /* downmixing level for rear channel */
     __fsf            levels[6]; /* multipliers for channels  */
     __fsf            volume;    /* local volume level */
};

/**********************************************************************************************************************/

#define DOWNMIX_LEVEL_3DB 0.70794578438413791

static void
playback_destructor( FusionObject *object,
                     bool          zombie,
                     void         *ctx )
{
     CorePlayback *playback = (CorePlayback*) object;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     D_DEBUG_AT( CoreSound_Playback, "Destroying playback %p (%p%s)\n",
                 playback, playback->buffer, zombie ? " ZOMBIE" : "" );

     fs_buffer_unlink( &playback->buffer );

     fusion_skirmish_destroy( &playback->lock );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
fs_playback_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Playbacks",
                                       sizeof(CorePlayback), sizeof(CorePlaybackNotification),
                                       playback_destructor, NULL, world );
}

/**********************************************************************************************************************/

DirectResult
fs_playback_create( CoreSound        *core,
                    CoreSoundBuffer  *buffer,
                    bool              notify,
                    CorePlayback    **ret_playback )
{
     float         volume;
     CorePlayback *playback;

     D_ASSERT( buffer != NULL );
     D_ASSERT( ret_playback != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p, notify %d)\n", __FUNCTION__, buffer, notify );

     /* Create the playback object. */
     playback = fs_core_create_playback( core );
     if (!playback)
          return DR_FUSION;

     /* Link buffer to playback object. */
     if (fs_buffer_link( &playback->buffer, buffer )) {
          fusion_object_destroy( &playback->object );
          return DR_FUSION;
     }

     fusion_skirmish_init( &playback->lock, "FusionSound Playback", fs_core_world( core ) );

     playback->core   = core;
     playback->notify = notify;
     playback->pitch  = FS_PITCH_ONE;

     /* Set default downmixing levels. */
     fs_playback_set_downmix( playback, DOWNMIX_LEVEL_3DB, DOWNMIX_LEVEL_3DB );

     /* Set default volume levels. */
     playback->levels[0] = FSF_ONE;
     playback->levels[1] = FSF_ONE;
     playback->levels[2] = playback->center;
     playback->levels[3] = playback->rear;
     playback->levels[4] = playback->rear;
     playback->levels[5] = FSF_ONE;

     /* Get local volume level. */
     fs_core_get_local_volume( core, &volume );
     playback->volume = fsf_from_float( volume );

     /* Activate the object. */
     fusion_object_activate( &playback->object );

     /* Return the new playback. */
     *ret_playback = playback;

     D_DEBUG_AT( CoreSound_Playback, "  -> %p\n", playback );

     return DR_OK;
}

DirectResult
fs_playback_enable( CorePlayback *playback )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Enable playback. */
     playback->disabled = false;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

static void
fs_playback_notify( CorePlayback                  *playback,
                    CorePlaybackNotificationFlags  flags,
                    int                            num )
{
     CorePlaybackNotification notification;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( !(flags & ~(CPNF_START | CPNF_STOP | CPNF_ADVANCE)) );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     if (flags & CPNF_START)
          playback->running = true;

     if (flags & CPNF_STOP)
          playback->running = false;

     if (!playback->notify)
          return;

     notification.flags    = flags;
     notification.playback = playback;
     notification.pos      = playback->position;
     notification.stop     = playback->running ? playback->stop : playback->position;
     notification.num      = num;

     fs_playback_dispatch( playback, &notification, NULL );
}

DirectResult
fs_playback_start( CorePlayback *playback,
                   bool          enable )
{
     DirectResult ret = DR_OK;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playlist. */
     if (fs_core_playlist_lock( playback->core ))
          return DR_FUSION;

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock )) {
          fs_core_playlist_unlock( playback->core );
          return DR_FUSION;
     }

     /* If the playback is disabled, play won't start. */
     if (enable)
          playback->disabled = false;

     /* Start the playback if not already running */
     if (!playback->running) {
          if (playback->disabled) {
               ret = DR_TEMPUNAVAIL;
          }
          else {
               ret = fs_core_add_playback( playback->core, playback );

               /* Notify listeners about the beginning of the playback. */
               if (ret == DR_OK)
                    fs_playback_notify( playback, CPNF_START, 0 );
          }
     }

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Unlock playlist. */
     fs_core_playlist_unlock( playback->core );

     return ret;
}

DirectResult
fs_playback_stop( CorePlayback *playback,
                  bool          disable )
{
     D_ASSERT( playback != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playlist. */
     if (fs_core_playlist_lock( playback->core ))
          return DR_FUSION;

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock )) {
          fs_core_playlist_unlock( playback->core );
          return DR_FUSION;
     }

     /* Stop the playback if it's running. */
     if (playback->running) {
          fs_core_remove_playback( playback->core, playback );

          /* Notify listeners about the end of the playback. */
          fs_playback_notify( playback, CPNF_STOP, 0 );
     }

     /* If the playback is enabled, play will start. */
     if (disable)
          playback->disabled = true;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Unlock playlist. */
     fs_core_playlist_unlock( playback->core );

     return DR_OK;
}

DirectResult
fs_playback_set_stop( CorePlayback *playback,
                      int           stop )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Adjust stop position. */
     playback->stop = stop;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_set_position( CorePlayback *playback,
                          int           position )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( position >= 0 );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Adjust the playback position. */
     playback->position = position;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_set_downmix( CorePlayback *playback,
                         float         center,
                         float         rear )
{
     CoreSoundBuffer       *buffer;
     CoreSoundDeviceConfig *config;

     D_ASSERT( playback != NULL );
     D_ASSERT( center >= 0.0f );
     D_ASSERT( center <= 1.0f );
     D_ASSERT( rear >= 0.0f );
     D_ASSERT( rear <= 1.0f );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     buffer = playback->buffer;
     config = fs_core_device_config( playback->core );

     if ( FS_MODE_HAS_CENTER( fs_buffer_mode( buffer ) ) && !FS_MODE_HAS_CENTER( config->mode ))
          playback->center = fsf_from_float( center );
     else
          playback->center = FSF_ONE;

     if ( FS_MODE_NUM_REARS( fs_buffer_mode( buffer ) ) && !FS_MODE_NUM_REARS( config->mode ))
          playback->rear = fsf_from_float( rear );
     else
          playback->rear = FSF_ONE;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_set_volume( CorePlayback *playback,
                        float         levels[6] )
{
     int i;

     D_ASSERT( playback != NULL );
     D_ASSERT( levels[0] >= 0.0f );
     D_ASSERT( levels[0] <= 64.0f );
     D_ASSERT( levels[1] >= 0.0f );
     D_ASSERT( levels[1] <= 64.0f );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Adjust volume. */
     for (i = 0; i < 6; i++)
          playback->levels[i] = fsf_from_float( levels[i] );

     /* Apply downmixing levels. */
     if (playback->center != FSF_ONE) {
          playback->levels[2] = fsf_mul( playback->levels[2], playback->center );
     }

     if (playback->rear != FSF_ONE) {
          playback->levels[3] = fsf_mul( playback->levels[3], playback->rear );
          playback->levels[4] = fsf_mul( playback->levels[4], playback->rear );
     }

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_set_local_volume( CorePlayback *playback,
                              float         level )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( level >= 0.0f );
     D_ASSERT( level <= 1.0f );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Set local volume level. */
     playback->volume = fsf_from_float( level );

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_set_pitch( CorePlayback *playback,
                       int           pitch )
{
     D_ASSERT( playback != NULL );
     D_ASSERT( pitch >= -64 * FS_PITCH_ONE );
     D_ASSERT( pitch <= +64 * FS_PITCH_ONE );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Adjust pitch. */
     playback->pitch = pitch;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_get_status( CorePlayback       *playback,
                        CorePlaybackStatus *ret_status,
                        int                *ret_position )
{
     D_ASSERT( playback != NULL );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Return status. */
     if (ret_status) {
          CorePlaybackStatus status = CPS_NONE;

          if (playback->running) {
               status |= CPS_PLAYING;

               if (playback->stop < 0)
                    status |= CPS_LOOPING;
          }

          *ret_status = status;
     }

     /* Return position. */
     if (ret_position)
          *ret_position = playback->position;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     return DR_OK;
}

DirectResult
fs_playback_mixto( CorePlayback  *playback,
                   __fsf         *dest,
                   int            rate,
                   FSChannelMode  mode,
                   int            max_frames,
                   __fsf          volume,
                   int           *ret_samples )
{
     DirectResult  ret;
     int           i;
     int           num;
     int           pos;
     __fsf        *levels;

     D_ASSERT( playback != NULL );
     D_ASSERT( playback->buffer != NULL );
     D_ASSERT( dest != NULL );
     D_ASSERT( max_frames > 0 );

     D_DEBUG_AT( CoreSound_Playback, "%s( %p )\n", __FUNCTION__, playback );

     /* Lock playback. */
     if (fusion_skirmish_prevail( &playback->lock ))
          return DR_FUSION;

     /* Set levels. */
     if (volume != FSF_ONE || playback->volume != FSF_ONE) {
          levels = alloca( 6 * sizeof(__fsf) );
          volume = fsf_mul( volume, playback->volume );
          for (i = 0; i < 6; i++)
               levels[i] = fsf_mul( playback->levels[i], volume );
     }
     else {
          levels = playback->levels;
     }

     /* Mix samples. */
     ret = fs_buffer_mixto( playback->buffer, dest, rate, mode, max_frames, playback->position, playback->stop, levels,
                            playback->pitch, &pos, &num, ret_samples );
     if (ret)
          playback->running = false;

     /* Set new position. */
     playback->position = pos;

     /* Unlock playback. */
     fusion_skirmish_dismiss( &playback->lock );

     /* Notify listeners about new position in the playback (and a possible end of the playback). */
     fs_playback_notify( playback, ret ? (CPNF_ADVANCE | CPNF_STOP) : CPNF_ADVANCE, num );

     return ret;
}
