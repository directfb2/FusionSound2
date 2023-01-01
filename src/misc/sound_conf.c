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

#include <direct/filesystem.h>
#include <direct/memcpy.h>
#include <direct/system.h>
#include <direct/util.h>
#include <fusionsound_util.h>
#include <fusionsound_version.h>
#include <fusion/conf.h>
#include <misc/sound_conf.h>

D_DEBUG_DOMAIN( FusionSound_Config, "FusionSound/Config", "FusionSound Runtime Configuration options" );

/**********************************************************************************************************************/

FSConfig          *fs_config = NULL;
static const char *fs_config_usage =
     "\n"
     " --fs-help                       Output FusionSound usage information and exit\n"
     " --fs:<option>[,<option>]...     Pass options to FusionSound (see below)\n"
     "\n"
     "FusionSound options:\n"
     "\n"
     "  help                           Output FusionSound usage information and exit\n"
     "  driver=<driver>                Specify the driver to use ('oss', 'alsa', etc.)\n"
     "  [no-]banner                    Show FusionSound banner at startup (default enabled)\n"
     "  [no-]wait                      Wait for slaves before quitting (default enabled)\n"
     "  [no-]deinit-check              Check if all allocated resources have been released on exit (default enabled)\n"
     "  session=<num>                  Select the multi application world which is joined or created\n"
     "                                 -1 forces the creation of a new world using the lowest unused session number\n"
     "  channels=<channels>            Set the default number of channels (default = 2)\n"
     "  channelmode=<channelmode>      Set the default channel mode (default = STEREO)\n"
     "  sampleformat=<sampleformat>    Set the default sample format (default = S16)\n"
     "  samplerate=<samplerate>        Set the default sample rate (default = 48000)\n"
     "  buffertime=<millisec>          Set the default buffer time (default = 25)\n"
     "  [no-]dither                    Enable dithering\n"
     "\n";

/**********************************************************************************************************************/

static void
print_config_usage()
{
    fprintf( stderr, "FusionSound version %d.%d.%d\n",
             FUSIONSOUND_MAJOR_VERSION, FUSIONSOUND_MINOR_VERSION, FUSIONSOUND_MICRO_VERSION );

    fprintf( stderr, "%s", fs_config_usage );

    fprintf( stderr, "%s%s", fusion_config_usage, direct_config_usage );
}

static DirectResult
parse_args( const char *args )
{
     int   len = strlen( args );
     char *name, *buf;

     buf = D_MALLOC( len + 1 );
     if (!buf)
          return D_OOM();

     name = buf;

     direct_memcpy( name, args, len + 1 );

     while (name && name[0]) {
          DirectResult  ret;
          char         *value;
          char         *next;

          if ((next = strchr( name, ',' )) != NULL)
               *next++ = '\0';

          if (strcmp( name, "help" ) == 0) {
               print_config_usage();
               exit( 1 );
          }

          if ((value = strchr( name, '=' )) != NULL)
               *value++ = '\0';

          ret = fs_config_set( buf, value );
          switch (ret) {
               case DR_OK:
                    break;
               default:
                    D_ERROR( "FusionSound/Config: Invalid option '%s' in args!\n", name );
                    D_FREE( buf );
                    return ret;
          }

          name = next;
     }

     D_FREE( buf );

     return DR_OK;
}

static void
config_allocate()
{
     if (fs_config)
          return;

     fs_config = D_CALLOC( 1, sizeof(FSConfig) );

     fs_config->banner       = true;

     fs_config->wait         = true;

     fs_config->deinit_check = true;

     fs_config->session      = 1;

     fs_config->channelmode  = FSCM_STEREO;
     fs_config->sampleformat = FSSF_S16;
     fs_config->samplerate   = 48000;
     fs_config->buffertime   = 25;
}

static DirectResult
config_read( const char *filename )
{
     DirectResult ret;
     DirectFile   f;
     char         line[400];

     ret = direct_file_open( &f, filename, O_RDONLY, 0 );
     if (ret) {
          D_DEBUG_AT( FusionSound_Config, "Unable to open config file '%s'!\n", filename );
          return DR_IO;
     }
     else {
          D_DEBUG_AT( FusionSound_Config, "Parsing config file '%s'\n", filename );
     }

     while (!direct_file_get_string( &f, line, 400 )) {
          char *name    = line;
          char *comment = strchr( line, '#' );
          char *value;

          if (comment) {
               *comment = 0;
          }

          value = strchr( line, '=' );

          if (value) {
               *value++ = 0;
               direct_trim( &value );
          }

          direct_trim( &name );

          if (!*name || *name == '#')
               continue;

          ret = fs_config_set( name, value );
          if (ret) {
               D_ERROR( "FusionSound/Config: Invalid option '%s' in config file '%s'!\n", name, filename );
               break;
          }
     }

     direct_file_close( &f );

     return ret;
}

/**********************************************************************************************************************/

