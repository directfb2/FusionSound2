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

#include <direct/stream.h>
#include <media/ifusionsoundmusicprovider.h>

D_DEBUG_DOMAIN( MusicProvider, "IFusionSoundMusicProvider", "IFusionSoundMusicProvider Interface" );

/**********************************************************************************************************************/

static DirectResult
IFusionSoundMusicProvider_AddRef( IFusionSoundMusicProvider *thiz )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_Release( IFusionSoundMusicProvider *thiz )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                           FSMusicProviderCapabilities *ret_caps )
{
     if (!ret_caps)
          return DR_INVARG;

     *ret_caps = FMCAPS_BASIC;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_EnumTracks( IFusionSoundMusicProvider *thiz,
                                      FSTrackCallback            callback,
                                      void                      *ctx )
{
     FSTrackDescription desc;
     DirectResult       ret;

     if (!callback)
          return DR_INVARG;

     ret = thiz->GetTrackDescription( thiz, &desc );
     if (ret)
          return ret;

     callback( 0, desc, ctx );

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_GetTrackID( IFusionSoundMusicProvider *thiz,
                                      FSTrackID                 *ret_track_id )
{
     if (!ret_track_id)
          return DR_INVARG;

     *ret_track_id = 0;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                               FSTrackDescription        *ret_desc )
{
     if (!ret_desc)
          return DR_INVARG;

     memset( ret_desc, 0, sizeof(FSTrackDescription) );

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                FSStreamDescription       *ret_desc )
{
     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags = FSSDF_NONE;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                FSBufferDescription       *ret_desc )
{
     if (!ret_desc)
          return DR_INVARG;

     ret_desc->flags = FSBDF_NONE;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_SelectTrack( IFusionSoundMusicProvider *thiz,
                                       FSTrackID                  track_id )
{
     if (track_id != 0)
          return DR_UNSUPPORTED;

     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_PlayToStream( IFusionSoundMusicProvider *thiz,
                                        IFusionSoundStream        *destination )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                        IFusionSoundBuffer        *destination,
                                        FMBufferCallback           callback,
                                        void                      *ctx )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_Stop( IFusionSoundMusicProvider *thiz )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetStatus( IFusionSoundMusicProvider *thiz,
                                     FSMusicProviderStatus     *status )
{
     if (!status)
          return DR_INVARG;

     *status = FMSTATE_UNKNOWN;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_SeekTo( IFusionSoundMusicProvider *thiz,
                                  double                     seconds )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetPos( IFusionSoundMusicProvider *thiz,
                                  double                    *ret_seconds )
{
     if (!ret_seconds)
          return DR_INVARG;

     *ret_seconds = 0.0;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_GetLength( IFusionSoundMusicProvider *thiz,
                                     double                    *ret_seconds )
{
     if (!ret_seconds)
          return DR_INVARG;

     *ret_seconds = 0.0;

     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                            FSMusicProviderPlaybackFlags  flags )
{
     return DR_UNIMPLEMENTED;
}

static DirectResult
IFusionSoundMusicProvider_WaitStatus( IFusionSoundMusicProvider *thiz,
                                      FSMusicProviderStatus      mask,
                                      unsigned int               timeout )
{
     return DR_UNIMPLEMENTED;
}

static void
IFusionSoundMusicProvider_Construct( IFusionSoundMusicProvider *thiz )
{
     D_DEBUG_AT( MusicProvider, "%s( %p )\n", __FUNCTION__, thiz );

     thiz->AddRef               = IFusionSoundMusicProvider_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_SetPlaybackFlags;
     thiz->WaitStatus           = IFusionSoundMusicProvider_WaitStatus;
}

DirectResult
IFusionSoundMusicProvider_Create( const char                 *filename,
                                  IFusionSoundMusicProvider **ret_interface )
{
     DirectResult                            ret;
     DirectInterfaceFuncs                   *funcs = NULL;
     IFusionSoundMusicProvider              *iface;
     IFusionSoundMusicProvider_ProbeContext  ctx;
     DirectStream                           *stream;

     D_DEBUG_AT( MusicProvider, "%s( '%s' )\n", __FUNCTION__, filename );

     /* Open the stream. */
     ret = direct_stream_create( filename, &stream );
     if (ret)
          return ret;

     /* Clear probe context header. */
     memset( ctx.header, 0, sizeof(ctx.header) );

     /* Fill out probe context. */
     ctx.filename = filename;
     ctx.mimetype = direct_stream_mime( stream );
     ctx.stream   = stream;

     /* Wait until 64 bytes are available. */
     ret = direct_stream_wait( stream, sizeof(ctx.header), NULL );
     if (ret) {
          direct_stream_destroy( stream );
          return ret;
     }

     /* Read the first 64 bytes. */
     direct_stream_peek( stream, sizeof(ctx.header), 0, ctx.header, NULL );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IFusionSoundMusicProvider", NULL, DirectProbeInterface, &ctx );
     if (ret) {
          direct_stream_destroy( stream );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( iface, IFusionSoundMusicProvider );

     /* Initialize interface pointers. */
     IFusionSoundMusicProvider_Construct( iface );

     /* Construct the interface. */
     ret = funcs->Construct( iface, filename, stream );
     if (ret)
          return ret;

     *ret_interface = iface;

     direct_stream_destroy( stream );

     return DR_OK;
}
