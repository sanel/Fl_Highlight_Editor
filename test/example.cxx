// this is starting comment

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Buffer.H>
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
	Fl_Text_Buffer *buf = (Fl_Text_Buffer*)data;
	const char *p = fl_file_chooser("Open file", "*", NULL, 1);
	if(!p) return;

	buf->loadfile(p);
}

int main(int argc, char **argv) {
	Fl_Double_Window *win = new Fl_Double_Window(400, 400, "Highlight test");
	win->begin();
		Fl_Box *rbox = new Fl_Box(25, 25, 60, 41);
		Fl_Group::current()->resizable(rbox);

		Fl_Text_Buffer *buf = new Fl_Text_Buffer();
		buf->loadfile("test/example.cxx");

		Fl_Highlight_Editor *editor = new Fl_Highlight_Editor(10, 10, 380, 350);
		editor->init_interpreter("./scheme");
		editor->buffer(buf);

		Fl_Button *openb = new Fl_Button(205, 365, 90, 25, "&Open");
		openb->callback(open_cb, buf);
		
		Fl_Button *closeb = new Fl_Button(300, 365, 90, 25, "&Close");
		closeb->callback(close_cb, win);
	win->end();
	win->show(argc, argv);
	int ret = Fl::run();
	delete editor;
	return ret;
}

// this is alread commented
// to EOF
