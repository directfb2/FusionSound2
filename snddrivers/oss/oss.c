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
#include <direct/util.h>
#include <linux/soundcard.h>

D_DEBUG_DOMAIN( OSS_Sound, "OSS/Sound", "OSS Sound Driver" );

FS_SOUND_DRIVER( oss )

/**********************************************************************************************************************/

typedef struct {
     int                    fd;

     CoreSoundDeviceConfig *config;

     void                  *buffer;

     int                    bytes_per_frame;
} OSSData;

/**********************************************************************************************************************/

static inline int
fs2oss_format( FSSampleFormat format )
{
     switch (format) {
          case FSSF_U8:
               return AFMT_U8;
          case FSSF_S16:
               return AFMT_S16_NE;
          default:
               break;
     }

     return -1;
}

static inline FSSampleFormat
oss2fs_format( int format )
{
     switch (format) {
          case AFMT_U8:
               return FSSF_U8;
          case AFMT_S16_NE:
               return FSSF_S16;
          default:
               break;
     }

     return -1;
}

static DirectResult
device_set_configuration( int                    fd,
                          CoreSoundDeviceConfig *config )
{
     int prof     = APF_NORMAL;
     int channels = FS_CHANNELS_FOR_MODE( config->mode );
     int fmt      = fs2oss_format( config->format );
     int rate     = config->rate;

     /* Set application profile. */
     if (ioctl( fd, SNDCTL_DSP_PROFILE, &prof ) < 0)
          D_WARN( "unable to set application profile" );

     /* Set number of channels. */
     if (ioctl( fd, SNDCTL_DSP_CHANNELS, &channels ) < 0 || channels != FS_CHANNELS_FOR_MODE( config->mode )) {
          D_ERROR( "OSS/Sound: Couldn't set channel mode!\n" );
          return DR_UNSUPPORTED;
     }

     /* Set output format. */
     if (fmt == -1 || ioctl( fd, SNDCTL_DSP_SETFMT, &fmt ) < 0 || oss2fs_format( fmt ) != config->format) {
          D_ERROR( "OSS/Sound: Couldn't set sample format!\n" );
          return DR_UNSUPPORTED;
     }

     /* Set sample rate. */
     if (ioctl( fd, SNDCTL_DSP_SPEED, &rate ) < 0) {
          D_ERROR( "OSS/Sound: Couldn't set sample rate!\n" );
          return DR_UNSUPPORTED;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
device_probe()
{
     const char *value;
     int         fd;

     if ((value = direct_config_get_value( "devdsp" )))
          fd = open( value, O_WRONLY | O_NONBLOCK );
     else
          fd = open( "/dev/dsp", O_WRONLY | O_NONBLOCK );

     if (fd < 0)
          return DR_IO;

     close( fd );

     return DR_OK;
}

static void
device_get_driver_info( SoundDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 2;

     snprintf( driver_info->name,   FS_SOUND_DRIVER_INFO_NAME_LENGTH,   "OSS" );
     snprintf( driver_info->vendor, FS_SOUND_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     driver_info->device_data_size = sizeof(OSSData);
}

static DirectResult
device_open( void                  *device_data,
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     DirectResult    ret;
     OSSData        *data = device_data;
     const char     *value;
     int             fd;
     audio_buf_info  ospace;

     D_DEBUG_AT( OSS_Sound, "%s()\n", __FUNCTION__ );

     /* Open sound device in non-blocking mode. */
     if ((value = direct_config_get_value( "devdsp" ))) {
          data->fd = open( value, O_WRONLY | O_NONBLOCK );
          D_INFO( "OSS/Sound: Using device %s as specified in FusionSound configuration\n", value );
     }
     else {
          data->fd = open( "/dev/dsp", O_WRONLY | O_NONBLOCK );
          D_INFO( "OSS/Sound: Using device /dev/dsp (default)\n" );
     }

     if (data->fd < 0) {
          D_PERROR( "OSS/Sound: Failed to open device!\n" );
          return DR_INIT;
     }

     fcntl( data->fd, F_SETFL, fcntl( data->fd, F_GETFL ) & ~O_NONBLOCK );

     /* Set close-on-exec flag. */
     fcntl( data->fd, F_SETFD, FD_CLOEXEC );

     /* Configure device. */
     ret = device_set_configuration( data->fd, config );
     if (ret) {
          close( data->fd );
          return ret;
     }

     data->config = config;

     data->bytes_per_frame = FS_CHANNELS_FOR_MODE( config->mode ) * FS_BYTES_PER_SAMPLE( config->format );

     data->buffer = D_MALLOC( config->buffersize * data->bytes_per_frame );
     if (!data->buffer) {
          close( data->fd );
          return D_OOM();
     }

     /* Query output space. */
     if (ioctl( data->fd, SNDCTL_DSP_GETOSPACE, &ospace ) < 0)
          D_WARN( "unable to get output space" );
     else
          D_INFO( "OSS/Sound: Max output delay is %u.%u ms\n",
                   (ospace.bytes / data->bytes_per_frame) * 1000  / config->rate,
                  ((ospace.bytes / data->bytes_per_frame) * 10000 / config->rate) % 10 );

     /* Fill device information. */
     if ((value = direct_config_get_value( "devmixer" )))
          fd = open( value, O_RDONLY );
     else
          fd = open( "/dev/mixer", O_RDONLY );

     if (fd > 0) {
          mixer_info mixer_info;
          int        mask = 0;

          memset( &mixer_info, 0, sizeof(mixer_info) );

          ioctl( fd, SOUND_MIXER_INFO, &mixer_info );

          direct_snputs( device_info->name, mixer_info.id, FS_SOUND_DEVICE_INFO_NAME_LENGTH );

          ioctl( fd, SOUND_MIXER_READ_DEVMASK, &mask );

          device_info->caps = DCF_NONE;
          if (mask & SOUND_MASK_PCM)
               device_info->caps |= DCF_VOLUME;

          close( fd );
     }

     return DR_OK;
}

static DirectResult
device_get_buffer( void          *device_data,
                   u8           **ret_addr,
                   unsigned int  *ret_avail )
{
     OSSData *data = device_data;

     *ret_addr  = data->buffer;
     *ret_avail = data->config->buffersize;

     return DR_OK;
}

static DirectResult
device_commit_buffer( void         *device_data,
                      unsigned int  frames )
{
     DirectResult  ret;
     OSSData      *data = device_data;

     if (write( data->fd, data->buffer, frames * data->bytes_per_frame ) < 0) {
          ret = errno2result( errno );
          D_DERROR( ret, "OSS/Sound: Couldn't write %u frames!\n", frames );
          return ret;
     }

     return DR_OK;
}

static void
device_get_output_delay( void *device_data,
                         int  *ret_delay )
{
     OSSData        *data = device_data;
     audio_buf_info  ospace;

     if (ioctl( data->fd, SNDCTL_DSP_GETOSPACE, &ospace ) < 0) {
          D_ONCE( "unable to get output space" );
          *ret_delay = 0;
          return;
     }

     *ret_delay = (ospace.fragsize * ospace.fragstotal - ospace.bytes) / data->bytes_per_frame;
}

static DirectResult
device_get_volume( void  *device_data,
                   float *ret_level )
{
     DirectResult  ret;
     const char   *value;
     int           fd;
     int           vol;

     if ((value = direct_config_get_value( "devmixer" )))
          fd = open( value, O_RDONLY );
     else
          fd = open( "/dev/mixer", O_RDONLY );

     if (fd < 0) {
          ret = errno2result( errno );
          return ret;
     }

     if (ioctl( fd, SOUND_MIXER_READ_PCM, &vol ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "OSS/Sound: SOUND_MIXER_READ_PCM failed!\n" );
          close( fd );
          return ret;
     }

     close( fd );

     *ret_level = ((vol & 0xff) + ((vol >> 8) & 0xff)) / 200.0f;

     return DR_OK;
}

static DirectResult
device_set_volume( void  *device_data,
                   float  level )
{
     DirectResult  ret;
     const char   *value;
     int           fd;
     int           vol;

     if ((value = direct_config_get_value( "devmixer" )))
          fd = open( value, O_RDONLY );
     else
          fd = open( "/dev/mixer", O_RDONLY );

     if (fd < 0) {
          ret = errno2result( errno );
          return ret;
     }

     vol  = level * 100.0f;
     vol |= vol << 8;
     if (ioctl( fd, SOUND_MIXER_WRITE_PCM, &vol ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "OSS/Sound: SOUND_MIXER_WRITE_PCM failed!\n" );
          close( fd );
          return ret;
     }

     close( fd );

     return DR_OK;
}

static DirectResult
device_suspend( void *device_data )
{
     OSSData *data = device_data;

     D_DEBUG_AT( OSS_Sound, "%s()\n", __FUNCTION__ );

     ioctl( data->fd, SNDCTL_DSP_RESET, 0 );

     close( data->fd );

     data->fd = -1;

     return DR_OK;
}

static DirectResult
device_resume( void *device_data )
{
     DirectResult  ret;
     OSSData      *data = device_data;
     const char   *value;

     D_DEBUG_AT( OSS_Sound, "%s()\n", __FUNCTION__ );

     if ((value = direct_config_get_value( "devdsp" )))
          data->fd = open( value, O_WRONLY );
     else
          data->fd = open( "/dev/dsp", O_WRONLY );

     if (data->fd < 0) {
          D_PERROR( "OSS/Sound: Failed to reopen device!\n" );
          return DR_INIT;
     }

     fcntl( data->fd, F_SETFD, FD_CLOEXEC );

     ret = device_set_configuration( data->fd, data->config );
     if (ret) {
          close( data->fd );
          data->fd = -1;
     }

     return ret;
}

static void
device_handle_fork( void             *device_data,
                    FusionForkAction  action,
                    FusionForkState   state )
{
     OSSData *data = device_data;

     if (action == FFA_CLOSE && state == FFS_CHILD) {
          close( data->fd );
          data->fd = -1;
     }
}

static void
device_close( void *device_data )
{
     OSSData *data = device_data;

     D_DEBUG_AT( OSS_Sound, "%s()\n", __FUNCTION__ );

     if (data->buffer)
          D_FREE( data->buffer );

     if (data->fd != -1) {
          ioctl( data->fd, SNDCTL_DSP_RESET, 0 );
          close( data->fd );
     }
}
