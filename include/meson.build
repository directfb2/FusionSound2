#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

fusionsound_headers = [
  'fusionsound.h',
  'fusionsound_util.h'
]

fusionsound_version_conf = configuration_data()

fusionsound_version_conf.set('FUSIONSOUND_MAJOR_VERSION', fusionsound_major_version)
fusionsound_version_conf.set('FUSIONSOUND_MINOR_VERSION', fusionsound_minor_version)
fusionsound_version_conf.set('FUSIONSOUND_MICRO_VERSION', fusionsound_micro_version)

configure_file(configuration: fusionsound_version_conf,
               output: 'fusionsound_version.h',
               install: true,
               install_dir: get_option('includedir') / 'fusionsound')

fusionsound_build_conf = configuration_data()

fusionsound_build_conf.set('FS_MAX_CHANNELS', get_option('multichannel') ? 6 : 2)

configure_file(configuration: fusionsound_build_conf,
               output: 'fusionsound_build.h',
               install: true,
               install_dir: get_option('includedir') / 'fusionsound')

fusionsound_strings = custom_target('fusionsound_strings',
                                    command: ['gen_fusionsound_strings.sh', '@INPUT@'],
                                    input: 'fusionsound.h',
                                    output: 'fusionsound_strings.h',
                                    capture: true,
                                    install: true,
                                    install_dir: get_option('includedir') / 'fusionsound')

install_headers(fusionsound_headers, subdir: 'fusionsound')
