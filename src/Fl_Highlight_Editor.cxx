/*
 * Fl_Highlight_Editor - extensible text editing widget
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
	scheme scm;
	char   *script_path;
	bool   loaded_styles_and_faces;

	Fl_Highlight_Editor *self; /* for doing redisplay and buffer() access from hi_update() callback */

	Fl_Text_Buffer *stylebuf;
	StyleTable     *styletable;
	ContextTable   *ctable;

	int styletable_size; /* size of styletable */
	int styletable_last; /* last item in styletable */

	Fl_Highlight_Editor_P();

	int  push_style(int color, int font, int size);
	void push_style_default(int color, int font, int size);
	void push_context(scheme *s, int type, pointer content, const char *face);
};

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

static void init_utils(scheme *s) {
	SCHEME_DEFINE2(s, _file_exists, "file-exists?",
				   "Check if given file is accessible.");
}

static void init_scheme_prelude(scheme *scm) {
	init_utils(scm);

#if USE_POSIX_REGEX
	init_regex(scm);
#endif
}

/* core widget code */

Fl_Highlight_Editor_P::Fl_Highlight_Editor_P() {
	script_path = NULL;
	self        = NULL;
	stylebuf    = NULL;
	styletable  = NULL;
	ctable      = NULL;
	styletable_size = styletable_last = 0;
	loaded_styles_and_faces = false;

	push_style_default(FL_BLACK, FL_COURIER, FL_NORMAL_SIZE); /* initial 'A' - plain */
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

#define FREE_AND_RETURN(o)	\
	delete o;			    \
	return;

void Fl_Highlight_Editor_P::push_context(scheme *s, int type, pointer content, const char *face) {
	ContextTable *t = new ContextTable();
	/* chr and pos are re-populated in load_face_table() */
	t->chr = 'A';
	t->pos = 0;
	t->face = face;
	t->type = type;
	t->next = NULL;

	if(!ctable)
		ctable = t;
	else
		ctable->last->next = t;

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

			if(regcomp(rx, p, REG_EXTENDED | REG_NEWLINE) != 0) {
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


	ctable->last = t;
}

Fl_Highlight_Editor::Fl_Highlight_Editor(int X, int Y, int W, int H, const char *l) :
	Fl_Text_Editor(X, Y, W, H, l), priv(NULL)
{
}

Fl_Highlight_Editor::~Fl_Highlight_Editor() {
	puts("~Fl_Highlight_Editor");
	if(!priv) return;

	if(priv->script_path)
		free(priv->script_path);

	scheme *pscm = &(priv->scm);
	scheme_deinit(pscm);

	if(priv->ctable) {
		ContextTable *it, *nx;
		for(it = priv->ctable; it; it = nx) {
			printf("removing: %s face\n", it->face);
			nx = it->next;
#if USE_POSIX_REGEX
			if(it->type == CONTEXT_TYPE_REGEX)
				regfree(it->object.rx);
#endif
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
	priv = new Fl_Highlight_Editor_P;
	priv->self = this;

	/* load interpreter */
	scheme *pscm = &(priv->scm);
	
	scheme_init(pscm);
	scheme_set_input_port_file(pscm, stdin);
	scheme_set_output_port_file(pscm, stdout);

	priv->script_path = strdup(script_folder);

	/* make *load-path* first */
	pointer ptr = pscm->vptr->cons(pscm, pscm->vptr->mk_string(pscm, script_folder), pscm->NIL);
	SCHEME_DEFINE_VAR(pscm, "*load-path*", ptr);

	SCHEME_DEFINE_VAR(pscm, "*editor-style-table*", pscm->NIL);
	SCHEME_DEFINE_VAR(pscm, "*editor-face-table*", pscm->NIL);

	/* assure prerequisites are loaded before main initialization file */
	init_scheme_prelude(pscm);

#if USE_BUNDLED_SCRIPTS
#   include "bundled_scripts.cxx"
	scheme_load_string(pscm, bundled_scripts_content);
#else
	char buf[PATH_MAX];
	FILE *fd;

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
#endif
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

static pointer load_style_table(Fl_Highlight_Editor_P *priv) {
	scheme *s = &(priv->scm);
	pointer tp, f, v, style_table = scheme_eval(s, s->vptr->mk_symbol(s, "*editor-style-table*"));
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

	return s->T;
}

#define VECTOR_GET_INT(scm, vec, pos, tmp, ret)	\
do {											\
	tmp = scm->vptr->vector_elem(vec, pos);		\
	if(!s->vptr->is_integer(tmp)) continue;		\
	ret = s->vptr->ivalue(tmp);					\
} while(0)

static pointer load_face_table(Fl_Highlight_Editor_P *priv) {
	scheme *s = &(priv->scm);
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

		VECTOR_GET_INT(s, v, 1, o, color);
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

	return s->T;
}

/* perform highlighting based on loaded context data */
static void hi_parse(ContextTable *ct, const char *text, char *style, int len) {
	if(!ct) return;

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
			}
		} else if(it->type == CONTEXT_TYPE_REGEX) {
#if USE_POSIX_REGEX
			/*
			 * Match against the whole text. However, how regexec() works, we continuously
			 * repeat matching to get offsets; we are not using grouping, so grouping submatches are ignored.
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
	memset(style, 'A', buf->length());
	style[buf->length()] = '\0';

	if(!priv->stylebuf)
		priv->stylebuf = new Fl_Text_Buffer(buf->length());

	hi_parse(priv->ctable, text, style, buf->length());

	priv->stylebuf->text(style);
	delete[] style;
	free(text);
}

static void hi_unfinished_cb(int, void*) {
	/* nothing done */
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

	/* this usually means we didn't initialize interpreter */
	if(!priv) return;

	/* in case we change the buffer, loaded faces and styles are still valid */
	if(!priv->loaded_styles_and_faces) {
		load_style_table(priv);
		load_face_table(priv);
		priv->loaded_styles_and_faces = true;
	}

	hi_init(priv, buf);

	/* setup initial style */
	highlight_data(priv->stylebuf, priv->styletable, priv->styletable_last, 'A',
				   hi_unfinished_cb, 0);

	buf->add_modify_callback(hi_update, priv);

#if ENABLE_STYLEBUF_DUMP	
	puts(priv->stylebuf->text());
#endif
}

int Fl_Highlight_Editor::handle(int e) {
	int ret = Fl_Text_Editor::handle(e);
	return ret;
}
