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

$(TARGET_LIB): src/Fl_Highlight_Editor.o src/ts/scheme.o
	$(AR) -rv $@ $^

test/example: test/example.o $(TARGET_LIB)
test/repl: test/repl.o $(TARGET_LIB)

clean:
	@rm -f $(TARGET_LIB)
	@rm -f src/*.o src/ts/*.o test/*.o
	@rm -f $(TESTS)
