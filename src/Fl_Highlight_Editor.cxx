/*
 * Fl_Highlight_Editor - extensible text editing
 * Copyright (c) 2013 Sanel Zukan.
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
#include <limits.h>
#include <FL/Fl_Highlight_Editor.H>

#include "ts/scheme.h"
#include "ts/scheme-private.h"

#undef cons
#undef immutable_cons

#if USE_POSIX_REGEX
#include <sys/types.h>
#include <regex.h>
#endif

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
 *  1) block matching - done by searching strings designated block start and end. Block matching is
 *     used for (example) block comments and will behave like Vim - if block start was found but not block end,
 *     the whole buffer from that position will be painted in given face.
 *
 *  2) regex matching - regex is first compiled and matched against the whole buffer. Founded chunks will be
 *     painted on the same position in StyleTable buffer. No regex grouping is considered as is not useful here.
 *
 *  3) exact matching - again done against the whole buffer, where exact string is searched for. Mainly useful for
 *     content where regex engine is expensive or for the cases where special characters are involved so escaping
 *     is not needed.
 *
 * When painting is started, we scan ContextTable and for every matched type, we paint with given character found
 * match position in StyleTable buffer. To understaind how this works, see Fl_Text_Display documentation and how syntax
 * highlighting was done.
 */
struct ContextTable {
	char chr;
	int  type; /* TODO: make it 'char' */

	/* FIXME: only pointer to scheme symbol; will it be GC-ed at some point? */
	const char *face;

	union {
		regex_t    *rx;
		const char *block[2];
		const char *exact;
	} object;

	ContextTable *next, *last;
};

struct Fl_Highlight_Editor_P {
	scheme scm;
	char   *script_path;

	Fl_Text_Buffer *stylebuf;
	StyleTable     *styletable;
	ContextTable   *ctable;

	int styletable_size; /* size of styletable */
	int styletable_cur;  /* current item in styletable */

	Fl_Highlight_Editor_P();

	void push_style(int color, int font, int size);
	void push_context(int type, pointer content, const char *face);
};

Fl_Highlight_Editor_P::Fl_Highlight_Editor_P() {
	script_path  = NULL;
	stylebuf     = NULL;
	styletable   = NULL;
	ctable       = NULL;
	styletable_size = styletable_cur = 0;

	push_style(FL_BLACK, FL_COURIER, FL_NORMAL_SIZE); /* initial 'A' - plain */
}

void Fl_Highlight_Editor_P::push_style(int color, int font, int size) {
	/* grow if needed */
	if(styletable_cur >= styletable_size) {
		if(!styletable_size) {
			styletable_size = 3;
			styletable = new StyleTable[styletable_size];
		} else {
			StyleTable *old = styletable;

			styletable_size *= 2;
			styletable = new StyleTable[styletable_size];

			for(int i = 0; i < styletable_cur; i++)
				styletable[i] = old[i];

			delete [] old;
		}
	}

	styletable[styletable_cur].color = color;
	styletable[styletable_cur].font  = font;
	styletable[styletable_cur].size  = size;

	styletable_cur++;
}

void Fl_Highlight_Editor_P::push_context(int type, pointer content, const char *face) {
	ContextTable *t = new ContextTable();
	t->chr = 'A';
	t->face = face;
	t->type = type;
	t->next = NULL;

	if(!ctable) {
		ctable = t;
	} else {
		/* increase character from whatever was set previously */
		t->chr = ctable->last->chr++;

		/* expected to be in range 'A' - 'z' */
		if(t->chr > 123) {
			puts("Exceeded limit of allowed faces");
			delete t;
			return;
		}

		ctable->last->next = t;
	}

	ctable->last = t;
}

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

static pointer scheme_error(scheme *sc, const char *tag, const char *str) {
	if(tag) 
		printf("Error: -- %s %s\n", tag, str);
	else
		printf("Error: s %s\n", str);

	return sc->F;
}

