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

#include <direct/log.h>
#include <direct/result.h>
#include <fusionsound_version.h>
#include <ifusionsound.h>
#include <misc/sound_conf.h>

D_DEBUG_DOMAIN( FusionSound_Main, "FusionSound/Main", "FusionSound Main Functions" );

/**********************************************************************************************************************/

IFusionSound *ifusionsound_singleton = NULL;

const char *
FusionSoundCheckVersion( unsigned int required_major,
                         unsigned int required_minor,
                         unsigned int required_micro )
{
     if (required_major > FUSIONSOUND_MAJOR_VERSION)
          return "FusionSound version too old (major mismatch)";
     if (required_major < FUSIONSOUND_MAJOR_VERSION)
          return "FusionSound version too new (major mismatch)";
     if (required_minor > FUSIONSOUND_MINOR_VERSION)
          return "FusionSound version too old (minor mismatch)";
     if (required_minor < FUSIONSOUND_MINOR_VERSION)
          return "FusionSound version too new (minor mismatch)";
     if (required_micro > FUSIONSOUND_MICRO_VERSION)
          return "FusionSound version too old (micro mismatch)";

     return NULL;
}

DirectResult
FusionSoundInit( int   *argc,
                 char **argv[] )
{
     DirectResult ret;

     ret = fs_config_init( argc, argv );
     if (ret)
          return ret;

     return DR_OK;
}

DirectResult
FusionSoundSetOption( const char *name,
                      const char *value )
{
     DirectResult ret;

     D_DEBUG_AT( FusionSound_Main, "%s( '%s', '%s' )\n", __FUNCTION__, name, value );

     if (fs_config == NULL) {
          D_ERROR( "FusionSound/Main: FusionSoundInit() has to be called before FusionSoundSetOption()!\n" );
          return DR_INIT;
     }

     if (ifusionsound_singleton) {
          D_ERROR( "FusionSound/Main: FusionSoundCreate() has already been called!\n" );
          return DR_INIT;
     }

     if (!name)
          return DR_INVARG;

     ret = fs_config_set( name, value );
     if (ret)
          return ret;

     return DR_OK;
}

DirectResult
FusionSoundCreate( IFusionSound **ret_interface )
{
     DirectResult ret;

     D_DEBUG_AT( FusionSound_Main, "%s( %p )\n", __FUNCTION__, ret_interface );

     if (!fs_config) {
          D_ERROR( "FusionSound/Main: FusionSoundInit() has to be called before FusionSoundCreate()!\n" );
          return DR_INIT;
     }

     if (!ret_interface)
          return DR_INVARG;

     if (ifusionsound_singleton) {
          D_DEBUG_AT( FusionSound_Main, "  -> using singleton %p\n", ifusionsound_singleton );

          ifusionsound_singleton->AddRef( ifusionsound_singleton );

          *ret_interface = ifusionsound_singleton;

          return DR_OK;
     }

     if (!(direct_config->quiet & DMT_BANNER) && fs_config->banner) {
          direct_log_printf( NULL,
                             "\n"
                             "   ~~~~~~~~~~~~~~~~~~~~~~~~~~| FusionSound %d.%d.%d %s |~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
                             "        (c) 2017-2023  DirectFB2 Open Source Project (fork of DirectFB)\n"
                             "        (c) 2012-2016  DirectFB integrated media GmbH\n"
                             "        (c) 2001-2016  The world wide DirectFB Open Source Community\n"
                             "        (c) 2000-2004  Convergence (integrated media) GmbH\n"
                             "      ----------------------------------------------------------------\n"
                             "\n",
                             FUSIONSOUND_MAJOR_VERSION, FUSIONSOUND_MINOR_VERSION, FUSIONSOUND_MICRO_VERSION,
                             FUSIONSOUND_VERSION_VENDOR );
     }

     DIRECT_ALLOCATE_INTERFACE( ifusionsound_singleton, IFusionSound );

     D_DEBUG_AT( FusionSound_Main, "  -> setting singleton to %p\n", ifusionsound_singleton );

     ret = IFusionSound_Construct( ifusionsound_singleton );
     if (ret) {
          D_DEBUG_AT( FusionSound_Main, "  -> resetting singleton to NULL!\n" );
          ifusionsound_singleton = NULL;
          return ret;
     }

     *ret_interface = ifusionsound_singleton;

     return DR_OK;
}

DirectResult
FusionSoundError( const char   *msg,
                  DirectResult  result )
{
     if (msg)
          direct_log_printf( NULL, "(!) FusionSoundError [%s]: %s\n", msg, DirectResultString( result ) );
     else
          direct_log_printf( NULL, "(!) FusionSoundError: %s\n", DirectResultString( result ) );

     return result;
}

const char *
FusionSoundErrorString( DirectResult result )
{
     return DirectResultString( result );
}

DirectResult
FusionSoundErrorFatal( const char   *msg,
                       DirectResult  result )
{
     FusionSoundError( msg, result );

     exit( result );
}
