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

## Initializing the widget and interpreter

Here is heavily commented example on how to initialize
Fl_Highlight_Editor and load interpreter:

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

    /*
     * Create Fl_Text_Buffer object that will hold the text. All
     * operations on text are handled by Fl_Text_Buffer.
     */
    Fl_Text_Buffer *buf = new Fl_Text_Buffer();

    /* load some example */
    buf->loadfile("test/example.cxx");

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
     */
     editor->init_interpreter("./scheme");

     /* assign Fl_Text_Buffer content to editor */
     editor->buffer(buf);
   win->end();
   win->show(argc, argv);
   return Fl::run();
}
```
