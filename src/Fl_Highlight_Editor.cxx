/*
 * Fl_Highlight_Editor - extensible text editing widget
 * Copyright (c) 2013-2014 Sanel Zukan.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <FL/Fl_Highlight_Editor.H>
#include <FL/Fl.H>

#include "ts/scheme.h"
#include "ts/scheme-private.h"

#undef cons
#undef immutable_cons

#if USE_ASSERT
# include <assert.h>
# define ASSERT(s) assert(s)
#else
# define ASSERT(s)
#endif

#if USE_POSIX_REGEX
# include <sys/types.h>
# include <regex.h>
#endif

#define DEFAULT_FACE "default-face"
#define STR_CMP(s1, s2) (strcmp((s1), (s2)) == 0)

extern int FL_NORMAL_SIZE; /* default FLTK font size */

typedef Fl_Text_Display::Style_Table_Entry StyleTable;

enum {
	CONTEXT_TYPE_INITIAL,
	CONTEXT_TYPE_TO_EOL,
	CONTEXT_TYPE_BLOCK,
	CONTEXT_TYPE_REGEX,
	CONTEXT_TYPE_EXACT,
	CONTEXT_TYPE_LAST  /* used to determine end of type list */
};

/*
 * This is table where are are stored rules for syntax highlighting. Syntax highlighting is done
 * by applying a couple of strategies:
 *
 *  1) block matching - done by searching strings marked as block start and block end. Block matching is
 *     used for (example) block comments and will behave like Vim - if block start was found but not block end,
 *     the whole buffer from that position will be painted in given face.
 *
 *  2) regex matching - regex is first compiled and matched against the whole buffer (or given region). Founded chunks will be
 *     painted on the same position in StyleTable buffer. No regex grouped submatches is considered as is not useful here.
 *
 *  3) exact matching - again done against the whole buffer (or given region), where exact string is searched for. Useful
 *     mainly for content where regex engine is expensive or for the cases where special characters are involved so escaping
 *     is not needed.
 *
 *  4) to eol (end of line) matching - exact string is searched for and the whole line, up to the end, is painted. This
 *     is intended for line comments.
 *
 * When painting is started, we scan ContextTable and for every matched type, we paint with given character found
 * match position in StyleTable buffer. To understaind how this works, see Fl_Text_Display documentation and how syntax
 * highlighting was done.
 */
struct ContextTable {
	char chr;
	/* index position inside StyleTable; filled in load_face_table() */
	int pos;
	/* FIXME: 'char' also can be used */
	int type;
	/* FIXME: only pointer to scheme symbol; will it be GC-ed at some point? */
	const char *face;

	union {
#if USE_POSIX_REGEX
		regex_t    *rx;
#endif
		const char *block[2];
		const char *exact;
	} object;

	ContextTable *next, *last;
};

struct Fl_Highlight_Editor_P {
	scheme *scm;
	char   *script_path;
	bool   loaded_context_and_faces;
	bool   update_cb_added;

	Fl_Highlight_Editor *self; /* for doing redisplay and buffer() access from hi_update() callback */

	Fl_Text_Buffer *stylebuf;
	StyleTable     *styletable;
	ContextTable   *ctable;

	int styletable_size; /* size of styletable */
	int styletable_last; /* last item in styletable */

	Fl_Highlight_Editor_P();

	int  push_style(int color, int font, int size);
	void push_style_default(int color, int font, int size);
	void clear_styles();

	void push_context(scheme *s, int type, pointer content, const char *face);
	void clear_contexts();
};

/*
 * regcomp() with ability to check if pattern starts/ends with '|'. Without checking, this could cause
 * infinite loop.
 */
#if USE_POSIX_REGEX_CHECK
static int regcomp_safe(regex_t *preg, const char *regex, int cflags) {
	int l = strlen(regex);

	/* also taken into account when ending OR token was escaped */
	if(regex[0] == '|' || ((l > 2) && regex[l - 1] == '|' && regex[l - 2] != '\\'))
		printf("Warning: regex '%s' is starting/ending with OR operator, which can cause infine loop!\n", regex);
	return regcomp(preg, regex, cflags);
}
#else
# define regcomp_safe regcomp
#endif

/* first some scheme functions we export */

#define SCHEME_RET_IF_FAIL(scm, expr, str)      \
  do {                                          \
	if(!(expr)) {                               \
	  return scheme_error(scm, NULL, str);      \
	}                                           \
  } while(0)

#define scheme_list_len(sc, lst) list_length((sc), (lst))

#define SCHEME_DEFINE(sc, func_ptr, func_name)							\
	sc->vptr->scheme_define(sc, sc->global_env,							\
							sc->vptr->mk_symbol(sc, func_name),			\
							sc->vptr->mk_foreign_func(sc, func_ptr))

#define SCHEME_DEFINE2(sc, func_ptr, func_name, doc) \
	SCHEME_DEFINE(sc, func_ptr, func_name)

#define SCHEME_DEFINE_VAR(sc, symname, value) scheme_define((sc), (sc)->global_env, (sc)->vptr->mk_symbol((sc), symname), value)

