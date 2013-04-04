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

struct Fl_Highlight_Editor_P {
	scheme scm;
	char   *script_path;

	Fl_Text_Buffer *stylebuf;

	Fl_Text_Display::Style_Table_Entry *styletable;
	int styletable_size;

	Fl_Highlight_Editor_P();
};

Fl_Highlight_Editor_P::Fl_Highlight_Editor_P() {
	script_path = NULL;
	stylebuf = NULL;
	styletable_size = 1;

	/* A - plain */
	styletable = new Fl_Text_Display::Style_Table_Entry[styletable_size];
	styletable[0].color = FL_BLACK;
	styletable[0].font = FL_COURIER;
	styletable[0].size = FL_NORMAL_SIZE;
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
	if(!priv) return;

	if(priv->script_path) free(priv->script_path);
	scheme_deinit(&(priv->scm));

	delete [] priv->styletable;
	delete priv->stylebuf;
	delete priv;
	priv = NULL;
}

void Fl_Highlight_Editor::init_interpreter(const char *script_folder, bool do_repl) {
	if(priv) return;

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
	pointer load_path = pscm->vptr->cons(pscm, pscm->vptr->mk_string(pscm, script_folder), pscm->NIL);
	SCHEME_DEFINE_VAR(pscm, "*load-path*", load_path);

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

	if(do_repl) scheme_load_named_file(pscm, stdin, 0);
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

static void style_unfinished_cb(int, void*) {}

static void style_update(int pos,
						 int ninserted,
						 int ndeleted,
						 int  /*nrestyled*/,
						 const char * /*deletedtext*/,
						 void *data)
{
	printf("inserted: %i deleted: %i pos: %i\n", ninserted, ndeleted, pos);
}

/*
 * initial paint; first load *editor-current-mode* from interpreter and try to get as much information
 * as possible, like comments, keywords and etc.
 */
static void style_init(Fl_Highlight_Editor_P *priv, Fl_Text_Buffer *buf) {
	char *style = new char[buf->length() + 1];

	/* setup default style */
	memset(style, 'A', buf->length());
	style[buf->length()] = '\0';

	/* get from interpreter details */
	scheme *s = &(priv->scm);
	pointer curmode = scheme_eval(s, s->vptr->mk_symbol(s, "*editor-current-mode*"));
}

void Fl_Highlight_Editor::buffer(Fl_Text_Buffer *buf) {
	/* prevent self assignment */
	if(Fl_Text_Display::buffer() == buf)
		return;

	Fl_Text_Display::buffer(buf);

	if(!priv)
		return;
	
	/* initialize style */
	style_init(priv, buf);
#if 0
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
