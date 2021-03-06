# Webcamoid, webcam capture application.
# Copyright (C) 2011-2017  Gonzalo Exequiel Pedone
#
# Webcamoid is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Webcamoid is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Webcamoid. If not, see <http://www.gnu.org/licenses/>.
#
# Web-Site: http://webcamoid.github.io/

exists(akcommons.pri) {
    include(akcommons.pri)
} else {
    exists(../../../../../akcommons.pri) {
        include(../../../../../akcommons.pri)
    } else {
        error("akcommons.pri file not found.")
    }
}

include(../dshow.pri)

CONFIG += staticlib c++11
CONFIG -= qt

DESTDIR = $${OUT_PWD}

TARGET = PlatformUtils

TEMPLATE = lib

LIBS = \
    -L$${OUT_PWD}/../../VCamUtils -lVCamUtils \
    -ladvapi32

SOURCES = \
    src/messageserver.cpp \
    src/utils.cpp

HEADERS =  \
    src/messageserver.h \
    src/utils.h \
    src/messagecommons.h

INCLUDEPATH += \
    ../..
