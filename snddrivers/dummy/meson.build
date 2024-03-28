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

library('fusionsound_dummy',
        'dummy.c',
        dependencies: fusionsound_dep,
        install: true,
        install_dir: join_paths(moduledir, 'snddrivers'))

pkgconfig.generate(filebase: 'fusionsound-snddriver-dummy',
                   variables: 'moduledir=' + moduledir,
                   name: 'FusionSound-snddriver-dummy',
                   description: 'Dummy sound driver',
                   libraries_private: ['-L${moduledir}/snddrivers',
                                       '-Wl,--whole-archive -lfusionsound_dummy -Wl,--no-whole-archive'])