/* Stolen from Fl_Help_View.cxx, Copyright 1997-2010 by Easy Software Products. */
Fl_Color named_to_fltk_color(const char *n, Fl_Color ret) {
	static const struct {
		const char *name;
		int r, g, b;
	} colors[] = {
		{ "black",		0x00, 0x00, 0x00 },
		{ "red",		0xff, 0x00, 0x00 },
		{ "green",		0x00, 0x80, 0x00 },
		{ "yellow",		0xff, 0xff, 0x00 },
		{ "blue",		0x00, 0x00, 0xff },
		{ "magenta",	0xff, 0x00, 0xff },
		{ "fuchsia",	0xff, 0x00, 0xff },
		{ "cyan",		0x00, 0xff, 0xff },
		{ "aqua",		0x00, 0xff, 0xff },
		{ "white",		0xff, 0xff, 0xff },
		{ "gray",		0x80, 0x80, 0x80 },
		{ "grey",		0x80, 0x80, 0x80 },
		{ "lime",		0x00, 0xff, 0x00 },
		{ "maroon",		0x80, 0x00, 0x00 },
		{ "navy",		0x00, 0x00, 0x80 },
		{ "olive",		0x80, 0x80, 0x00 },
		{ "purple",		0x80, 0x00, 0x80 },
		{ "silver",		0xc0, 0xc0, 0xc0 },
		{ "teal",		0x00, 0x80, 0x80 }
	};

	if(!n || !n[0]) return ret;

	if(n[0] == '#') {
		int r, g, b, rgb;
		rgb = strtol(n + 1, NULL, 16);
		if(strlen(n) > 4) {
			r = rgb >> 16;
			g = (rgb >> 8) & 255;
			b = rgb & 255;
		} else {
			r = (rgb >> 8) * 17;
			g = ((rgb >> 4) & 15) * 17;
			b = (rgb & 15) * 17;
		}
		return (fl_rgb_color((uchar)r, (uchar)g, (uchar)b));
	} else {
		for (int i = 0; i < (int)(sizeof(colors) / sizeof(colors[0])); i ++)
			if (!strcasecmp(n, colors[i].name))
				return fl_rgb_color(colors[i].r, colors[i].g, colors[i].b);
	}

	return ret;
}

static pointer scheme_error(scheme *sc, const char *tag, const char *str) {
	if(tag)
		printf("Error: -- %s %s\n", tag, str);
	else
		printf("Error: %s\n", str);

	return sc->F;
}

/* simple vprintf-like function for easier creating lists */
static pointer scheme_argsf(scheme *sc, const char *fmt, ...) {
	pointer p = sc->NIL;

	va_list vl;
	va_start(vl, fmt);

	for(int i = 0; fmt[i]; i++) {
		switch(fmt[i]) {
			case 's':
				p = sc->vptr->cons(sc, sc->vptr->mk_string(sc, va_arg(vl, const char*)), p);
				break;
			case 'S':
				p = sc->vptr->cons(sc, sc->vptr->mk_symbol(sc, va_arg(vl, const char*)), p);
				break;
			case 'i':
				p = sc->vptr->cons(sc, sc->vptr->mk_integer(sc, va_arg(vl, long)), p);
				break;
			case 'd':
				p = sc->vptr->cons(sc, sc->vptr->mk_real(sc, va_arg(vl, double)), p);
				break;
			case 'c':
				p = sc->vptr->cons(sc, sc->vptr->mk_character(sc, va_arg(vl, int)), p);
				break;
			case 'L':
				p = sc->vptr->cons(sc, va_arg(vl, pointer), p);
				break;
			case 'A': {
				for(pointer o = va_arg(vl, pointer); o != sc->NIL; o = sc->vptr->pair_cdr(o))
					p = sc->vptr->cons(sc, sc->vptr->pair_car(o), p);
				break;
			} default:
				printf("Warning: unknown format char '%c', skipping...\n", fmt[i]);
				break;
		}
	}

	va_end(vl);
	/* elements in 'p' are due cons-ing in reverse order now */
	return scheme_reverse_in_place(sc, sc->NIL, p);
}

/* calls (editor-run-hook) */
INLINE static pointer scheme_run_hook(scheme *sc, const char *hook, pointer args) {
	/* construct '(editor-run-hook hook-str hook args) */
	args = sc->vptr->cons(sc, sc->vptr->mk_symbol(sc, hook), args);
	args = sc->vptr->cons(sc, sc->vptr->mk_string(sc, hook), args);
	args = sc->vptr->cons(sc, sc->vptr->mk_symbol(sc, "editor-run-hook"), args);

	return scheme_eval(sc, args);
}

/* regex */
#if USE_POSIX_REGEX
INLINE static void _rx_free(void *r) {
	regex_t *rx = (regex_t*)r;
	regfree(rx);
}

