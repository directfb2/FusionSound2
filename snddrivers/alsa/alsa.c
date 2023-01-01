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

#include <config.h>
#include <alsa/asoundlib.h>
#include <core/sound_driver.h>

D_DEBUG_DOMAIN( ALSA_Sound, "ALSA/Sound", "ALSA Sound Driver" );

FS_SOUND_DRIVER( alsa )

/**********************************************************************************************************************/

typedef struct {
     snd_pcm_t             *pcm;

     CoreSoundDeviceConfig *config;

     void                  *buffer;

     snd_pcm_uframes_t      offset;
} ALSAData;

/**********************************************************************************************************************/

static inline snd_pcm_format_t
fs2alsa_format( FSSampleFormat format )
{
     switch (format) {
          case FSSF_U8:
               return SND_PCM_FORMAT_U8;
          case FSSF_S16:
               return SND_PCM_FORMAT_S16;
          case FSSF_S24:
#ifdef WORDS_BIGENDIAN
               return SND_PCM_FORMAT_S24_3BE;
#else
               return SND_PCM_FORMAT_S24_3LE;
#endif
          case FSSF_S32:
               return SND_PCM_FORMAT_S32;
          case FSSF_FLOAT:
               return SND_PCM_FORMAT_FLOAT;
          default:
               break;
     }

     return SND_PCM_FORMAT_UNKNOWN;
}

