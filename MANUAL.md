# Fl_Highlight_Editor manual

## Introduction

This manual will try to give you some introduction on how to use
Fl_Highlight_Editor widget and how to extend the builtin language with
own constructs, modes and highlight styles. No previous Emacs
knowlege is assumed (e.g. you don't have to know what are faces or
styles) but basic Scheme, C++ and FLTK knowledge is required.

If you are not familiar with the Scheme language, please read
[Yet Another Scheme Tutorial](http://www.shido.info/lisp/idx_scm_e.html)
or any other online tutorial you find suitable; after all, there are
many Scheme tutorials online.

## Initializing widget and interpreter

Here is example on how to initialize Fl_Highlight_Editor, load
interpreter and read some file:

```cpp
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Text_Buffer.H>

/* our library */
#include "FL/Fl_Highlight_Editor.H"

int main(int argc, char **argv) {
  /* create initial window */
  Fl_Double_Window *win = new Fl_Double_Window(400, 400, "Test #1");
  win->begin();
    /* initialize editor object */
    Fl_Highlight_Editor *editor = new Fl_Highlight_Editor(10, 10, 380, 350);

    /*
     * Initialize interpreter by pointing it to folder where are
     * Scheme source files for Fl_Highlight_Editor. These files
     * implements modes, syntax higlighting logic and many more.
     *
     * Unless you initialize interpreter or point it to folder where
     * are not Scheme source files, Fl_Highlight_Editor will behave
     * just like Fl_Text_Editor: it will display text allowing you
     * basic operation on that text.
	 *
	 * You should probably initialize interpreter as soon as possible so
	 * interpreter can run hooks when file is loaded.
     */
    editor->init_interpreter("./scheme");

    /* load some example */
	editor->loadfile("test/example.cxx");
  win->end();

  /* show the window and enter event loop */
  win->show(argc, argv);
  return Fl::run();
}
```

## Some obligatory terms

Before we continue explaining widget details and internals, let we
describe some terms used in this manual. If you are using Emacs, you
will find these terms quite familiar.

### mode

Mode is state of editor loaded on some action; in our case mode
represents state of widget editing capabilities. For example, mode can
alter default editor behavior (e.g. changing keys, replacing tab
character with spaces), load syntax highlighting and etc.

Comparing to Emacs modes where each mode can change almost any editor
option, Fl_Highlight_Editor modes are more limited.

### face

Face is detail how to draw text in widget or parts of text. Each face
has name, font name, type, size and color. For now, face does not have
option to set background color.

### style

TODO

### hook

Hook is a function (Scheme function) called when something
happens. For example, when you a load file (via ''loadfile()'' member),
a hook will be called; the same applies when you save file with ''savefile()''.

Hooks represents efficient way to extend or alter widget behavior,
without writing C++ code.
