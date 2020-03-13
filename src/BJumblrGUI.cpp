/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
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

#include "BJumblrGUI.hpp"
#include "BUtilities/to_string.hpp"
#include "MessageDefinitions.hpp"

BJumblrGUI::BJumblrGUI (const char *bundle_path, const LV2_Feature *const *features, PuglNativeWindow parentWindow) :
	Window (1020, 620, "B.Jumblr", parentWindow, true),
	controller (NULL), write_function (NULL),
	pluginPath (bundle_path ? std::string (bundle_path) : std::string ("")),
	sz (1.0), bgImageSurface (nullptr),
	uris (), forge (), editMode (0), clipBoard (),
	cursor (0), wheelScrolled (false), padPressed (false), deleteMode (false),
	samplePath ("."),
	mContainer (0, 0, 1020, 620, "main"),
	messageLabel (400, 45, 600, 20, "ctlabel", ""),
	padSurface (18, 118, 924, 454, "box"),
	monitorWidget (20, 120, 920, 450, "monitor"),
	sourceListBox (60, 90, 120, 20, 120, 60, "menu", BItems::ItemList ({{0, "Audio stream"}, {1, "Sample"}}), 0),
	loadButton (200, 90, 20, 20, "menu/button"),
	sampleNameLabel (230, 90, 180, 20, "boxlabel", ""),
	playButton (18, 588, 24, 24, "widget", "Play"),
	bypassButton (48, 588, 24, 24, "widget", "Bypass"),
	stopButton (78, 588, 24, 24, "widget", "Stop"),
	syncWidget (400, 590, 95, 20, "widget", 0),
	zeroStepOffsetButton (0, 0, 20, 20, "menu/button"),
	decStepOffsetButton (25, 0, 20, 20, "menu/button"),
	hostSyncButton (50, 0, 20, 20, "menu/button"),
	incStepOffsetButton (75, 0, 20, 20, "menu/button"),
	editModeListBox (530, 590, 90, 20, 0, -60, 90, 60, "menu",
			 BItems::ItemList ({{0, "Add"}, {1, "Replace"}}), 0),
	stepSizeListBox (650, 590, 70, 20, 0, -280, 70, 280, "menu",
			 BItems::ItemList ({{0.0625, "1/16"}, {0.83333333, "1/12"}, {0.125, "1/8"}, {0.1666667, "1/6"},
			 		    {0.25, "1/4"}, {0.3333333, "1/3"}, {0.5, "1/2"}, {0.666666667, "2/3"}, {0.75, "3/4"},
					    {1, "1"}, {2, "2"}, {2, "3"}, {4, "4"}}), 1),
	stepBaseListBox (730, 590, 90, 20, 0, -80, 90, 80, "menu", BItems::ItemList ({{0, "Seconds"}, {1, "Beats"}, {2, "Bars"}}), 1),
	padSizeListBox (850, 590, 90, 20, 0, -240, 100, 240, "menu",
		     BItems::ItemList ({{2, "2 Steps"}, {3, "3 Steps"}, {4, "4 Steps"}, {6, "6 Steps"}, {8, "8 Steps"}, {9, "9 Steps"},
		     			{12, "12 Steps"}, {16, "16 Steps"}, {18, "18 Steps"}, {24, "24 Steps"}, {32, "32 Steps"}}), 16),
	levelDial (960, 290, 40, 48, "dial", 1.0, 0.0, 1.0, 0.01, "%1.2f"),
	delayDisplayLabel (958, 380, 44, 20, "smboxlabel", ""),
	manualProgressionDelayWidget (0, 0, 0, 0, "widget", 0.0, -32.0, 32.0, 0.0),
	resetDelayButton (958, 408, 44, 22, "widget", "Reset delay"),
	increaseDelayButton (958, 438, 44, 22, "widget", "Increase delay"),
	decreaseDelayButton (958, 468, 44, 22, "widget", "Decrease delay"),
	speedDial (960, 520, 40, 48, "dial", 1.0, 0.0, 4.0, 0.25, "%1.2f"),
	helpButton (958, 588, 24, 24, "widget", "Help"),
	ytButton (988, 588, 24, 24, "widget", "Video"),
	fileChooser (nullptr)
{
	// Init editButtons
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i] = HaloToggleButton (128 + i * 30, 588, 24, 24, "widget", editLabels[i]);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i] = HaloButton (298 + i * 30, 588, 24, 24, "widget", editLabels[i + EDIT_RESET]);

	for (int i = 0; i < 5; ++i) levelButtons[i] = HaloToggleButton (958, 138 + i * 30, 44, 22, "widget", BUtilities::to_string (1.0 - 0.25 * double(i), "%1.2f"));
	levelButtons[0].setValue (1.0);

	// Link controllerWidgets
	controllerWidgets[SOURCE] = (BWidgets::ValueWidget*) &sourceListBox;
	controllerWidgets[PLAY] = (BWidgets::ValueWidget*) &playButton;
	controllerWidgets[NR_OF_STEPS] = (BWidgets::ValueWidget*) &padSizeListBox;
	controllerWidgets[STEP_SIZE] = (BWidgets::ValueWidget*) &stepSizeListBox;
	controllerWidgets[STEP_BASE] = (BWidgets::ValueWidget*) &stepBaseListBox;
	controllerWidgets[STEP_OFFSET] = (BWidgets::ValueWidget*) &syncWidget;
	controllerWidgets[MANUAL_PROGRSSION_DELAY] = (BWidgets::ValueWidget*) &manualProgressionDelayWidget;
	controllerWidgets[SPEED] = (BWidgets::ValueWidget*) &speedDial;

	// Init controller values
	for (int i = 0; i < MAXCONTROLLERS; ++i) controllers[i] = controllerWidgets[i]->getValue ();

	// Set callback functions
	for (int i = 0; i < MAXCONTROLLERS; ++i) controllerWidgets[i]->setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	bypassButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	stopButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	syncWidget.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	zeroStepOffsetButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, syncButtonClickedCallback);
	decStepOffsetButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, syncButtonClickedCallback);
	hostSyncButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, syncButtonClickedCallback);
	incStepOffsetButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, syncButtonClickedCallback);
	editModeListBox.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i].setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, edit1ChangedCallback);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i].setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, edit2ChangedCallback);
	for (int i = 0; i < 5; ++i) levelButtons[i].setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, levelChangedCallback);
	levelDial.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, levelChangedCallback);
	resetDelayButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, delayButtonsClickedCallback);
	increaseDelayButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, delayButtonsClickedCallback);
	decreaseDelayButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, delayButtonsClickedCallback);
	helpButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, helpButtonClickedCallback);
	ytButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, ytButtonClickedCallback);
	loadButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, loadButtonClickedCallback);
	sampleNameLabel.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, loadButtonClickedCallback);

	padSurface.setDraggable (true);
	padSurface.setCallbackFunction (BEvents::BUTTON_PRESS_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::BUTTON_RELEASE_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::POINTER_DRAG_EVENT, padsPressedCallback);

	padSurface.setScrollable (true);
	padSurface.setCallbackFunction (BEvents::WHEEL_SCROLL_EVENT, padsScrolledCallback);

	padSurface.setFocusable (true);
	padSurface.setCallbackFunction (BEvents::FOCUS_IN_EVENT, padsFocusedCallback);
	padSurface.setCallbackFunction (BEvents::FOCUS_OUT_EVENT, padsFocusedCallback);

	// Configure widgets
	loadButton.hide();
	sampleNameLabel.hide();

	// Load background & apply theme
	bgImageSurface = cairo_image_surface_create_from_png ((pluginPath + BG_FILE).c_str());
	for (int i = 0; i < 5; ++i)
	{
		BColors::Color col = BColors::yellow;
		col.applyBrightness (- 0.25 * double(i));
		drawButton (bgImageSurface, 960, 140 + i * 30, 40, 18, col);
	}

	BColors::Color col = BColors::white;
	col.applyBrightness (- 0.25);
	drawButton (bgImageSurface, 960, 410, 40, 18, col, BUTTON_HOME_SYMBOL);
	drawButton (bgImageSurface, 960, 440, 40, 18, col, BUTTON_UP_SYMBOL);
	drawButton (bgImageSurface, 960, 470, 40, 18, col, BUTTON_DOWN_SYMBOL);

	widgetBg.loadFillFromCairoSurface (bgImageSurface);
	applyTheme (theme);

	// Pack widgets
	mContainer.add (messageLabel);
	mContainer.add (playButton);
	mContainer.add (bypassButton);
	mContainer.add (stopButton);
	for (int i = 0; i < EDIT_RESET; ++i) mContainer.add (edit1Buttons[i]);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) mContainer.add (edit2Buttons[i]);
	syncWidget.add (zeroStepOffsetButton);
	syncWidget.add (decStepOffsetButton);
	syncWidget.add (hostSyncButton);
	syncWidget.add (incStepOffsetButton);
	mContainer.add (syncWidget);
	mContainer.add (editModeListBox);
	mContainer.add (stepSizeListBox);
	mContainer.add (stepBaseListBox);
	mContainer.add (padSizeListBox);
	mContainer.add (padSurface);
	mContainer.add (monitorWidget);
	for (int i = 0; i < 5; ++i) mContainer.add (levelButtons[i]);
	mContainer.add (sourceListBox);
	mContainer.add (loadButton);
	mContainer.add (sampleNameLabel);
	mContainer.add (levelDial);
	mContainer.add (delayDisplayLabel);
	mContainer.add (manualProgressionDelayWidget);
	mContainer.add (resetDelayButton);
	mContainer.add (increaseDelayButton);
	mContainer.add (decreaseDelayButton);
	mContainer.add (speedDial);
	mContainer.add (helpButton);
	mContainer.add (ytButton);

	drawPad();
	add (mContainer);
	getKeyGrabStack()->add (this);

	pattern.clear ();

	//Scan host features for URID map
	LV2_URID_Map* map = NULL;
	for (int i = 0; features[i]; ++i)
	{
		if (strcmp(features[i]->URI, LV2_URID__map) == 0)
		{
			map = (LV2_URID_Map*) features[i]->data;
		}
	}
	if (!map) throw std::invalid_argument ("Host does not support urid:map");

	//Map URIS
	getURIs (map, &uris);

	// Initialize forge
	lv2_atom_forge_init (&forge, map);
}

