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
  scheme/mode.ss

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