static pointer _rx_compile(scheme *s, pointer args) {
	int len, flags = 0;
	pointer arg;
	char *pattern;

	len = scheme_list_len(s, args);
	SCHEME_RET_IF_FAIL(s, (len > 0 && len < 3), "Bad number of arguments.");

	arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, s->vptr->is_string(arg), "First argument must be a string.");

	pattern = s->vptr->string_value(arg);

	args = s->vptr->pair_cdr(args);
	arg = s->vptr->pair_car(args);

	/* got flag list */
	if(arg != s->NIL) {
		pointer o;
		char *sym;

		for(pointer it = arg; it != s->NIL; it = s->vptr->pair_cdr(it)) {
			o = s->vptr->pair_car(it);
			SCHEME_RET_IF_FAIL(s, s->vptr->is_symbol(o), "Parameter must be a symbol");

			sym = s->vptr->symname(o);

			/* tinyscheme always make symbols as lowercase */
			if(STR_CMP(sym, "rx_extended")) {
			  flags |= REG_EXTENDED;
			  continue;
			}

			if(STR_CMP(sym, "rx_ignore_case")) {
			  flags |= REG_ICASE;
			  continue;
			}

			if(STR_CMP(sym, "rx_newline")) {
			  flags |= REG_NEWLINE;
			  continue;
			}

			return scheme_error(s, "regex-compile", ": bad regex option. Only supported options are: RX_EXTENDED, RX_IGNORE_CASE and RX_NEWLINE");
		}
	}

	regex_t *rx = (regex_t*)malloc(sizeof(regex_t));

	if(regcomp_safe(rx, pattern, flags) != 0) {
		free(rx);
		return s->F;
	}

	return s->vptr->mk_opaque(s, "REGEX", rx, _rx_free);
}

static pointer _rx_type(scheme *s, pointer args) {
	pointer arg;
	int ret = 0;

	arg = s->vptr->pair_car(args);
	if(s->vptr->is_opaque(arg)) {
		const char *tag = s->vptr->opaquetag(arg);
		ret = (tag && STR_CMP(tag, "REGEX"));
	}

	return ret ? s->T : s->F;
}

static pointer _rx_match(scheme *s, pointer args) {
	SCHEME_RET_IF_FAIL(s, _rx_type(s, args) == s->T, "Expected regex object as first argument.");

	pointer arg;
	regex_t *rx;
	char *str;

	arg = s->vptr->pair_car(args);
	rx = (regex_t*)s->vptr->opaquevalue(arg);

	args = s->vptr->pair_cdr(args);
	arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, s->vptr->is_string(arg), "Expected string object as second argument.");

	str = s->vptr->string_value(arg);
	return regexec(rx, str, (size_t)0, NULL, 0) == REG_NOMATCH ? s->F : s->T;
}
#endif /* USE_POSIX_REGEX */

static pointer _file_exists(scheme *s, pointer args) {
	pointer arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_string(arg), "Expected string object as first argument.");

	int ret = access(s->vptr->string_value(arg), F_OK);
	return ret == 0 ? s->T : s->F;
}

static pointer _system(scheme *s, pointer args) {
	pointer arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_string(arg), "Expected string object as first argument.");

	int ret = system(s->vptr->string_value(arg));
	return s->vptr->mk_integer(s, ret);
}

static pointer _buffer_string(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	/* no buffer */
	if(!priv->self->buffer())
		return s->F;

	char *t = priv->self->buffer()->text();
	pointer ret = s->vptr->mk_string(s, t);
	free(t);

	return ret;
}

static pointer _point(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	return s->vptr->mk_integer(s, priv->self->insert_position());
}

static pointer _goto_char(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_integer(arg),
					   "Expected number as first argument.");

	priv->self->insert_position(s->vptr->ivalue(arg));
	return s->T;
}

static pointer _beginning_of_line(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	priv->self->kf_home(0, priv->self);
	return s->T;
}

static pointer _end_of_line(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	priv->self->kf_end(0, priv->self);
	return s->T;
}

static pointer _set_tab_width(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_integer(arg),
					   "Expected number as first argument.");

	Fl_Text_Buffer *b = priv->self->buffer();
	if(!b) return s->F;

	b->tab_distance(s->vptr->ivalue(arg));
	return s->T;
}

static pointer _get_tab_width(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	int ret = -1;
	Fl_Text_Buffer *b = priv->self->buffer();
	if(b) ret = b->tab_distance();

	return s->vptr->mk_integer(s, ret);
}

static pointer _set_tab_expand(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	priv->self->expand_tabs(arg == s->T);
	return s->T;
}

static pointer _editor_repaint_context_chaged(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	priv->self->repaint(Fl_Highlight_Editor::REPAINT_CONTEXT);
	return s->T;
}

static pointer _editor_repaint_face_chaged(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	priv->self->repaint(Fl_Highlight_Editor::REPAINT_STYLE);
	return s->T;
}

static pointer _editor_set_background_color(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL, "This function expects argument.");

	int c;
	if(s->vptr->is_integer(arg))
		c = s->vptr->ivalue(arg);
	else if(s->vptr->is_string(arg))
		c = named_to_fltk_color(s->vptr->string_value(arg), FL_BLACK);
	else {
		SCHEME_RET_IF_FAIL(s, false, "This function expects number or string argument.");
		return s->F;
	}

	priv->self->color(c, FL_SELECTION_COLOR);
	return s->T;
}

static pointer _editor_set_cursor_color(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL, "This function expects argument.");

	int c;
	if(s->vptr->is_integer(arg))
		c = s->vptr->ivalue(arg);
	else if(s->vptr->is_string(arg))
		c = named_to_fltk_color(s->vptr->string_value(arg), FL_BLACK);
	else {
		SCHEME_RET_IF_FAIL(s, false, "This function expects number or string argument.");
		return s->F;
	}

	priv->self->cursor_color(c);
	return s->T;
}

