#!/bin/sh
#
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

mknames() {
ENUM=$1
PREFIX=$2
NULL=$3
NAME=$4
VALUE=$5
HEADER=$6

cat << EOF

struct FS${NAME}Name {
     ${ENUM} ${VALUE};
     const char *name;
};

#define FusionSound${NAME}Names(Identifier) struct FS${NAME}Name Identifier[] = { \\
EOF

egrep "^ +${PREFIX}_[0-9A-Za-z_]+[ ,]" $HEADER | grep -v ${PREFIX}_${NULL} | perl -p -e "s/^\\s*(${PREFIX}_)([\\w_]+)[ ,].*/     \\{ \\1\\2, \\\"\\2\\\" \\}, \\\\/"

cat << EOF
     { ($ENUM) ${PREFIX}_${NULL}, "${NULL}" } \\
};
EOF
}

echo \#ifndef __FUSIONSOUND_STRINGS_H__
echo \#define __FUSIONSOUND_STRINGS_H__
echo
echo \#include \<fusionsound.h\>
mknames FSSampleFormat FSSF UNKNOWN SampleFormat format $1
mknames FSChannelMode FSCM UNKNOWN ChannelMode mode $1
echo
echo \#endif
