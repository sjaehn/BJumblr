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
#include "BUtilities/Path.hpp"
#include "BUtilities/vsystem.hpp"
#include "MessageDefinitions.hpp"
#include "MidiDefs.hpp"

inline double floorfrac (const double value) {return value - floor (value);}
inline double floormod (const double numer, const double denom) {return numer - floor(numer / denom) * denom;}

BJumblrGUI::BJumblrGUI (const char *bundle_path, const LV2_Feature *const *features, PuglNativeView parentWindow) :
	Window (1020, 620, "B.Jumblr", parentWindow, true, PUGL_MODULE, 0),
	controller (NULL), write_function (NULL),
	pluginPath (bundle_path ? std::string (bundle_path) : std::string ("")),
	sz (1.0), bgImageSurface (nullptr),
	uris (), forge (), editMode (0),
	patternFlipped (false),
	clipBoard (),
	cursor (0), wheelScrolled (false), padPressed (false), deleteMode (false),
	samplePath ("."), sampleStart (0), sampleEnd (0), sampleLoop (false),
	actPage (0), nrPages (1), pageOffset (0),
	mContainer (0, 0, 1020, 620, "main"),
	messageLabel (400, 45, 600, 20, "ctlabel", ""),
	pageWidget (18, 86, 504, 30, "widget", 0.0),
	pageBackSymbol (0, 0, 10, 30, "tab", LEFTSYMBOL),
	pageForwardSymbol (482, 0, 10, 30, "tab", RIGHTSYMBOL),

	sourceLabel (510, 95, 60, 10, "ylabel", BJUMBLR_LABEL_SOURCE),
	calibrationLabel (405, 577, 80, 10, "ylabel", BJUMBLR_LABEL_CALIBRATION),
	stepEditModeLabel (535, 577, 80, 10, "ylabel", BJUMBLR_LABEL_STEP_EDIT_MODE),
	stepSizeLabel (690, 577, 80, 10, "ylabel", BJUMBLR_LABEL_STEP_SIZE),
	patternSizeLabel (850, 577, 80, 10, "ylabel", BJUMBLR_LABEL_PATTERN_SIZE),
	padLevelLabel (950, 120, 60, 10, "ylabel", BJUMBLR_LABEL_PAD_LEVEL),
	playbackLabel (950, 350, 60, 10, "ylabel", BJUMBLR_LABEL_PLAYBACK),
	speedLabel (950, 515, 60, 10, "ylabel", BJUMBLR_LABEL_SPEED),

	midiBox (18, 118, 510, 120, "screen", 0),
	midiText (20, 10, 450, 20, "tlabel", BJUMBLR_LABEL_MIDI_PAGE " #1"),
	midiStatusLabel (10, 30, 130, 20, "ylabel", BJUMBLR_LABEL_MIDI_STATUS),
	midiStatusListBox
	(
		10, 50, 130, 20, 0, 20, 130, 100, "menu",
		BItems::ItemList ({{0, BJUMBLR_LABEL_NONE}, {9, BJUMBLR_LABEL_NOTE_ON}, {8, BJUMBLR_LABEL_NOTE_OFF}, {11, BJUMBLR_LABEL_CC}}),
		0
	),
	midiChannelLabel (150, 30, 50, 20, "ylabel", BJUMBLR_LABEL_CHANNEL),
	midiChannelListBox
	(
		150, 50, 50, 20, 0, 20, 50, 360, "menu",
		BItems::ItemList
		({
			{0, BJUMBLR_LABEL_ALL}, {1, "1"}, {2, "2"}, {3, "3"},
			{4, "4"}, {5, "5"}, {6, "6"}, {7, "7"},
			{8, "8"}, {9, "9"}, {10, "10"}, {11, "11"},
			{12, "12"}, {13, "13"}, {14, "14"}, {15, "15"}, {16, "16"}
		}),
		0
	),
	midiNoteLabel (210, 30, 160, 20, "ylabel", BJUMBLR_LABEL_NOTE),
	midiNoteListBox (210, 50, 160, 20, 0, 20, 160, 360, "menu", BItems::ItemList ({NOTELIST}), 128),
	midiValueLabel (380, 30, 50, 20, "ylabel", BJUMBLR_LABEL_VALUE),
	midiValueListBox (380, 50, 50, 20, 0, 20, 50, 360, "menu", BItems::ItemList ({VALLIST}), 128),
	midiLearnButton (440, 50, 60, 20, "menu/button", BJUMBLR_LABEL_LEARN),
	midiCancelButton (170, 90, 60, 20, "menu/button", BJUMBLR_LABEL_CANCEL),
	midiOkButton (280, 90, 60, 20, "menu/button", BJUMBLR_LABEL_OK),

	flipButton (940, 120, 10, 10, "widget"),
	padSurface (18, 128, 924, 434, "box"),
	markerFwd (0, 130 + 15.5 * (430.0 / 16.0) - 10, 20, 20, "widget", MARKER_RIGHT),
	markerRev (940, 130 + 15.5 * (430.0 / 16.0) - 10, 20, 20, "widget", MARKER_LEFT),
	monitorWidget (20, 130, 920, 430, "monitor"),
	sourceListBox (570, 90, 120, 20, 120, 60, "menu", BItems::ItemList ({{0, BJUMBLR_LABEL_AUDIO_STREAM}, {1, BJUMBLR_LABEL_SAMPLE}}), 0),
	loadButton (710, 90, 20, 20, "menu/button"),
	sampleNameLabel (740, 90, 160, 20, "boxlabel", ""),
	sampleAmpDial (918, 88, 24, 24, "dial", 1.0, 0.0, 1.0, 0.0),
	playButton (18, 588, 24, 24, "widget", BJUMBLR_LABEL_PLAY),
	bypassButton (48, 588, 24, 24, "widget", BJUMBLR_LABEL_BYPASS),
	stopButton (78, 588, 24, 24, "widget", BJUMBLR_LABEL_STOP),
	syncWidget (400, 590, 95, 20, "widget", 0),
	zeroStepOffsetButton (0, 0, 20, 20, "menu/button"),
	decStepOffsetButton (25, 0, 20, 20, "menu/button"),
	hostSyncButton (50, 0, 20, 20, "menu/button"),
	incStepOffsetButton (75, 0, 20, 20, "menu/button"),
	editModeListBox (530, 590, 90, 20, 0, -60, 90, 60, "menu",
			 BItems::ItemList ({{0, BJUMBLR_LABEL_ADD}, {1, BJUMBLR_LABEL_REPLACE}}), 0),
	stepSizeListBox (650, 590, 70, 20, 0, -280, 70, 280, "menu",
			 BItems::ItemList ({{0.0625, "1/16"}, {0.83333333, "1/12"}, {0.125, "1/8"}, {0.1666667, "1/6"},
			 		    {0.25, "1/4"}, {0.3333333, "1/3"}, {0.5, "1/2"}, {0.666666667, "2/3"}, {0.75, "3/4"},
					    {1, "1"}, {2, "2"}, {2, "3"}, {4, "4"}}), 1),
	stepBaseListBox (730, 590, 90, 20, 0, -80, 90, 80, "menu", BItems::ItemList ({{0, BJUMBLR_LABEL_SECONDS}, {1, BJUMBLR_LABEL_BEATS}, {2, BJUMBLR_LABEL_BARS}}), 1),
	padSizeListBox (850, 590, 90, 20, 0, -240, 100, 240, "menu",
		     BItems::ItemList ({{2, "2 " BJUMBLR_LABEL_STEPS}, {3, "3 " BJUMBLR_LABEL_STEPS}, {4, "4 " BJUMBLR_LABEL_STEPS},
		     			{6, "6 " BJUMBLR_LABEL_STEPS}, {8, "8 " BJUMBLR_LABEL_STEPS}, {9, "9 " BJUMBLR_LABEL_STEPS},
		     			{12, "12 " BJUMBLR_LABEL_STEPS}, {16, "16 " BJUMBLR_LABEL_STEPS}, {18, "18 " BJUMBLR_LABEL_STEPS},
					{24, "24 " BJUMBLR_LABEL_STEPS}, {32, "32 " BJUMBLR_LABEL_STEPS}}), 16),
	levelDial (960, 290, 40, 48, "dial", 1.0, 0.0, 1.0, 0.01, "%1.2f"),
	delayDisplayLabel (958, 370, 44, 20, "smboxlabel", ""),
	manualProgressionDelayWidget (0, 0, 0, 0, "widget", 0.0, -32.0, 32.0, 0.0),
	resetDelayButton (958, 398, 44, 22, "widget", BJUMBLR_LABEL_RESET_DELAY),
	increaseDelayButton (958, 428, 44, 22, "widget", BJUMBLR_LABEL_INCREASE_DELAY),
	decreaseDelayButton (958, 488, 44, 22, "widget", BJUMBLR_LABEL_DECREASE_DELAY),
	setStartDelayButton (958, 458, 44, 22, "widget", BJUMBLR_LABEL_DELAY_TO_START),
	speedDial (960, 530, 40, 48, "dial", 1.0, 0.0, 4.0, 0.25, "%1.2f"),
	helpButton (958, 588, 24, 24, "widget", BJUMBLR_LABEL_HELP),
	ytButton (988, 588, 24, 24, "widget", BJUMBLR_LABEL_TUTORIAL),
	fileChooser (nullptr)
{
	// Init tabs
	for (int i = 0; i < MAXPAGES; ++i)
	{
		tabs[i].container = BWidgets::Widget (i * 80, 0, 78, 30, "tab");
		tabs[i].icon = BWidgets::ImageIcon (0, 8, 40, 20, "widget", pluginPath + "inc/page" + std::to_string (i + 1) + ".png");
		tabs[i].playSymbol = SymbolWidget (40, 12, 20, 12, "symbol", PLAYSYMBOL);
		tabs[i].midiSymbol = SymbolWidget (60, 12, 20, 12, "symbol", MIDISYMBOL);
		for (int j = 0; j < 4; ++j) tabs[i].symbols[j] = SymbolWidget (68 - j * 10, 2, 8, 8, "symbol", SWSymbol(j));
		for (int j = 0; j < 4; ++j) tabs[i].midiWidgets[j] = BWidgets::ValueWidget (0, 0, 0, 0, "widget", 0);
	}

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
	controllerWidgets[PAGE] = (BWidgets::ValueWidget*) &pageWidget;
	for (int i = 0; i < MAXPAGES; ++i)
	{
		for (int j = 0; j < 4; ++j) controllerWidgets[MIDI + i * 4 + j] = (BWidgets::ValueWidget*) &tabs[i].midiWidgets[j];
	}

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
	setStartDelayButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, delayButtonsClickedCallback);
	helpButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, helpButtonClickedCallback);
	ytButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, ytButtonClickedCallback);
	loadButton.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, loadButtonClickedCallback);
	sampleNameLabel.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, loadButtonClickedCallback);
	sampleAmpDial.setCallbackFunction(BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);

	pageBackSymbol.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, pageScrollClickedCallback);
	pageForwardSymbol.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, pageScrollClickedCallback);

	for (Tab& t : tabs)
	{
		t.container.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, pageClickedCallback);
		t.playSymbol.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, pagePlayClickedCallback);
		t.midiSymbol.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, midiSymbolClickedCallback);
		for (SymbolWidget& s : t.symbols) s.setCallbackFunction(BEvents::BUTTON_PRESS_EVENT, pageSymbolClickedCallback);
	}

	midiLearnButton.setCallbackFunction(BEvents::VALUE_CHANGED_EVENT, midiButtonClickedCallback);
	midiCancelButton.setCallbackFunction(BEvents::VALUE_CHANGED_EVENT, midiButtonClickedCallback);
	midiOkButton.setCallbackFunction(BEvents::VALUE_CHANGED_EVENT, midiButtonClickedCallback);
	midiStatusListBox.setCallbackFunction(BEvents::VALUE_CHANGED_EVENT, midiStatusChangedCallback);

	padSurface.setDraggable (true);
	padSurface.setCallbackFunction (BEvents::BUTTON_PRESS_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::BUTTON_RELEASE_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::POINTER_DRAG_EVENT, padsPressedCallback);

	padSurface.setScrollable (true);
	padSurface.setCallbackFunction (BEvents::WHEEL_SCROLL_EVENT, padsScrolledCallback);

	padSurface.setFocusable (true);
	padSurface.setCallbackFunction (BEvents::FOCUS_IN_EVENT, padsFocusedCallback);
	padSurface.setCallbackFunction (BEvents::FOCUS_OUT_EVENT, padsFocusedCallback);

	flipButton.setClickable (true);
	flipButton.setCallbackFunction (BEvents::BUTTON_CLICK_EVENT, patternFlippedClickedCallback);

	// Configure widgets
	loadButton.hide();
	sampleNameLabel.hide();
	sampleAmpDial.hide();
	midiBox.hide();
	pageBackSymbol.setFocusable (false);
	pageForwardSymbol.setFocusable (false);
	pageBackSymbol.hide();
	pageForwardSymbol.hide();
	for (int i = 0; i < MAXPAGES; ++i)
	{
		tabs[i].playSymbol.setState (BColors::INACTIVE);
		tabs[i].midiSymbol.setState (BColors::INACTIVE);
		for (int j = 0; j < 4; ++j) tabs[i].symbols[j].setState (BColors::ACTIVE);
		if (i > 0) tabs[i].container.hide();
	}
	tabs[0].container.rename ("activetab");
	tabs[0].playSymbol.setState (BColors::ACTIVE);
	tabs[0].symbols[CLOSESYMBOL].hide(); // -
	tabs[0].symbols[LEFTSYMBOL].hide(); // <
	tabs[0].symbols[RIGHTSYMBOL].hide(); // >
	for (Tab& t : tabs) t.icon.setClickable (false);

	for (Pattern& p : pattern) p.clear();

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
	drawButton (bgImageSurface, 960, 400, 40, 18, col, BUTTON_HOME_SYMBOL);
	drawButton (bgImageSurface, 960, 430, 40, 18, col, BUTTON_UP_SYMBOL);
	drawButton (bgImageSurface, 960, 490, 40, 18, col, BUTTON_DOWN_SYMBOL);
	drawButton (bgImageSurface, 960, 460, 40, 18, col, BUTTON_BOTTOM_SYMBOL);

	widgetBg.loadFillFromCairoSurface (bgImageSurface);
	applyTheme (theme);

	// Pack widgets
	mContainer.add (sourceLabel);
	mContainer.add (calibrationLabel);
	mContainer.add (stepEditModeLabel);
	mContainer.add (stepSizeLabel);
	mContainer.add (patternSizeLabel);
	mContainer.add (padLevelLabel);
	mContainer.add (playbackLabel);
	mContainer.add (speedLabel);

	mContainer.add (messageLabel);
	mContainer.add (pageWidget);
	pageWidget.add (pageBackSymbol);
	pageWidget.add (pageForwardSymbol);
	for (int i = MAXPAGES - 1; i >= 0; --i)
	{
		Tab& t = tabs[i];
		t.container.add (t.icon);
		for (SymbolWidget& s : t.symbols) t.container.add (s);
		for (BWidgets::ValueWidget& m : t.midiWidgets) t.container.add (m);
		t.container.add (t.playSymbol);
		t.container.add (t.midiSymbol);
		pageWidget.add (t.container);
	}
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
	mContainer.add (flipButton);
	mContainer.add (markerFwd);
	mContainer.add (markerRev);
	mContainer.add (monitorWidget);
	for (int i = 0; i < 5; ++i) mContainer.add (levelButtons[i]);
	mContainer.add (sourceListBox);
	mContainer.add (loadButton);
	mContainer.add (sampleNameLabel);
	mContainer.add (sampleAmpDial);
	mContainer.add (levelDial);
	mContainer.add (delayDisplayLabel);
	mContainer.add (manualProgressionDelayWidget);
	mContainer.add (resetDelayButton);
	mContainer.add (increaseDelayButton);
	mContainer.add (decreaseDelayButton);
	mContainer.add (setStartDelayButton);
	mContainer.add (speedDial);
	mContainer.add (helpButton);
	mContainer.add (ytButton);

	mContainer.add (midiBox);
	midiBox.add (midiText);
	midiBox.add (midiStatusLabel);
	midiBox.add (midiStatusListBox);
	midiBox.add (midiChannelLabel);
	midiBox.add (midiChannelListBox);
	midiBox.add (midiNoteLabel);
	midiBox.add (midiNoteListBox);
	midiBox.add (midiValueLabel);
	midiBox.add (midiValueListBox);
	midiBox.add (midiLearnButton);
	midiBox.add (midiCancelButton);
	midiBox.add (midiOkButton);

	drawPad();
	add (mContainer);
	getKeyGrabStack()->add (this);

	for (Pattern& p : pattern) p.clear ();

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
				LV2_Atom *oEdit = NULL, *oPage = NULL, *oPad = NULL, *oFull = NULL;
				int page = -1;
				lv2_atom_object_get(obj,
						    uris.notify_editMode, &oEdit,
						    uris.notify_padPage, &oPage,
						    uris.notify_pad, &oPad,
						    uris.notify_padFullPattern, &oFull,
						    NULL);

				if (oEdit && (oEdit->type == uris.atom_Int))
				{
					editModeListBox.setValue (((LV2_Atom_Int*)oEdit)->body);
				}

				if (oPage && (oPage->type == uris.atom_Int))
				{
					page = (((LV2_Atom_Int*)oPage)->body);

					while (page >= nrPages) pushPage();
				}

				if (oPad && (oPad->type == uris.atom_Vector) && (page >= 0) && (page < MAXPAGES))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oPad;
					if (vec->body.child_type == uris.atom_Float)
					{
						if (wheelScrolled)
						{
							pattern[page].store ();
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
								pattern[page].setPad (row, step, Pad (pMes[i]));
							}
						}
						pattern[page].store ();
						if (page == actPage) drawPad ();
					}
				}

				if (oFull && (oFull->type == uris.atom_Vector) && (page >= 0) && (page < MAXPAGES))
				{
					const LV2_Atom_Vector* vec = (const LV2_Atom_Vector*) oFull;
					if (vec->body.child_type == uris.atom_Float)
					{
						if (wheelScrolled)
						{
							pattern[page].store ();
							wheelScrolled = false;
						}

						uint32_t size = (uint32_t) ((oFull->size - sizeof(LV2_Atom_Vector_Body)) / sizeof (Pad));
						Pad* data = (Pad*)(&vec->body + 1);

						if (size == MAXSTEPS * MAXSTEPS)
						{
							// Copy pattern data
							for (int r = 0; r < MAXSTEPS; ++r)
							{
								for (int s = 0; s < MAXSTEPS; ++s)
								{
									pattern[page].setPad (r, s, data[r * MAXSTEPS + s]);
								}
							}
						}

						pattern[page].store ();
						if (page == actPage) drawPad ();
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
				const LV2_Atom* oPath = NULL, *oStart = NULL, *oEnd = NULL, *oAmp = NULL, *oLoop = NULL;
				lv2_atom_object_get
				(
					obj,
					uris.notify_samplePath, &oPath,
					uris.notify_sampleStart, &oStart,
					uris.notify_sampleEnd, &oEnd,
					uris.notify_sampleAmp, &oAmp,
					uris.notify_sampleLoop, &oLoop,
					0
				);
				if (oPath && (oPath->type == uris.atom_Path))
				{
					const BUtilities::Path p = BUtilities::Path ((const char*)LV2_ATOM_BODY_CONST(oPath));
					samplePath = p.dir();
					sampleNameLabel.setText (p.filename());
				}

				if (oStart && (oStart->type == uris.atom_Long)) sampleStart = ((LV2_Atom_Long*)oStart)->body;
				if (oEnd && (oEnd->type == uris.atom_Long)) sampleEnd = ((LV2_Atom_Long*)oEnd)->body;
				if (oAmp && (oAmp->type == uris.atom_Float)) sampleAmpDial.setValue (((LV2_Atom_Float*)oAmp)->body);
				if (oLoop && (oLoop->type == uris.atom_Bool)) sampleLoop = bool (((LV2_Atom_Bool*)oLoop)->body);
			}

			// Status notifications
			else if (obj->body.otype == uris.notify_statusEvent)
			{
				LV2_Atom *oMax = NULL, *oSched = NULL, *oPlay = NULL, *oMid = NULL, *oCursor = NULL, *oDelay = NULL, *oFlip = NULL;
				lv2_atom_object_get
				(
					obj,
					uris.notify_maxPage, &oMax,
					uris.notify_schedulePage, &oSched,
					uris.notify_playbackPage, &oPlay,
					uris.notify_midiLearned, &oMid,
					uris.notify_cursor, &oCursor,
					uris.notify_progressionDelay, &oDelay,
					uris.notify_padFlipped, &oFlip,
					NULL
				);

				if (oMax && (oMax->type == uris.atom_Int))
				{
					int newPages = (((LV2_Atom_Int*)oMax)->body);

					while (newPages > nrPages) pushPage();
					while (newPages < nrPages) popPage();
				}

				if (oSched && (oSched->type == uris.atom_Int))
				{
					int newSched = (((LV2_Atom_Int*)oSched)->body);
					pageWidget.setValue (LIMIT (newSched, 0, MAXPAGES - 1));
				}

				if (oPlay && (oPlay->type == uris.atom_Int))
				{
					int newPlay = (((LV2_Atom_Int*)oPlay)->body);
					newPlay = LIMIT (newPlay, 0, MAXPAGES - 1);
					int val = pageWidget.getValue();
					for (int i = 0; i < MAXPAGES; ++i)
					{
						if (i == newPlay) tabs[i].playSymbol.setState (BColors::ACTIVE);
						else if (i != val) tabs[i].playSymbol.setState (BColors::INACTIVE);
					}
					drawPad();
				}

				if (oMid && (oMid->type == uris.atom_Int))
				{
					uint32_t newMid = (((LV2_Atom_Int*)oMid)->body);
					uint8_t st = newMid >> 24;
					uint8_t ch = (newMid >> 16) & 0xFF;
					uint8_t nt = (newMid >> 8) & 0xFF;
					uint8_t vl = newMid & 0xFF;
					midiStatusListBox.setValue ((st == 8) || (st == 9) || (st == 11) ? st : 0);
					midiChannelListBox.setValue (LIMIT (ch, 0, 15) + 1);
					midiNoteListBox.setValue (LIMIT (nt, 0, 127));
					midiValueListBox.setValue (LIMIT (vl, 0, 127));
					midiLearnButton.setValue (0.0);
				}

				// Cursor notifications
				if (oCursor && (oCursor->type == uris.atom_Float) && (cursor != ((LV2_Atom_Float*)oCursor)->body))
				{
					const int iOldCursor = cursor;
					cursor = ((LV2_Atom_Float*)oCursor)->body;
					if (int (cursor) != iOldCursor)
					{
						setMarkers();
						drawPad ();
					}
				}

				// Delay notifications
				if (oDelay && (oDelay->type == uris.atom_Float))
				{
					float delay = ((LV2_Atom_Float*)oDelay)->body;
					std::string delaytxt = BUtilities::to_string (delay, "%5.2f");
					if (delaytxt != delayDisplayLabel.getText()) delayDisplayLabel.setText (delaytxt);
				}

				// Flip notification
				if (oFlip && (oFlip->type == uris.atom_Bool))
				{
					const bool newFlip = ((LV2_Atom_Bool*)oFlip)->body;
					if (newFlip != patternFlipped)
					{
						patternFlipped = newFlip;
						monitorWidget.flip (patternFlipped);
						setMarkers();
						drawPad();
					}
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
	tLabelFont.setFontSize (12 * sz);
	tgLabelFont.setFontSize (12 * sz);
	lfLabelFont.setFontSize (12 * sz);
	boldLfLabelFont.setFontSize (12 * sz);
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
	RESIZE (pageWidget, 18, 86, 504, 30, sz);
	updatePageContainer();
	for (int i = 0; i < MAXPAGES; ++i)
	{
		RESIZE (tabs[i].icon, 0, 8, 40, 20, sz);
		RESIZE (tabs[i].playSymbol, 40, 12, 20, 12, sz);
		RESIZE (tabs[i].midiSymbol, 60, 12, 20, 12, sz);
		for (int j = 0; j < 4; ++j) RESIZE (tabs[i].symbols[j], 68 - j * 10, 2, 8, 8, sz);
	}

	RESIZE (sourceLabel, 510, 95, 60, 10, sz);
	RESIZE (calibrationLabel, 405, 577, 80, 10, sz);
	RESIZE (stepEditModeLabel ,535, 577, 80, 10, sz);
	RESIZE (stepSizeLabel, 690, 577, 80, 10, sz);
	RESIZE (patternSizeLabel, 850, 577, 80, 10, sz);
	RESIZE (padLevelLabel, 950, 120, 60, 10, sz);
	RESIZE (playbackLabel, 950, 350, 60, 10, sz);
	RESIZE (speedLabel, 950, 515, 60, 10, sz);

	RESIZE (midiBox, 290, 168, 510, 120, sz);
	RESIZE (midiText, 20, 10, 450, 20, sz);
	RESIZE (midiStatusLabel, 10, 30, 130, 20, sz);
	RESIZE (midiStatusListBox, 10, 50, 130, 20, sz);
	midiStatusListBox.resizeListBox(BUtilities::Point (130 * sz, 100 * sz));
	midiStatusListBox.moveListBox(BUtilities::Point (0, 20 * sz));
	midiStatusListBox.resizeListBoxItems(BUtilities::Point (130 * sz, 20 * sz));
	RESIZE (midiChannelLabel, 150, 30, 50, 20, sz);
	RESIZE (midiChannelListBox, 150, 50, 50, 20, sz);
	midiChannelListBox.resizeListBox(BUtilities::Point (50 * sz, 360 * sz));
	midiChannelListBox.moveListBox(BUtilities::Point (0, 20 * sz));
	midiChannelListBox.resizeListBoxItems(BUtilities::Point (50 * sz, 20 * sz));
	RESIZE (midiNoteLabel, 210, 30, 160, 20, sz);
	RESIZE (midiNoteListBox, 210, 50, 160, 20, sz);
	midiNoteListBox.resizeListBox(BUtilities::Point (160 * sz, 360 * sz));
	midiNoteListBox.moveListBox(BUtilities::Point (0, 20 * sz));
	midiNoteListBox.resizeListBoxItems(BUtilities::Point (160 * sz, 20 * sz));
	RESIZE (midiValueLabel, 380, 30, 50, 20, sz);
	RESIZE (midiValueListBox, 380, 50, 50, 20, sz);
	midiValueListBox.resizeListBox(BUtilities::Point (50 * sz, 360 * sz));
	midiValueListBox.moveListBox(BUtilities::Point (0, 20 * sz));
	midiValueListBox.resizeListBoxItems(BUtilities::Point (50 * sz, 20 * sz));
	RESIZE (midiLearnButton, 440, 50, 60, 20, sz);
	RESIZE (midiCancelButton, 170, 90, 60, 20, sz);
	RESIZE (midiOkButton, 280, 90, 60, 20, sz);

	RESIZE (flipButton, 940, 120, 10, 10, sz);
	RESIZE (padSurface, 18, 128, 924, 434, sz);
	setMarkers();
	RESIZE (monitorWidget, 20, 130, 920, 430, sz);
	RESIZE (sourceListBox, 570, 90, 120, 20, sz);
	sourceListBox.resizeListBox (BUtilities::Point (120 * sz, 60 * sz));
	sourceListBox.resizeListBoxItems (BUtilities::Point (120 * sz, 20 * sz));
	RESIZE (loadButton, 710, 90, 20, 20, sz);
	RESIZE (sampleNameLabel, 740, 90, 160, 20, sz);
	RESIZE (sampleAmpDial, 918, 88, 24, 24, sz);
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
	RESIZE (delayDisplayLabel, 958, 370, 44, 20, sz);
	RESIZE (increaseDelayButton, 958, 398, 44, 22, sz);
	RESIZE (increaseDelayButton, 958, 428, 44, 22, sz);
	RESIZE (decreaseDelayButton, 958, 488, 44, 22, sz);
	RESIZE (setStartDelayButton, 958, 458, 44, 22, sz);
	RESIZE (speedDial, 960, 530, 40, 48, sz);
	RESIZE (helpButton, 958, 588, 24, 24, sz);
	RESIZE (ytButton, 988, 588, 24, 24, sz);
	if (fileChooser) RESIZE ((*fileChooser), 200, 140, 640, 400, sz);

	applyTheme (theme);
	drawPad ();
	show ();
}

void BJumblrGUI::applyTheme (BStyles::Theme& theme)
{
	mContainer.applyTheme (theme);
	messageLabel.applyTheme (theme);
	pageWidget.applyTheme (theme);
	pageBackSymbol.applyTheme (theme);
	pageForwardSymbol.applyTheme (theme);
	for (Tab& t : tabs)
	{
		t.container.applyTheme (theme);
		t.icon.applyTheme (theme);
		t.playSymbol.applyTheme (theme);
		t.midiSymbol.applyTheme (theme);
		for (SymbolWidget& s : t.symbols) s.applyTheme (theme);
	}

	sourceLabel.applyTheme (theme);
	calibrationLabel.applyTheme (theme);
	stepEditModeLabel.applyTheme (theme);
	stepSizeLabel.applyTheme (theme);
	patternSizeLabel.applyTheme (theme);
	padLevelLabel.applyTheme (theme);
	playbackLabel.applyTheme (theme);
	speedLabel.applyTheme (theme);

	midiBox.applyTheme (theme);
	midiText.applyTheme (theme);
	midiStatusLabel.applyTheme (theme);
	midiStatusListBox.applyTheme (theme);
	midiChannelLabel.applyTheme (theme);
	midiChannelListBox.applyTheme (theme);
	midiNoteLabel.applyTheme (theme);
	midiNoteListBox.applyTheme (theme);
	midiValueLabel.applyTheme (theme);
	midiValueListBox.applyTheme (theme);
	midiLearnButton.applyTheme (theme);
	midiCancelButton.applyTheme (theme);
	midiOkButton.applyTheme (theme);

	flipButton.applyTheme (theme);
	padSurface.applyTheme (theme);
	markerFwd.applyTheme (theme);
	markerRev.applyTheme (theme);
	monitorWidget.applyTheme (theme);
	sourceListBox.applyTheme (theme);
	loadButton.applyTheme (theme);
	sampleNameLabel.applyTheme (theme);
	sampleAmpDial.applyTheme (theme);
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
	setStartDelayButton.applyTheme (theme);
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
			sampleStart = fileChooser->getStart();
			sampleEnd = fileChooser->getEnd();
			sampleLoop = fileChooser->getLoop();
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
	std::string path = samplePath + BUTILITIES_PATH_SLASH + sampleNameLabel.getText();
	uint8_t obj_buf[1024];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_pathEvent);
	lv2_atom_forge_key(&forge, uris.notify_samplePath);
	lv2_atom_forge_path (&forge, path.c_str(), path.size() + 1);
	lv2_atom_forge_key(&forge, uris.notify_sampleStart);
	lv2_atom_forge_long(&forge, sampleStart);
	lv2_atom_forge_key(&forge, uris.notify_sampleEnd);
	lv2_atom_forge_long(&forge, sampleEnd);
	lv2_atom_forge_key(&forge, uris.notify_sampleAmp);
	lv2_atom_forge_float(&forge, sampleAmpDial.getValue());
	lv2_atom_forge_key(&forge, uris.notify_sampleLoop);
	lv2_atom_forge_bool(&forge, int32_t (sampleLoop));
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_sampleAmp ()
{
	uint8_t obj_buf[1024];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_pathEvent);
	lv2_atom_forge_key(&forge, uris.notify_sampleAmp);
	lv2_atom_forge_float(&forge, sampleAmpDial.getValue());
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

void BJumblrGUI::send_maxPage ()
{
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&forge, uris.notify_maxPage);
	lv2_atom_forge_int(&forge, nrPages);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_playbackPage ()
{
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&forge, uris.notify_playbackPage);
	lv2_atom_forge_int(&forge, int (pageWidget.getValue()));
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_requestMidiLearn ()
{
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&forge, uris.notify_requestMidiLearn);
	lv2_atom_forge_bool(&forge, midiLearnButton.getValue() != 0.0);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_pad (int page)
{
	Pad padmsg [MAXSTEPS * MAXSTEPS];
	for (int r = 0; r < MAXSTEPS; ++r)
	{
		for (int s = 0; s < MAXSTEPS; ++s)
		{
			padmsg[r * MAXSTEPS + s] = pattern[page].getPad (r, s);
		}
	}

	uint8_t obj_buf[8192];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_padEvent);
	lv2_atom_forge_key(&forge, uris.notify_padPage);
	lv2_atom_forge_int(&forge, page);
	lv2_atom_forge_key(&forge, uris.notify_padFullPattern);
	lv2_atom_forge_vector(&forge, sizeof(float), uris.atom_Float, MAXSTEPS * MAXSTEPS * sizeof(Pad) / sizeof(float), (void*) padmsg);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_pad (int page, int row, int step)
{
	PadMessage padmsg (step, row, pattern[page].getPad (row, step));

	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_padEvent);
	lv2_atom_forge_key(&forge, uris.notify_padPage);
	lv2_atom_forge_int(&forge, page);
	lv2_atom_forge_key(&forge, uris.notify_pad);
	lv2_atom_forge_vector(&forge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float), (void*) &padmsg);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::send_flip ()
{
	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_statusEvent);
	lv2_atom_forge_key(&forge, uris.notify_padFlipped);
	lv2_atom_forge_bool(&forge, patternFlipped);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BJumblrGUI::pushPage ()
{
	if (nrPages >= MAXPAGES) return;

	tabs[nrPages - 1].symbols[CLOSESYMBOL].show();
	tabs[nrPages - 1].symbols[RIGHTSYMBOL].show();

	tabs[nrPages].container.show();
	tabs[nrPages].symbols[CLOSESYMBOL].show();
	tabs[nrPages].symbols[LEFTSYMBOL].show();
	tabs[nrPages].symbols[RIGHTSYMBOL].hide();

	if (nrPages == MAXPAGES - 1)
	{
		for (Tab& t : tabs) t.symbols[ADDSYMBOL].hide();
	}

	++nrPages;
	updatePageContainer();
}

void BJumblrGUI::popPage ()
{
	if (nrPages <= 1) return;

	tabs[nrPages - 2].symbols[RIGHTSYMBOL].hide();
	if (nrPages == 2) tabs[0].symbols[CLOSESYMBOL].hide();
	tabs[nrPages - 1].container.hide();
	for (Tab& t : tabs) t.symbols[ADDSYMBOL].show();

	if (actPage >= nrPages - 1) gotoPage (nrPages - 2);
	if (pageWidget.getValue() >= nrPages - 1) pageWidget.setValue (0);

	--nrPages;
	updatePageContainer();
}

void BJumblrGUI::gotoPage (const int page)
{
	if ((page < 0) || (page >= nrPages)) return;

	actPage = page;
	for (int i = 0; i < MAXPAGES; ++i)
	{
		if (i == page) tabs[i].container.rename ("activetab");
		else tabs[i].container.rename ("tab");
		tabs[i].container.applyTheme (theme);
	}
	drawPad();
	updatePageContainer();
}

void BJumblrGUI::insertPage (const int page)
{
	if ((page < 0) || (nrPages >= MAXPAGES)) return;

	pushPage();
	if (actPage >= page) gotoPage (actPage + 1);
	if (pageWidget.getValue() >= page) pageWidget.setValue (pageWidget.getValue() + 1);

	// Move pages
	for (int i = nrPages - 1; i > page; --i)
	{
		pattern[i] = pattern[i - 1];
		send_pad (i);
		if (i == actPage) drawPad();
		for (int j = 0; j < NR_MIDI_CTRLS; ++j)
		{
			tabs[i].midiWidgets[j].setValue (tabs[i - 1].midiWidgets[j].getValue());
		}
	}

	// Init new page
	pattern[page].clear();
	for (int i = 0; i < MAXSTEPS; ++i) pattern[page].setPad (i, i, Pad (1.0));
	send_pad (page);
	if (page == actPage) drawPad();
	tabs[page].midiWidgets[STATUS].setValue (0);
	tabs[page].midiWidgets[CHANNEL].setValue (0);
	tabs[page].midiWidgets[NOTE].setValue (128);
	tabs[page].midiWidgets[VALUE].setValue (128);
}

void BJumblrGUI::deletePage (const int page)
{
	if ((page < 0) || (page >= nrPages)) return;

	if (actPage > page) gotoPage (actPage - 1);
	if (pageWidget.getValue() > page) pageWidget.setValue (pageWidget.getValue() - 1);

	for (int i = page; i < nrPages -1; ++i)
	{
		pattern[i] = pattern[i + 1];
		send_pad (i);
		if (i == actPage) drawPad ();
		for (int j = 0; j < NR_MIDI_CTRLS; ++j)
		{
			tabs[i].midiWidgets[j].setValue (tabs[i + 1].midiWidgets[j].getValue());
		}
	}

	tabs[nrPages - 1].midiWidgets[STATUS].setValue (0);
	tabs[nrPages - 1].midiWidgets[CHANNEL].setValue (0);
	tabs[nrPages - 1].midiWidgets[NOTE].setValue (128);
	tabs[nrPages - 1].midiWidgets[VALUE].setValue (128);

	popPage();
	send_maxPage();
}

void BJumblrGUI::swapPage (const int page1, const int page2)
{
	if ((page1 < 0) || (page1 >= nrPages) || (page2 < 0) || (page2 >= nrPages)) return;

	Pattern p;
	p.clear();
	p = pattern[page1];
	pattern[page1] = pattern[page2];
	pattern[page2] = p;
	send_pad (page1);
	send_pad (page2);

	if (actPage == page1) gotoPage (page2);
	else if (actPage == page2) gotoPage (page1);

	if (pageWidget.getValue() == page1) pageWidget.setValue (page2);
	else if (pageWidget.getValue() == page2) pageWidget.setValue (page1);

	for (int j = 0; j < NR_MIDI_CTRLS; ++j)
	{
		double v = tabs[page1].midiWidgets[j].getValue();
		tabs[page1].midiWidgets[j].setValue (tabs[page2].midiWidgets[j].getValue());
		tabs[page2].midiWidgets[j].setValue (v);
	}
}

void BJumblrGUI::updatePageContainer()
{
	if (nrPages > 6) pageOffset = LIMIT (pageOffset, 0, nrPages - 6);
	else pageOffset = 0;

	int x0 = (pageOffset == 0 ? 0 : 12 * sz);

	if (pageOffset != 0) pageBackSymbol.show();
	else pageBackSymbol.hide();

	if (pageOffset + 6 < nrPages) pageForwardSymbol.show();
	else pageForwardSymbol.hide();

	for (int p = 0; p < nrPages; ++p)
	{
		if ((p < pageOffset) || (p >= pageOffset + 6)) tabs[p].container.hide();
		else
		{
			tabs[p].container.moveTo (x0 + (p - pageOffset) * 80 * sz, 0);
			tabs[p].container.resize (78 * sz, 30 * sz);
			tabs[p].container.show();
		}
	}

	for (int p = nrPages; p < MAXPAGES; ++p ) tabs[p].container.hide();

	pageBackSymbol.moveTo (0, 0);
	pageBackSymbol.resize (10 * sz, 30 * sz);
	pageForwardSymbol.moveTo (x0 + 480 * sz, 0);
	pageForwardSymbol.resize (10 * sz, 30 * sz);
}

bool BJumblrGUI::validatePad (int page)
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
					if (pattern[page].getPad (r, s).level != 0.0)
					{
						pattern[page].setPad (r, s, Pad(0));
						send_pad (page, r, s);
						changed = true;
					}
				}

				// First active pad
				else if (pattern[page].getPad (r, s).level != 0.0)
				{
					padOn = true;
				}
			}

			// Empty step: Set default
			if (!padOn)
			{
				pattern[page].setPad (s, s, Pad (1.0));
				send_pad (page, s, s);
				changed = true;
			}
		}
	}

	return (!changed);
}

