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
#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( CoreSound_Buffer, "CoreSound/Buffer", "FusionSound Core Buffer" );

/**********************************************************************************************************************/

struct __FS_CoreSoundBuffer {
     FusionObject         object;

     int                  length;
     FSChannelMode        mode;
     FSSampleFormat       format;
     int                  rate;
     int                  bytes;
     void                *data;

     FusionSHMPoolShared *shmpool;
};

/**********************************************************************************************************************/

static void
buffer_destructor( FusionObject *object,
                   bool          zombie,
                   void         *ctx )
{
     CoreSoundBuffer *buffer = (CoreSoundBuffer*) object;

     D_ASSERT( buffer != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "Destroying buffer %p (len %d, mode %08x, fmt %08x, rate %d%s)\n",
                 buffer, buffer->length, buffer->mode, buffer->format, buffer->rate, zombie ? " ZOMBIE" : "" );

     SHFREE( buffer->shmpool, buffer->data );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
fs_buffer_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Sound Buffers",
                                       sizeof(CoreSoundBuffer), sizeof(CoreSoundBufferNotification),
                                       buffer_destructor, NULL, world );
}

/**********************************************************************************************************************/

DirectResult
fs_buffer_create( CoreSound        *core,
                  int               length,
                  FSChannelMode     mode,
                  FSSampleFormat    format,
                  int               rate,
                  CoreSoundBuffer **ret_buffer )
{
     int                  bytes;
     int                  channels;
     CoreSoundBuffer     *buffer;
     FusionSHMPoolShared *pool;

     D_ASSERT( core != NULL );
     D_ASSERT( length > 0 );
     D_ASSERT( mode != FSCM_UNKNOWN );
     D_ASSERT( format != FSSF_UNKNOWN );
     D_ASSERT( rate > 0 );
     D_ASSERT( ret_buffer != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "%s( len %d, mode %08x, fmt %08x, rate %d )\n", __FUNCTION__,
                 length, mode, format, rate );

     /* Create the buffer object. */
     buffer = fs_core_create_buffer( core );
     if (!buffer)
          return DR_FUSION;

     bytes    = FS_BYTES_PER_SAMPLE( format );
     channels = FS_CHANNELS_FOR_MODE( mode );
     pool     = fs_core_shmpool( core );

     buffer->data = SHMALLOC( pool, length * bytes * channels );
     if (!buffer->data) {
          fusion_object_destroy( &buffer->object );
          return DR_NOLOCALMEMORY;
     }

     buffer->length  = length;
     buffer->mode    = mode;
     buffer->format  = format;
     buffer->rate    = rate;
     buffer->bytes   = bytes * channels;
     buffer->shmpool = pool;

     /* Activate the object. */
     fusion_object_activate( &buffer->object );

     /* Return the new buffer. */
     *ret_buffer = buffer;

     D_DEBUG_AT( CoreSound_Buffer, "  -> %p\n", buffer );

     return DR_OK;
}

DirectResult
fs_buffer_lock( CoreSoundBuffer  *buffer,
                int               pos,
                int               length,
                void            **ret_data,
                int              *ret_bytes )
{
     D_ASSERT( buffer != NULL );
     D_ASSERT( pos >= 0 );
     D_ASSERT( pos < buffer->length );
     D_ASSERT( length >= 0 );
     D_ASSERT( length + pos <= buffer->length );
     D_ASSERT( ret_data != NULL );
     D_ASSERT( ret_bytes != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "%s( %p, pos %d, len %d )\n", __FUNCTION__, buffer, pos, length );

     if (!length)
          length = buffer->length - pos;

     *ret_data  = buffer->data + buffer->bytes * pos;
     *ret_bytes = buffer->bytes * length;

     return DR_OK;
}

DirectResult
fs_buffer_unlock( CoreSoundBuffer *buffer )
{
     D_ASSERT( buffer != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "%s( %p )\n", __FUNCTION__, buffer );

     return DR_OK;
}

int fs_buffer_bytes  ( CoreSoundBuffer  *buffer )
{
     D_ASSERT( buffer != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "%s( %p )\n", __FUNCTION__, buffer );

     return buffer->bytes;
};

FSChannelMode fs_buffer_mode( CoreSoundBuffer  *buffer )
{
     D_ASSERT( buffer != NULL );

     D_DEBUG_AT( CoreSound_Buffer, "%s( %p )\n", __FUNCTION__, buffer );

     return buffer->mode;
};

