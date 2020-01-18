/*  B.Noname
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

#include "BNonameGUI.hpp"
#include "BUtilities/to_string.hpp"

BNonameGUI::BNonameGUI (const char *bundle_path, const LV2_Feature *const *features, PuglNativeWindow parentWindow) :
	Window (840, 620, "B.Noname", parentWindow, true),
	controller (NULL), write_function (NULL),
	pluginPath (bundle_path ? std::string (bundle_path) : std::string ("")),
	sz (1.0), bgImageSurface (nullptr),
	uris (), forge (), clipBoard (),
	cursor (0), wheelScrolled (false), padPressed (false), deleteMode (false),
	mContainer (0, 0, 840, 620, "main"),
	padSurface (18, 88, 804, 484, "box"),
	playButton (18, 578, 24, 24, "widget", "Play"),
	stopButton (48, 578, 24, 24, "widget", "Stop"),
	stepSizeListBox (460, 580, 100, 20, 0, -160, 100, 160, "menu",
			 BItems::ItemList ({{0.0625, "1/16"}, {0.125, "1/8"}, {0.25, "1/4"}, {0.5, "1/2"}, {1, "1"}, {2, "2"}, {4, "4"}}), 1),
	stepBaseListBox (580, 580, 100, 20, 0, -80, 100, 80, "menu", BItems::ItemList ({{0, "Seconds"}, {1, "Beats"}, {2, "Bars"}}), 1),
	padSizeListBox (720, 580, 100, 20, 0, -140, 100, 140, "menu",
		     BItems::ItemList ({{4, "4 Steps"}, {8, "8 Steps"}, {12, "12 Steps"}, {16, "16 Steps"}, {24, "24 Steps"}, {32, "32 Steps"}}), 16)

{
	// Init editButtons
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i] = HaloToggleButton (108 + i * 30, 578, 24, 24, "widget", editLabels[i]);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i] = HaloButton (288 + i * 30, 578, 24, 24, "widget", editLabels[i + EDIT_RESET]);

	// Link controllerWidgets
	controllerWidgets[PLAY] = (BWidgets::ValueWidget*) &playButton;
	controllerWidgets[NR_OF_STEPS] = (BWidgets::ValueWidget*) &padSizeListBox;
	controllerWidgets[STEP_SIZE] = (BWidgets::ValueWidget*) &stepSizeListBox;
	controllerWidgets[STEP_BASE] = (BWidgets::ValueWidget*) &stepBaseListBox;

	// Init controller values
	for (int i = 0; i < MAXCONTROLLERS; ++i) controllers[i] = controllerWidgets[i]->getValue ();

	// Set callback functions
	for (int i = 0; i < MAXCONTROLLERS; ++i) controllerWidgets[i]->setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	stopButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, valueChangedCallback);
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i].setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, edit1ChangedCallback);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i].setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, edit2ChangedCallback);

	padSurface.setDraggable (true);
	padSurface.setCallbackFunction (BEvents::BUTTON_PRESS_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::BUTTON_RELEASE_EVENT, padsPressedCallback);
	padSurface.setCallbackFunction (BEvents::POINTER_DRAG_EVENT, padsPressedCallback);

	padSurface.setScrollable (true);
	padSurface.setCallbackFunction (BEvents::WHEEL_SCROLL_EVENT, padsScrolledCallback);

	// Apply theme
	bgImageSurface = cairo_image_surface_create_from_png ((pluginPath + BG_FILE).c_str());
	widgetBg.loadFillFromCairoSurface (bgImageSurface);
	applyTheme (theme);

	// Pack widgets
	mContainer.add (playButton);
	mContainer.add (stopButton);
	for (int i = 0; i < EDIT_RESET; ++i) mContainer.add (edit1Buttons[i]);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) mContainer.add (edit2Buttons[i]);
	mContainer.add (stepSizeListBox);
	mContainer.add (stepBaseListBox);
	mContainer.add (padSizeListBox);
	mContainer.add (padSurface);

	drawPad();
	add (mContainer);

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

BNonameGUI::~BNonameGUI ()
{
	send_ui_off ();
}

void BNonameGUI::Pattern::clear ()
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

Pad BNonameGUI::Pattern::getPad (const size_t row, const size_t step) const
{
	return pads[LIMIT (row, 0, MAXSTEPS)][LIMIT (step, 0, MAXSTEPS)];
}
void BNonameGUI::Pattern::setPad (const size_t row, const size_t step, const Pad& pad)
{
	size_t r = LIMIT (row, 0, MAXSTEPS);
	size_t s = LIMIT (step, 0, MAXSTEPS);
	changes.oldMessage.push_back (PadMessage (s, r, pads[r][s]));
	changes.newMessage.push_back (PadMessage (s, r, pad));
	pads[r][s] = pad;
}

std::vector<PadMessage> BNonameGUI::Pattern::undo ()
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

std::vector<PadMessage> BNonameGUI::Pattern::redo ()
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

void BNonameGUI::Pattern::store ()
{
	if (changes.newMessage.empty ()) return;

	journal.push (changes.oldMessage, changes.newMessage);
	changes.oldMessage.clear ();
	changes.newMessage.clear ();
}

void BNonameGUI::port_event(uint32_t port, uint32_t buffer_size,
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
				LV2_Atom *oPad = NULL;
				lv2_atom_object_get(obj, uris.notify_pad, &oPad, NULL);

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

			// Status notifications
			else if (obj->body.otype == uris.notify_statusEvent)
			{
				LV2_Atom *oCursor = NULL;
				lv2_atom_object_get
				(
					obj, uris.notify_cursor, &oCursor,
					NULL
				);

				// Cursor notifications
				if (oCursor && (oCursor->type == uris.atom_Int) && (cursor != ((LV2_Atom_Int*)oCursor)->body))
				{
					cursor = ((LV2_Atom_Int*)oCursor)->body;
					drawPad ();
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

void BNonameGUI::resize ()
{
	hide ();
	//Scale fonts
	ctLabelFont.setFontSize (12 * sz);
	tgLabelFont.setFontSize (12 * sz);
	iLabelFont.setFontSize (18 * sz);
	lfLabelFont.setFontSize (12 * sz);

	//Background
	cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 840 * sz, 620 * sz);
	cairo_t* cr = cairo_create (surface);
	cairo_scale (cr, sz, sz);
	cairo_set_source_surface(cr, bgImageSurface, 0, 0);
	cairo_paint(cr);
	widgetBg.loadFillFromCairoSurface(surface);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	//Scale widgets
	RESIZE (mContainer, 0, 0, 840, 620, sz);
	RESIZE (padSurface, 18, 88, 804, 484, sz);
	RESIZE (playButton, 18, 578, 24, 24, sz);
	RESIZE (stopButton, 48, 578, 24, 24, sz);
	for (int i = 0; i < EDIT_RESET; ++i) RESIZE (edit1Buttons[i], 108 + i * 30, 578, 24, 24, sz);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) RESIZE (edit2Buttons[i], 288 + i * 30, 578, 24, 24, sz);
	RESIZE (stepSizeListBox, 460, 580, 100, 20, sz);
	stepSizeListBox.resizeListBox(BUtilities::Point (100 * sz, 160 * sz));
	stepSizeListBox.moveListBox(BUtilities::Point (0, -160 * sz));
	stepSizeListBox.resizeListBoxItems(BUtilities::Point (100 * sz, 20 * sz));
	RESIZE (stepBaseListBox, 580, 580, 100, 20, sz);
	stepBaseListBox.resizeListBox(BUtilities::Point (100 * sz, 80 * sz));
	stepBaseListBox.moveListBox(BUtilities::Point (0, -80 * sz));
	stepBaseListBox.resizeListBoxItems(BUtilities::Point (100 * sz, 20 * sz));
	RESIZE (padSizeListBox, 720, 580, 100, 20, sz);
	padSizeListBox.resizeListBox(BUtilities::Point (100 * sz, 140 * sz));
	padSizeListBox.moveListBox(BUtilities::Point (0, -140 * sz));
	padSizeListBox.resizeListBoxItems(BUtilities::Point (100 * sz, 20 * sz));

	applyTheme (theme);
	drawPad ();
	show ();
}

void BNonameGUI::applyTheme (BStyles::Theme& theme)
{
	mContainer.applyTheme (theme);

	padSurface.applyTheme (theme);
	playButton.applyTheme (theme);
	stopButton.applyTheme (theme);
	for (int i = 0; i < EDIT_RESET; ++i) edit1Buttons[i].applyTheme (theme);
	for (int i = 0; i < MAXEDIT - EDIT_RESET; ++i) edit2Buttons[i].applyTheme (theme);
	stepSizeListBox.applyTheme (theme);
	stepBaseListBox.applyTheme (theme);
	padSizeListBox.applyTheme (theme);
}

void BNonameGUI::onConfigureRequest (BEvents::ExposeEvent* event)
{
	Window::onConfigureRequest (event);

	sz = (getWidth() / 840 > getHeight() / 620 ? getHeight() / 620 : getWidth() / 840);
	resize ();
}

void BNonameGUI::send_ui_on ()
{
	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.ui_on);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BNonameGUI::send_ui_off ()
{
	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.ui_off);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BNonameGUI::send_pad (int row, int step)
{
	PadMessage padmsg (step, row, pattern.getPad (row, step));

	uint8_t obj_buf[128];
	lv2_atom_forge_set_buffer(&forge, obj_buf, sizeof(obj_buf));

	LV2_Atom_Forge_Frame frame;
	LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&forge, &frame, 0, uris.notify_padEvent);
	lv2_atom_forge_key(&forge, uris.notify_pad);
	lv2_atom_forge_vector(&forge, sizeof(float), uris.atom_Float, sizeof(PadMessage) / sizeof(float), (void*) &padmsg);
	lv2_atom_forge_pop(&forge, &frame);
	write_function(controller, CONTROL, lv2_atom_total_size(msg), uris.atom_eventTransfer, msg);
}

void BNonameGUI::valueChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	BNonameGUI* ui = (BNonameGUI*) widget->getMainWindow();
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

		// Layout changed
		if (controllerNr == NR_OF_STEPS)
		{
			ui->drawPad ();
		}
	}

	// Buttons
	else if (value == 1.0)
	{
		// Stop
		if (widget == &ui->stopButton) ui->playButton.setValue (0.0);
	}
}

void BNonameGUI::edit1ChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	if (value != 1.0) return;
	BNonameGUI* ui = (BNonameGUI*) widget->getMainWindow();
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

void BNonameGUI::edit2ChangedCallback(BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ValueWidget* widget = (BWidgets::ValueWidget*) event->getWidget ();
	if (!widget) return;
	float value = widget->getValue();
	if (value != 1.0) return;
	BNonameGUI* ui = (BNonameGUI*) widget->getMainWindow();
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
			ui->drawPad ();
		}
		break;

		default:	break;
	}
}

void BNonameGUI::padsPressedCallback (BEvents::Event* event)
{
	if (!event) return;
	BEvents::PointerEvent* pointerEvent = (BEvents::PointerEvent*) event;
	BWidgets::DrawingSurface* widget = (BWidgets::DrawingSurface*) event->getWidget ();
	if (!widget) return;
	BNonameGUI* ui = (BNonameGUI*) widget->getMainWindow();
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
		int row = maxstep - 1 - int ((pointerEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep));
		int step = (pointerEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);

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
								for (int r = 0; r < int (ui->clipBoard.data.size ()); ++r)
								{
									for (int s = 0; s < int (ui->clipBoard.data[r].size ()); ++s)
									{
										if
										(
											(row - r >= 0) &&
											(row - r < maxstep) &&
											(step + s >= 0) &&
											(step + s < maxstep)
										)
										{
											ui->pattern.setPad (row - r, step + s, ui->clipBoard.data.at(r).at(s));
											ui->drawPad (row - r, step + s);
											ui->send_pad (row - r, step + s);
										}
									}
								}
							}
						}

					}

					// Set (or unset) pad
					else
					{
						if (!ui->padPressed) ui->deleteMode = (oldPad.level == 1.0);
						Pad newPad = (ui->deleteMode ? Pad (0.0) : Pad (1.0));
						ui->pattern.setPad (row, step, newPad);
						ui->drawPad (row, step);
						ui->send_pad (row, step);
					}

					ui->padPressed = true;
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

					if (editNr == EDIT_XFLIP)
					{
						for (int ds = 0; ds < int ((clipSMax + 1 - clipSMin) / 2); ++ds)
						{
							for (int r = clipRMin; r <= clipRMax; ++r)
							{
								Pad pd = ui->pattern.getPad (r, clipSMin + ds);
								ui->pattern.setPad (r, clipSMin + ds, ui->pattern.getPad (r, clipSMax - ds));
								ui->send_pad (r, clipSMin + ds);
								ui->pattern.setPad (r, clipSMax - ds, pd);
								ui->send_pad (r, clipSMax - ds);
							}
						}
						ui->pattern.store ();
					}

					if (editNr == EDIT_YFLIP)
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

					ui->clipBoard.data.clear ();
					for (int r = clipRMax; r >= clipRMin; --r)
					{
						std::vector<Pad> padRow;
						padRow.clear ();
						for (int s = clipSMin; s <= clipSMax; ++s) padRow.push_back (ui->pattern.getPad (r, s));
						ui->clipBoard.data.push_back (padRow);
					}

					if (editNr == EDIT_CUT)
					{
						for (int r = clipRMax; r >= clipRMin; --r)
						{
							for (int s = clipSMin; s <= clipSMax; ++s)
							{
								ui->pattern.setPad (r, s,  Pad ());
								ui->send_pad (r, s);
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

void BNonameGUI::padsScrolledCallback (BEvents::Event* event)
{
	if ((event) && (event->getWidget ()) && (event->getWidget()->getMainWindow()) &&
		((event->getEventType () == BEvents::WHEEL_SCROLL_EVENT)))
	{
		BWidgets::DrawingSurface* widget = (BWidgets::DrawingSurface*) event->getWidget ();
		BNonameGUI* ui = (BNonameGUI*) widget->getMainWindow();
		BEvents::WheelEvent* wheelEvent = (BEvents::WheelEvent*) event;

		// Get size of drawing area
		const double width = ui->padSurface.getEffectiveWidth ();
		const double height = ui->padSurface.getEffectiveHeight ();

		int maxstep = ui->controllerWidgets[NR_OF_STEPS]->getValue ();
		int row = (maxstep - 1) - ((int) ((wheelEvent->getPosition ().y - widget->getYOffset()) / (height / maxstep)));
		int step = (wheelEvent->getPosition ().x - widget->getXOffset()) / (width / maxstep);

		if ((row >= 0) && (row < maxstep) && (step >= 0) && (step < maxstep))
		{
			Pad pd = ui->pattern.getPad (row, step);
			pd.level = LIMIT (pd.level + 0.01 * wheelEvent->getDelta().y, 0.0, 1.0);
			ui->pattern.setPad (row, step, pd);
			ui->drawPad (row, step);
			ui->send_pad (row, step);
			ui->wheelScrolled = true;
		}
	}
}

void BNonameGUI::drawPad ()
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

void BNonameGUI::drawPad (int row, int step)
{
	cairo_surface_t* surface = padSurface.getDrawingSurface();
	cairo_t* cr = cairo_create (surface);
	drawPad (cr, row, step);
	cairo_destroy (cr);
	padSurface.update();
}

void BNonameGUI::drawPad (cairo_t* cr, int row, int step)
{
	int maxstep = controllerWidgets[NR_OF_STEPS]->getValue ();
	if ((!cr) || (cairo_status (cr) != CAIRO_STATUS_SUCCESS) || (row < 0) || (row >= maxstep) || (step < 0) ||
		(step >= maxstep)) return;

	// Get size of drawing area
	const double width = padSurface.getEffectiveWidth ();
	const double height = padSurface.getEffectiveHeight ();
	const double w = width / maxstep;
	const double h = height / maxstep;
	const double x = step * w;
	const double y = (maxstep - row - 1) * h;
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

	if (strcmp(plugin_uri, BNONAME_URI) != 0)
	{
		std::cerr << "BNoname.lv2#GUI: GUI does not support plugin with URI " << plugin_uri << std::endl;
		return NULL;
	}

	for (int i = 0; features[i]; ++i)
	{
		if (!strcmp(features[i]->URI, LV2_UI__parent)) parentWindow = (PuglNativeWindow) features[i]->data;
		else if (!strcmp(features[i]->URI, LV2_UI__resize)) resize = (LV2UI_Resize*)features[i]->data;
	}
	if (parentWindow == 0) std::cerr << "BNoname.lv2#GUI: No parent window.\n";

	// New instance
	BNonameGUI* ui;
	try {ui = new BNonameGUI (bundle_path, features, parentWindow);}
	catch (std::exception& exc)
	{
		std::cerr << "BNoname.lv2#GUI: Instantiation failed. " << exc.what () << std::endl;
		return NULL;
	}

	ui->controller = controller;
	ui->write_function = write_function;

	// Reduce min GUI size for small displays
	double sz = 1.0;
	int screenWidth  = getScreenWidth ();
	int screenHeight = getScreenHeight ();
	if ((screenWidth < 600) || (screenHeight < 460)) sz = 0.5;
	else if ((screenWidth < 880) || (screenHeight < 660)) sz = 0.66;

	if (resize) resize->ui_resize(resize->handle, 840 * sz, 620 * sz);

	*widget = (LV2UI_Widget) puglGetNativeWindow (ui->getPuglView ());

	ui->send_ui_on();

	return (LV2UI_Handle) ui;
}

void cleanup(LV2UI_Handle ui)
{
	BNonameGUI* self = (BNonameGUI*) ui;
	delete self;
}

void port_event(LV2UI_Handle ui, uint32_t port_index, uint32_t buffer_size,
	uint32_t format, const void* buffer)
{
	BNonameGUI* self = (BNonameGUI*) ui;
	self->port_event(port_index, buffer_size, format, buffer);
}

static int call_idle (LV2UI_Handle ui)
{
	BNonameGUI* self = (BNonameGUI*) ui;
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
		BNONAME_GUI_URI,
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
