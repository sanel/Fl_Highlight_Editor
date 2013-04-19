# Fl_Highlight_Editor

Extensible and customizable editing widget for [FLTK](http://www.fltk.org).

<p align="center">
<img src="https://github.com/sanel/Fl_Highlight_Editor/images/shot.png"
     alt="Fl_Highlight_Editor" title="Fl_Highlight_Editor screenshot">
</p>

## Usage

*Fl_Highlight_Editor* is a drop in replacement for
[Fl_Text_Editor](http://www.fltk.org/doc-1.3/classFl__Text__Editor.html) and
[Fl_Text_Display](http://www.fltk.org/doc-1.3/classFl__Text__Display.html)
widgets with ability to highlight the text and modify it with embeded
[Scheme](http://en.wikipedia.org/wiki/Scheme_%20programming_language%20) scripting language.

The widget is fully self-contained and it doesn't depends on anything
else except FLTK library, making it suitable for putting inside
existing project.

*Fl_Highlight_Editor* design is inspired with
[Emacs](http://www.gnu.org/software/emacs) so you will find similar
terms and functions names. However, this widget is not Emacs nor
complete editor, but component to build editors and advanced text
handlind widgets.

If you are not familiar with Scheme or
[Elisp](http://en.wikipedia.org/wiki/Elisp), here are some links for
the start:

 * [Yet Another Scheme Tutorial](http://www.shido.info/lisp/idx_scm_e.html)
 * [Emacs Manuals Page](http://www.gnu.org/software/emacs/#Manuals)

If you would like to play with Scheme, with Fl_Highlight_Editor comes
small Scheme
[REPL](http://en.wikipedia.org/wiki/Read\u2013eval\u2013print_loop);
you can start it with:

```
./test/repl
```

and you will get something like this:

```
Interpreter booted in 20.0ms.

>
```

Lines starting with *>* tells you can insert Scheme expressions and
they will be evaluated. If you used Python or Ruby before, this is like you
started *python* or *irb* commands and you begin to instert language
specific construct.

Here are few examples:

```scheme
> (+ 1 2 3)
6
> (println "Holaaa")
Holaa
#t
> (quit)
$
``` 

You can do even more with it after you get familiar with the
language. Make sure to check [Manual](MANUAL.md).

## Installation

Make sure you have installed latest FLTK stable version (1.3.x), C++
compiler and GNU make. Running:

```
make
```

will compile the code. To see it in action, run examples in *test* folder.

## License

LGPL with exception ([the same license](http://www.fltk.org/COPYING.php) as FLTK).
