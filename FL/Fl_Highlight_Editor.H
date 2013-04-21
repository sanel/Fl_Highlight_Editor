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

#ifndef FL_HIGHLIGHT_EDITOR_H
#define FL_HIGHLIGHT_EDITOR_H

#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>

/** Set to 1 if you have or going to use POSIX regex as matching engine. */
#define USE_POSIX_REGEX 1

/** Set to 1 if you want assert() be used for internal checks. */
#define USE_ASSERT 1

struct Fl_Highlight_Editor_P;

/**
 * Class for higlighting text in extensible manner.
 */
class FL_EXPORT Fl_Highlight_Editor : public Fl_Text_Editor {
private:
	Fl_Highlight_Editor_P *priv;

public:
	/** Constructor; creates widget at given point with given dimensions. */
	Fl_Highlight_Editor(int X, int Y, int W, int H, const char *l = 0);

	/** Destructor. Cleans up things and unloads interpreter. */
	~Fl_Highlight_Editor();

	/**
	 * Initialize interpreter pointing to <i>script_folder</i>. This folder will be used as starting
	 * point where initial scripts will be searched for.
	 *
	 * If <i>do_repl</i> was set to true, interpreter will start to listen input from stdin; this is
	 * intended for testing purposes mainly or to construct command line tools.
	 */
	void init_interpreter(const char *script_folder, bool do_repl = false);

	/**
	 * Returns path for script folder. Unless init_interpreter() was called before, it will return NULL.
	 * Returned pointer points to internally allocated string and will be frieed when object was destroyed.
	 */
	const char *script_folder(void);

	/**
	 * Assign Fl_Text_Buffer object. This function behaves exactly the same as Fl_Text_Editor::buffer(), execept
	 * it will additionally initialize style buffer.
	 */
	void buffer(Fl_Text_Buffer *buf);

	/** Return currently set Fl_Text_Buffer object. */
	Fl_Text_Buffer *buffer(void) { return Fl_Text_Editor::buffer(); }

	/**
	 * Load file in buffer. If not Fl_Text_Buffer was provided (with <i>buffer()</i> member), it will be created
	 * and assigned.
	 *
	 * Before this function loads file, it will call interpreter's <b>*editor-before-loadfile-hook*</b> with filename
	 * as parameter. This hook is used to initialize highlight mode deduced from filename.
	 *
	 * After file was successfully loaded, it will call interpreter's <b>*editor-after-loadfile-hook*</b>.
	 *
	 * Returns 0 if succeded or non-zero on error. Inspect error with strerror().
	 */
	int loadfile(const char *file, int buflen = 128 * 1024);

	/** Overriden Fl_Text_Editor method for handling events. */
	int handle(int event);
};

#endif