typedef struct {
#ifdef WORDS_BIGENDIAN
     s8 c;
     u8 b;
     u8 a;
#else
     u8 a;
     u8 b;
     s8 c;
#endif
} __attribute__((packed)) s24;

#define FORMAT u8
#define TYPE   u8
#define FSF_FROM_SRC(s,i) fsf_from_u8(s[i])
#include "sound_mix.h"
#undef  FSF_FROM_SRC
#undef  TYPE
#undef  FORMAT

#define FORMAT s16
#define TYPE   s16
#define FSF_FROM_SRC(s,i) fsf_from_s16(s[i])
#include "sound_mix.h"
#undef  FSF_FROM_SRC
#undef  TYPE
#undef  FORMAT

#define FORMAT s24
#define TYPE   s24
#define FSF_FROM_SRC(s,i) fsf_from_s24((long) (s[i].a | (s[i].b << 8) | (s[i].c << 16)))
#include "sound_mix.h"
#undef  FSF_FROM_SRC
#undef  TYPE
#undef  FORMAT

#define FORMAT s32
#define TYPE   s32
#define FSF_FROM_SRC(s,i) fsf_from_s32(s[i])
#include "sound_mix.h"
#undef  FSF_FROM_SRC
#undef  TYPE
#undef  FORMAT

#define FORMAT f32
#define TYPE   float
#define FSF_FROM_SRC(s,i) fsf_from_float(s[i])
#include "sound_mix.h"
#undef  FSF_FROM_SRC
#undef  TYPE
#undef  FORMAT

typedef int (*SoundMXFunc) ( CoreSoundBuffer *buffer,
                             __fsf           *mixing,
                             FSChannelMode    mode,
                             long             pos,
                             long             inc,
                             long             max,
                             __fsf            levels[6],
                             bool             last );

static const SoundMXFunc MIX_FW[FS_NUM_SAMPLEFORMATS][FS_MAX_CHANNELS] = {
     {
          mix_from_u8_mono_fw,   mix_from_u8_stereo_fw,
#if FS_MAX_CHANNELS > 2
          mix_from_u8_multi_fw,  mix_from_u8_multi_fw,
          mix_from_u8_multi_fw,  mix_from_u8_multi_fw
#endif
     }, /* FSSF_U8 */
     {
          mix_from_s16_mono_fw,  mix_from_s16_stereo_fw,
#if FS_MAX_CHANNELS > 2
          mix_from_s16_multi_fw, mix_from_s16_multi_fw,
          mix_from_s16_multi_fw, mix_from_s16_multi_fw
#endif
     }, /* FSSF_S16 */
     {
          mix_from_s24_mono_fw,  mix_from_s24_stereo_fw,
#if FS_MAX_CHANNELS > 2
          mix_from_s24_multi_fw, mix_from_s24_multi_fw,
          mix_from_s24_multi_fw, mix_from_s24_multi_fw
#endif
     }, /* FSSF_S24 */
     {
          mix_from_s32_mono_fw,  mix_from_s32_stereo_fw,
#if FS_MAX_CHANNELS > 2
          mix_from_s32_multi_fw, mix_from_s32_multi_fw,
          mix_from_s32_multi_fw, mix_from_s32_multi_fw
#endif
     }, /* FSSF_S32 */
     {
          mix_from_f32_mono_fw,  mix_from_f32_stereo_fw,
#if FS_MAX_CHANNELS > 2
          mix_from_f32_multi_fw, mix_from_f32_multi_fw,
          mix_from_f32_multi_fw, mix_from_f32_multi_fw
#endif
     }  /* FSSF_FLOAT */
};