/* regex */
#if USE_POSIX_REGEX
static void _rx_free(void *r) {
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
			if(strcmp(sym, "rx_extended") == 0) {
			  flags |= REG_EXTENDED;
			  continue;
			}

			if(strcmp(sym, "rx_ignore_case") == 0) {
			  flags |= REG_ICASE;
			  continue;
			}

			if(strcmp(sym, "rx_newline") == 0) {
			  flags |= REG_NEWLINE;
			  continue;
			}

			return scheme_error(s, "regex-compile", ": bad regex option. Only supported options are: RX_EXTENDED, RX_IGNORE_CASE and RX_NEWLINE");
		}
	}

	regex_t *rx = (regex_t*)malloc(sizeof(regex_t));

	if(regcomp(rx, pattern, flags) != 0) {
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
		ret = (tag && (strcmp(tag, "REGEX") == 0));
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

static void init_regex(scheme *scm) {
	SCHEME_DEFINE2(scm, _rx_compile, "regex-compile",
				   "Compile regular expression into binary (and fast object). If fails, returns #f.");
	SCHEME_DEFINE2(scm, _rx_type, "regex?",
				   "Check if given parameter is regular expression object.");
	SCHEME_DEFINE2(scm, _rx_match, "regex-match",
				   "Returns #t or #f if given string matches regular expression object.");
}
#endif /* USE_POSIX_REGEX */

static pointer _file_exists(scheme *s, pointer args) {
	pointer arg = s->vptr->pair_car(args);
	SCHEME_RET_IF_FAIL(s, arg != s->NIL, "Expected string object as first argument.");

	int ret = access(s->vptr->string_value(arg), F_OK);
	return ret == 0 ? s->T : s->F;
}

static void init_utils(scheme *s, const char *script_folder) {
	SCHEME_DEFINE2(s, _file_exists, "file-exists?",
				   "Check if given file is accessible.");
}

static void init_scheme_prelude(scheme *scm, const char *script_folder) {
	init_utils(scm, script_folder);

#if USE_POSIX_REGEX
	init_regex(scm);
#endif
}

Fl_Highlight_Editor::Fl_Highlight_Editor(int X, int Y, int W, int H, const char *l) :
	Fl_Text_Editor(X, Y, W, H, l), priv(NULL)
{
}

Fl_Highlight_Editor::~Fl_Highlight_Editor() {
	puts("~Fl_Highlight_Editor");
	if(!priv) return;

	if(priv->script_path) free(priv->script_path);
	scheme *pscm = &(priv->scm);
	scheme_deinit(pscm);

	if(priv->ctable) {
		ContextTable *it, *nx;
		for(it = priv->ctable; it; it = nx) {
			printf("removing: %s face\n", it->face);
			nx = it->next;
			delete it;
		}
	}

	delete [] priv->styletable;
	delete priv->stylebuf;
	delete priv;
	priv = NULL;
}

void Fl_Highlight_Editor::init_interpreter(const char *script_folder, bool do_repl) {
	if(priv) return;

	clock_t started = clock();
	char buf[PATH_MAX];
	FILE *fd;

	priv = new Fl_Highlight_Editor_P;

	/* load interpreter */
	scheme *pscm = &(priv->scm);
	
	scheme_init(pscm);
	priv->script_path = strdup(script_folder);

	scheme_set_input_port_file(pscm, stdin);
	scheme_set_output_port_file(pscm, stdout);

	/* make *load-path* first */
	pointer ptr = pscm->vptr->cons(pscm, pscm->vptr->mk_string(pscm, script_folder), pscm->NIL);
	SCHEME_DEFINE_VAR(pscm, "*load-path*", ptr);

	SCHEME_DEFINE_VAR(pscm, "*editor-style-table*", pscm->NIL);

	/* assure prerequisites are loaded before main initialization file */
	init_scheme_prelude(pscm, script_folder);

	snprintf(buf, sizeof(buf), "%s/boot.ss", script_folder);
	fd = fopen(buf, "r");
	if(fd) {
		pscm->vptr->load_file(pscm, fd);
		fclose(fd);
	}

	/* editor specific code */
	snprintf(buf, sizeof(buf), "%s/editor.ss", script_folder);
	fd = fopen(buf, "r");
	if(fd) {
		pscm->vptr->load_file(pscm, fd);
		fclose(fd);
	}

	float diff = (((float)clock() - (float)started) / CLOCKS_PER_SEC) * 1000;
	printf("Interpreter booted in %4.1fms.\n", diff);

	if(do_repl)
		scheme_load_named_file(pscm, stdin, 0);
}

const char *Fl_Highlight_Editor::script_folder(void) {
	if(!priv)
		return NULL;
	return priv->script_path;
}

int Fl_Highlight_Editor::handle(int e) {
	int ret = Fl_Text_Editor::handle(e);
	return ret;
}


void style_unfinished_cb(int, void*) {}

static void style_update(int pos, int ninserted, int ndeleted, int  /*nrestyled*/, const char * /*deletedtext*/, void *data) {
	printf("inserted: %i deleted: %i pos: %i\n", ninserted, ndeleted, pos);
}

static pointer load_style_table(Fl_Highlight_Editor_P *priv, Fl_Text_Buffer *buf) {
	scheme *s = &(priv->scm);
	pointer tp, f, v, style_table = scheme_eval(s, s->vptr->mk_symbol(s, "*editor-style-table*"));
	char *face;

	for(pointer it = style_table; it != s->NIL; it = s->vptr->pair_cdr(it)) {
		v = s->vptr->pair_car(it);

		if(!s->vptr->is_vector(v))
			continue;

		if(s->vptr->vector_length(v) != 3) {
			printf("Expected vector len of 3 but got len '%i'\n", s->vptr->vector_length(v));
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
		priv->push_context(s->vptr->ivalue(tp), s->vptr->vector_elem(v, 1), face);
	}

	return s->T;
}

void Fl_Highlight_Editor::buffer(Fl_Text_Buffer *buf) {
	/* prevent self assignment */
	if(Fl_Text_Display::buffer() == buf)
		return;

	Fl_Text_Display::buffer(buf);

	if(!priv)
		return;

	load_style_table(priv, buf);
#if 0	
	/* initialize style */
	//style_init(priv, buf);

	char *style = new char[buf->length() + 1];
	memset(style, 'A', buf->length());
	style[buf->length()] = '\0';

	if(!priv->stylebuf)
		priv->stylebuf = new Fl_Text_Buffer(buf->length());

	priv->stylebuf->text(style);
	delete [] style;

	/* setup initial style */
	highlight_data(priv->stylebuf,
				   priv->styletable,
				   priv->styletable_size,
				   'A',
				   style_unfinished_cb, 0);

	buf->add_modify_callback(style_update, this);
#endif
}
