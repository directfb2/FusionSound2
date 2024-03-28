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

#ifndef __MISC__SOUND_CONF_H__
#define __MISC__SOUND_CONF_H__

#include <core/coretypes_sound.h>

/**********************************************************************************************************************/

typedef struct {
     char           *snddriver;
     bool            banner;
     bool            wait;
     bool            deinit_check;
     int             session;
     FSChannelMode   channelmode;
     FSSampleFormat  sampleformat;
     int             samplerate;
     int             buffertime;
     bool            dither;
} FSConfig;

/**********************************************************************************************************************/

extern FSConfig *fs_config;

/*
 * Set indiviual option.
 */
DirectResult fs_config_set ( const char  *name,
                             const char  *value );

/*
 * Allocate config struct, fill with defaults and parse command line options for overrides.
 */
DirectResult fs_config_init( int         *argc,
                             char       **argv[] );

#endif