DirectResult
fs_config_set( const char *name,
               const char *value )
{
     bool fsoption = true;

     if (strcmp( name, "driver" ) == 0) {
          if (value) {
               if (fs_config->driver)
                    D_FREE( fs_config->driver );

               fs_config->driver = D_STRDUP( value );
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No driver specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "banner" ) == 0) {
          fs_config->banner = true;
     } else
     if (strcmp( name, "no-banner" ) == 0) {
          fs_config->banner = false;
     } else
     if (strcmp( name, "wait" ) == 0) {
          fs_config->wait = true;
     } else
     if (strcmp( name, "no-wait" ) == 0) {
          fs_config->wait = false;
     } else
     if (strcmp( name, "deinit-check" ) == 0) {
          fs_config->deinit_check = true;
     } else
     if (strcmp( name, "no-deinit-check" ) == 0) {
          fs_config->deinit_check = false;
     } else
     if (strcmp( name, "session" ) == 0) {
          if (value) {
               int session;

               if (sscanf( value, "%d", &session ) < 1) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               fs_config->session = session;
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "channels" ) == 0) {
          if (value) {
               int channels;

               if (sscanf( value, "%d", &channels ) < 1) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse channels!\n", name );
                    return DR_INVARG;
               }

               if (channels < 1 || channels > FS_MAX_CHANNELS) {
                    D_ERROR( "FusionSound/Config: '%s': Unsupported channels '%d'!\n", name, channels );
                    return DR_INVARG;
               }

               fs_config->channelmode = fs_mode_for_channels( channels );
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No channels specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "channelmode" ) == 0) {
          if (value) {
               FSChannelMode mode;

               mode = fs_channelmode_parse( value );
               if (mode == FSCM_UNKNOWN) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse mode!\n", name );
                    return DR_INVARG;
               }

               fs_config->channelmode = mode;
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No mode specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "sampleformat" ) == 0) {
          if (value) {
               FSSampleFormat format;

               format = fs_sampleformat_parse( value );
               if (format == FSSF_UNKNOWN) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse format!\n", name );
                    return DR_INVARG;
               }

               fs_config->sampleformat = format;
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No format specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "samplerate" ) == 0) {
          if (value) {
               int rate;

               if (sscanf( value, "%d", &rate ) < 1) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               if (rate < 1) {
                    D_ERROR( "FusionSound/Config: '%s': Unsupported value '%d'!\n", name, rate );
                    return DR_INVARG;
               }

               fs_config->samplerate = rate;
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "buffertime" ) == 0) {
          if (value) {
               int time;

               if (sscanf( value, "%d", &time ) < 1) {
                    D_ERROR( "FusionSound/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               if (time < 1 || time > 5000) {
                    D_ERROR( "FusionSound/Config: '%s': Unsupported value '%d'!\n", name, time );
                    return DR_INVARG;
               }

               fs_config->buffertime = time;
          }
          else {
               D_ERROR( "FusionSound/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "dither" ) == 0) {
          fs_config->dither = true;
     } else
     if (strcmp( name, "no-dither" ) == 0) {
          fs_config->dither = false;
     }
     else {
          fsoption = false;
          if (fusion_config_set( name, value ) && direct_config_set( name, value ))
               return DR_INVARG;
     }

     if (fsoption)
          D_DEBUG_AT( FusionSound_Config, "Set %s '%s'\n", name, value ?: "" );

     return DR_OK;
}

DirectResult
fs_config_init( int   *argc,
                char **argv[] )
{
     DirectResult  ret;
     char         *home = direct_getenv( "HOME" );
     char         *prog = NULL;
     char         *fsargs;

     if (fs_config)
          return DR_OK;

     config_allocate();

     /* Read system settings. */
     ret = config_read( SYSCONFDIR"/fusionsoundrc" );
     if (ret && ret != DR_IO)
          return ret;

     /* Read user settings. */
     if (home) {
          int  len = strlen( home ) + strlen( "/.fusionsoundrc" ) + 1;
          char buf[len];

          snprintf( buf, len, "%s/.fusionsoundrc", home );

          ret = config_read( buf );
          if (ret && ret != DR_IO)
               return ret;
     }

     /* Get application name. */
     if (argc && *argc && argv && *argv) {
          prog = strrchr( (*argv)[0], '/' );

          if (prog)
               prog++;
          else
               prog = (*argv)[0];
     }

     /* Read global application settings. */
     if (prog && prog[0]) {
          int  len = strlen( SYSCONFDIR"/fusionsoundrc." ) + strlen( prog ) + 1;
          char buf[len];

          snprintf( buf, len, SYSCONFDIR"/fusionsoundrc.%s", prog );

          ret = config_read( buf );
          if (ret && ret != DR_IO)
               return ret;
     }

     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen( home ) + strlen( "/.fusionsoundrc." ) + strlen( prog ) + 1;
          char buf[len];

          snprintf( buf, len, "%s/.fusionsoundrc.%s", home, prog );

          ret = config_read( buf );
          if (ret  &&  ret != DR_IO)
               return ret;
     }

     /* Read settings from environment variable. */
     fsargs = direct_getenv( "FSARGS" );
     if (fsargs) {
          ret = parse_args( fsargs );
          if (ret)
               return ret;
     }

     /* Read settings from command line. */
     if (argc && argv) {
          int i;

          for (i = 1; i < *argc; i++) {
               if (strcmp( (*argv)[i], "--fs-help" ) == 0) {
                    print_config_usage();
                    exit( 1 );
               }

               if (strncmp( (*argv)[i], "--fs:", 5 ) == 0) {
                    ret = parse_args( (*argv)[i] + 5 );
                    if (ret)
                         return ret;

                    (*argv)[i] = NULL;
               }
          }

          for (i = 1; i < *argc; i++) {
               int k;

               for (k = i; k < *argc; k++)
                    if ((*argv)[k] != NULL)
                         break;

               if (k > i) {
                    int j;

                    k -= i;

                    for (j = i + k; j < *argc; j++)
                         (*argv)[j-k] = (*argv)[j];

                    *argc -= k;
               }
          }
     }

     return DR_OK;
}
