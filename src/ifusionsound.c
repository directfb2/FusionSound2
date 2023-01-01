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

#include <buffer/ifusionsoundbuffer.h>
#include <buffer/ifusionsoundstream.h>
#include <core/core_sound.h>
#include <core/sound_buffer.h>
#include <core/sound_device.h>
#include <fusionsound_util.h>
#include <ifusionsound.h>
#include <media/ifusionsoundmusicprovider.h>

D_DEBUG_DOMAIN( FusionSound, "IFusionSound", "IFusionSound Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IFusionSound
 */
typedef struct {
     int        ref;  /* reference counter */

     CoreSound *core; /* the core object */
} IFusionSound_data;

/**********************************************************************************************************************/

static DirectResult
IFusionSound_Destruct( IFusionSound *thiz )
{
     DirectResult       ret;
     IFusionSound_data *data = thiz->priv;

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_core_destroy( data->core, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     if (thiz == ifusionsound_singleton)
          ifusionsound_singleton = NULL;

     return ret;
}

static DirectResult
IFusionSound_AddRef( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionSound_Release( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          return IFusionSound_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionSound_GetDeviceDescription( IFusionSound        *thiz,
                                   FSDeviceDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!ret_desc)
          return DR_INVARG;

     *ret_desc = *fs_core_device_description( data->core );

     return DR_OK;
}

static DirectResult
IFusionSound_CreateBuffer( IFusionSound               *thiz,
                           const FSBufferDescription  *desc,
                           IFusionSoundBuffer        **ret_interface )
{
     DirectResult           ret;
     FSChannelMode          mode;
     FSSampleFormat         format;
     int                    rate;
     CoreSoundBuffer       *buffer;
     CoreSoundDeviceConfig *config;
     IFusionSoundBuffer    *iface;
     int                    length = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!desc || !ret_interface)
          return DR_INVARG;

     config = fs_core_device_config( data->core );
     mode   = config->mode;
     format = config->format;
     rate   = config->rate;

     if (desc->flags & ~FSBDF_ALL)
          return DR_INVARG;

     if (desc->flags & FSBDF_CHANNELMODE) {
          switch (desc->channelmode) {
               case FSCM_MONO:
               case FSCM_STEREO:
#if FS_MAX_CHANNELS > 2
               case FSCM_STEREO21:
               case FSCM_STEREO30:
               case FSCM_STEREO31:
               case FSCM_SURROUND30:
               case FSCM_SURROUND31:
               case FSCM_SURROUND40_2F2R:
               case FSCM_SURROUND41_2F2R:
               case FSCM_SURROUND40_3F1R:
               case FSCM_SURROUND41_3F1R:
               case FSCM_SURROUND50:
               case FSCM_SURROUND51:
#endif
                    mode = desc->channelmode;
                    break;

               default:
                    return DR_INVARG;
          }
     }
     else if (desc->flags & FSBDF_CHANNELS) {
          switch (desc->channels) {
               case 1 ... FS_MAX_CHANNELS:
                    mode = fs_mode_for_channels( desc->channels );
                    break;

               default:
                    return DR_INVARG;
          }
     }

     if (desc->flags & FSBDF_SAMPLEFORMAT) {
          switch (format) {
               case FSSF_U8:
               case FSSF_S16:
               case FSSF_S24:
               case FSSF_S32:
               case FSSF_FLOAT:
                    format = desc->sampleformat;
                    break;

               default:
                    return DR_INVARG;
          }
     }

     if (desc->flags & FSBDF_SAMPLERATE) {
          if (desc->samplerate < 100)
               return DR_UNSUPPORTED;

          rate = desc->samplerate;
     }

     if (desc->flags & FSBDF_LENGTH)
          length = desc->length;

     if (length < 1)
          return DR_INVARG;

     if (length > FS_MAX_FRAMES)
          return DR_LIMITEXCEEDED;

     ret = fs_buffer_create( data->core, length, mode, format, rate, &buffer );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IFusionSoundBuffer );

     ret = IFusionSoundBuffer_Construct( iface, data->core, buffer, length, mode, format, rate );

     fs_buffer_unref( buffer );

     if (ret == DR_OK)
          *ret_interface = iface;

     return ret;
}

static DirectResult
IFusionSound_CreateStream( IFusionSound               *thiz,
                           const FSStreamDescription  *desc,
                           IFusionSoundStream        **ret_interface )
{
     DirectResult           ret;
     FSChannelMode          mode;
     FSSampleFormat         format;
     int                    rate;
     CoreSoundBuffer       *buffer;
     CoreSoundDeviceConfig *config;
     IFusionSoundStream    *iface;
     int                    buffersize = 0;
     int                    prebuffer  = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!ret_interface)
          return DR_INVARG;

     config = fs_core_device_config( data->core );
     mode   = config->mode;
     format = config->format;
     rate   = config->rate;

     if (desc) {
          if (desc->flags & ~FSSDF_ALL)
               return DR_INVARG;

          if (desc->flags & FSSDF_CHANNELMODE) {
               switch (desc->channelmode) {
                    case FSCM_MONO:
                    case FSCM_STEREO:
#if FS_MAX_CHANNELS > 2
                    case FSCM_STEREO21:
                    case FSCM_STEREO30:
                    case FSCM_STEREO31:
                    case FSCM_SURROUND30:
                    case FSCM_SURROUND31:
                    case FSCM_SURROUND40_2F2R:
                    case FSCM_SURROUND41_2F2R:
                    case FSCM_SURROUND40_3F1R:
                    case FSCM_SURROUND41_3F1R:
                    case FSCM_SURROUND50:
                    case FSCM_SURROUND51:
#endif
                         mode = desc->channelmode;
                         break;

                    default:
                         return DR_INVARG;
               }
          }
          else if (desc->flags & FSSDF_CHANNELS) {
               switch (desc->channels) {
                    case 1 ... FS_MAX_CHANNELS:
                         mode = fs_mode_for_channels( desc->channels );
                         break;

                    default:
                         return DR_INVARG;
               }
          }

          if (desc->flags & FSSDF_SAMPLEFORMAT) {
               switch (desc->sampleformat) {
                    case FSSF_U8:
                    case FSSF_S16:
                    case FSSF_S24:
                    case FSSF_S32:
                    case FSSF_FLOAT:
                         format = desc->sampleformat;
                         break;

                    default:
                         return DR_INVARG;
               }
          }

          if (desc->flags & FSSDF_SAMPLERATE) {
               if (desc->samplerate < 100)
                    return DR_UNSUPPORTED;

               rate = desc->samplerate;
          }

          if (desc->flags & FSSDF_BUFFERSIZE) {
               if (desc->buffersize < 1)
                    return DR_INVARG;

               buffersize = desc->buffersize;
          }

          if (desc->flags & FSSDF_PREBUFFER) {
               if (desc->prebuffer >= buffersize)
                    return DR_INVARG;

               prebuffer = desc->prebuffer;
          }
     }

     /* Default ring buffer size is 200 milliseconds. */
     if (!buffersize)
          buffersize = rate / 5;

     /* Limit ring buffer size to 5 seconds. */
     if (buffersize > rate * 5)
          return DR_LIMITEXCEEDED;

     ret = fs_buffer_create( data->core, buffersize, mode, format, rate, &buffer );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IFusionSoundStream );

     ret = IFusionSoundStream_Construct( iface, data->core, buffer, buffersize, mode, format, rate, prebuffer );

     fs_buffer_unref( buffer );

     if (ret == DR_OK)
          *ret_interface = iface;

     return ret;
}