bool BJumblrGUI::validatePad (int page, int row, int step, Pad& pad)
{
	bool changed = false;

	// REPLACE mode
	if (editMode == 1)
	{
		if (pad.level != 0.0)
		{
			pattern[page].setPad (row, step, pad);
			send_pad (page, row, step);

			for (int r = 0; r < MAXSTEPS; ++r)
			{
				// Clear all other active pads
				if (r != row)
				{
					if (pattern[page].getPad (r, step).level != 0.0)
					{
						pattern[page].setPad (r, step, Pad(0));
						send_pad (page, r, step);
						changed = true;
					}
				}
			}
		}
	}

	else
	{
		pattern[page].setPad (row, step, pad);
		send_pad (page, row, step);
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
							ui->sampleAmpDial.hide();
						}
						else
						{
							ui->loadButton.show();
							ui->sampleNameLabel.show();
							ui->sampleAmpDial.show();
						}
						break;

			case NR_OF_STEPS:	ui->drawPad ();
						break;

			case PLAY:		ui->bypassButton.setValue (value == 2.0 ? 1 : 0);
						break;

			case PAGE:		for (int i = 0; i < MAXPAGES; ++i)
						{
							if (ui->tabs[i].playSymbol.getState() != BColors::ACTIVE)
							{
								if (i == value) ui->tabs[i].playSymbol.setState (BColors::NORMAL);
								else ui->tabs[i].playSymbol.setState (BColors::INACTIVE);
							}
						}
						ui->drawPad();
						break;

			default:		if (controllerNr >= MIDI)
						{
							int page = (controllerNr - MIDI) / NR_MIDI_CTRLS;
							int midiCtrl = (controllerNr - MIDI) % NR_MIDI_CTRLS;

							if (midiCtrl == STATUS)
							{
								if (value < 8) ui->tabs[page].midiSymbol.setState (BColors::INACTIVE);
								else ui->tabs[page].midiSymbol.setState (BColors::ACTIVE);
							}
						}
						break;
		}
	}

	// Other widgets
	else if (widget == &ui->editModeListBox)
	{
		ui->editMode = ui->editModeListBox.getValue();
		ui->send_editMode();
		for (int i = 0; i < ui->nrPages; ++i)
		{
			if (!ui->validatePad(i))
			{
				if (i == ui->actPage) ui->drawPad();
				ui->pattern[i].store ();
			}
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

	else if (widget == &ui->sampleAmpDial) ui->send_sampleAmp();
}


void BJumblrGUI::pageClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::Widget* widget = event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	for (int i = 0; i < ui->nrPages; ++i)
	{
		if (&ui->tabs[i].container == widget)
		{
			ui->gotoPage (i);
			break;
		}
	}
}