BJumblrGUI::~BJumblrGUI ()
{
	if (fileChooser) delete fileChooser;
	send_ui_off ();
}

void BJumblrGUI::Pattern::clear ()
{
	Pad pad0 = Pad ();

	changes.oldMessage.clear ();
	changes.newMessage.clear ();
	journal.clear ();

	for (int r = 0; r < MAXSTEPS; ++r)
	{
		for (int s = 0; s < MAXSTEPS; ++s)
		{
			setPad (r, s, pad0);
		}
	}

	store ();
}

Pad BJumblrGUI::Pattern::getPad (const size_t row, const size_t step) const
{
	return pads[LIMIT (row, 0, MAXSTEPS)][LIMIT (step, 0, MAXSTEPS)];
}
void BJumblrGUI::Pattern::setPad (const size_t row, const size_t step, const Pad& pad)
{
	size_t r = LIMIT (row, 0, MAXSTEPS);
	size_t s = LIMIT (step, 0, MAXSTEPS);
	changes.oldMessage.push_back (PadMessage (s, r, pads[r][s]));
	changes.newMessage.push_back (PadMessage (s, r, pad));
	pads[r][s] = pad;
}

std::vector<PadMessage> BJumblrGUI::Pattern::undo ()
{
	if (!changes.newMessage.empty ()) store ();

	std::vector<PadMessage> padMessages = journal.undo ();
	std::reverse (padMessages.begin (), padMessages.end ());
	for (PadMessage const& p : padMessages)
	{
		size_t r = LIMIT (p.row, 0, MAXSTEPS);
		size_t s = LIMIT (p.step, 0, MAXSTEPS);
		pads[r][s] = Pad (p);
	}

	return padMessages;
}

std::vector<PadMessage> BJumblrGUI::Pattern::redo ()
{
	if (!changes.newMessage.empty ()) store ();

	std::vector<PadMessage> padMessages = journal.redo ();
	for (PadMessage const& p : padMessages)
	{
		size_t r = LIMIT (p.row, 0, MAXSTEPS);
		size_t s = LIMIT (p.step, 0, MAXSTEPS);
		pads[r][s] = Pad (p);
	}

	return padMessages;
}

void BJumblrGUI::Pattern::store ()
{
	if (changes.newMessage.empty ()) return;

	journal.push (changes.oldMessage, changes.newMessage);
	changes.oldMessage.clear ();
	changes.newMessage.clear ();
}