static DirectResult
IFusionSound_CreateMusicProvider( IFusionSound               *thiz,
                                  const char                 *filename,
                                  IFusionSoundMusicProvider **ret_interface )
{
     DirectResult               ret;
     IFusionSoundMusicProvider *iface;

     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!ret_interface || !filename)
          return DR_INVARG;

     /* Create (probing) the music provider. */
     ret = IFusionSoundMusicProvider_Create( filename, &iface );

     if (ret == DR_OK)
          *ret_interface = iface;

     return ret;
}

static DirectResult
IFusionSound_GetMasterVolume( IFusionSound *thiz,
                              float        *ret_level )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!ret_level)
          return DR_INVARG;

     return fs_core_get_master_volume( data->core, ret_level );
}

static DirectResult
IFusionSound_SetMasterVolume( IFusionSound *thiz,
                              float         level )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (level < 0.0f || level > 1.0f)
          return DR_INVARG;

     return fs_core_set_master_volume( data->core, level );
}

static DirectResult
IFusionSound_GetLocalVolume( IFusionSound *thiz,
                             float        *ret_level )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (!ret_level)
          return DR_INVARG;

     return fs_core_get_local_volume( data->core, ret_level );
}

static DirectResult
IFusionSound_SetLocalVolume( IFusionSound *thiz,
                             float         level )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments */
     if (level < 0.0f || level > 1.0f)
          return DR_INVARG;

     return fs_core_set_local_volume( data->core, level );
}

static DirectResult
IFusionSound_Suspend( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     return fs_core_suspend( data->core );
}

static DirectResult
IFusionSound_Resume( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     return fs_core_resume( data->core );
}

static DirectResult
IFusionSound_GetMasterFeedback( IFusionSound *thiz,
                                float        *ret_left,
                                float        *ret_right )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSound )

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     return fs_core_get_master_feedback( data->core, ret_left, ret_right );
}

DirectResult
IFusionSound_Construct( IFusionSound *thiz )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSound );

     D_DEBUG_AT( FusionSound, "%s( %p )\n", __FUNCTION__, thiz );

     ret = fs_core_create( &data->core );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref = 1;

     thiz->AddRef               = IFusionSound_AddRef;
     thiz->Release              = IFusionSound_Release;
     thiz->GetDeviceDescription = IFusionSound_GetDeviceDescription;
     thiz->CreateBuffer         = IFusionSound_CreateBuffer;
     thiz->CreateStream         = IFusionSound_CreateStream;
     thiz->CreateMusicProvider  = IFusionSound_CreateMusicProvider;
     thiz->GetMasterVolume      = IFusionSound_GetMasterVolume;
     thiz->SetMasterVolume      = IFusionSound_SetMasterVolume;
     thiz->GetLocalVolume       = IFusionSound_GetLocalVolume;
     thiz->SetLocalVolume       = IFusionSound_SetLocalVolume;
     thiz->Suspend              = IFusionSound_Suspend;
     thiz->Resume               = IFusionSound_Resume;
     thiz->GetMasterFeedback    = IFusionSound_GetMasterFeedback;

     return DR_OK;
}
