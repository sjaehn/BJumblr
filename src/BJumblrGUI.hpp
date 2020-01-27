/*  B.Jumblr
 * LV2 Plugin
 *
 * Copyright (C) 2018, 2019 by Sven Jähnichen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BJUMBLRGUI_HPP_
#define BJUMBLRGUI_HPP_

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/time/time.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <iostream>
#include <algorithm>

#include "BWidgets/Widget.hpp"
#include "BWidgets/Window.hpp"
#include "BWidgets/Label.hpp"
#include "BWidgets/DrawingSurface.hpp"
#include "BWidgets/DialValue.hpp"
#include "BWidgets/PopupListBox.hpp"
#include "BWidgets/LeftButton.hpp"
#include "BWidgets/PlusButton.hpp"
#include "BWidgets/MinusButton.hpp"
#include "screen.h"

#include "drawbutton.hpp"
#include "HaloButton.hpp"
#include "HaloToggleButton.hpp"
#include "HomeButton.hpp"
#include "PadSurface.hpp"
#include "definitions.h"
#include "Ports.hpp"
#include "Urids.hpp"
#include "Pad.hpp"
#include "PadMessage.hpp"
#include "Journal.hpp"

#define BG_FILE "inc/surface.png"
#define MAXUNDO 20

#define RESIZE(widget, x, y, w, h, sz) {widget.moveTo ((x) * (sz), (y) * (sz)); widget.resize ((w) * (sz), (h) * (sz));}

enum editIndex
{
	EDIT_CUT	= 0,
	EDIT_COPY	= 1,
	EDIT_XFLIP	= 2,
	EDIT_YFLIP	= 3,
	EDIT_PASTE	= 4,
	EDIT_RESET	= 5,
	EDIT_UNDO	= 6,
	EDIT_REDO	= 7,
	MAXEDIT		= 8
};

const std::string editLabels[MAXEDIT] = {"Select & cut", "Select & copy", "Select & X flip", "Select & Y flip", "Paste", "Reset", "Undo", "Redo"};

class BJumblrGUI : public BWidgets::Window
{
public:
	BJumblrGUI (const char *bundle_path, const LV2_Feature *const *features, PuglNativeWindow parentWindow);
	~BJumblrGUI ();
	void port_event (uint32_t port_index, uint32_t buffer_size, uint32_t format, const void *buffer);
	void send_ui_on ();
	void send_ui_off ();
	void send_pad (int row, int step);
	virtual void onConfigureRequest (BEvents::ExposeEvent* event) override;
	void applyTheme (BStyles::Theme& theme) override;

	LV2UI_Controller controller;
	LV2UI_Write_Function write_function;

private:
	static void valueChangedCallback(BEvents::Event* event);
	static void levelChangedCallback(BEvents::Event* event);
	static void edit1ChangedCallback(BEvents::Event* event);
	static void edit2ChangedCallback(BEvents::Event* event);
	static void padsPressedCallback (BEvents::Event* event);
	static void padsScrolledCallback (BEvents::Event* event);
	static void padsFocusedCallback (BEvents::Event* event);
	static void syncButtonClickedCallback(BEvents::Event* event);
	virtual void resize () override;
	bool validatePad ();
	bool validatePad (int row, int step, Pad& pad);
	void drawPad ();
	void drawPad (int row, int step);
	void drawPad (cairo_t* cr, int row, int step);

	std::string pluginPath;
	double sz;
	cairo_surface_t* bgImageSurface;

	BJumblrURIs uris;
	LV2_Atom_Forge forge;

	// Controllers
	std::array<BWidgets::ValueWidget*, MAXCONTROLLERS> controllerWidgets;
	std::array<float, MAXCONTROLLERS> controllers;
	int editMode;

	//Pads
	class Pattern
	{
	public:
		void clear ();
		Pad getPad (const size_t row, const size_t step) const;
		void setPad (const size_t row, const size_t step, const Pad& pad);
		std::vector<PadMessage> undo ();
		std::vector<PadMessage> redo ();
		void store ();
	private:
		Journal<std::vector<PadMessage>, MAXUNDO> journal;
		Pad pads [MAXSTEPS] [MAXSTEPS];
		struct
		{
			std::vector<PadMessage> oldMessage;
			std::vector<PadMessage> newMessage;
		} changes;
	};

	Pattern pattern;

	struct ClipBoard
	{
		std::vector<std::vector<Pad>> data;
		std::pair<int, int> origin;
		std::pair<int, int> extends;
		bool ready = true;
		std::chrono::steady_clock::time_point time;
	};

	ClipBoard clipBoard;

	// Cursor
	int cursor;
	bool wheelScrolled;
	bool padPressed;
	bool deleteMode;

	//Widgets
	BWidgets::Widget mContainer;
	BWidgets::Label messageLabel;
	PadSurface padSurface;
	HaloToggleButton playButton;
	HaloToggleButton bypassButton;
	HaloButton stopButton;
	std::array<HaloToggleButton, EDIT_RESET> edit1Buttons;
	std::array<HaloButton, MAXEDIT - EDIT_RESET> edit2Buttons;
	BWidgets::ValueWidget syncWidget;
	BWidgets::LeftButton zeroStepOffsetButton;
	BWidgets::MinusButton decStepOffsetButton;
	HomeButton hostSyncButton;
	BWidgets::PlusButton incStepOffsetButton;
	BWidgets::PopupListBox editModeListBox;
	BWidgets::PopupListBox stepSizeListBox;
	BWidgets::PopupListBox stepBaseListBox;
	BWidgets::PopupListBox padSizeListBox;
	std::array<HaloToggleButton, 5> levelButtons;
	BWidgets::DialValue levelDial;


	// Definition of styles
	BColors::ColorSet fgColors = {{{0.75, 0.75, 0.0, 1.0}, {1.0, 1.0, 0.25, 1.0}, {0.1, 0.1, 0.0, 1.0}, {0.0, 0.0, 0.0, 0.0}}};
	BColors::ColorSet txColors = {{{0.75, 0.75, 0.0, 1.0}, {1.0, 1.0, 0.0, 1.0}, {0.1, 0.1, 0.0, 1.0}, {0.0, 0.0, 0.0, 0.0}}};
	BColors::ColorSet tgColors = {{BColors::grey, BColors::white, BColors::grey, BColors::darkgrey}};
	BColors::ColorSet bgColors = {{{0.15, 0.15, 0.15, 1.0}, {0.3, 0.3, 0.3, 1.0}, {0.05, 0.05, 0.05, 1.0}, {0.0, 0.0, 0.0, 1.0}}};
	BColors::ColorSet tgBgColors = {{{0.0, 0.03, 0.06, 1.0}, {0.3, 0.3, 0.3, 1.0}, {0.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0, 1.0}}};
	BColors::ColorSet ltColors = {{{1.0, 1.0, 1.0, 1.0}, {1.0, 1.0, 1.0, 1.0}, {0.25, 0.25, 0.25, 1.0}, {0.0, 0.0, 0.0, 1.0}}};
	BColors::Color ink = {0.0, 0.25, 0.5, 1.0};
	BColors::Color light = {1.0, 1.0, 1.0, 1.0};
	BColors::Color evenPadBgColor = {0.0, 0.03, 0.06, 1.0};
	BColors::Color oddPadBgColor = {0.0, 0.0, 0.0, 1.0};

	BStyles::Border border = {{ink, 1.0}, 0.0, 2.0, 0.0};
	BStyles::Border menuBorder = {{BColors::darkgrey, 1.0}, 0.0, 0.0, 0.0};
	BStyles::Border labelborder = {BStyles::noLine, 4.0, 0.0, 0.0};
	BStyles::Border focusborder = BStyles::Border (BStyles::Line (BColors::Color (0.0, 0.0, 0.0, 0.5), 2.0));
	BStyles::Fill widgetBg = BStyles::noFill;
	BStyles::Fill menuBg = BStyles::Fill (BColors::Color (0.0, 0.0, 0.05, 1.0));
	BStyles::Fill screenBg = BStyles::Fill (BColors::Color (0.0, 0.0, 0.0, 0.8));
	BStyles::Fill boxBg = BStyles::Fill (BColors::Color (0.0, 0.0, 0.0, 0.9));
	BStyles::Font ctLabelFont = BStyles::Font ("Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL, 12.0,
						   BStyles::TEXT_ALIGN_CENTER, BStyles::TEXT_VALIGN_MIDDLE);
	BStyles::Font tgLabelFont = BStyles::Font ("Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL, 12.0,
						   BStyles::TEXT_ALIGN_CENTER, BStyles::TEXT_VALIGN_MIDDLE);
	BStyles::Font iLabelFont = BStyles::Font ("Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD, 18.0,
						  BStyles::TEXT_ALIGN_CENTER, BStyles::TEXT_VALIGN_MIDDLE);
	BStyles::Font lfLabelFont = BStyles::Font ("Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL, 12.0,
						   BStyles::TEXT_ALIGN_LEFT, BStyles::TEXT_VALIGN_MIDDLE);
	BStyles::StyleSet defaultStyles = {"default", {{"background", STYLEPTR (&BStyles::noFill)},
					  {"border", STYLEPTR (&BStyles::noBorder)}}};
	BStyles::StyleSet labelStyles = {"labels", {{"background", STYLEPTR (&BStyles::noFill)},
					{"border", STYLEPTR (&labelborder)},
					{"textcolors", STYLEPTR (&BColors::whites)},
					{"font", STYLEPTR (&ctLabelFont)}}};
	BStyles::StyleSet focusStyles = {"labels", {{"background", STYLEPTR (&screenBg)},
					{"border", STYLEPTR (&focusborder)},
					{"textcolors", STYLEPTR (&ltColors)},
					{"font", STYLEPTR (&lfLabelFont)}}};

	BStyles::Theme theme = BStyles::Theme
	({
		defaultStyles,
		{"B.Jumblr", 		{{"background", STYLEPTR (&BStyles::blackFill)},
					 {"border", STYLEPTR (&BStyles::noBorder)}}},
		{"main", 		{{"background", STYLEPTR (&widgetBg)},
					 {"border", STYLEPTR (&BStyles::noBorder)}}},
		{"widget", 		{{"uses", STYLEPTR (&defaultStyles)}}},
		{"widget/focus",	{{"uses", STYLEPTR (&focusStyles)}}},
		{"screen", 		{{"background", STYLEPTR (&screenBg)}}},
		{"box", 		{{"background", STYLEPTR (&boxBg)},
					{"border", STYLEPTR (&border)}}},
		{"box/focus",		{{"uses", STYLEPTR (&focusStyles)}}},
		{"button", 		{{"background", STYLEPTR (&BStyles::blackFill)},
					 {"border", STYLEPTR (&border)}}},
		{"tgbutton", 		{{"border", STYLEPTR (&BStyles::noBorder)},
					 {"textcolors", STYLEPTR (&tgColors)},
					 {"bgcolors", STYLEPTR (&tgBgColors)},
					 {"font", STYLEPTR (&tgLabelFont)}}},
		{"tgbutton/focus",	{{"uses", STYLEPTR (&focusStyles)}}},
		{"dial", 		{{"uses", STYLEPTR (&defaultStyles)},
					 {"fgcolors", STYLEPTR (&fgColors)},
					 {"bgcolors", STYLEPTR (&bgColors)},
					 {"textcolors", STYLEPTR (&fgColors)},
					 {"font", STYLEPTR (&ctLabelFont)}}},
		{"dial/focus", 		{{"uses", STYLEPTR (&focusStyles)}}},
		{"ctlabel",	 	{{"uses", STYLEPTR (&labelStyles)}}},
		{"lflabel",	 	{{"uses", STYLEPTR (&labelStyles)},
					 {"font", STYLEPTR (&lfLabelFont)}}},
		{"menu",	 	{{"border", STYLEPTR (&menuBorder)},
					 {"background", STYLEPTR (&menuBg)}}},
		{"menu/item",	 	{{"uses", STYLEPTR (&defaultStyles)},
					 {"border", STYLEPTR (&labelborder)},
					 {"textcolors", STYLEPTR (&BColors::whites)},
					 {"font", STYLEPTR (&lfLabelFont)}}},
		{"menu/button",	 	{{"border", STYLEPTR (&menuBorder)},
					 {"background", STYLEPTR (&menuBg)},
					 {"bgcolors", STYLEPTR (&bgColors)}}},
		{"menu/listbox",	{{"border", STYLEPTR (&menuBorder)},
					 {"background", STYLEPTR (&menuBg)}}},
		{"menu/listbox/item",	{{"uses", STYLEPTR (&defaultStyles)},
					 {"border", STYLEPTR (&labelborder)},
					 {"textcolors", STYLEPTR (&BColors::whites)},
					 {"font", STYLEPTR (&lfLabelFont)}}},
		{"menu/listbox//button",{{"border", STYLEPTR (&menuBorder)},
					 {"background", STYLEPTR (&menuBg)},
					 {"bgcolors", STYLEPTR (&bgColors)}}}
	});
};

#endif /* BJUMBLRGUI_HPP_ */
