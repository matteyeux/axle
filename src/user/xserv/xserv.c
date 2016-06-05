#include "xserv.h"
#include <std/math.h>

//has the screen been modified this refresh?
static char dirtied = 0;

View* label_superview_int(Screen* screen, Label* label) {
	//traverse view heirarchy, finding the view that contains the label
	for (int i = 0; i < screen->window->subwindows.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subwindows));
		View* view = window->content_view;
		if (array_m_index(label, &(view->labels)) != -1) return view;
		for (int j = 0; j < view->subviews.size; j++) {
			View* subview = array_m_lookup(j, &(view->subviews));
			if (array_m_index(label, &(subview->labels)) != -1) return subview;
		}
	}
	return NULL;
}

View* image_superview_int(Screen* screen, Image* image) {
	//traverse view hierarchy, finding view which contains image
	for (int i = 0; i < screen->window->subwindows.size; i++) {
		Window* window = array_m_lookup(i, &(screen->window->subwindows));
		View* view = window->content_view;
		if (array_m_index(image, &(view->images)) != -1) return view;
		for (int j = 0; j < view->subviews.size; i++) {
			View* subview = array_m_lookup(j, &(view->subviews));
			if (array_m_index(image, &(subview->images)) != -1) return subview;
		}
	}
	return NULL;
}

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING 2

Rect convert_frame(View* view, Rect frame) {
	if (!view) return frame;

	Rect ret;
	ret.origin.x = frame.origin.x + view->frame.origin.x;
	ret.origin.y = frame.origin.y + view->frame.origin.y;
	ret.size.width = MIN(frame.size.width, view->frame.size.width);
	ret.size.height = MIN(frame.size.height, view->frame.size.height);
	return ret;
}

void draw_label(Screen* screen, Label* label) {
	View* superview = label_superview_int(screen, label);
	if (!label->needs_redraw && !superview->needs_redraw) return;

	dirtied = 1;

	Rect frame = convert_frame(superview, label->frame);

	int idx = 0;
	char* str = label->text;
	int x = frame.origin.x;
	int y = frame.origin.y;
	while (str[idx] != NULL) {
		//go to next line if necessary
		if ((x + CHAR_WIDTH + CHAR_PADDING) > (frame.origin.x + frame.size.width) || str[idx] == '\n') {
			x = frame.origin.x;

			//quit if going to next line would exceed view bounds
			if ((y + CHAR_WIDTH + CHAR_PADDING) > (frame.origin.y + frame.size.height)) break;

			y += CHAR_HEIGHT + CHAR_PADDING;
		}

		draw_char(screen, screen->font, str[idx], x, y, label->text_color);
		
		x += CHAR_WIDTH + CHAR_PADDING;

		idx++;
	}

	label->needs_redraw = 0;
}

void draw_image(Screen* screen, Image* image) {
	View* superview = image_superview_int(screen, image);
	if (!image->needs_redraw && !superview->needs_redraw) return;

	dirtied = 1;

	Rect frame = convert_frame(superview, image->frame);

	//iterate through every pixel in the bitmap and draw it
	int num_pixels = frame.size.width * frame.size.height;
	for (int i = 0; i < num_pixels; i++) {
		int x = frame.origin.x + (i % frame.size.width);
		int y = frame.origin.y + (i / frame.size.height);
		//putpixel(screen, x, y, image->bitmap[i]); 
	}

	image->needs_redraw = 0;
}

void draw_view(Screen* screen, View* view) {
	if (!view->needs_redraw && !view->superview->needs_redraw) return;

	dirtied = 1;

	Rect frame = convert_frame(view->superview, view->frame);
	//Rect frame = view->frame;

	//fill view with its background color
	draw_rect(screen, frame, view->background_color, THICKNESS_FILLED);

	//draw any labels this view has
	for (int i = 0; i < view->labels.size; i++) {
		Label* label = (Label*)array_m_lookup(i, &(view->labels));
		draw_label(screen, label);
	}

	//draw any images this view has
	for (int i = 0; i < view->images.size; i++) {
		Image* image = (Image*)array_m_lookup(i, &(view->images));
		draw_image(screen, image);
	}

	//draw each subview of this view
	for (int i = 0; i < view->subviews.size; i++) {
		View* subview = (View*)array_m_lookup(i, &(view->subviews));
		draw_view(screen, subview);
	}

	view->needs_redraw = 0;
}

void draw_window(Screen* screen, Window* window) {
	if (!window->needs_redraw && !window->content_view->needs_redraw && !window->title_view->needs_redraw			) return;

	dirtied = 1;

	//paint navy blue window
	draw_rect(screen, window->frame, window->border_color, 1);

	//only draw a title bar if title_view exists
	if (window->title_view) {
		draw_view(screen, window->title_view);
	}

	//put a small red square in top left corner of the window
	Size close_button_size = size_make(5, 5);
	Rect close_button = rect_make(window->frame.origin, close_button_size);
	draw_rect(screen, close_button, color_make(255, 0, 0), THICKNESS_FILLED);

	//only draw the content view if content_view exists
	if (window->content_view) {
		draw_view(screen, window->content_view);
	}

	window->needs_redraw = 0;
}

void add_taskbar(Screen* screen) {
	Size taskbar_size = size_make(screen->window->size.width, screen->window->size.height * 0.05);
	Coordinate taskbar_origin = point_make(0, screen->window->size.height - taskbar_size.height);
	Window* taskbar_window = create_window(rect_make(taskbar_origin, taskbar_size));
	add_subwindow(screen->window, taskbar_window);

	View* taskbar_view = create_view(rect_make(taskbar_origin, taskbar_size));
	taskbar_window->content_view = taskbar_view;
	taskbar_view->background_color = color_make(11, 136, 155);

	Coordinate name_label_origin = point_make(taskbar_view->frame.size.width * 0.925, taskbar_view->frame.origin.y + taskbar_view->frame.size.height / 2 - (CHAR_HEIGHT / 2));
	Rect label_rect = rect_make(name_label_origin, size_make(taskbar_size.width - name_label_origin.x, taskbar_size.height));
	Label* name_label = create_label(label_rect, "axle os");
	add_sublabel(taskbar_view, name_label);
}

void draw_desktop(Screen* screen) {
	/*
	Coordinate origin = point_make(0, 0);
	Size sz = size_make(screen->window->size.width, screen->window->size.height);
	Rect r = rect_make(origin, sz);
	draw_rect(screen, r, color_make(192, 192, 192), THICKNESS_FILLED);

	add_taskbar(screen);
	*/
	//paint every child window
	for (int i = 0; i < screen->window->subwindows.size; i++) {
		Window* win = (Window*)(array_m_lookup(i, &(screen->window->subwindows)));
		draw_window(screen, win);
	}
}

char xserv_draw(Screen* screen) {
	dirtied = 0;
	draw_desktop(screen);
	return dirtied;
}
