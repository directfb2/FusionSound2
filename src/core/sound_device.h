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

#ifndef __CORE__SOUND_DEVICE_H__
#define __CORE__SOUND_DEVICE_H__

#include <core/coretypes_sound.h>
#include <direct/modules.h>
#include <fusion/fusion.h>

DECLARE_MODULE_DIRECTORY( fs_sound_drivers );

/**********************************************************************************************************************/

#define FS_SOUND_DRIVER_ABI_VERSION 5

typedef struct {
     int major; /* major version */
     int minor; /* minor version */
} SoundDriverVersion;

typedef struct {
     SoundDriverVersion version;

     char               name[FS_SOUND_DRIVER_INFO_NAME_LENGTH];       /* Name of sound driver */
     char               vendor[FS_SOUND_DRIVER_INFO_VENDOR_LENGTH];   /* Vendor (or author) of the driver */
     char               url[FS_SOUND_DRIVER_INFO_URL_LENGTH];         /* URL for driver updates */
     char               license[FS_SOUND_DRIVER_INFO_LICENSE_LENGTH]; /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int       device_data_size;
} SoundDriverInfo;

typedef enum {
     DCF_NONE   = 0x00000000, /* None of these. */

     DCF_VOLUME = 0x00000001, /* The device supports volume level adjustment. */

     DCF_ALL    = 0x00000001  /* All of these. */
} DeviceCapabilitiesFlags;

#define FS_SOUND_DEVICE_INFO_NAME_LENGTH 96

typedef struct {
     char                    name[FS_SOUND_DEVICE_INFO_NAME_LENGTH]; /* Device name. */

     DeviceCapabilitiesFlags caps;                                   /* Device capabilities. */
} SoundDeviceInfo;

struct __FS_CoreSoundDeviceConfig {
     FSChannelMode  mode;
     FSSampleFormat format;
     unsigned int   rate;
     unsigned int   buffersize;
};

typedef struct {
     /*
      * Probe.
      */
     DirectResult (*Probe)         ( void );

     /*
      * Get driver information.
      */
     void         (*GetDriverInfo) ( SoundDriverInfo        *driver_info);

     /*
      * Open the device, get device information and apply given configuration.
      */
     DirectResult (*OpenDevice)    ( void                   *device_data,
                                     SoundDeviceInfo        *device_info,
                                     CoreSoundDeviceConfig  *config );

     /*
      * Begin access to the ring buffer, return buffer pointer and available frames.
      */
     DirectResult (*GetBuffer)     ( void                   *device_data,
                                     u8                    **ret_addr,
                                     unsigned int           *ret_avail );

     /*
      * Finish access to the ring buffer, commit the specified number of frames.
      */
     DirectResult (*CommitBuffer)  ( void                   *device_data,
                                     unsigned int            frames );

     /*
      * Get output delay in frames.
      */
     void         (*GetOutputDelay)( void                   *device_data,
                                     int                    *ret_delay );

     /*
      * Get volume level.
      */
     DirectResult (*GetVolume)     ( void                   *device_data,
                                     float                  *ret_level );

     /*
      * Set volume level.
      */
     DirectResult (*SetVolume)     ( void                   *device_data,
                                     float                   level );

     /*
      * Suspend the device.
      */
     DirectResult (*Suspend)       ( void                   *device_data );

     /*
      * Resume the device.
      */
     DirectResult (*Resume)        ( void                   *device_data );

     /*
      * Handle fork.
      */
     void         (*HandleFork)    ( void                   *device_data,
                                     FusionForkAction        action,
                                     FusionForkState         state );

     /*
      * Close the device.
      */
     void         (*CloseDevice)   ( void                   *device_data );
} SoundDriverFuncs;

/**********************************************************************************************************************/

DirectResult            fs_device_initialize      ( CoreSound              *core,
                                                    CoreSoundDeviceConfig  *config,
                                                    CoreSoundDevice       **ret_device );

void                    fs_device_shutdown        ( CoreSoundDevice        *device );

void                    fs_device_get_description (  CoreSoundDevice       *device,
                                                     FSDeviceDescription   *ret_desc );

DeviceCapabilitiesFlags fs_device_get_capabilities( CoreSoundDevice        *device );

DirectResult            fs_device_get_buffer      ( CoreSoundDevice        *device,
                                                    u8                    **ret_addr,
                                                    unsigned int           *ret_avail );

DirectResult            fs_device_commit_buffer   ( CoreSoundDevice        *device,
                                                    unsigned int            frames );

void                    fs_device_get_output_delay( CoreSoundDevice        *device,
                                                    int                    *ret_delay );

DirectResult            fs_device_get_volume      ( CoreSoundDevice        *device,
                                                    float                  *ret_level );

DirectResult            fs_device_set_volume      ( CoreSoundDevice        *device,
                                                    float                   level );

DirectResult            fs_device_suspend         ( CoreSoundDevice        *device );

DirectResult            fs_device_resume          ( CoreSoundDevice        *device );

void                    fs_device_handle_fork     ( CoreSoundDevice        *device,
                                                    FusionForkAction        action,
                                                    FusionForkState         state );

#endif
