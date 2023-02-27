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

cc = meson.get_compiler('c')

config_conf = configuration_data()

config_conf.set('SIZEOF_LONG', cc.sizeof('long'), description: 'The size of long, as computed by sizeof.')

config_conf.set('WORDS_BIGENDIAN', host_machine.endian() == 'big', description: 'Byte ordering is bigendian.')

configure_file(configuration: config_conf, output: 'config.h')

config_inc = include_directories('.')
fusionsound_inc = include_directories('include', 'src')

pkgconfig = import('pkgconfig')

# core library

subdir('include')
subdir('src')

# interface modules

subdir('interfaces/IFusionSoundMusicProvider')

# snddriver modules

subdir('snddrivers/dummy')
if get_option('alsa')
  subdir('snddrivers/alsa')
endif
if get_option('oss')
  subdir('snddrivers/oss')
endif

# generate .pc files

fs_update_pkgconfig_conf = configuration_data()

fs_update_pkgconfig_conf.set('PKGCONFIGDIR', join_paths(get_option('prefix'), get_option('libdir'), 'pkgconfig'))

fs_update_pkgconfig_conf.set('VERSION', fusionsound_version)

fs_update_pkgconfig = configure_file(configuration: fs_update_pkgconfig_conf,
                                     input: 'fs-update-pkgconfig.in',
                                     output: 'fs-update-pkgconfig',
                                     install: true,
                                     install_dir: get_option('bindir'))

meson.add_install_script(fs_update_pkgconfig)