void BJumblrGUI::pageSymbolClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	SymbolWidget* widget = (SymbolWidget*)event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	for (int i = 0; i < ui->nrPages; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if (&ui->tabs[i].symbols[j] == widget)
			{
				switch (j)
				{
					// Symbol +
					case ADDSYMBOL:		ui->insertPage (i + 1);
								break;

					// Symbol -
					case CLOSESYMBOL: 	ui->deletePage (i);
								break;

					// Symbol <
					case LEFTSYMBOL:	ui->swapPage (i, i - 1);
								break;

					// Symbol >
					case RIGHTSYMBOL:	ui->swapPage (i, i + 1);
								break;
				}
				return;
			}
		}
	}
}

void BJumblrGUI::pagePlayClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	SymbolWidget* widget = (SymbolWidget*)event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	for (int i = 0; i < ui->nrPages; ++i)
	{
		if (&ui->tabs[i].playSymbol == widget)
		{
			ui->pageWidget.setValue (i);
			break;
		}
	}
}

void BJumblrGUI::pageScrollClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	SymbolWidget* widget = (SymbolWidget*)event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if (widget == &ui->pageBackSymbol) --ui->pageOffset;
	else if (widget == &ui->pageForwardSymbol) ++ui->pageOffset;

	ui->updatePageContainer();
}