static DirectResult
device_set_configuration( snd_pcm_t             *pcm,
                          CoreSoundDeviceConfig *config,
                          bool                   dma )
{
     snd_pcm_hw_params_t *params;
     int                  dir;
     unsigned int         periods    = 2;
     unsigned int         buffertime = (long long) config->buffersize * 1000000 / config->rate;

     snd_pcm_hw_params_alloca( &params );

     /* Choose all params. */
     if (snd_pcm_hw_params_any( pcm, params ) < 0) {
          D_ERROR( "ALSA/Sound: Unable to choose all params!\n" );
          return DR_FAILURE;
     }

     /* Set access type. */
     if (snd_pcm_hw_params_set_access( pcm, params,
                                       dma ? SND_PCM_ACCESS_MMAP_INTERLEAVED : SND_PCM_ACCESS_RW_INTERLEAVED ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set interleaved %saccess!\n", dma ? "MMAP " : "RW " );
          return DR_FAILURE;
     }

     /* Set number of channels. */
     if (snd_pcm_hw_params_set_channels( pcm, params, FS_CHANNELS_FOR_MODE( config->mode ) ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set channel mode!\n" );
          return DR_UNSUPPORTED;
     }

     /* Set output format. */
     if (snd_pcm_hw_params_set_format( pcm, params, fs2alsa_format( config->format ) ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set sample format!\n" );
          return DR_UNSUPPORTED;
     }

     /* Disable software resampling. */
     snd_pcm_hw_params_set_rate_resample( pcm, params, 0 );

     /* Set sample rate. */
     dir = 0;
     if (snd_pcm_hw_params_set_rate_near( pcm, params, &config->rate, &dir ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set sample rate!\n" );
          return DR_UNSUPPORTED;
     }

     /* Set buffer time. */
     dir = 0;
     if (snd_pcm_hw_params_set_buffer_time_near( pcm, params, &buffertime, &dir ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set buffer time!\n" );
          return DR_UNSUPPORTED;
     }

     /* Set number of periods. */
     dir = 1;
     if (snd_pcm_hw_params_set_periods_near( pcm, params, &periods, &dir ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't set number of periods!\n" );
          return DR_UNSUPPORTED;
     }

     /* Install params. */
     if (snd_pcm_hw_params( pcm, params ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't install params!\n" );
          return DR_UNSUPPORTED;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
device_probe()
{
     int         err;
     const char *value;
     snd_pcm_t  *pcm;

     if ((value = direct_config_get_value( "devname" )))
          err = snd_pcm_open( &pcm, value, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK );
     else
          err = snd_pcm_open( &pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK );

     if (err < 0)
          return DR_IO;

     snd_pcm_close( pcm );

     return DR_OK;
}

static void
device_get_driver_info( SoundDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 2;

     snprintf( driver_info->name,   FS_SOUND_DRIVER_INFO_NAME_LENGTH,   "ALSA" );
     snprintf( driver_info->vendor, FS_SOUND_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     driver_info->device_data_size = sizeof(ALSAData);
}

static DirectResult
device_open( void                  *device_data,
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     DirectResult  ret;
     int           err;
     ALSAData     *data = device_data;
     const char   *value;
     snd_ctl_t    *ctl;
     bool          dma;

     D_DEBUG_AT( ALSA_Sound, "%s()\n", __FUNCTION__ );

     /* Open sound device in non-blocking mode. */
     if ((value = direct_config_get_value( "devname" ))) {
          err = snd_pcm_open( &data->pcm, value, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK );
          D_INFO( "ALSA/Sound: Using device '%s' as specified in FusionSound configuration\n", value );
     }
     else {
          err = snd_pcm_open( &data->pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK );
          D_INFO( "ALSA/Sound: Using device 'default' (default)\n" );
     }

     if (err < 0) {
          D_ERROR( "ALSA/Sound: Failed to open device!    --> %s\n", snd_strerror( err ) );
          return DR_IO;
     }

     /* DMA. */
     if (direct_config_has_name( "dma" ) && !direct_config_has_name( "no-dma" )) {
          dma = true;
          D_INFO( "ALSA/Sound: Using DMA\n" );
     }
     else
          dma = false;

     /* Set non-block mode. */
     if (snd_pcm_nonblock( data->pcm, 0 ) < 0) {
          D_ERROR( "ALSA/Sound: Couldn't disable non-blocking mode!\n" );
          return DR_IO;
     }

     /* Configure device. */
     ret = device_set_configuration( data->pcm, config, dma );
     if (ret) {
          snd_pcm_close( data->pcm );
          return ret;
     }

     data->config = config;

     if (!dma) {
          int bytes_per_frame = FS_CHANNELS_FOR_MODE( config->mode ) * FS_BYTES_PER_SAMPLE( config->format );

          data->buffer = D_MALLOC( config->buffersize * bytes_per_frame );
          if (!data->buffer) {
               snd_pcm_close( data->pcm );
               return D_OOM();
          }
     }

     /* Fill device information. */
     if ((value = direct_config_get_value( "devname" )))
          err = snd_ctl_open( &ctl, value, SND_CTL_READONLY | SND_CTL_NONBLOCK );
     else
          err = snd_ctl_open( &ctl, "default", SND_CTL_READONLY | SND_CTL_NONBLOCK );

     if (err == 0) {
          snd_ctl_card_info_t *info;

          snd_ctl_card_info_alloca( &info );

          snd_ctl_card_info( ctl, info );

          snprintf( device_info->name, FS_SOUND_DEVICE_INFO_NAME_LENGTH, snd_ctl_card_info_get_name( info ) );

          device_info->caps = DCF_VOLUME;

          snd_ctl_close( ctl );
     }

     return DR_OK;
}

static int
try_recover( snd_pcm_t *pcm,
             int        err )
{
     switch (err) {
          case -EPIPE:
               err = snd_pcm_prepare( pcm );
               break;

          case -ESTRPIPE:
               while ((err = snd_pcm_resume( pcm )) == -EAGAIN)
                    sleep( 1 );
               if (err < 0)
                    err = snd_pcm_prepare( pcm );
               break;

          default:
               break;
     }

     return err;
}

static DirectResult
device_get_buffer( void          *device_data,
                   u8           **ret_addr,
                   unsigned int  *ret_avail )
{
     ALSAData *data = device_data;

     if (data->buffer) {
          *ret_addr  = data->buffer;
          *ret_avail = data->config->buffersize;
     }
     else {
          int                           err;
          snd_pcm_sframes_t             avail;
          snd_pcm_uframes_t             frames;
          const snd_pcm_channel_area_t *areas;

          while (1) {
               avail = snd_pcm_avail_update( data->pcm );
               if (avail < 0) {
                    err = try_recover( data->pcm, avail );
                    if (err < 0) {
                         D_ERROR( "ALSA/Sound: snd_pcm_avail_update() failed!    --> %s\n", snd_strerror( err ) );
                         return DR_FAILURE;
                     }
                     continue;
               }
               else if (avail == 0) {
                    if (snd_pcm_state( data->pcm ) == SND_PCM_STATE_PREPARED)
                         err = snd_pcm_start( data->pcm );
                    else
                         err = snd_pcm_wait( data->pcm, -1 );
                    if (err < 0)
                         err = try_recover( data->pcm, err );
                    if (err < 0) {
                         D_ERROR( "ALSA/Sound: snd_pcm_avail_update() failed!    --> %s\n", snd_strerror( err ) );
                         return DR_FAILURE;
                    }
                    continue;
               }

               frames = avail;

               err = snd_pcm_mmap_begin( data->pcm, &areas, &data->offset, &frames );
               if (err < 0) {
                    err = try_recover( data->pcm, err );
                    if (err < 0) {
                         D_ERROR( "ALSA/Sound: snd_pcm_mmap_begin() failed!    --> %s\n", snd_strerror( err ) );
                         return DR_FAILURE;
                    }
                    continue;
               }

               *ret_addr  = areas[0].addr + (data->offset * areas[0].step >> 3);
               *ret_avail = frames;
               break;
          }
     }

     return DR_OK;
}

static DirectResult
device_commit_buffer( void         *device_data,
                      unsigned int  frames )
{
     int                err;
     ALSAData          *data = device_data;
     snd_pcm_sframes_t  r;

     if (data->buffer) {
          u8 *buffer = data->buffer;

          while (frames) {
               r = snd_pcm_writei( data->pcm, buffer, frames );
               if (r < 0) {
                    err = try_recover( data->pcm, r );
                    if (err < 0) {
                         D_ERROR( "ALSA/Sound: snd_pcm_writei() failed!    --> %s\n", snd_strerror( err ) );
                         return DR_FAILURE;
                    }
               }
               frames -= r;
               buffer += snd_pcm_frames_to_bytes( data->pcm, r );
          }
     }
     else {
          while (1) {
               r = snd_pcm_mmap_commit( data->pcm, data->offset, frames );
               if (r < 0) {
                    err = try_recover( data->pcm, r );
                    if (r < 0) {
                         D_ERROR( "ALSA/Sound: snd_pcm_mmap_commit() failed!    --> %s\n", snd_strerror( err ) );
                         return DR_FAILURE;
                    }
                    continue;
               }
               break;
          }
     }

     return DR_OK;
}

static void
device_get_output_delay( void *device_data,
                         int  *ret_delay )
{
     ALSAData          *data  = device_data;
     snd_pcm_sframes_t  delay = 0;

     snd_pcm_delay( data->pcm, &delay );

     *ret_delay = delay;
}

static DirectResult
device_get_volume( void  *device_data,
                   float *ret_level )
{
     DirectResult          ret = DR_OK;
     int                   err;
     const char           *value;
     snd_mixer_t          *mixer;
     snd_mixer_selem_id_t *sid;
     snd_mixer_elem_t     *elem;
     long                  vol, min, max;

     if (snd_mixer_open( &mixer, 0 ) < 0)
          return DR_IO;

     if ((value = direct_config_get_value( "devname" )))
          err = snd_mixer_attach( mixer, value );
     else
          err = snd_mixer_attach( mixer, "default" );

     if (err < 0) {
          snd_mixer_close( mixer );
          return DR_IO;
     }

     if (snd_mixer_selem_register( mixer, NULL, NULL ) < 0) {
          snd_mixer_close( mixer );
          return DR_FAILURE;
     }

     if (snd_mixer_load( mixer ) < 0) {
          snd_mixer_close( mixer );
          return DR_FAILURE;
     }

     snd_mixer_selem_id_alloca( &sid );
     snd_mixer_selem_id_set_name( sid, "PCM" );

     elem = snd_mixer_find_selem( mixer, sid );
     if (!elem) {
          snd_mixer_close( mixer );
          return DR_UNSUPPORTED;
     }

     snd_mixer_selem_get_playback_volume_range( elem, &min, &max );

     if (snd_mixer_selem_get_playback_volume( elem, 0, &vol ) < 0)
          ret = DR_UNSUPPORTED;
     else
          *ret_level = (float) (vol - min) / (max - min);

     snd_mixer_close( mixer );

     return ret;
}

static DirectResult
device_set_volume( void  *device_data,
                   float  level )
{
     DirectResult          ret = DR_OK;
     int                   err;
     const char           *value;
     snd_mixer_t          *mixer;
     snd_mixer_selem_id_t *sid;
     snd_mixer_elem_t     *elem;
     long                  vol, min, max;

     if (snd_mixer_open( &mixer, 0 ) < 0)
          return DR_IO;

     if ((value = direct_config_get_value( "devname" )))
          err = snd_mixer_attach( mixer, value );
     else
          err = snd_mixer_attach( mixer, "default" );

     if (err < 0) {
          snd_mixer_close( mixer );
          return DR_IO;
     }

     if (snd_mixer_selem_register( mixer, NULL, NULL ) < 0) {
          snd_mixer_close( mixer );
          return DR_FAILURE;
     }

     if (snd_mixer_load( mixer ) < 0) {
          snd_mixer_close( mixer );
          return DR_FAILURE;
     }

     snd_mixer_selem_id_alloca( &sid );
     snd_mixer_selem_id_set_name( sid, "PCM" );

     elem = snd_mixer_find_selem( mixer, sid );
     if (!elem) {
          snd_mixer_close( mixer );
          return DR_UNSUPPORTED;
     }

     snd_mixer_selem_get_playback_volume_range( elem, &min, &max );

     vol = level * (max - min) + min;

     if (snd_mixer_selem_set_playback_volume_all( elem, vol ) < 0)
          ret = DR_UNSUPPORTED;

     snd_mixer_close( mixer );

     return ret;
}

static DirectResult
device_suspend( void *device_data )
{
     ALSAData *data = device_data;

     D_DEBUG_AT( ALSA_Sound, "%s()\n", __FUNCTION__ );

     snd_pcm_drop( data->pcm );

     snd_pcm_close( data->pcm );

     data->pcm = NULL;

     return DR_OK;
}

static DirectResult
device_resume( void *device_data )
{
     DirectResult  ret;
     int           err;
     ALSAData     *data = device_data;
     const char   *value;
     bool          dma;

     D_DEBUG_AT( ALSA_Sound, "%s()\n", __FUNCTION__ );

     if ((value = direct_config_get_value( "devname" )))
          err = snd_pcm_open( &data->pcm, value, SND_PCM_STREAM_PLAYBACK, 0 );
     else
          err = snd_pcm_open( &data->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0 );

     if (err < 0) {
          D_ERROR( "ALSA/Sound: Failed to reopen device!    --> %s\n", snd_strerror( err ) );
          return DR_IO;
     }

     if (direct_config_has_name( "dma" ) && !direct_config_has_name( "no-dma" ))
          dma = true;
     else
          dma = false;

     ret = device_set_configuration( data->pcm, data->config, dma );
     if (ret) {
          snd_pcm_close( data->pcm );
          data->pcm = NULL;
     }

     return ret;
}

static void
device_handle_fork( void             *device_data,
                    FusionForkAction  action,
                    FusionForkState   state )
{
     if (action == FFA_CLOSE) {
          switch (state) {
               case FFS_PREPARE:
                    device_suspend( device_data );
                    break;
               case FFS_PARENT:
                    device_resume( device_data );
                    break;
               default:
                    break;
          }
     }
}

static void
device_close( void *device_data )
{
     ALSAData *data = device_data;

     D_DEBUG_AT( ALSA_Sound, "%s()\n", __FUNCTION__ );

     if (data->buffer)
          D_FREE( data->buffer );

     if (data->pcm) {
          snd_pcm_drop( data->pcm );
          snd_pcm_close( data->pcm );
     }
}
