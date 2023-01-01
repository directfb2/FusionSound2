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

#ifndef __CORE__SOUND_BUFFER_H__
#define __CORE__SOUND_BUFFER_H__

#include <core/coretypes_sound.h>
#include <core/fs_types.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

typedef enum {
     CSBNF_NONE
} CoreSoundBufferNotificationFlags;

typedef struct {
     CoreSoundBufferNotificationFlags  flags;
     CoreSoundBuffer                  *buffer;
} CoreSoundBufferNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of sound buffer objects.
 */
FusionObjectPool *fs_buffer_pool_create( const FusionWorld  *world );

/*
 * Generates fs_buffer_ref(), fs_buffer_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSoundBuffer, fs_buffer )

/**********************************************************************************************************************/

DirectResult      fs_buffer_create     ( CoreSound          *core,
                                         int                 length,
                                         FSChannelMode       mode,
                                         FSSampleFormat      format,
                                         int                 rate,
                                         CoreSoundBuffer   **ret_buffer );

DirectResult      fs_buffer_lock        ( CoreSoundBuffer   *buffer,
                                         int                 pos,
                                         int                 length,
                                         void              **ret_data,
                                         int                *ret_bytes );

DirectResult      fs_buffer_unlock      ( CoreSoundBuffer   *buffer );

int               fs_buffer_bytes       ( CoreSoundBuffer   *buffer );

FSChannelMode     fs_buffer_mode        ( CoreSoundBuffer   *buffer );

DirectResult      fs_buffer_mixto       ( CoreSoundBuffer   *buffer,
                                          __fsf             *dest,
                                          int                rate,
                                          FSChannelMode      mode,
                                          int                max_frames,
                                          int                pos,
                                          int                stop,
                                          __fsf              levels[6],
                                          int                pitch,
                                          int               *ret_pos,
                                          int               *ret_num,
                                          int               *ret_written );

#endif