void BJumblrGUI::port_event(uint32_t port, uint32_t buffer_size,
	uint32_t format, const void* buffer)
{
	// Notify port
	if ((format == uris.atom_eventTransfer) && (port == NOTIFY))
	{
		const LV2_Atom* atom = (const LV2_Atom*) buffer;
		if ((atom->type == uris.atom_Blank) || (atom->type == uris.atom_Object))
		{
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*) atom;

			// Pad notification
			if (obj->body.otype == uris.notify_padEvent)
			{
				LV2_Atom *oEdit = NULL, *oPad = NULL;
				lv2_atom_object_get(obj,
						    uris.notify_editMode, &oEdit,
						    uris.notify_pad, &oPad,
						    NULL);

				if (oEdit && (oEdit->type == uris.atom_Int))
				{
					editModeListBox.setValue (((LV2_Atom_Int*)oEdit)->body);
				}

				if (oPad && (oPad->type == uris.atom_Vector))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oPad;
					if (vec->body.child_type == uris.atom_Float)
					{
						if (wheelScrolled)
						{
							pattern.store ();
							wheelScrolled = false;
						}

						uint32_t size = (uint32_t) ((oPad->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (PadMessage));
						PadMessage* pMes = (PadMessage*)(&vec->body + 1);
						for (unsigned int i = 0; i < size; ++i)

						{
							int step = (int) pMes[i].step;
							int row = (int) pMes[i].row;
							if ((step >= 0) && (step < MAXSTEPS) && (row >= 0) && (row < MAXSTEPS))
							{
								pattern.setPad (row, step, Pad (pMes[i]));
							}
						}
						pattern.store ();
						drawPad ();
					}
				}
			}

			// Message notification
			else if (obj->body.otype == uris.notify_messageEvent)
			{
				const LV2_Atom* data = NULL;
				lv2_atom_object_get(obj, uris.notify_message, &data, 0);
				if (data && (data->type == uris.atom_Int))
				{
					const int messageNr = ((LV2_Atom_Int*)data)->body;
					std::string msg = ((messageNr >= NO_MSG) && (messageNr < MAXMESSAGES) ? messageStrings[messageNr] : "");
					messageLabel.setText (msg);
				}
			}

			// Path notification
			else if (obj->body.otype == uris.notify_pathEvent)
			{
				const LV2_Atom* data = NULL;
				lv2_atom_object_get(obj, uris.notify_samplePath, &data, 0);
				if (data && (data->type == uris.atom_Path))
				{
					sampleNameLabel.setText ((const char*)LV2_ATOM_BODY_CONST(data));
					// TODO Split to path and file name
				}
			}

			// Status notifications
			else if (obj->body.otype == uris.notify_statusEvent)
			{
				LV2_Atom *oCursor = NULL, *oDelay = NULL;
				lv2_atom_object_get
				(
					obj,
					uris.notify_cursor, &oCursor,
					 uris.notify_progressionDelay, &oDelay,
					NULL
				);

				// Cursor notifications
				if (oCursor && (oCursor->type == uris.atom_Int) && (cursor != ((LV2_Atom_Int*)oCursor)->body))
				{
					cursor = ((LV2_Atom_Int*)oCursor)->body;
					drawPad ();
				}

				// Delay notifications
				if (oDelay && (oDelay->type == uris.atom_Float))
				{
					float delay = ((LV2_Atom_Float*)oDelay)->body;
					std::string delaytxt = BUtilities::to_string (delay, "%5.2f");
					if (delaytxt != delayDisplayLabel.getText()) delayDisplayLabel.setText (delaytxt);
				}
			}

			// Monitor notification
			if (obj->body.otype == uris.notify_waveformEvent)
			{
				int start = -1;

				const LV2_Atom *oStart = NULL, *oData = NULL;
				lv2_atom_object_get (obj,
						     uris.notify_waveformStart, &oStart,
						     uris.notify_waveformData, &oData,
						     NULL);
				if (oStart && (oStart->type == uris.atom_Int)) start = ((LV2_Atom_Int*)oStart)->body;

				if (oData && (oData->type == uris.atom_Vector))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oData;
					if (vec->body.child_type == uris.atom_Float)
					{
						uint32_t size = (uint32_t) ((oData->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (float));
						float* data = (float*) (&vec->body + 1);
						if ((start >= 0) && (size > 0))
						{
							monitorWidget.addData (start, size, data);
							monitorWidget.redrawRange (start, size);
						}
					}
				}
			}
		}
	}

	// Scan remaining ports
	else if ((format == 0) && (port >= CONTROLLERS))
	{
		float* pval = (float*) buffer;
		controllerWidgets[port - CONTROLLERS]->setValue (*pval);
	}

}