static pointer _editor_set_cursor_shape(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_symbol(arg),
					   "This function expects symbol as argument.");

	const char *sym = (const char*)s->vptr->symname(arg);
	int c = Fl_Text_Display::NORMAL_CURSOR;

	if(STR_CMP(sym, "normal")) {
		/* default; not setting anything for self-documentation purpose */
	} else if(STR_CMP(sym, "caret")) {
		c = Fl_Text_Display::CARET_CURSOR;
	} else if(STR_CMP(sym, "dim")) {
		c = Fl_Text_Display::DIM_CURSOR;
	} else if(STR_CMP(sym, "block")) {
		c = Fl_Text_Display::BLOCK_CURSOR;
	} else if(STR_CMP(sym, "heavy")) {
		c = Fl_Text_Display::HEAVY_CURSOR;
	} else {
		SCHEME_RET_IF_FAIL(s, false, "Cursor type not in 'caret/dim/block/heavy' form");
	}

	priv->self->cursor_style(c);
	return s->T;
}

static pointer _editor_set_fltk_font_face(scheme *s, pointer args) {
	pointer arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_integer(arg),
					   "Expected number as first argument.");
	int        fltk_font;
	const char *font_name;

	fltk_font = s->vptr->ivalue(arg);
	args = s->vptr->pair_cdr(args);
	arg = s->vptr->pair_car(args);

	SCHEME_RET_IF_FAIL(s, arg != s->NIL && s->vptr->is_string(arg),
					   "Expected string as second argument.");

	font_name = s->vptr->string_value(arg);
	/*
	 * TODO: set_font() requires 'font_name' to be in static memory; for now is stored inside
	 * tinyscheme memory, so I'm not sure will it be erased at some point.
	 */
	Fl::set_font(fltk_font, font_name);
	return s->T;
}

static pointer _editor_dump_style_table(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);
	pointer ret = s->NIL, tmp;

	for(int i = 0; i < priv->styletable_last; i++) {
		tmp = s->vptr->cons(s, s->vptr->mk_character(s, 'A' + i), s->NIL);
		/* color, font, size */
		tmp = s->vptr->cons(s, s->vptr->mk_integer(s, priv->styletable[i].color), tmp);
		tmp = s->vptr->cons(s, s->vptr->mk_integer(s, priv->styletable[i].size), tmp);
		tmp = s->vptr->cons(s, s->vptr->mk_integer(s, priv->styletable[i].font), tmp);

		ret = s->vptr->cons(s, scheme_reverse_in_place(s, s->NIL, tmp), ret);
	}

	return scheme_reverse_in_place(s, s->NIL, ret);
}

static pointer _editor_dump_style_buf(scheme *s, pointer args) {
	ASSERT(s->ext_data != NULL);
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)(s->ext_data);

	pointer ret;
	char *txt = priv->stylebuf ? priv->stylebuf->text() : NULL;
	const char *p = txt ? (const char*)txt : "";
	ret = s->vptr->mk_string(s, p);
	if(txt) free(txt);

	return ret;
}

/* export this symbols to intepreter */
static void init_scheme_prelude(scheme *s, Fl_Highlight_Editor_P *priv) {
	/* So functions can access buffer(), self and etc. Accessed with 's->ext_data'. */
	scheme_set_external_data(s, priv);

	/* base functions */
	SCHEME_DEFINE2(s, _file_exists, "file-exists?", "Check if given file is accessible.");
	SCHEME_DEFINE2(s, _system, "system", "Run external command.");

#if USE_POSIX_REGEX
	SCHEME_DEFINE2(s, _rx_compile, "regex-compile",
				   "Compile regular expression into binary (and fast object). If fails, returns #f.");
	SCHEME_DEFINE2(s, _rx_type, "regex?",
				   "Check if given parameter is regular expression object.");
	SCHEME_DEFINE2(s, _rx_match, "regex-match",
				   "Returns #t or #f if given string matches regular expression object.");
#endif

	/* functions for accessing text; if buffer() not available, does nothing */
	SCHEME_DEFINE2(s, _buffer_string, "buffer-string",
				   "Return content of buffer. In buffer not available, returns #f.");
	SCHEME_DEFINE2(s, _point, "point", "Return current cursor position.");
	SCHEME_DEFINE2(s, _goto_char, "goto-char", "Move cursor to given position.");
	SCHEME_DEFINE2(s, _beginning_of_line, "beginning-of-line", "Move cursor to beginning of line.");
	SCHEME_DEFINE2(s, _end_of_line, "end-of-line", "Move cursor to end of line.");
	SCHEME_DEFINE2(s, _set_tab_width, "set-tab-width", "Set TAB width.");
	SCHEME_DEFINE2(s, _get_tab_width, "get-tab-width", "Get TAB width. If buffer not available, returns -1.");
	SCHEME_DEFINE2(s, _set_tab_expand, "set-tab-expand", "Replace TAB with spaces. Uses TAB width.");
	SCHEME_DEFINE2(s, _editor_repaint_context_chaged, "editor-repaint-context-changed", "Update global context table and redraw.");
	SCHEME_DEFINE2(s, _editor_repaint_face_chaged, "editor-repaint-face-changed", "Update global face table and redraw.");
	SCHEME_DEFINE2(s, _editor_set_background_color, "editor-set-background-color", "Set editor background color. FLTK colors codes are accepted.");
	SCHEME_DEFINE2(s, _editor_set_cursor_color, "editor-set-cursor-color", "Set cursor color.");
	SCHEME_DEFINE2(s, _editor_set_cursor_shape, "editor-set-cursor-shape", "Set cursor shape.");
	SCHEME_DEFINE2(s, _editor_set_fltk_font_face, "editor-set-fltk-font-face", "Change FLTK font by assigning it font name.");

	/* for debugging */
	SCHEME_DEFINE2(s, _editor_dump_style_table, "editor-dump-style-table", "Returns internal copy of style table. For debugging purposes.");
	SCHEME_DEFINE2(s, _editor_dump_style_buf, "editor-dump-style-buffer", "Returns internal copy of style buffer. For debugging purposes.");
}

