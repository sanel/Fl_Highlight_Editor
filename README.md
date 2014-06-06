# Fl_Highlight_Editor

Extensible and customizable editing widget for
[FLTK](http://www.fltk.org) UI library.

<p align="center">
<img src="https://raw.github.com/sanel/Fl_Highlight_Editor/master/images/shot.png"
     alt="Fl_Highlight_Editor" title="Fl_Highlight_Editor screenshot">
</p>

## Usage

Fl_Highlight_Editor is a drop in replacement for
[Fl_Text_Editor](http://www.fltk.org/doc-1.3/classFl__Text__Editor.html) and
[Fl_Text_Display](http://www.fltk.org/doc-1.3/classFl__Text__Display.html)
widgets with ability to highlight the text and modify it with embedded
[Scheme](http://en.wikipedia.org/wiki/Scheme_%20programming_language%20) scripting language.

The widget is fully self-contained and it doesn't depend on anything
except FLTK library, making it suitable for embedding inside existing
project.

Fl_Highlight_Editor design is inspired with
[Emacs](http://www.gnu.org/software/emacs) editor and you will find
many used terms and functions names similar with it. However, this
widget is not Emacs nor complete editor, but component to build
editors and advanced text handling widgets.

Usage details and how to extend it are described in [Manual](MANUAL.md).

## Installation

Make sure you have installed FLTK stable version (1.3.x), C++ compiler
and GNU make. Running:

```
make
```

will compile the code. To see it in action, run examples in *test*
folder, e.g.:

```
./test/example README.md
```

will display content of `README.md` and highlight it with
`markdown-mode`.

If you would like to embed core Scheme files inside binary, keeping
highlight code separate, you can run:

```
make BUNDLE_SCRIPTS=1
```

## License

LGPL with exception ([the same license](http://www.fltk.org/COPYING.php) as FLTK).