void BJumblrGUI::resize ()
{
	hide ();
	//Scale fonts
	ctLabelFont.setFontSize (12 * sz);
	tgLabelFont.setFontSize (12 * sz);
	lfLabelFont.setFontSize (12 * sz);
	smLabelFont.setFontSize (8 * sz);

	//Background
	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1020 * sz, 620 * sz);
	cairo_t* cr = cairo_create (surface);
	cairo_scale (cr, sz, sz);
	cairo_set_source_surface(cr, bgImageSurface, 0, 0);
	cairo_paint(cr);
	widgetBg.loadFillFromCairoSurface(surface);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	//Scale widgets
	RESIZE (mContainer, 0, 0, 1020, 620, sz);
	RESIZE (messageLabel, 400, 45, 600, 20, sz);
	RESIZE (padSurface, 18, 118, 924, 454, sz);
	RESIZE (monitorWidget, 20, 120, 920, 450, sz);
	RESIZE (sourceListBox, 60, 90, 120, 20, sz);
	sourceListBox.resizeListBox (BUtilities::Point (120 * sz, 60 * sz));
	sourceListBox.resizeListBoxItems (BUtilities::Point (120 * sz, 20 * sz));
	RESIZE (loadButton, 200, 90, 20, 20, sz);
	RESIZE (sampleNameLabel, 230, 90, 180, 20, sz);
	RESIZE (playButton, 18, 588, 24, 24, sz);
	RESIZE (bypassButton, 48, 588, 24, 24, sz);
	RESIZE (stopButton, 78, 588, 24, 24, sz);
	for (int i = 0; i < EDIT_RESET; ++i) RESIZE (edit1Buttons[i], 128 + i * 30, 588, 24, 24, sz);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) RESIZE (edit2Buttons[i], 298 + i * 30, 588, 24, 24, sz);
	RESIZE (syncWidget, 400, 590, 95, 20, sz);
	RESIZE (zeroStepOffsetButton, 0, 0, 20, 20, sz);
	RESIZE (decStepOffsetButton, 25, 0, 20, 20, sz);
	RESIZE (hostSyncButton, 50, 0, 20, 20, sz);
	RESIZE (incStepOffsetButton, 75, 0, 20, 20, sz);
	RESIZE (editModeListBox, 530, 590, 90, 20, sz);
	editModeListBox.resizeListBox(BUtilities::Point (90 * sz, 60 * sz));
	editModeListBox.moveListBox(BUtilities::Point (0, -60 * sz));
	editModeListBox.resizeListBoxItems(BUtilities::Point (90 * sz, 20 * sz));
	RESIZE (stepSizeListBox, 650, 590, 70, 20, sz);
	stepSizeListBox.resizeListBox(BUtilities::Point (70 * sz, 280 * sz));
	stepSizeListBox.moveListBox(BUtilities::Point (0, -280 * sz));
	stepSizeListBox.resizeListBoxItems(BUtilities::Point (70 * sz, 20 * sz));
	RESIZE (stepBaseListBox, 730, 590, 90, 20, sz);
	stepBaseListBox.resizeListBox(BUtilities::Point (90 * sz, 80 * sz));
	stepBaseListBox.moveListBox(BUtilities::Point (0, -80 * sz));
	stepBaseListBox.resizeListBoxItems(BUtilities::Point (90 * sz, 20 * sz));
	RESIZE (padSizeListBox, 850, 590, 90, 20, sz);
	padSizeListBox.resizeListBox(BUtilities::Point (90 * sz, 240 * sz));
	padSizeListBox.moveListBox(BUtilities::Point (0, -240 * sz));
	padSizeListBox.resizeListBoxItems(BUtilities::Point (90 * sz, 20 * sz));
	for (int i = 0; i < 5; ++i) RESIZE (levelButtons[i], 958, 138 + 30 * i, 44, 22, sz);
	RESIZE (levelDial, 960, 290, 40, 48, sz);
	RESIZE (delayDisplayLabel, 958, 380, 44, 20, sz);
	RESIZE (increaseDelayButton, 958, 408, 44, 22, sz);
	RESIZE (increaseDelayButton, 958, 438, 44, 22, sz);
	RESIZE (decreaseDelayButton, 958, 468, 44, 22, sz);
	RESIZE (speedDial, 960, 520, 40, 48, sz);
	RESIZE (helpButton, 958, 588, 24, 24, sz);
	RESIZE (ytButton, 988, 588, 24, 24, sz);
	if (fileChooser) RESIZE ((*fileChooser), 200, 120, 300, 400, sz);

	applyTheme (theme);
	drawPad ();
	show ();
}

void BJumblrGUI::applyTheme (BStyles::Theme& theme)
{
	mContainer.applyTheme (theme);
	messageLabel.applyTheme (theme);
	padSurface.applyTheme (theme);
	monitorWidget.applyTheme (theme);
	sourceListBox.applyTheme (theme);
	loadButton.applyTheme (theme);
	sampleNameLabel.applyTheme (theme);
	playButton.applyTheme (theme);
	bypassButton.applyTheme (theme);
	stopButton.applyTheme (theme);
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i].applyTheme (theme);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i].applyTheme (theme);
	syncWidget.applyTheme (theme);
	zeroStepOffsetButton.applyTheme (theme);
	decStepOffsetButton.applyTheme (theme);
	hostSyncButton.applyTheme (theme);
	incStepOffsetButton.applyTheme (theme);
	editModeListBox.applyTheme (theme);
	stepSizeListBox.applyTheme (theme);
	stepBaseListBox.applyTheme (theme);
	padSizeListBox.applyTheme (theme);
	for (int i = 0; i < 5; ++i) levelButtons[i].applyTheme (theme);
	levelDial.applyTheme (theme);
	delayDisplayLabel.applyTheme (theme);
	manualProgressionDelayWidget.applyTheme (theme);
	resetDelayButton.applyTheme (theme);
	increaseDelayButton.applyTheme (theme);
	decreaseDelayButton.applyTheme (theme);
	speedDial.applyTheme (theme);
	helpButton.applyTheme (theme);
	ytButton.applyTheme (theme);
	if (fileChooser) fileChooser->applyTheme (theme);
}

void BJumblrGUI::onConfigureRequest (BEvents::ExposeEvent* event)
{
	Window::onConfigureRequest (event);

	sz = (getWidth() / 1020 > getHeight() / 620 ? getHeight() / 620 : getWidth() / 1020);
	resize ();
}

void BJumblrGUI::onCloseRequest (BEvents::WidgetEvent* event)
{
	if (!event) return;
	Widget* requestWidget = event->getRequestWidget ();
	if (!requestWidget) return;

	if (requestWidget == fileChooser)
	{
		if (fileChooser->getValue() == 1.0)
		{
			sampleNameLabel.setText (fileChooser->getFileName());
			samplePath = fileChooser->getPath();
			send_samplePath ();
		}

		// Close fileChooser
		mContainer.release (fileChooser);	// TODO Check why this is required
		delete fileChooser;
		fileChooser = nullptr;
		return;
	}

	Window::onCloseRequest (event);
}

void BJumblrGUI::onKeyPressed (BEvents::KeyEvent* event)
{
	if ((event) && (event->getKey() == BDevices::KEY_SHIFT))
	{
		monitorWidget.setScrollable (true);
	}
}

void BJumblrGUI::onKeyReleased (BEvents::KeyEvent* event)
{
	if ((event) && (event->getKey() == BDevices::KEY_SHIFT))
	{
		monitorWidget.setScrollable (false);
	}
}

void BJumblrGUI::send_ui_on ()
{
	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.ui_on);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_ui_off ()
{
	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.ui_off);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_samplePath ()
{
	std::string path = samplePath + "/" + sampleNameLabel.getText();
	uint8_t obj_buf[1024];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_pathEvent);
	lv2_atom_forge_key(&forge, uris.notify_samplePath);
	lv2_atom_forge_path (&forge, path.c_str(), path.size() + 1);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_editMode ()
{
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_padEvent);
	lv2_atom_forge_key(&forge, uris.notify_editMode);
	lv2_atom_forge_int(&forge, editMode);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_pad (int row, int step)
{
	PadMessage padmsg (step, row, pattern.getPad (row, step));

	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_padEvent);
	lv2_atom_forge_key(&forge, uris.notify_editMode);
	lv2_atom_forge_int(&forge, editMode);
	lv2_atom_forge_key(&forge, uris.notify_pad);
	lv2_atom_forge_vector(&forge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float), (void*) &padmsg);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