void BJumblrGUI::midiSymbolClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	SymbolWidget* widget = (SymbolWidget*)event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	for (int i = 0; i < ui->nrPages; ++i)
	{
		if (widget == &ui->tabs[i].midiSymbol)
		{
			ui->midiText.setText (BJUMBLR_LABEL_MIDI_PAGE " #" + std::to_string (i + 1));
			ui->midiStatusListBox.setValue (ui->controllers[MIDI + i * NR_MIDI_CTRLS + STATUS]);
			ui->midiChannelListBox.setValue (ui->controllers[MIDI + i * NR_MIDI_CTRLS + CHANNEL]);
			ui->midiNoteListBox.setValue (ui->controllers[MIDI + i * NR_MIDI_CTRLS + NOTE]);
			ui->midiValueListBox.setValue (ui->controllers[MIDI + i * NR_MIDI_CTRLS + VALUE]);
			ui->midiBox.setValue (i);
			ui->midiBox.show();
			return;
		}
	}
}

void BJumblrGUI::midiButtonClickedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	if (widget == &ui->midiLearnButton)
	{
		if (value == 1) ui->send_requestMidiLearn();
	}

	else if (widget == &ui->midiCancelButton)
	{
		if (value == 1)
		{
			ui->midiLearnButton.setValue (0);
			ui->midiBox.hide();
		}
	}

	else if (widget == &ui->midiOkButton)
	{
		if (value == 1)
		{
			int page = ui->midiBox.getValue();
			ui->midiLearnButton.setValue (0);
			ui->tabs[page].midiWidgets[STATUS].setValue (ui->midiStatusListBox.getValue());
			ui->tabs[page].midiWidgets[CHANNEL].setValue (ui->midiChannelListBox.getValue());
			ui->tabs[page].midiWidgets[NOTE].setValue (ui->midiNoteListBox.getValue());
			ui->tabs[page].midiWidgets[VALUE].setValue (ui->midiValueListBox.getValue());
			ui->midiBox.hide();
		}
	}
}