/* core widget code */

Fl_Highlight_Editor_P::Fl_Highlight_Editor_P() {
	scm         = NULL;
	script_path = NULL;
	self        = NULL;
	stylebuf    = NULL;
	styletable  = NULL;
	ctable      = NULL;
	styletable_size = styletable_last = 0;
	loaded_context_and_faces = false;
	update_cb_added = false;
	/* initial 'A' - plain */
	push_style_default(FL_BLACK, FL_COURIER, FL_NORMAL_SIZE);
}

int Fl_Highlight_Editor_P::push_style(int color, int font, int size) {
	/* grow if needed */
	if(styletable_last >= styletable_size) {
		if(!styletable_size) {
			styletable_size = 3;
			styletable = new StyleTable[styletable_size];
		} else {
			StyleTable *old = styletable;

			styletable_size *= 2;
			styletable = new StyleTable[styletable_size];

			for(int i = 0; i < styletable_last; i++)
				styletable[i] = old[i];

			delete [] old;
		}
	}

	styletable[styletable_last].color = color;
	styletable[styletable_last].font  = font;
	styletable[styletable_last].size  = size;

	/* returns valid position, then increase it */
	return styletable_last++;
}

void Fl_Highlight_Editor_P::push_style_default(int color, int font, int size) {
	if(!styletable_size)
		push_style(color, font, size);
	else {
		styletable[0].color = color;
		styletable[0].font = font;
		styletable[0].size = size;
	}
}

void Fl_Highlight_Editor_P::clear_styles(void) {
	delete [] styletable;
	styletable = NULL;
	styletable_last = styletable_size = 0;
}

#define FREE_AND_RETURN(o)	\
	delete o;			\
	return;

void Fl_Highlight_Editor_P::push_context(scheme *s, int type, pointer content, const char *face) {
	ContextTable *t = new ContextTable();
	/* chr and pos are re-populated in load_face_table() */
	t->chr = 'A';
	t->pos = 0;
	t->face = face;
	t->type = type;
	t->last = t->next = NULL;

	switch(type) {
		case CONTEXT_TYPE_INITIAL:
		case CONTEXT_TYPE_LAST:
			break;
		case CONTEXT_TYPE_REGEX: {
#if USE_POSIX_REGEX
			if(!s->vptr->is_string(content)) {
				puts("Pattern must be a string");
				FREE_AND_RETURN(t);
			}

			regex_t    *rx = (regex_t*)malloc(sizeof(regex_t));
			const char *p  = (const char*)s->vptr->string_value(content);

			if(regcomp_safe(rx, p, REG_EXTENDED | REG_NEWLINE) != 0) {
				printf("Failed to compile pattern '%s'\n", p);
				free(rx);
				FREE_AND_RETURN(t);
			}

			t->object.rx = rx;
#endif
			break;
		}

		case CONTEXT_TYPE_TO_EOL:
		case CONTEXT_TYPE_EXACT: {
			if(!s->vptr->is_string(content)) {
				puts("Exact value must be a string");
				FREE_AND_RETURN(t);
			}

			/* FIXME: strdup()? */
			t->object.exact = (const char*)s->vptr->string_value(content);
			break;
		}

		case CONTEXT_TYPE_BLOCK: {
			if(!s->vptr->is_pair(content)) {
				puts("Block must be block type");
				FREE_AND_RETURN(t);
			}

			pointer start = s->vptr->pair_car(content);
			pointer end = s->vptr->pair_cdr(content);

			if(!s->vptr->is_string(start) || !s->vptr->is_string(end)) {
				puts("Block tokens must be string type");
				FREE_AND_RETURN(t);
			}

			/* FIXME: strdup()? */
			t->object.block[0] = s->vptr->string_value(start);
			t->object.block[1] = s->vptr->string_value(end);
		}

		default: break;
	}


	/* correct pointers at the end, so we don't assign junk in case FREE_AND_RETURN() was called */
	if(!ctable)
		ctable = t;
	else
		ctable->last->next = t;

	ctable->last = t;
}

void Fl_Highlight_Editor_P::clear_contexts(void) {
	if(!ctable) return;

	ContextTable *it, *nx;
	for(it = ctable; it; it = nx) {
		printf("removing: %s face\n", it->face);
		nx = it->next;

#if USE_POSIX_REGEX
		if(it->type == CONTEXT_TYPE_REGEX)
			regfree(it->object.rx);
#endif
		delete it;
	}

	ctable = NULL;
}

Fl_Highlight_Editor::Fl_Highlight_Editor(int X, int Y, int W, int H, const char *l) :
	Fl_Text_Editor(X, Y, W, H, l), priv(NULL)
{
	do_expand_tabs = 0;
	add_key_binding(FL_Tab, 0, Fl_Highlight_Editor::tab_press);
}