bool BJumblrGUI::validatePad ()
{
	bool changed = false;

	// REPLACE mode
	if (editMode == 1)
	{
		for (int s = 0; s < MAXSTEPS; ++s)
		{
			bool padOn = false;
			for (int r = 0; r < MAXSTEPS; ++r)
			{
				// Clear if there is already an active pad
				if (padOn)
				{
					if (pattern.getPad (r, s).level != 0.0)
					{
						pattern.setPad (r, s, Pad(0));
						send_pad (r, s);
						changed = true;
					}
				}

				// First active pad
				else if (pattern.getPad (r, s).level != 0.0)
				{
					padOn = true;
				}
			}

			// Empty step: Set default
			if (!padOn)
			{
				pattern.setPad (s, s, Pad (1.0));
				send_pad (s, s);
				changed = true;
			}
		}
	}

	return (!changed);
}

bool BJumblrGUI::validatePad (int row, int step, Pad& pad)
{
	bool changed = false;

	// REPLACE mode
	if (editMode == 1)
	{
		if (pad.level != 0.0)
		{
			pattern.setPad (row, step, pad);
			send_pad (row, step);

			for (int r = 0; r < MAXSTEPS; ++r)
			{
				// Clear all other active pads
				if (r != row)
				{
					if (pattern.getPad (r, step).level != 0.0)
					{
						pattern.setPad (r, step, Pad(0));
						send_pad (r, step);
						changed = true;
					}
				}
			}
		}
	}

	else
	{
		pattern.setPad (row, step, pad);
		send_pad (row, step);
	}

	return (!changed);
}

void BJumblrGUI::valueChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	int controllerNr = -1;

	// Identify controller
	for (int i = 0; i < MAXCONTROLLERS; ++i)
	{
		if (widget == ui->controllerWidgets[i])
		{
			controllerNr = i;
			break;
		}
	}

	// Controllers
	if (controllerNr >= 0)
	{
		ui->controllers[controllerNr] = value;
		ui->write_function(ui->controller, CONTROLLERS + controllerNr, sizeof(float), 0, &ui->controllers[controllerNr]);

		switch (controllerNr)
		{
			case SOURCE:		if (value == 0.0)
						{
							ui->loadButton.hide();
							ui->sampleNameLabel.hide();
						}
						else
						{
							ui->loadButton.show();
							ui->sampleNameLabel.show();
						}
						break;

			case NR_OF_STEPS:	ui->drawPad ();
						break;

			case PLAY:		ui->bypassButton.setValue (value == 2.0 ? 1 : 0);
						break;

			default:		break;
		}
	}

	// Other widgets
	else if (widget == &ui->editModeListBox)
	{
		ui->editMode = ui->editModeListBox.getValue();
		ui->send_editMode();
		if (!ui->validatePad())
		{
			ui->drawPad();
			ui->pattern.store ();
		}
	}

	// Buttons
	else if (widget == &ui->bypassButton)
	{
		if ((value == 0.0) && (ui->playButton.getValue() == 2.0)) ui->playButton.setValue (0.0);
		else if (value == 1.0) ui->playButton.setValue (2.0);
	}

	else if (widget == &ui->stopButton)
	{
		if (value == 1.0)
		{
			ui->playButton.setValue (0.0);
			ui->bypassButton.setValue (0.0);
		}
	}
}

void BJumblrGUI::levelChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if (widget == &ui->levelDial)
	{
		for (int i = 0; i < 5; ++i)
		{
			if (value == 1.0 - double (i) * 0.25) ui->levelButtons[i].setValue (1.0);
			else ui->levelButtons[i].setValue (0.0);
		}
	}

	else
	{

		int buttonNr = -1;

		// Identify controller
		for (int i = 0; i < 5; ++i)
		{
			if (widget == &ui->levelButtons[i])
			{
				buttonNr = i;
				break;
			}
		}

		// Controllers
		if ((buttonNr >= 0) && (value == 1.0)) ui->levelDial.setValue (1.0 - double (buttonNr) * 0.25);
	}
}

void BJumblrGUI::edit1ChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	if (value != 1.0) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	// Identify editButtons: CUT ... PASTE
	int widgetNr = -1;
	for (int i = 0; i < EDIT_RESET; ++i)
	{
		if (widget == &ui->edit1Buttons[i])
		{
			widgetNr = i;
			break;
		}
	}

	// Untoggle all other edit1Buttons
	if (widgetNr >= 0)
	{
		// Allow only one button pressed
		for (int i = 0; i < EDIT_RESET; ++i)
		{
			if (i != widgetNr) ui->edit1Buttons[i].setValue (0.0);
		}
	}
}

void BJumblrGUI::edit2ChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	if (value != 1.0) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	// Identify editButtons: RESET ... REDO
	int widgetNr = -1;
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i)
	{
		if (widget == &ui->edit2Buttons[i])
		{
			widgetNr = i + EDIT_RESET;
			break;
		}
	}

	// RESET ... REDO
	switch (widgetNr)
	{
		case EDIT_RESET:
		{
			if (ui->wheelScrolled)
			{
				ui->pattern.store ();
				ui->wheelScrolled = false;
			}

			Pad p0 = Pad ();
			for (int r = 0; r < MAXSTEPS; ++r)
			{
				for (int s = 0; s < MAXSTEPS; ++s)
				{
					if (s == r) ui->pattern.setPad (r, s, Pad (1.0));
					else ui->pattern.setPad (r, s, p0);
					ui->send_pad (r, s);
				}
			}

			ui->drawPad ();
			ui->pattern.store ();
		}
		break;

		case EDIT_UNDO:
		{
			std::vector<PadMessage> padMessages = ui->pattern.undo ();
			for (PadMessage const& p : padMessages)
			{
				size_t r = LIMIT (p.row, 0, MAXSTEPS);
				size_t s = LIMIT (p.step, 0, MAXSTEPS);
				ui->send_pad (r, s);
			}
			ui->validatePad();
			ui->drawPad ();
		}
		break;

		case EDIT_REDO:
		{
			std::vector<PadMessage> padMessages = ui->pattern.redo ();
			for (PadMessage const& p : padMessages)
			{
				size_t r = LIMIT (p.row, 0, MAXSTEPS);
				size_t s = LIMIT (p.step, 0, MAXSTEPS);
				ui->send_pad (r, s);
			}
			ui->validatePad();
			ui->drawPad ();
		}
		break;

		default:	break;
	}
}

