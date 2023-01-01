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

#include <core/sound_device.h>
#include <misc/sound_conf.h>

D_DEBUG_DOMAIN( CoreSound_Device, "CoreSound/Device", "FusionSound Core Device" );

DEFINE_MODULE_DIRECTORY( fs_sound_drivers, "snddrivers", FS_SOUND_DRIVER_ABI_VERSION );

/**********************************************************************************************************************/

struct __FS_CoreSoundDevice {
     DirectModuleEntry      *module;

     const SoundDriverFuncs *funcs;

     SoundDriverInfo         driver_info;

     void                   *device_data;
     SoundDeviceInfo         device_info;
};

/**********************************************************************************************************************/

DirectResult
fs_device_initialize( CoreSound              *core,
                      CoreSoundDeviceConfig  *config,
                      CoreSoundDevice       **ret_device )
{
     DirectResult       ret;
     CoreSoundDevice   *device;
     DirectModuleEntry *module;

     D_DEBUG_AT( CoreSound_Device, "%s( %p, %p )\n", __FUNCTION__, core, config );

     D_ASSERT( core != NULL );
     D_ASSERT( ret_device != NULL );

     device = D_CALLOC( 1, sizeof(CoreSoundDevice) );
     if (!device)
          return D_OOM();

     snprintf( device->driver_info.name,   FS_SOUND_DRIVER_INFO_NAME_LENGTH,   "none" );
     snprintf( device->driver_info.vendor, FS_SOUND_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     if (!fs_config->driver || strcmp( fs_config->driver, "none" )) {
          /* Build a list of available drivers. */
          direct_modules_explore_directory( &fs_sound_drivers );

          /* Load driver. */
          direct_list_foreach (module, fs_sound_drivers.entries) {
               const SoundDriverFuncs *funcs;

               funcs = direct_module_ref( module );
               if (!funcs)
                    continue;

               if (!device->module && (!fs_config->driver || !strcmp( module->name, fs_config->driver ))) {
                    if (funcs->Probe() == DR_OK) {
                         device->module = module;
                         device->funcs  = funcs;

                         funcs->GetDriverInfo( &device->driver_info );
                    }
               }
               else
                    direct_module_unref( module );
          }

          if (!device->module) {
               if (fs_config->driver) {
                    D_ERROR( "CoreSound/Device: Sound driver '%s' not found!\n", fs_config->driver );
               } else {
                    D_ERROR( "CoreSound/Device: No sound driver found!\n" );
               }

               D_FREE( device );
               return DR_FAILURE;
          }

          if (device->driver_info.device_data_size) {
               device->device_data = D_CALLOC( 1, device->driver_info.device_data_size );
               if (!device->device_data) {
                    direct_module_unref( device->module );
                    D_FREE( device );
                    return D_OOM();
               }
          }

          /* Open sound device. */
          ret = device->funcs->OpenDevice( device->device_data, &device->device_info, config );
          if (ret) {
               D_ERROR( "CoreSound/Device: Could not open device!\n" );
               direct_module_unref( device->module );
               D_FREE( device );
               return ret;
          }
     }

     D_INFO( "FusionSound/Device: %s %d.%d (%s)\n", device->driver_info.name,
             device->driver_info.version.major, device->driver_info.version.minor, device->driver_info.vendor );

     D_INFO( "FusionSound/Device: %u Hz, %u channel(s), %u bits, %.1f ms\n",
             config->rate, FS_CHANNELS_FOR_MODE( config->mode ), FS_BITS_PER_SAMPLE( config->format ),
             (float) config->buffersize / config->rate * 1000 );

     /* Return the new device. */
     *ret_device = device;

     D_DEBUG_AT( CoreSound_Device, "  -> %p\n", device );

     return DR_OK;
}

void
fs_device_shutdown( CoreSoundDevice *device )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs) {
          device->funcs->CloseDevice( device->device_data );

          direct_module_unref( device->module );

          if (device->device_data)
               D_FREE( device->device_data );
     }

     D_FREE( device );
}

void
fs_device_get_description( CoreSoundDevice     *device,
                           FSDeviceDescription *ret_desc )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );
     D_ASSERT( ret_desc != NULL );

     strcpy( ret_desc->name, device->device_info.name );

     memcpy( &ret_desc->driver, &device->driver_info, sizeof(FSSoundDriverInfo) );
}

DeviceCapabilitiesFlags
fs_device_get_capabilities( CoreSoundDevice *device )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     return device->device_info.caps;
}

DirectResult
fs_device_get_buffer( CoreSoundDevice  *device,
                      u8              **ret_addr,
                      unsigned int     *ret_avail )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );
     D_ASSERT( ret_addr != NULL );
     D_ASSERT( ret_avail != NULL );

     if (device->funcs)
          return device->funcs->GetBuffer( device->device_data, ret_addr, ret_avail );

     return DR_UNSUPPORTED;
}

DirectResult
fs_device_commit_buffer( CoreSoundDevice *device,
                         unsigned int     frames )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs)
          return device->funcs->CommitBuffer( device->device_data, frames );

     return DR_UNSUPPORTED;
}

void
fs_device_get_output_delay( CoreSoundDevice *device,
                            int             *ret_delay )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );
     D_ASSERT( ret_delay != NULL );

     if (device->funcs)
          device->funcs->GetOutputDelay( device->device_data, ret_delay );
     else
          *ret_delay = 0;
}

DirectResult
fs_device_get_volume( CoreSoundDevice *device,
                      float           *ret_level )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );
     D_ASSERT( ret_level != NULL );

     if (device->funcs)
          return device->funcs->GetVolume( device->device_data, ret_level );

     return DR_UNSUPPORTED;
}

DirectResult
fs_device_set_volume( CoreSoundDevice *device,
                      float            level )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs)
          return device->funcs->SetVolume( device->device_data, level );

     return DR_UNSUPPORTED;
}

DirectResult
fs_device_suspend( CoreSoundDevice *device )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs)
          return device->funcs->Suspend( device->device_data );

     return DR_OK;
}

DirectResult
fs_device_resume( CoreSoundDevice *device )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs)
          return device->funcs->Resume( device->device_data );

     return DR_OK;
}

void
fs_device_handle_fork( CoreSoundDevice  *device,
                       FusionForkAction  action,
                       FusionForkState   state )
{
     D_DEBUG_AT( CoreSound_Device, "%s( %p )\n", __FUNCTION__, device );

     D_ASSERT( device != NULL );

     if (device->funcs)
          device->funcs->HandleFork( device->device_data, action, state );
}
