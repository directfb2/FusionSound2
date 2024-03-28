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

#include <core/sound_driver.h>
#include <misc/sound_conf.h>

D_DEBUG_DOMAIN( Dummy_Sound, "Dummy/Sound", "Dummy Sound Driver" );

FS_SOUND_DRIVER( dummy )

/**********************************************************************************************************************/

static DirectResult
device_probe()
{
     /* Loaded only when requested. */
     if (!fs_config->snddriver || strcmp( fs_config->snddriver, "dummy" ))
          return DR_UNSUPPORTED;

     return DR_OK;
}

static void
device_get_driver_info( SoundDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 1;

     snprintf( driver_info->name,   FS_SOUND_DRIVER_INFO_NAME_LENGTH,   "Dummy" );
     snprintf( driver_info->vendor, FS_SOUND_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     driver_info->device_data_size = 16384;
}

static DirectResult
device_open( void                  *device_data,
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     D_DEBUG_AT( Dummy_Sound, "%s()\n", __FUNCTION__ );

     /* Fill device information. */
     snprintf( device_info->name, FS_SOUND_DEVICE_INFO_NAME_LENGTH, "dummy" );

     device_info->caps = DCF_NONE;

     return DR_OK;
}

static DirectResult
device_get_buffer( void          *device_data,
                   u8           **ret_addr,
                   unsigned int  *ret_avail )
{
     *ret_addr  = device_data;
     *ret_avail = 16384;

     return DR_OK;
}

static DirectResult
device_commit_buffer( void         *device_data,
                      unsigned int  frames )
{
     return DR_OK;
}

static void
device_get_output_delay( void *device_data,
                         int  *ret_delay )
{
     *ret_delay = 0;
}

static DirectResult
device_get_volume( void *device_data,
                   float *ret_level )
{
     return DR_UNSUPPORTED;
}

static DirectResult
device_set_volume( void  *device_data,
                   float  level )
{
     return DR_UNSUPPORTED;
}

static DirectResult
device_suspend( void *device_data )
{
     D_DEBUG_AT( Dummy_Sound, "%s()\n", __FUNCTION__ );

     return DR_OK;
}

static DirectResult
device_resume( void *device_data )
{
     D_DEBUG_AT( Dummy_Sound, "%s()\n", __FUNCTION__ );

     return DR_OK;
}

static void
device_handle_fork( void             *device_data,
                    FusionForkAction  action,
                    FusionForkState   state )
{
}

static void
device_close( void *device_data )
{
     D_DEBUG_AT( Dummy_Sound, "%s()\n", __FUNCTION__ );
}
