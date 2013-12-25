// this is starting comment

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>

#include "FL/Fl_Highlight_Editor.H"

/* these are only markers for testing */
int XXX= 0;

static void close_cb(Fl_Widget *w, void *data) {
	Fl_Double_Window *win = (Fl_Double_Window*)data;
	win->hide();
}

static void open_cb(Fl_Widget *w, void *data) {
	Fl_Highlight_Editor *ed = (Fl_Highlight_Editor*)data;
	const char *p = fl_file_chooser("Open file", "*", NULL, 1);
	if(!p) return;

	ed->loadfile(p);
}

int main(int argc, char **argv) {
	if(argc != 2) {
		printf("Usage: %s [filename]\n", argv[0]);
		return 1;
	}

	Fl_Double_Window *win = new Fl_Double_Window(400, 400, "Highlight test");
	win->begin();
		Fl_Box *rbox = new Fl_Box(25, 25, 60, 41);
		Fl_Group::current()->resizable(rbox);

		Fl_Highlight_Editor *editor = new Fl_Highlight_Editor(10, 10, 380, 350);
		editor->init_interpreter("./scheme");
		editor->loadfile(argv[1]);

		Fl_Button *openb = new Fl_Button(205, 365, 90, 25, "&Open");
		openb->callback(open_cb, editor);
		
		Fl_Button *closeb = new Fl_Button(300, 365, 90, 25, "&Close");
		closeb->callback(close_cb, win);
	win->end();
	win->show();

	int ret = Fl::run();
	delete editor;
	return ret;
}

// this is alread commented
// to EOF