Fl_Highlight_Editor::~Fl_Highlight_Editor() {
	puts("~Fl_Highlight_Editor");
	if(!priv) return;

	if(priv->script_path)
		free(priv->script_path);

	if(priv->scm)
		scheme_deinit(priv->scm);

	priv->clear_contexts();
	priv->clear_styles();

	delete priv->stylebuf;
	delete priv;
	priv = NULL;
}

void Fl_Highlight_Editor::init_interpreter(const char *script_folder, bool do_repl) {
	if(priv) return;

	clock_t started = clock();
	priv = new Fl_Highlight_Editor_P;
	priv->self = this;

	/* load interpreter */
	scheme *scm = scheme_init_new();
	scheme_set_input_port_file(scm, stdin);
	scheme_set_output_port_file(scm, stdout);

	priv->script_path = strdup(script_folder);

	/* make *load-path* first */
	pointer ptr = scm->vptr->cons(scm, scm->vptr->mk_string(scm, script_folder), scm->NIL);
	SCHEME_DEFINE_VAR(scm, "*load-path*", ptr);

	SCHEME_DEFINE_VAR(scm, "*editor-current-mode*", scm->F);
	SCHEME_DEFINE_VAR(scm, "*editor-context-table*", scm->NIL);
	SCHEME_DEFINE_VAR(scm, "*editor-face-table*", scm->NIL);
	SCHEME_DEFINE_VAR(scm, "*editor-auto-mode-alist*", scm->NIL);
	SCHEME_DEFINE_VAR(scm, "*editor-before-loadfile-hook*", scm->NIL);
	SCHEME_DEFINE_VAR(scm, "*editor-after-loadfile-hook*", scm->NIL);

	/* assure prerequisites are loaded before main initialization file */
	init_scheme_prelude(scm, priv);

#if USE_BUNDLED_SCRIPTS
#   include "bundled_scripts.cxx"
	scheme_load_string(scm, bundled_scripts_content);
#else
	char buf[PATH_MAX];
	FILE *fd;

	snprintf(buf, sizeof(buf), "%s/boot.ss", script_folder);
	fd = fopen(buf, "r");
	if(fd) {
		scm->vptr->load_file(scm, fd);
		fclose(fd);
	}

	/* editor specific code */
	snprintf(buf, sizeof(buf), "%s/editor.ss", script_folder);
	fd = fopen(buf, "r");
	if(fd) {
		scm->vptr->load_file(scm, fd);
		fclose(fd);
	}
#endif

	float diff = (((float)clock() - (float)started) / CLOCKS_PER_SEC) * 1000;
	printf("Interpreter booted in %4.1fms.\n", diff);

	if(do_repl)
		scheme_load_named_file(scm, stdin, 0);

	priv->scm = scm;
}

void Fl_Highlight_Editor::load_script_file(const char *path) {
	if(!priv) return;
	FILE *fd = fopen(path, "r");
	if(!fd) return;

	scheme_load_named_file(priv->scm, fd, path);
	fclose(fd);
}

void Fl_Highlight_Editor::load_script_string(const char *str) {
	if(!priv) return;
	scheme_load_string(priv->scm, str);
}

const char *Fl_Highlight_Editor::script_folder(void) {
	if(!priv) return NULL;
	return priv->script_path;
}

static Fl_Highlight_Editor_P *load_context_table(Fl_Highlight_Editor_P *priv) {
	scheme *s = priv->scm;
	pointer tp, f, v, style_table = scheme_eval(s, s->vptr->mk_symbol(s, "*editor-context-table*"));
	char *face;

	for(pointer it = style_table; it != s->NIL; it = s->vptr->pair_cdr(it)) {
		v = s->vptr->pair_car(it);

		if(!s->vptr->is_vector(v))
			continue;

		if(s->vptr->vector_length(v) != 3) {
			printf("Expected vector len of 3 but got len '%i'\n", (int)s->vptr->vector_length(v));
			continue;
		}

		/* fetch type */
		tp = s->vptr->vector_elem(v, 0);
		if(!s->vptr->is_integer(tp))
			continue;

		/* fetch face; we limit ourself face must be either symbol or string */
		f = s->vptr->vector_elem(v, 2);
		if(!s->vptr->is_symbol(f) && !s->vptr->is_string(f))
			continue;

		face = s->vptr->is_symbol(f) ? s->vptr->symname(f) : s->vptr->string_value(f);
		priv->push_context(s, s->vptr->ivalue(tp), s->vptr->vector_elem(v, 1), face);
	}

	return priv;
}

#define VECTOR_GET_INT(scm, vec, pos, tmp, ret)	\
do {											\
	tmp = scm->vptr->vector_elem(vec, pos);		\
	if(!s->vptr->is_integer(tmp)) continue;		\
	ret = s->vptr->ivalue(tmp);					\
} while(0)

