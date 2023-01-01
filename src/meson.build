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

fusionsound_sources = [
  'fusionsound.c',
  'ifusionsound.c',
  'buffer/ifusionsoundbuffer.c',
  'buffer/ifusionsoundstream.c',
  'core/core_sound.c',
  'core/playback.c',
  'core/sound_buffer.c',
  'core/sound_device.c',
  'media/ifusionsoundmusicprovider.c',
  'misc/sound_conf.c',
  'misc/sound_util.c', fusionsound_strings,
  'playback/ifusionsoundplayback.c'
]

core_headers = [
  'core/coretypes_sound.h',
  'core/sound_device.h',
  'core/sound_driver.h'
]

media_headers = [
  'media/ifusionsoundmusicprovider.h'
]

misc_headers = [
  'misc/sound_conf.h'
]

build_h = configuration_data()

build_h.set_quoted('FUSIONSOUND_VERSION_VENDOR', get_option('vendor-version'))

build_h.set10('FS_LINEAR_FILTER', get_option('linear-filter'))

build_h.set('FS_IEEE_FLOATS', get_option('ieee-floats'))

configure_file(configuration: build_h,
               output: 'build.h',
               install: true,
               install_dir: join_paths(get_option('includedir'), 'fusionsound-internal'))

libfusionsound = library('fusionsound-@0@.@1@'.format(fusionsound_major_version, fusionsound_minor_version),
                         fusionsound_sources,
                         include_directories: [config_inc, fusionsound_inc],
                         c_args: ['-DSYSCONFDIR="' + join_paths(get_option('prefix'), get_option('sysconfdir')) + '"'],
                         dependencies: [direct_dep, fusion_dep],
                         version: '@0@.0.0'.format(fusionsound_micro_version),
                         install: true)

install_symlink('libfusionsound' + libsuffix,
                pointing_to: 'libfusionsound-@0@.@1@'.format(fusionsound_major_version, fusionsound_minor_version) + libsuffix,
                install_dir: join_paths(get_option('prefix'), get_option('libdir')))

install_headers('ifusionsound.h', subdir: 'fusionsound-internal')
install_headers(core_headers,     subdir: 'fusionsound-internal/core')
install_headers(media_headers,    subdir: 'fusionsound-internal/media')
install_headers(misc_headers,     subdir: 'fusionsound-internal/misc')

pkgconfig.generate(filebase: 'fusionsound',
                   name: 'FusionSound',
                   description: 'Audio subsystem using Fusion IPC',
                   requires: 'direct fusion',
                   libraries: '-L${libdir} -lfusionsound',
                   subdirs: 'fusionsound')

pkgconfig.generate(filebase: 'fusionsound-internal',
                   variables: 'moduledir=' + moduledir,
                   name: 'FusionSound-internal',
                   description: 'FusionSound internal module infrastructure',
                   requires: 'fusionsound',
                   subdirs: 'fusionsound-internal')

fusionsound_dep = declare_dependency(include_directories: fusionsound_inc, link_with: libfusionsound, dependencies: direct_dep)
