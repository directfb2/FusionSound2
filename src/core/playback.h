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

#ifndef __CORE__PLAYBACK_H__
#define __CORE__PLAYBACK_H__

#include <core/coretypes_sound.h>
#include <core/fs_types.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

#define FS_PITCH_BITS 14
#define FS_PITCH_ONE  (1 << FS_PITCH_BITS)

typedef enum {
     CPS_NONE     = 0x00000000,
     CPS_PLAYING  = 0x00000001,
     CPS_LOOPING  = 0x00000002
} CorePlaybackStatus;

/**********************************************************************************************************************/

typedef enum {
     CPNF_START   = 0x00000001,
     CPNF_STOP    = 0x00000002,
     CPNF_ADVANCE = 0x00000004
} CorePlaybackNotificationFlags;

typedef struct {
     CorePlaybackNotificationFlags  flags;
     CorePlayback                  *playback;
     int                            pos;    /* Current playback position. */
     int                            stop;   /* Position at which the playback will stop or has stopped.
                                               A negative value indicates looping. */
     int                            num;    /* Number of samples played (CPNF_ADVANCE) or zero. */
} CorePlaybackNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of playback objects.
 */
FusionObjectPool *fs_playback_pool_create     ( const FusionWorld   *world );

/*
 * Generates fs_playback_ref(), fs_playback_attach() etc.
 */
FUSION_OBJECT_METHODS( CorePlayback, fs_playback )

/**********************************************************************************************************************/

DirectResult      fs_playback_create          ( CoreSound           *core,
                                                CoreSoundBuffer     *buffer,
                                                bool                 notify,
                                                CorePlayback       **ret_playback );

DirectResult      fs_playback_enable          ( CorePlayback        *playback );

DirectResult      fs_playback_start           ( CorePlayback        *playback,
                                                bool                 enable );

DirectResult      fs_playback_stop            ( CorePlayback        *playback,
                                                bool                 disable );

DirectResult      fs_playback_set_stop        ( CorePlayback        *playback,
                                                int                  stop );

DirectResult      fs_playback_set_position    ( CorePlayback        *playback,
                                                int                  position );

DirectResult      fs_playback_set_downmix     ( CorePlayback        *playback,
                                                float                center,
                                                float                rear );

DirectResult      fs_playback_set_volume      ( CorePlayback        *playback,
                                                float                levels[6] );

DirectResult      fs_playback_set_local_volume( CorePlayback        *playback,
                                                float                level );

DirectResult      fs_playback_set_pitch       ( CorePlayback        *playback,
                                                int                  pitch );

DirectResult      fs_playback_get_status      ( CorePlayback        *playback,
                                                CorePlaybackStatus  *ret_status,
                                                int                 *ret_position );

DirectResult      fs_playback_mixto           ( CorePlayback        *playback,
                                                __fsf               *dest,
                                                int                  dest_rate,
                                                FSChannelMode        dest_mode,
                                                int                  max_frames,
                                                __fsf                volume,
                                                int                 *ret_samples );

#endif