static Fl_Highlight_Editor_P *load_face_table(Fl_Highlight_Editor_P *priv) {
	scheme *s = priv->scm;
	const char *face;
	pointer o, v, face_table = scheme_eval(s, s->vptr->mk_symbol(s, "*editor-face-table*"));
	int font = 0, color = 0, size = 0;

	for(pointer it = face_table; it != s->NIL; it = s->vptr->pair_cdr(it)) {
		v = s->vptr->pair_car(it);

		if(!s->vptr->is_vector(v) || s->vptr->vector_length(v) != 4)
			continue;

		o = s->vptr->vector_elem(v, 0);
		if(!s->vptr->is_symbol(o) && !s->vptr->is_string(o))
			continue;

		face = s->vptr->is_string(o) ? s->vptr->string_value(o) : s->vptr->symname(o);

		/* we support both FLTK and html colors */
		o = s->vptr->vector_elem(v, 1);
		if(s->vptr->is_integer(o))
			color = s->vptr->ivalue(o);
		else if(s->vptr->is_string(o))
			color = named_to_fltk_color(s->vptr->string_value(o), FL_BLACK);
		else
			continue;

		VECTOR_GET_INT(s, v, 2, o, size);
		VECTOR_GET_INT(s, v, 3, o, font);

		if(STR_CMP(face, DEFAULT_FACE)) {
			priv->push_style_default(color, font, size);
		} else {
			/* Lookup face in face table and assign position and paint character. This could be done much efficiently with hash table... */
			for(ContextTable *ct = priv->ctable; ct; ct = ct->next) {
				if(ct->face && STR_CMP(ct->face, face)) {
					ct->pos = priv->push_style(color, font, size);

					/* FIXME: we adjust character here; move it to somewhere more visible */
					ct->chr = 'A' + ct->pos;
					printf("Loading face: %s %c\n", ct->face, ct->chr);

					/*
					 * Now scan remaining elements and find entries with the same face and adjust thier 'pos' and 'chr' members. With this
					 * multiple entries with the same face, but different token matchers will have the same paint character and position inside
					 * FLTK style table.
					 */
					for(ContextTable *tmp = ct; tmp; tmp = tmp->next) {
						if(STR_CMP(ct->face, tmp->face)) {
							tmp->pos = ct->pos;
							tmp->chr = ct->chr;
						}
					}

					/* done, go home */
					break;
				}
			}
		}
	}

	return priv;
}

/* perform highlighting based on loaded context data */
static char *hi_parse(ContextTable *ct, const char *text, char *style, int len) {
	if(!ct) return NULL;

	/* repaint region with default style first */
	memset(style, 'A', len);

	for(ContextTable *it = ct; it; it = it->next) {
		if(it->type == CONTEXT_TYPE_EXACT || it->type == CONTEXT_TYPE_TO_EOL) {
			const char *p, *what = it->object.exact;
			int start, l = strlen(what);

			if(it->type == CONTEXT_TYPE_EXACT) {
				for(p = strstr(text, what); p; p = strstr(p + l, what)) {
					start = p - text;

					for(int i = start; i < (start + l); i++)
						style[i] = it->chr;
				}
			} else {
				p = text;
				/* paint match from found token to the end of the line or buffer */
				for(p = strstr(text, what); p; p = strstr(p, what)) {
					start = p - text;

					for(int i = start; *p && *p != '\n'; i++, p++)
						style[i] = it->chr;
				}
			}
		} else if(it->type == CONTEXT_TYPE_BLOCK) {
			const char *bstart, *bend, *p, *e;
			int slen;

			bstart = it->object.block[0];
			bend   = it->object.block[1];
			slen   = strlen(bstart);

			for(p = strstr(text, bstart); p; p = strstr(p, bstart)) {
				e = strstr(p + slen, bend);

				/* this will also handle the case when block end wasn't found, so it will paint to the end of the file */
				for(int i = p - text; *p; p++, i++) {
					style[i] = it->chr;
					if(p == e) {
						/* paint ending character too */
						style[++i] = it->chr;
						break;
					}
				}

				if(!*p) break;
				p += slen;
			}
		} else if(it->type == CONTEXT_TYPE_REGEX) {
#if USE_POSIX_REGEX
			/*
			 * Match against the whole text. However, how regexec() works, we are continuously
			 * matching to get offsets; grouping submatches are ignored as no grouping is used.
			 */
			regmatch_t pmatch[1];
			const char *str = text;
			int i, end;

			for(str = text; regexec(it->object.rx, str, 1, pmatch, 0) != REG_NOMATCH; str += pmatch[0].rm_eo) {
				ASSERT(pmatch[0].rm_so != -1);
				ASSERT(pmatch[0].rm_eo != -1);

				i   = str - text + pmatch[0].rm_so - 1;
				end = str - text + pmatch[0].rm_eo - 1;
				while(i++ < end)
					style[i] = it->chr;
			}
#endif
		}
	}

	return style;
}

/* highlighting functions and callbacks */
static void hi_init(Fl_Highlight_Editor_P *priv, Fl_Text_Buffer *buf) {
	/* do nothing unless we have something inside style table */
	if(!priv->styletable_size)
		return;

	char *style, *text;

	style = new char[buf->length() + 1];
	text = buf->text();

	/* the first style is always marked with 'A' */
	style[buf->length()] = '\0';

	if(!priv->stylebuf)
		priv->stylebuf = new Fl_Text_Buffer(buf->length());

	hi_parse(priv->ctable, text, style, buf->length());

	priv->stylebuf->text(style);
	delete[] style;
	free(text);
}