void BJumblrGUI::midiStatusChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::PopupListBox* widget = (BWidgets::PopupListBox*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;

	BWidgets::PopupListBox& nlb = ui->midiNoteListBox;
	BWidgets::Label& nl = ui->midiNoteLabel;
	int nr = nlb.getValue();

	if (value == 11)
	{
		nlb = BWidgets::PopupListBox
		(
			210 * ui->sz, 50 * ui->sz, 160 * ui->sz, 20 * ui->sz, 0, 20 * ui->sz, 160 * ui->sz, 360 *ui->sz,
			"menu",
			BItems::ItemList ({CCLIST}),
			0
		);
		nl.setText (BJUMBLR_LABEL_CC);
	}

	else
	{
		nlb = BWidgets::PopupListBox
		(
			210 * ui->sz, 50 * ui->sz, 160 * ui->sz, 20 * ui->sz, 0, 20 * ui->sz, 160 * ui->sz, 360 * ui->sz,
			"menu",
			BItems::ItemList ({NOTELIST}),
			0
		);
		nl.setText (BJUMBLR_LABEL_NOTE);
	}

	nlb.resizeListBoxItems(BUtilities::Point (160 * ui->sz, 20 * ui->sz));
	nlb.applyTheme (ui->theme);
	nlb.setValue (nr);
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

	int page = ui->actPage;

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
				ui->pattern[page].store ();
				ui->wheelScrolled = false;
			}

			Pad p0 = Pad ();
			for (int r = 0; r < MAXSTEPS; ++r)
			{
				for (int s = 0; s < MAXSTEPS; ++s)
				{
					if (s == r) ui->pattern[page].setPad (r, s, Pad (1.0));
					else ui->pattern[page].setPad (r, s, p0);
					ui->send_pad (page, r, s);
				}
			}

			ui->drawPad ();
			ui->pattern[page].store ();
		}
		break;

		case EDIT_UNDO:
		{
			std::vector<PadMessage> padMessages = ui->pattern[page].undo ();
			for (PadMessage const& p : padMessages)
			{
				size_t r = LIMIT (p.row, 0, MAXSTEPS);
				size_t s = LIMIT (p.step, 0, MAXSTEPS);
				ui->send_pad (page, r, s);
			}
			ui->validatePad (page);
			ui->drawPad ();
		}
		break;

		case EDIT_REDO:
		{
			std::vector<PadMessage> padMessages = ui->pattern[page].redo ();
			for (PadMessage const& p : padMessages)
			{
				size_t r = LIMIT (p.row, 0, MAXSTEPS);
				size_t s = LIMIT (p.step, 0, MAXSTEPS);
				ui->send_pad (page, r, s);
			}
			ui->validatePad (page);
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

	int page = ui->actPage;

	if
	(
		(event->getEventType () == BEvents::BUTTON_PRESS_EVENT) ||
		(event->getEventType () == BEvents::BUTTON_RELEASE_EVENT) ||
		(event->getEventType () == BEvents::POINTER_DRAG_EVENT)
	)
	{
		if (ui->wheelScrolled)
		{
			ui->pattern[page].store ();
			ui->wheelScrolled = false;
		}

		// Get size of drawing area
		const double width = ui->padSurface.getEffectiveWidth ();
		const double height = ui->padSurface.getEffectiveHeight ();

		const int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
		const int s0 = maxstep - 1 - int ((pointerEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
		const int r0 = (pointerEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);
		const int step = (ui->patternFlipped ? r0 : s0);
		const int row = (ui->patternFlipped ? s0 : r0);


		if ((event->getEventType () == BEvents::BUTTON_PRESS_EVENT) || (event->getEventType () == BEvents::POINTER_DRAG_EVENT))
		{

			if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
			{
				Pad oldPad = ui->pattern[page].getPad (row, step);

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
								const int f = (ui->patternFlipped ? -1 : 1);
								for (int r = 0; r < int (ui->clipBoard.data.size ()); ++r)
								{
									for (int s = 0; s < int (ui->clipBoard.data[r].size ()); ++s)
									{
										if
										(
											(row + f * r >= 0) &&
											(row + f * r < maxstep) &&
											(step - f * s >= 0) &&
											(step - f * s < maxstep)
										)
										{
											const int clr = (ui->patternFlipped ? ui->clipBoard.data.size () - 1 - r : r);
											const int cls = (ui->patternFlipped ? ui->clipBoard.data[r].size () - 1 - s : s);
											if (!ui->validatePad (page, row + f * r, step - f * s, ui->clipBoard.data.at(clr).at(cls)))
											{
												valid = false;
											}
											else if (valid) ui->drawPad (row + f * r, step - f * s);
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
						if (!ui->validatePad (page, row, step, newPad)) ui->drawPad();
						else ui->drawPad (row,step);
					}

					ui->padPressed = true;
				}

				else if (pointerEvent->getButton() == BDevices::RIGHT_BUTTON)
				{
					ui->levelDial.setValue (ui->pattern[page].getPad (row, step).level);
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
					if (((editNr == EDIT_XFLIP) && (!ui->patternFlipped)) || ((editNr == EDIT_YFLIP) && (ui->patternFlipped)))
					{
						for (int dr = 0; dr < int ((clipRMax + 1 - clipRMin) / 2); ++dr)
						{
							for (int s = clipSMin; s <= clipSMax; ++s)
							{
								Pad pd = ui->pattern[page].getPad (clipRMin + dr, s);
								ui->pattern[page].setPad (clipRMin + dr, s, ui->pattern[page].getPad (clipRMax -dr, s));
								ui->send_pad (page, clipRMin + dr, s);
								ui->pattern[page].setPad (clipRMax - dr, s, pd);
								ui->send_pad (page, clipRMax - dr, s);
							}
						}
						ui->pattern[page].store ();
					}

					// YFLIP
					// Validation required for REPLACE mode
					if (((editNr == EDIT_YFLIP) && (!ui->patternFlipped)) || ((editNr == EDIT_XFLIP) && (ui->patternFlipped)))
					{
						// Temp. copy selection
						Pad pads[clipRMax + 1 - clipRMin][clipSMax + 1 - clipSMin];
						for (int ds = 0; ds <= clipSMax - clipSMin; ++ds)
						{
							for (int dr = 0; dr <= clipRMax - clipRMin; ++dr)
							{
								pads[dr][ds] = ui->pattern[page].getPad (clipRMin + dr, clipSMin + ds);
							}
						}

						// X flip temp. copy & paste selection
						bool valid = true;
						for (int ds = 0; ds <= clipSMax - clipSMin; ++ds)
						{
							for (int dr = 0; dr <= clipRMax - clipRMin; ++dr)
							{
								if (!ui->validatePad (page, clipRMin + dr, clipSMax - ds, pads[dr][ds]))
								{
									valid = false;
								}
								else if (valid) ui->drawPad (clipRMin + dr, clipSMax - ds);
							}
						}
						if (!valid) ui->drawPad();
						ui->pattern[page].store ();
					}

					// Store selected data in clipboard after flip (XFLIP, YFLIP)
					// Or store selected data in clipboard before deletion (CUT)
					// Or store selected data anyway (COPY)
					ui->clipBoard.data.clear ();
					for (int r = clipRMin; r <= clipRMax; ++r)
					{
						std::vector<Pad> padRow;
						padRow.clear ();
						for (int s = clipSMax; s >= clipSMin; --s) padRow.push_back (ui->pattern[page].getPad (r, s));
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
								if (ui->pattern[page].getPad (r, s) != Pad())
								{
									// REPLACE mode: delete everything except default pads
									if (ui->editMode == 1)
									{
										if (r != s)
										{
											ui->pattern[page].setPad (r, s,  Pad ());
											ui->send_pad (page, r, s);
											empty = true;
										}

										else empty = true;
									}

									// ADD mode: simply delete
									else
									{
										ui->pattern[page].setPad (r, s,  Pad ());
										ui->send_pad (page, r, s);
									}
								}
							}

							// Emptied column in REPLACE mode: set default
							if (empty)
							{
								ui->pattern[page].setPad (s, s,  Pad (1.0));
								ui->send_pad (page, s, s);
							}
						}
						ui->pattern[page].store ();
					}

					ui->clipBoard.ready = true;
					ui->drawPad ();
				}
			}

			else
			{
				ui->padPressed = false;
				ui->pattern[page].store ();
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

		const int page = ui->actPage;
		const int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
		const int s0 = maxstep - 1 - int ((wheelEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
		const int r0 = (wheelEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);
		const int step = (ui->patternFlipped ? r0 : s0);
		const int row = (ui->patternFlipped ? s0 : r0);

		if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
		{
			Pad pd = ui->pattern[page].getPad (row, step);
			pd.level = LIMIT (pd.level + 0.01 * wheelEvent->getDelta().y, 0.0, 1.0);
			if (!ui->validatePad (page, row, step, pd)) ui->drawPad();
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

	const int page = ui->actPage;
	const int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
	const int s0 = maxstep - 1 - int ((focusEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
	const int r0 = (focusEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);
	const int step = (ui->patternFlipped ? r0 : s0);
	const int row = (ui->patternFlipped ? s0 : r0);

	if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
	{
		ui->padSurface.focusText.setText
		(
			BJUMBLR_LABEL_ROW ": " + std::to_string (row + 1) + "\n" +
			BJUMBLR_LABEL_STEP ": " + std::to_string (step + 1) + "\n" +
			BJUMBLR_LABEL_LEVEL ": " + BUtilities::to_string (ui->pattern[page].getPad (row, step).level, "%1.2f")
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
		offset = (int (ui->controllers[NR_OF_STEPS] - int (ui->cursor) + ui->controllers[STEP_OFFSET])) % int (ui->controllers[NR_OF_STEPS]);
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
	ui->fileChooser = new SampleChooser
	(
		200, 140, 640, 400, "filechooser", ui->samplePath,
		std::vector<BWidgets::FileFilter>
		{
			BWidgets::FileFilter {BJUMBLR_LABEL_ALL_FILES, std::regex (".*")},
			BWidgets::FileFilter {BJUMBLR_LABEL_AUDIO_FILES, std::regex (".*\\.((wav)|(wave)|(aif)|(aiff)|(au)|(sd2)|(flac)|(caf)|(ogg)|(mp3))$", std::regex_constants::icase)}
		},
		std::vector<std::string>
		{
			BJUMBLR_LABEL_OK, BJUMBLR_LABEL_OPEN, BJUMBLR_LABEL_CANCEL,
			"", "", BJUMBLR_LABEL_NEW_FOLDER, BJUMBLR_LABEL_CANT_CREATE_NEW_FOLDER, BJUMBLR_LABEL_PLAY_AS_LOOP,
			BJUMBLR_LABEL_FILE, BJUMBLR_LABEL_SELECTION_START, BJUMBLR_LABEL_SELECTION_END,
			BJUMBLR_LABEL_FRAMES, BJUMBLR_LABEL_NO_FILE_SELECTED
		}
	);
	if (ui->fileChooser)
	{
		const std::string filename = ui->sampleNameLabel.getText();
		if (filename != "")
		{
			ui->fileChooser->setFileName (ui->sampleNameLabel.getText());
			ui->fileChooser->setStart (ui->sampleStart);
			ui->fileChooser->setEnd (ui->sampleEnd);
			ui->fileChooser->setLoop (ui->sampleLoop);
		}

		RESIZE ((*ui->fileChooser), 200, 140, 640, 400, ui->sz);
		ui->fileChooser->applyTheme (ui->theme);
		ui->fileChooser->selectFilter (BJUMBLR_LABEL_AUDIO_FILES);
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
	else if (widget == &ui->setStartDelayButton)
	{
		ui->manualProgressionDelayWidget.setValue
		(
			floormod
			(
				ui->manualProgressionDelayWidget.getValue() - floorfrac (ui->cursor),
				ui->controllerWidgets[NR_OF_STEPS]->getValue ()
			)
		);
	}
}

void BJumblrGUI::patternFlippedClickedCallback (BEvents::Event* event)
{
	if (!event) return;
	BWidgets::Widget* widget = event->getWidget ();
	if (!widget) return;
	BJumblrGUI* ui = (BJumblrGUI*) widget->getMainWindow();
	if (!ui) return;
	ui->patternFlipped = !ui->patternFlipped;
	ui->monitorWidget.flip (ui->patternFlipped);
	ui->setMarkers();
	ui->drawPad();
	ui->send_flip();
}

void BJumblrGUI::helpButtonClickedCallback (BEvents::Event* event)
{
	char cmd[] = WWW_BROWSER_CMD;
	char param[] = HELP_URL;
	char* argv[] = {cmd, param, NULL};
	std::cerr << "BJumblr.lv2#GUI: Call " << HELP_URL << " for help.\n";
	if (BUtilities::vsystem (argv) == -1) std::cerr << "BJumblr.lv2#GUI: Couldn't fork.\n";
}

void BJumblrGUI::ytButtonClickedCallback (BEvents::Event* event)
{
	char cmd[] = WWW_BROWSER_CMD;
	char param[] = YT_URL;
	char* argv[] = {cmd, param, NULL};
	std::cerr << "BJumblr.lv2#GUI: Call " << YT_URL << " for tutorial video.\n";
	if (BUtilities::vsystem (argv) == -1) std::cerr << "BJumblr.lv2#GUI: Couldn't fork.\n";
}

void BJumblrGUI::setMarkers()
{
	const double maxstep = controllerWidgets[NR_OF_STEPS]->getValue ();
	markerFwd.resize (20 * sz, 20 * sz);
	markerRev.resize (20 * sz, 20 * sz);
	if (patternFlipped)
	{
		markerFwd.setMarker (MARKER_DOWN);
		markerRev.setMarker (MARKER_UP);
		markerFwd.moveTo ((20 + (0.5 + int (cursor)) * (920.0 / maxstep) - 10) * sz, 110 * sz);
		markerRev.moveTo ((20 + (0.5 + int (cursor)) * (920.0 / maxstep) - 10) * sz, 560 * sz);
	}
	else
	{
		markerFwd.setMarker (MARKER_RIGHT);
		markerRev.setMarker (MARKER_LEFT);
		markerFwd.moveTo (0, (130 + (maxstep - 0.5 - int (cursor)) * (430.0 / maxstep) - 10) * sz);
		markerRev.moveTo (940 * sz, (130 + (maxstep - 0.5 - int (cursor)) * (430.0 / maxstep) - 10) *sz);
	}
}

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
	const double x = (patternFlipped ? step : row) * w;
	const double y = (maxstep - 1 - (patternFlipped ? row : step)) * h;
	const double xr = round (x);
	const double yr = round (y);
	const double wr = round (x + w) - xr;
	const double hr = round (y + h) - yr;

	// Draw background
	// Odd or even?
	BColors::Color bg =
	(
		int (cursor) == step ?
		BColors::Color (0.25, 0.25, 0.0, 1.0) :
		((int (row / 4) % 2) ? oddPadBgColor : evenPadBgColor)
	);

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
	Pad pd = pattern[actPage].getPad (row, step);
	Pad pdc = pattern[actPage].getPad (row, cursor);
	BColors::Color color = BColors::yellow;
	color.applyBrightness (pd.level - 1.0);
	if ((tabs[actPage].playSymbol.getState() == BColors::ACTIVE) && (pdc.level != 0.0)) color.applyBrightness (pdc.level * 0.75);
	drawButton (cr, xr + 1, yr + 1, wr - 2, hr - 2, color);
}

static LV2UI_Handle instantiate (const LV2UI_Descriptor *descriptor,
						  const char *plugin_uri,
						  const char *bundle_path,
						  LV2UI_Write_Function write_function,
						  LV2UI_Controller controller,
						  LV2UI_Widget *widget,
						  const LV2_Feature *const *features)
{
	PuglNativeView parentWindow = 0;
	LV2UI_Resize* resize = NULL;

	if (strcmp(plugin_uri, BJUMBLR_URI) != 0)
	{
		std::cerr << "BJumblr.lv2#GUI: GUI does not support plugin with URI " << plugin_uri << std::endl;
		return NULL;
	}

	for (int i = 0; features[i]; ++i)
	{
		if (!strcmp(features[i]->URI, LV2_UI__parent)) parentWindow = (PuglNativeView) features[i]->data;
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

static void cleanup(LV2UI_Handle ui)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	if (self) delete self;
}

static void port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size,
	uint32_t format, const void* buffer)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	if (self) self->port_event(port_index, buffer_size, format, buffer);
}

static int call_idle (LV2UI_Handle ui)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	if (self) self->handleEvents ();
	return 0;
}

static int call_resize (LV2UI_Handle ui, int width, int height)
{
	BJumblrGUI* self = (BJumblrGUI*) ui;
	if (!self) return 0;

	BEvents::ExposeEvent* ev = new BEvents::ExposeEvent (self, self, BEvents::CONFIGURE_REQUEST_EVENT, self->getPosition().x, self->getPosition().y, width, height);
	self->addEventToQueue (ev);
	return 0;
}

static const LV2UI_Idle_Interface idle = {call_idle};
static const LV2UI_Resize resize = {nullptr, call_resize} ;

static const void* extension_data(const char* uri)
{
	if (!strcmp(uri, LV2_UI__idleInterface)) return &idle;
	if(!strcmp(uri, LV2_UI__resize)) return &resize;
	else return NULL;
}

static const LV2UI_Descriptor guiDescriptor = {
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
