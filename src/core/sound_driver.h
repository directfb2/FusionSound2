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

#ifndef __CORE__SOUND_DRIVER_H__
#define __CORE__SOUND_DRIVER_H__

#include <core/sound_device.h>

/**********************************************************************************************************************/

static DirectResult device_probe           ( void );

static void         device_get_driver_info ( SoundDriverInfo        *driver_info );

static DirectResult device_open            ( void                   *device_data,
                                             SoundDeviceInfo        *device_info,
                                             CoreSoundDeviceConfig  *config );

static DirectResult device_get_buffer      ( void                   *device_data,
                                             u8                    **ret_addr,
                                             unsigned int           *ret_avail );

static DirectResult device_commit_buffer   ( void                   *device_data,
                                             unsigned int            frames );

static void         device_get_output_delay( void                   *device_data,
                                             int                    *ret_delay );

static DirectResult device_get_volume      ( void                   *device_data,
                                             float                  *ret_level );

static DirectResult device_set_volume      ( void                   *device_data,
                                             float                   level );

static DirectResult device_suspend         ( void                   *device_data );

static DirectResult device_resume          ( void                   *device_data );

static void         device_handle_fork     ( void                   *device_data,
                                             FusionForkAction        action,
                                             FusionForkState         state );

static void         device_close           ( void                   *device_data );

static const SoundDriverFuncs snddriver_funcs = {
     .Probe          = device_probe,
     .GetDriverInfo  = device_get_driver_info,
     .OpenDevice     = device_open,
     .GetBuffer      = device_get_buffer,
     .CommitBuffer   = device_commit_buffer,
     .GetOutputDelay = device_get_output_delay,
     .GetVolume      = device_get_volume,
     .SetVolume      = device_set_volume,
     .Suspend        = device_suspend,
     .Resume         = device_resume,
     .HandleFork     = device_handle_fork,
     .CloseDevice    = device_close
};

#define FS_SOUND_DRIVER(shortname)                         \
                                                           \
__attribute__((constructor))                               \
static void                                                \
fusionsound_##shortname##_ctor()                           \
{                                                          \
     direct_modules_register( &fs_sound_drivers,           \
                              FS_SOUND_DRIVER_ABI_VERSION, \
                              #shortname,                  \
                              &snddriver_funcs );          \
}

#endif
