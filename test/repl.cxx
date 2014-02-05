#include <FL/Fl_Highlight_Editor.H>

int main(int argc, char **argv) {
	Fl_Highlight_Editor editor(0, 0, 0, 0, 0);
	editor.init_interpreter("scheme", true);

	return 0;
}
