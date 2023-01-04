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

project('FusionSound2', 'c',
        version: '2.0.0',
        meson_version: '>= 0.61',
        default_options: 'buildtype=release')

fusionsound_version = meson.project_version()
fusionsound_major_version = fusionsound_version.split('.')[0].to_int()
fusionsound_minor_version = fusionsound_version.split('.')[1].to_int()
fusionsound_micro_version = fusionsound_version.split('.')[2].to_int()

if get_option('default_library') == 'both'
  error('''Choose library type between 'shared' or 'static'.''')
elif get_option('default_library') == 'shared'
  libsuffix = '.so'
else
  libsuffix = '.a'
endif

moduledir = dependency('directfb-internal').get_variable(pkgconfig: 'moduledir')

direct_dep = dependency('direct')
fusion_dep = dependency('fusion')

config_h = configuration_data()

cc = meson.get_compiler('c')

config_h.set('SIZEOF_LONG', cc.sizeof('long'), description: 'The size of long, as computed by sizeof.')

config_h.set('WORDS_BIGENDIAN', host_machine.endian() == 'big', description: 'Byte ordering is bigendian.')

configure_file(configuration: config_h, output: 'config.h')

config_inc = include_directories('.')
fusionsound_inc = include_directories('include', 'src')

pkgconfig = import('pkgconfig')

# core library

subdir('include')
subdir('src')

# snddriver modules

libfusionsound_snddrivers = []
libfusionsound_snddrivers_private = []
subdir('snddrivers/dummy')
if get_option('default_library') == 'static'
  libfusionsound_snddrivers += ['-L${moduledir}/snddrivers']
  libfusionsound_snddrivers += ['-Wl,--whole-archive -lfusionsound_dummy']
endif
if get_option('alsa')
  subdir('snddrivers/alsa')
  if get_option('default_library') == 'static'
    libfusionsound_snddrivers += ['-lfusionsound_alsa']
    libfusionsound_snddrivers_private += run_command('pkg-config', '--static', '--libs', 'alsa',
                                                     check: true).stdout().strip()
  endif
endif
if get_option('oss')
  subdir('snddrivers/oss')
  if get_option('default_library') == 'static'
    libfusionsound_snddrivers += ['-lfusionsound_oss']
  endif
endif
if get_option('default_library') == 'static'
  libfusionsound_snddrivers += ['-Wl,--no-whole-archive']
endif

pkgconfig.generate(filebase: 'fusionsound-snddrivers',
                   variables: 'moduledir=' + moduledir,
                   name: 'FusionSound-snddrivers',
                   description: 'FusionSound snddriver modules',
                   libraries: libfusionsound_snddrivers,
                   libraries_private: libfusionsound_snddrivers_private)

# interface modules

libfusionsound_interfaces = []
libfusionsound_interfaces_private = []
subdir('interfaces/IFusionSoundMusicProvider')
if get_option('default_library') == 'static'
  libfusionsound_interfaces += ['-L${moduledir}/interfaces/IFusionSoundMusicProvider']
  libfusionsound_interfaces += ['-Wl,--whole-archive -lifusionsoundmusicprovider_wave -Wl,--no-whole-archive']
endif

pkgconfig.generate(filebase: 'fusionsound-interfaces',
                   variables: 'moduledir=' + moduledir,
                   name: 'FusionSound-interfaces',
                   description: 'FusionSound interface modules',
                   libraries: libfusionsound_interfaces,
                   libraries_private: libfusionsound_interfaces_private)