void BJumblrGUI::padsPressedCallback (BEvents::Event* event)
{
	if (!event) return;
	BEvents::PointerEvent* pointerEvent = (BEvents::PointerEvent*) event;
	BWidgets::DrawingSurface* widget = (BWidgets::DrawingSurface*) event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if
	(
		(event->getEventType () == BEvents::BUTTON_PRESS_EVENT) ||
		(event->getEventType () == BEvents::BUTTON_RELEASE_EVENT) ||
		(event->getEventType () == BEvents::POINTER_DRAG_EVENT)
	)
	{
		if (ui->wheelScrolled)
		{
			ui->pattern.store ();
			ui->wheelScrolled = false;
		}

		// Get size of drawing area
		const double width = ui->padSurface.getEffectiveWidth ();
		const double height = ui->padSurface.getEffectiveHeight ();

		int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
		int step = maxstep - 1 - int ((pointerEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
		int row = (pointerEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);

		if ((event->getEventType () == BEvents::BUTTON_PRESS_EVENT) || (event->getEventType () == BEvents::POINTER_DRAG_EVENT))
		{

			if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
			{
				Pad oldPad = ui->pattern.getPad (row, step);

				// Left button
				if (pointerEvent->getButton() == BDevices::LEFT_BUTTON)
				{
					// Check if edit mode
					int editNr = -1;
					for (int i = 0; i < EDIT_RESET; ++i)
					{
						if (ui->edit1Buttons[i].getValue() != 0.0)
						{
							editNr = i;
							break;
						}
					}

					// Edit
					if (editNr >= 0)
					{
						if ((editNr == EDIT_CUT) || (editNr == EDIT_COPY) || (editNr == EDIT_XFLIP) || (editNr == EDIT_YFLIP))
						{
							if (ui->clipBoard.ready)
							{
								ui->clipBoard.origin = std::make_pair (row, step);
								ui->clipBoard.extends = std::make_pair (0, 0);
								ui->clipBoard.ready = false;
								ui->drawPad (row, step);
							}

							else
							{
								std::pair<int, int> newExtends = std::make_pair (row - ui->clipBoard.origin.first, step - ui->clipBoard.origin.second);
								if (newExtends != ui->clipBoard.extends)
								{
									ui->clipBoard.extends = newExtends;
									ui->drawPad ();
								}
							}
						}

						else if (editNr == EDIT_PASTE)
						{
							if (!ui->clipBoard.data.empty ())
							{
								bool valid = true;
								for (int r = 0; r < int (ui->clipBoard.data.size ()); ++r)
								{
									for (int s = 0; s < int (ui->clipBoard.data[r].size ()); ++s)
									{
										if
										(
											(row + r >= 0) &&
											(row + r < maxstep) &&
											(step - s >= 0) &&
											(step - s < maxstep)
										)
										{
											if (!ui->validatePad (row + r, step - s, ui->clipBoard.data.at(r).at(s)))
											{
												valid = false;
											}
											else if (valid) ui->drawPad (row + r, step - s);
										}
									}
								}
								if (!valid) ui->drawPad();
							}
						}

					}

					// Set (or unset) pad
					else
					{
						if (!ui->padPressed) ui->deleteMode = ((oldPad.level == float (ui->levelDial.getValue())) && (ui->editMode != 1));
						Pad newPad = (ui->deleteMode ? Pad (0.0) : Pad (ui->levelDial.getValue()));
						if (!ui->validatePad (row, step, newPad)) ui->drawPad();
						else ui->drawPad (row,step);
					}

					ui->padPressed = true;
				}

				else if (pointerEvent->getButton() == BDevices::RIGHT_BUTTON)
				{
					ui->levelDial.setValue (ui->pattern.getPad (row, step).level);
				}
			}
		}

		else if ((event->getEventType () == BEvents::BUTTON_RELEASE_EVENT) && (pointerEvent->getButton() == BDevices::LEFT_BUTTON))
		{
			// Check if edit mode
			int editNr = -1;
			for (int i = 0; i < EDIT_RESET; ++i)
			{
				if (ui->edit1Buttons[i].getValue() != 0.0)
				{
					editNr = i;
					break;
				}
			}

			// Edit mode
			if (editNr >= 0)
			{
				if ((editNr == EDIT_CUT) || (editNr == EDIT_COPY) || (editNr == EDIT_XFLIP) || (editNr == EDIT_YFLIP))
				{
					int clipRMin = ui->clipBoard.origin.first;
					int clipRMax = ui->clipBoard.origin.first + ui->clipBoard.extends.first;
					if (clipRMin > clipRMax) std::swap (clipRMin, clipRMax);
					int clipSMin = ui->clipBoard.origin.second;
					int clipSMax = ui->clipBoard.origin.second + ui->clipBoard.extends.second;
					if (clipSMin > clipSMax) std::swap (clipSMin, clipSMax);

					// XFLIP
					// No need to validate
					if (editNr == EDIT_XFLIP)
					{
						for (int dr = 0; dr < int ((clipRMax + 1 - clipRMin) / 2); ++dr)
						{
							for (int s = clipSMin; s <= clipSMax; ++s)
							{
								Pad pd = ui->pattern.getPad (clipRMin + dr, s);
								ui->pattern.setPad (clipRMin + dr, s, ui->pattern.getPad (clipRMax -dr, s));
								ui->send_pad (clipRMin + dr, s);
								ui->pattern.setPad (clipRMax - dr, s, pd);
								ui->send_pad (clipRMax - dr, s);
							}
						}
						ui->pattern.store ();
					}

					// YFLIP
					// Validation required for REPLACE mode
					if (editNr == EDIT_YFLIP)
					{
						// Temp. copy selection
						Pad pads[clipRMax + 1 - clipRMin][clipSMax + 1 - clipSMin];
						for (int ds = 0; ds <= clipSMax - clipSMin; ++ds)
						{
							for (int dr = 0; dr <= clipRMax - clipRMin; ++dr)
							{
								pads[dr][ds] = ui->pattern.getPad (clipRMin + dr, clipSMin + ds);
							}
						}

						// X flip temp. copy & paste selection
						bool valid = true;
						for (int ds = 0; ds <= clipSMax - clipSMin; ++ds)
						{
							for (int dr = 0; dr <= clipRMax - clipRMin; ++dr)
							{
								if (!ui->validatePad (clipRMin + dr, clipSMax - ds, pads[dr][ds]))
								{
									valid = false;
								}
								else if (valid) ui->drawPad (clipRMin + dr, clipSMax - ds);
							}
						}
						if (!valid) ui->drawPad();
						ui->pattern.store ();
					}

					// Store selected data in clipboard after flip (XFLIP, YFLIP)
					// Or store selected data in clipboard before deletion (CUT)
					// Or store selected data anyway (COPY)
					ui->clipBoard.data.clear ();
					for (int r = clipRMin; r <= clipRMax; ++r)
					{
						std::vector<Pad> padRow;
						padRow.clear ();
						for (int s = clipSMax; s >= clipSMin; --s) padRow.push_back (ui->pattern.getPad (r, s));
						ui->clipBoard.data.push_back (padRow);
					}

					// CUT
					// Validation required for REPLACE mode
					if (editNr == EDIT_CUT)
					{
						for (int s = clipSMin; s <= clipSMax; ++s)
						{
							bool empty = false;

							for (int r = clipRMin; r <= clipRMax; ++r)
							{
								// Limit action to not empty pads
								if (ui->pattern.getPad (r, s) != Pad())
								{
									// REPLACE mode: delete everything except default pads
									if (ui->editMode == 1)
									{
										if (r != s)
										{
											ui->pattern.setPad (r, s,  Pad ());
											ui->send_pad (r, s);
											empty = true;
										}

										else empty = true;
									}

									// ADD mode: simply delete
									else
									{
										ui->pattern.setPad (r, s,  Pad ());
										ui->send_pad (r, s);
									}
								}
							}

							// Emptied column in REPLACE mode: set default
							if (empty)
							{
								ui->pattern.setPad (s, s,  Pad (1.0));
								ui->send_pad (s, s);
							}
						}
						ui->pattern.store ();
					}

					ui->clipBoard.ready = true;
					ui->drawPad ();
				}
			}

			else
			{
				ui->padPressed = false;
				ui->pattern.store ();
			}
		}
	}
}

void BJumblrGUI::padsScrolledCallback (BEvents::Event* event)
{
	if ((event) && (event->getWidget ()) && (event->getWidget()->getMainWindow()) &&
		((event->getEventType () == BEvents::WHEEL_SCROLL_EVENT)))
	{
		BWidgets::DrawingSurface* widget = (BWidgets::DrawingSurface*) event->getWidget ();
		BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
		BEvents::WheelEvent* wheelEvent = (BEvents::WheelEvent*) event;

		// Get size of drawing area
		const double width = ui->padSurface.getEffectiveWidth ();
		const double height = ui->padSurface.getEffectiveHeight ();

		int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
		int step = maxstep - 1 - int ((wheelEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
		int row = (wheelEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);

		if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
		{
			Pad pd = ui->pattern.getPad (row, step);
			pd.level = LIMIT (pd.level + 0.01 * wheelEvent->getDelta().y, 0.0, 1.0);
			if (!ui->validatePad (row, step, pd)) ui->drawPad();
			else ui->drawPad (row, step);
			ui->wheelScrolled = true;
		}
	}
}

void BJumblrGUI::padsFocusedCallback (BEvents::Event* event)
{
	if (!event) return;
	BEvents::FocusEvent* focusEvent = (BEvents::FocusEvent*) event;
	BWidgets::DrawingSurface* widget = (BWidgets::DrawingSurface*) event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	// Get size of drawing area
	const double width = ui->padSurface.getEffectiveWidth ();
	const double height = ui->padSurface.getEffectiveHeight ();

	int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
	int step = maxstep - 1 - int ((focusEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
	int row = (focusEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);

	if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
	{
		ui->padSurface.focusText.setText
		(
			"Row: " + std::to_string (row + 1) + "\n" +
			"Step: " + std::to_string (step + 1) + "\n" +
			"Level: " + BUtilities::to_string (ui->pattern.getPad (row, step).level, "%1.2f")
		);
	}
}

void BJumblrGUI::syncButtonClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	if (value != 1.0) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	int offset = 0;

	if (widget == &ui->zeroStepOffsetButton)
	{
		offset = (int (ui->controllers[NR_OF_STEPS] - ui->cursor + ui->controllers[STEP_OFFSET])) % int (ui->controllers[NR_OF_STEPS]);
	}

	else if (widget == &ui->decStepOffsetButton)
	{
		offset = (int (ui->controllers[STEP_OFFSET] + ui->controllers[NR_OF_STEPS]) - 1) % int (ui->controllers[NR_OF_STEPS]);
	}

	else if (widget == &ui->hostSyncButton) offset = 0;

	else if (widget == &ui->incStepOffsetButton)
	{
		offset = int (ui->controllers[STEP_OFFSET] + 1) % int (ui->controllers[NR_OF_STEPS]);
	}

	else return;

	ui->syncWidget.setValue (offset);
}

void BJumblrGUI::loadButtonClickedCallback (BEvents::Event* event)
{
	if (!event) return;
	BWidgets::Widget* widget = event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if (ui->fileChooser) delete ui->fileChooser;
	ui->fileChooser = new BWidgets::FileChooser
	(
		200, 120, 300, 400, "filechooser", ui->samplePath,
		std::vector<BWidgets::FileFilter>
		{
			BWidgets::FileFilter {"All files", std::regex (".*")},
			BWidgets::FileFilter {"Audio files", std::regex (".*\\.((wav)|(wave)|(aif)|(aiff)|(au)|(sd2)|(flac)|(caf)|(ogg))$", std::regex_constants::icase)}
		},
		"Open");
	if (ui->fileChooser)
	{
		RESIZE ((*ui->fileChooser), 200, 120, 300, 400, ui->sz);
		ui->mContainer.add (*ui->fileChooser);
	}
}

void BJumblrGUI::delayButtonsClickedCallback (BEvents::Event* event)
{
	if (!event) return;
	HaloButton* widget = (HaloButton*) event->getWidget ();
	if (!widget) return;
	double val = widget->getValue();
	if (!val) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if (widget == &ui->resetDelayButton) ui->manualProgressionDelayWidget.setValue (0.0);
	else if (widget == &ui->increaseDelayButton) ui->manualProgressionDelayWidget.setValue (ui->manualProgressionDelayWidget.getValue() + 1.0);
	else if (widget == &ui->decreaseDelayButton) ui->manualProgressionDelayWidget.setValue (ui->manualProgressionDelayWidget.getValue() - 1.0);
}

void BJumblrGUI::helpButtonClickedCallback (BEvents::Event* event) {system(OPEN_CMD " " HELP_URL);}
void BJumblrGUI::ytButtonClickedCallback (BEvents::Event* event) {system(OPEN_CMD " " YT_URL);}

void BJumblrGUI::drawPad ()
{
	cairo_surface_t* surface = padSurface.getDrawingSurface();
	cairo_t* cr = cairo_create (surface);
	int maxstep = controllerWidgets[NR_OF_STEPS]->getValue ();
	for (int row = 0; row < maxstep; ++row)
	{
		for (int step = 0; step < maxstep; ++step) drawPad (cr, row, step);
	}
	cairo_destroy (cr);
	padSurface.update();
}

void BJumblrGUI::drawPad (int row, int step)
{
	cairo_surface_t* surface = padSurface.getDrawingSurface();
	cairo_t* cr = cairo_create (surface);
	drawPad (cr, row, step);
	cairo_destroy (cr);
	padSurface.update();
}

void BJumblrGUI::drawPad (cairo_t* cr, int row, int step)
{
	int maxstep = controllerWidgets[NR_OF_STEPS]->getValue ();
	if ((!cr) || (cairo_status (cr) != CAIRO_STATUS_SUCCESS) || (row < 0) || (row >= maxstep) || (step < 0) ||
		(step >= maxstep)) return;

	// Get size of drawing area
	const double width = padSurface.getEffectiveWidth ();
	const double height = padSurface.getEffectiveHeight ();
	const double w = width / maxstep;
	const double h = height / maxstep;
	const double x = row * w;
	const double y = (maxstep - 1 - step) * h;
	const double xr = round (x);
	const double yr = round (y);
	const double wr = round (x + w) - xr;
	const double hr = round (y + h) - yr;

	// Draw background
	// Odd or even?
	BColors::Color bg = ((int (step / 4) % 2) ? oddPadBgColor : evenPadBgColor);

	// Highlight selection
	int clipRMin = clipBoard.origin.first;
	int clipRMax = clipBoard.origin.first + clipBoard.extends.first;
	if (clipRMin > clipRMax) std::swap (clipRMin, clipRMax);
	int clipSMin = clipBoard.origin.second;
	int clipSMax = clipBoard.origin.second + clipBoard.extends.second;
	if (clipSMin > clipSMax) std::swap (clipSMin, clipSMax);
	if ((!clipBoard.ready) && (row >= clipRMin) && (row <= clipRMax) && (step >= clipSMin) && (step <= clipSMax)) bg.applyBrightness (1.5);

	cairo_set_source_rgba (cr, CAIRO_RGBA (bg));
	cairo_set_line_width (cr, 0.0);
	cairo_rectangle (cr, xr, yr, wr, hr);
	cairo_fill (cr);

	// Draw pad
	Pad pd = pattern.getPad (row, step);
	BColors::Color color = BColors::yellow;
	color.applyBrightness (pd.level - 1.0);
	if (cursor == step) color.applyBrightness (0.75);
	drawButton (cr, xr + 1, yr + 1, wr - 2, hr - 2, color);
}

LV2UI_Handle instantiate (const LV2UI_Descriptor *descriptor,
						  const char *plugin_uri,
						  const char *bundle_path,
						  LV2UI_Write_Function write_function,
						  LV2UI_Controller controller,
						  LV2UI_Widget *widget,
						  const LV2_Feature *const *features)
{
	PuglNativeWindow parentWindow = 0;
	LV2UI_Resize* resize = NULL;

	if (strcmp(plugin_uri, BJUMBLR_URI) != 0)
	{
		std::cerr << "BJumblr.lv2#GUI: GUI does not support plugin with URI " << plugin_uri << std::endl;
		return NULL;
	}

	for (int i = 0; features[i]; ++i)
	{
		if (!strcmp(features[i]->URI, LV2_UI__parent)) parentWindow = (PuglNativeWindow) features[i]->data;
		else if (!strcmp(features[i]->URI, LV2_UI__resize)) resize = (LV2UI_Resize*)features[i]->data;
	}
	if (parentWindow == 0) std::cerr << "BJumblr.lv2#GUI: No parent window.\n";

	// New instance
	BJumblrGUI* ui;
	try {ui = new BJumblrGUI (bundle_path, features, parentWindow);}
	catch (std::exception& exc)
	{
		std::cerr << "BJumblr.lv2#GUI: Instantiation failed. " << exc.what () << std::endl;
		return NULL;
	}

	ui->controller = controller;
	ui->write_function = write_function;

	// Reduce min GUI size for small displays
	double sz = 1.0;
	int screenWidth  = getScreenWidth ();
	int screenHeight = getScreenHeight ();
	if ((screenWidth < 730) || (screenHeight < 460)) sz = 0.5;
	else if ((screenWidth < 1060) || (screenHeight < 660)) sz = 0.66;

	if (resize) resize->ui_resize(resize->handle, 1020 * sz, 620 * sz);

	*widget = (LV2UI_Widget) puglGetNativeWindow (ui->getPuglView ());

	ui->send_ui_on();

	return (LV2UI_Handle) ui;
}

void cleanup(LV2UI_Handle ui)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	delete self;
}

void port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size,
	uint32_t format, const void* buffer)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	self->port_event(port_index, buffer_size, format, buffer);
}

static int call_idle (LV2UI_Handle ui)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	self->handleEvents ();
	return 0;
}

static const LV2UI_Idle_Interface idle = { call_idle };

static const void* extension_data(const char* uri)
{
	if (!strcmp(uri, LV2_UI__idleInterface)) return &idle;
	else return NULL;
}

const LV2UI_Descriptor guiDescriptor = {
		BJUMBLR_GUI_URI,
		instantiate,
		cleanup,
		port_event,
		extension_data
};

// LV2 Symbol Export
LV2_SYMBOL_EXPORT const LV2UI_Descriptor *lv2ui_descriptor(uint32_t index)
{
	switch (index) {
	case 0: return &guiDescriptor;
	default:return NULL;
    }
}

/* End of LV2 specific declarations
 *
 * *****************************************************************************
 *
 *
 */
