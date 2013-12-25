
# Fl_Highlight_Editor - extensible text editing widget
# Copyright (c) 2013 Sanel Zukan.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library. If not, see <http://www.gnu.org/licenses/>.

CC       = $(shell fltk-config --cc)
CXX      = $(shell fltk-config --cxx)
DEBUG    = -g
CXXFLAGS = $(shell fltk-config --cxxflags) -I. -Wall
LDLIBS   = $(shell fltk-config --ldflags) -lstdc++
AR       = ar

TARGET_LIB = lib/libfltk_highlight.a
TESTS      = test/example test/repl

all: $(TARGET_LIB) $(TESTS)

.SUFFIXES: .o .c .cxx

%.o: %.cxx
	$(CXX) $(CXXFLAGS) $(DEBUG) -c -o $@ $<

%.o: %.c
	$(CC) $(CXXFLAGS) $(DEBUG) -c -o $@ $<

OBJECTS = $(BUNDLED_SCRIPTS) src/Fl_Highlight_Editor.o src/ts/scheme.o

ifeq ($(BUNDLE_SCRIPTS), 1)
# NOTE: order is important, as everything will be zipped inside single file
SCHEME_FILES =        \
  scheme/boot.ss      \
  scheme/constants.ss \
  scheme/utils.ss     \
  scheme/editor.ss

CXXFLAGS += -DUSE_BUNDLED_SCRIPTS
BUNDLED_SCRIPTS = src/bundled_scripts.o

src/bundled_scripts.cxx: $(SCHEME_FILES)
	./tools/gen-c-string.sh $(SCHEME_FILES) > $@
endif

$(TARGET_LIB): $(OBJECTS)
	$(AR) -rv $@ $^

test/example: test/example.o $(TARGET_LIB)
test/repl: test/repl.o $(TARGET_LIB)

clean:
	rm -f $(TARGET_LIB)
	rm -f src/*.o src/ts/*.o test/*.o
	rm -f src/bundled_scripts.cxx
	rm -f $(TESTS)