static const SoundMXFunc MIX_RW[FS_NUM_SAMPLEFORMATS][FS_MAX_CHANNELS] = {
     {
          mix_from_u8_mono_rw,   mix_from_u8_stereo_rw,
#if FS_MAX_CHANNELS > 2
          mix_from_u8_multi_rw,  mix_from_u8_multi_rw,
          mix_from_u8_multi_rw,  mix_from_u8_multi_rw
#endif
     }, /* FSSF_U8 */
     {
          mix_from_s16_mono_rw,  mix_from_s16_stereo_rw,
#if FS_MAX_CHANNELS > 2
          mix_from_s16_multi_rw, mix_from_s16_multi_rw,
          mix_from_s16_multi_rw, mix_from_s16_multi_rw
#endif
     }, /* FSSF_S16 */
     {
          mix_from_s24_mono_rw,  mix_from_s24_stereo_rw,
#if FS_MAX_CHANNELS > 2
          mix_from_s24_multi_rw, mix_from_s24_multi_rw,
          mix_from_s24_multi_rw, mix_from_s24_multi_rw
#endif
     }, /* FSSF_S24 */
     {
          mix_from_s32_mono_rw,  mix_from_s32_stereo_rw,
#if FS_MAX_CHANNELS > 2
          mix_from_s32_multi_rw, mix_from_s32_multi_rw,
          mix_from_s32_multi_rw, mix_from_s32_multi_rw
#endif
     }, /* FSSF_S32 */
     {
          mix_from_f32_mono_rw,  mix_from_f32_stereo_rw,
#if FS_MAX_CHANNELS > 2
          mix_from_f32_multi_rw, mix_from_f32_multi_rw,
          mix_from_f32_multi_rw, mix_from_f32_multi_rw
#endif
     }  /* FSSF_FLOAT */
};

DirectResult
fs_buffer_mixto( CoreSoundBuffer *buffer,
                 __fsf           *dest,
                 int              rate,
                 FSChannelMode    mode,
                 int              max_frames,
                 int              pos,
                 int              stop,
                 __fsf            levels[6],
                 int              pitch,
                 int             *ret_pos,
                 int             *ret_num,
                 int             *ret_len )
{
     long long  inc;
     long long  max;
     int        num;
     int        len;
     bool       last = false;

     D_ASSERT( buffer != NULL );
     D_ASSERT( buffer->data != NULL );
     D_ASSERT( pos >= 0 );
     D_ASSERT( pos < buffer->length );
     D_ASSERT( stop <= buffer->length );
     D_ASSERT( dest != NULL );
     D_ASSERT( max_frames >= 0 );

     D_DEBUG_AT( CoreSound_Buffer, "%s( %p, len %d, rate %d, mode %08x, max_frames %d, pos %d, stop %d )\n",
                 __FUNCTION__, buffer, buffer->length, rate, mode, max_frames, pos, stop );

     inc = (long long) buffer->rate * pitch / rate;
     max = (long long) max_frames * inc;
#if SIZEOF_LONG == 4
     if (inc > 0x7fffffffll)
          inc = 0x7fffffffll;
     else if (inc < -0x7fffffffll)
          inc = -0x7fffffffll;

     if (max > 0x7fffffffll)
          max = 0x7fffffffll;
     else if (max < -0x7fffffffll)
          max = -0x7fffffffll;
#endif

     if (stop >= 0) {
          long long tmp;

          if (pitch < 0) {
               /* Start position is greater than stop position. */
               if (pos <= stop)
                    stop -= buffer->length;
               tmp = (long long)(stop - pos) << FS_PITCH_BITS;
               if (max <= tmp) {
                    max  = tmp;
                    last = true;
               }
          }
          else {
               /* Stop position is greater than start position. */
               if (pos >= stop)
                    stop += buffer->length;
               tmp = (long long)(stop - pos) << FS_PITCH_BITS;
               if (max >= tmp) {
                    max  = tmp;
                    last = true;
               }
          }
     }

     /* Mix the data into the buffer. */
     if ((long) inc && (levels[0] || levels[1])) {
          SoundMXFunc func;
          int         format_index  = FS_SAMPLEFORMAT_INDEX( buffer->format );
          int         channel_index = FS_CHANNELS_FOR_MODE( buffer->mode ) - 1;

          func = (pitch < 0) ? MIX_RW[format_index][channel_index] : MIX_FW[format_index][channel_index];
          len  = func( buffer, dest, mode, pos, inc, max, levels, last );
     }
     else {
          /* Produce silence. */
          len = ((long) inc) ? max / inc : max_frames;
     }

     num  = max >> FS_PITCH_BITS;
     pos += num;
     pos %= buffer->length;
     if (pos < 0)
          pos += buffer->length;

     /* Return new position. */
     if (ret_pos)
          *ret_pos = pos;

     /* Return number of samples mixed in. */
     if (ret_num)
          *ret_num = ABS( num );

     /* Return number of samples written. */
     if (ret_len)
          *ret_len = len;

     D_DEBUG_AT( CoreSound_Buffer, "  -> new pos %d, mixed %d (%d/%d)\n", pos, ABS( num ), len, max_frames );

     return last ? DR_BUFFEREMPTY : DR_OK;
}
