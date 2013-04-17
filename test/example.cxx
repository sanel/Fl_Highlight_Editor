#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Button.H>

#include "FL/Fl_Highlight_Editor.H"

static void close_cb(Fl_Widget *w, void *data) {
	Fl_Double_Window *win = (Fl_Double_Window*)data;
	win->hide();
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
		Fl_Button *bb = new Fl_Button(300, 365, 90, 25, "&Close");
		bb->callback(close_cb, win);
	win->end();
	win->show(argc, argv);
	int ret = Fl::run();
	delete editor;
	return ret;
}