/* Mostly stolen from FLTK editor.cxx example. Obviously (c)-ed by editor.cxx author... */
static void hi_update(int pos, int ninserted, int ndeleted,
					  int  nrestyled, const char *deletedtext,
					  void *data)
{
	Fl_Highlight_Editor_P *priv = (Fl_Highlight_Editor_P*)data;
	Fl_Text_Buffer *buf         = priv->self->buffer();

	if(ninserted == 0 && ndeleted == 0) {
		priv->stylebuf->unselect();
		return;
	}

	char *style, *text, last;
	int  start, end;

	if(ninserted > 0) {
		/* insert characters in style buffer */
		style = new char[ninserted + 1];
		memset(style, 'A', ninserted);
		style[ninserted] = '\0';

		priv->stylebuf->replace(pos, pos + ndeleted, style);
		delete[] style;
	} else {
		/* just delete characters in style buffer */
		priv->stylebuf->remove(pos, pos + ndeleted);
	}

	/* select the area that was just updated */
	priv->stylebuf->select(pos, pos + ninserted - ndeleted);

	/*
	 * Re-parse the changed region; we do this by parsing from the
	 * beginning of the previous line of the changed region to the end of
	 * the line of the changed region...  Then we check the last
	 * style character and keep updating if we have a multi-line
	 * comment character...
	 */
	start = buf->line_start(pos);
	end   = buf->line_end(pos + ninserted);
	text  = buf->text_range(start, end);
	style = priv->stylebuf->text_range(start, end);
	last  = (start == end) ? 0 : style[end - start - 1];

	hi_parse(priv->ctable, text, style, end - start);

	priv->stylebuf->replace(start, end, style);
	priv->self->redisplay_range(start, end);

	if(start == end || last != style[end - start - 1]) {
		/*
		 * Either the user deleted some text, or the last character on the line changed
		 * styles, so reparse the remainder of the buffer...
		 */
		free(text);
		free(style);

		end   = buf->length();
		text  = buf->text_range(start, end);
		style = priv->stylebuf->text_range(start, end);

		hi_parse(priv->ctable, text, style, end - start);
		priv->stylebuf->replace(start, end, style);
		priv->self->redisplay_range(start, end);
	}


	free(text);
	free(style);
}

void Fl_Highlight_Editor::buffer(Fl_Text_Buffer *buf) {
	/* prevent self assignment */
	if(Fl_Text_Display::buffer() == buf)
		return;

	Fl_Text_Display::buffer(buf);

	if(priv) {
		/* new buffer requires another update callback */
		priv->update_cb_added = false;
		repaint(Fl_Highlight_Editor::REPAINT_ALL);
	}
}

void Fl_Highlight_Editor::repaint(int what, const char *mode) {
	if(!priv || !buffer()) return;

	if(what & Fl_Highlight_Editor::REPAINT_CONTEXT) {
		puts("Repainting context...");
		priv->clear_contexts();
		priv = load_context_table(priv);
	}

	if(what & Fl_Highlight_Editor::REPAINT_STYLE) {
		puts("Repainting styles...");
		priv->clear_styles();
		priv = load_face_table(priv);
	}

	/*
	 * Initialize stylebuf. This function is called after we have loaded faces and contexts so it can do
	 * initial parsing and actual painting. However, it is called before highlight_data(), so we can have
	 * something highlighted on initial show.
	 */
	hi_init(priv, buffer());

	/* ASSERT(priv->stylebuf != NULL); */
	if(priv->stylebuf == NULL) {
		puts("No stylebuf!! Check scheme code or something went wrong...");
		return;
	}

	/* notify Fl_Text_Display highligher about styletable changes */
	if(what & Fl_Highlight_Editor::REPAINT_STYLE) {
		highlight_data(priv->stylebuf, priv->styletable, priv->styletable_last, 'A', 0, 0);
	}

	if(!priv->update_cb_added) {
		buffer()->add_modify_callback(hi_update, priv);
		priv->update_cb_added = true;
	}
}

int Fl_Highlight_Editor::loadfile(const char *file, int buflen) {
	if(!buffer())
		buffer(new Fl_Text_Buffer());

	int ret;

	if(priv) scheme_run_hook(priv->scm, "*editor-before-loadfile-hook*", scheme_argsf(priv->scm, "s", file));

	ret = buffer()->loadfile(file, buflen);
	if(ret != 0) return ret;

	if(priv) scheme_run_hook(priv->scm, "*editor-after-loadfile-hook*", scheme_argsf(priv->scm, "s", file));
	return ret;
}

int Fl_Highlight_Editor::savefile(const char *file, int buflen) {
	if(!buffer())
		buffer(new Fl_Text_Buffer());

	int ret;

	if(!priv) scheme_run_hook(priv->scm, "*editor-before-savefile-hook*", scheme_argsf(priv->scm, "s", file));

	ret = buffer()->savefile(file, buflen);
	if(ret != 0) return ret;

	if(priv) scheme_run_hook(priv->scm, "*editor-after-savefile-hook*", scheme_argsf(priv->scm, "s", file));
	return ret;
}

int Fl_Highlight_Editor::handle(int e) {
	int ret = Fl_Text_Editor::handle(e);
	return ret;
}

int Fl_Highlight_Editor::tab_press(int c, Fl_Text_Editor *e) {
	Fl_Highlight_Editor *ed = (Fl_Highlight_Editor*)e;
	if(!ed->expand_tabs()) return 0;
	Fl_Text_Buffer *buf = ed->buffer();
	if(!buf) return 0;

	int i, tabsz = buf->tab_distance();
	char *spaces = new char[tabsz + 1];
	for(i = 0; i < tabsz; i++)
		spaces[i] = ' ';
	spaces[i] = '\0';

	ed->insert(spaces);
	delete [] spaces;
	return 1;
}
