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

#include <fusionsound_strings.h>
#include <fusionsound_util.h>

/**********************************************************************************************************************/

static const FusionSoundSampleFormatNames(fs_sampleformat_names)
static const FusionSoundChannelModeNames(fs_channelmode_names)

/**********************************************************************************************************************/

const char *
fs_sampleformat_name( FSSampleFormat format )
{
     int i = 0;

     do {
          if (format == fs_sampleformat_names[i].format)
               return fs_sampleformat_names[i].name;
     } while (fs_sampleformat_names[i++].format != FSSF_UNKNOWN);

     return "<invalid>";
}

const char *
fs_channelmode_name( FSChannelMode mode )
{
     int i = 0;

     do {
          if (mode == fs_channelmode_names[i].mode)
               return fs_channelmode_names[i].name;
     } while (fs_channelmode_names[i++].mode != FSCM_UNKNOWN);

     return "<invalid>";
}

/**********************************************************************************************************************/

FSChannelMode
fs_mode_for_channels( int channels )
{
     switch (channels) {
          case 1:
               return FSCM_MONO;
          case 2:
               return FSCM_STEREO;
          case 3:
               return FSCM_STEREO30;
          case 4:
               return FSCM_SURROUND40_2F2R;
          case 5:
               return FSCM_SURROUND50;
          case 6:
               return FSCM_SURROUND51;
     }

     return FSCM_UNKNOWN;
}

FSSampleFormat
fs_sampleformat_parse( const char *format )
{
     int i;

     for (i = 0; fs_sampleformat_names[i].format != FSSF_UNKNOWN; i++) {
          if (!strcasecmp( format, fs_sampleformat_names[i].name ))
               return fs_sampleformat_names[i].format;
     }

     return FSSF_UNKNOWN;
}

FSChannelMode
fs_channelmode_parse( const char *mode )
{
     int i;

     for (i = 0; fs_channelmode_names[i].mode != FSCM_UNKNOWN; i++) {
          if (!strcasecmp( mode, fs_channelmode_names[i].name ))
               return fs_channelmode_names[i].mode;
     }

     return FSCM_UNKNOWN;
}
