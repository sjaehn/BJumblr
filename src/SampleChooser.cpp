/* B.Oops
 * Glitch effect sequencer LV2 plugin
 *
 * Copyright (C) 2020 by Sven Jähnichen
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

#include "SampleChooser.hpp"
#include <new>

#ifndef SF_FORMAT_MP3
#ifndef MINIMP3_IMPLEMENTATION
#define MINIMP3_IMPLEMENTATION
#endif
#endif
#include "Sample.hpp"

SampleChooser::SampleChooser () : SampleChooser (0.0, 0.0, 0.0, 0.0, "FileChooser") {}

SampleChooser::SampleChooser (const double x, const double y, const double width, const double height, const std::string& name) :
	SampleChooser (x, y, width, height, name, ".", {}, "OK") {}

SampleChooser::SampleChooser (const double x, const double y, const double width, const double height, const std::string& name,
			      const std::string& path):
	SampleChooser (x, y, width, height, name, path, {}, "OK") {}

SampleChooser::SampleChooser (const double x, const double y, const double width, const double height, const std::string& name,
			      const std::string& path, const std::vector<BWidgets::FileFilter>& filters) :
	SampleChooser (x, y, width, height, name, path, filters, "OK") {}

SampleChooser::SampleChooser (const double x, const double y, const double width, const double height, const std::string& name,
			      const std::string& path, const std::vector<BWidgets::FileFilter>& filters, const std::string& buttonText) :
	SampleChooser (x, y, width, height, name, path, filters, std::vector<std::string>{"OK"}) {}

SampleChooser::SampleChooser (const double x, const double y, const double width, const double height, const std::string& name,
			      const std::string& path, const std::vector<BWidgets::FileFilter>& filters, const std::vector<std::string>& texts) :
	FileChooser (x, y, width, height, name, path, filters, texts),
	waveform (0, 0, 0, 0, name + "/textbox"),
	scrollbar (0, 0, 0, 0, name + "/scrollbar", 0.0, 1.0, 0.0, 1.0, 0.0),
	startMarker (0, 0, 0, 0, name + "/marker"),
	endMarker (0, 0, 0, 0, name + "/marker"),
	sizeLabel (0, 0, 0, 0, name + "/label"),
	startLabel (0, 0, 0, 0, name + "/label"),
	endLabel (0, 0, 0, 0, name + "/label"),
	loopCheckbox (0, 0, 0, 0, name + "/checkbox"),
	loopLabel (0, 0, 0, 0, name + "/label"),
	noFileLabel (0, 0, 0, 0, name + "/label"),
	sample (nullptr)
{
	std::vector<std::string> sampleLabels = {"Play selection as loop", "File", "Selection start", "Selection end", "frames", "No audio file selected"};
	labels.insert (labels.end(), sampleLabels.begin(), sampleLabels.end());
	for (int i = BWIDGETS_DEFAULT_SAMPLECHOOSER_PLAY_AS_LOOP_INDEX; (i < int(texts.size())) && (i < int(labels.size())); ++i) labels[i] = texts[i];
	loopLabel.setText (labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_PLAY_AS_LOOP_INDEX]);
	noFileLabel.setText (labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_NO_FILE_INDEX]);
	noFileLabel.hide();

	fileListBox.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, sfileListBoxClickedCallback);
	waveform.setBackground (BWIDGETS_DEFAULT_MENU_BACKGROUND);
	waveform.setBorder ({{BColors::grey, 1.0}, 0.0, 3.0, 0.0});
	scrollbar.minButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, scrollbarChangedCallback);
	scrollbar.maxButton.setCallbackFunction (BEvents::VALUE_CHANGED_EVENT, scrollbarChangedCallback);
	startMarker.setDraggable (true);
	startMarker.setCallbackFunction (BEvents::POINTER_DRAG_EVENT, lineDraggedCallback);
	endMarker.setDraggable (true);
	endMarker.setCallbackFunction (BEvents::POINTER_DRAG_EVENT, lineDraggedCallback);
	fileNameBox.setCallbackFunction (BEvents::MESSAGE_EVENT, filenameEnteredCallback);
	add (waveform);
	waveform.add (startMarker);
	waveform.add (endMarker);
	add (scrollbar);
	add (sizeLabel);
	add (startLabel);
	add (endLabel);
	add (loopCheckbox);
	add (loopLabel);
	add (noFileLabel);
}

SampleChooser::SampleChooser (const SampleChooser& that) :
	FileChooser (that), waveform (that.waveform), scrollbar (that.scrollbar),
	startMarker (that.startMarker), endMarker (that.endMarker),
	sizeLabel (that.sizeLabel), startLabel (that.startLabel), endLabel (that.endLabel),
	loopCheckbox (that.loopCheckbox), loopLabel (that.loopLabel), noFileLabel (that.noFileLabel),
	sample (nullptr)
{
	try {sample = new Sample (*that.sample);}
	catch (std::exception& exc) {throw exc;}

	add (waveform);
	waveform.add (startMarker);
	waveform.add (endMarker);
	add (scrollbar);
	add (sizeLabel);
	add (startLabel);
	add (endLabel);
	add (loopCheckbox);
	add (loopLabel);
	add (noFileLabel);
}

SampleChooser::~SampleChooser()
{
	if (sample) delete (sample);
}

SampleChooser& SampleChooser::operator= (const SampleChooser& that)
{
	waveform.release (&scrollbar);
	waveform.release (&startMarker);
	release (&waveform);
	release (&endMarker);
	release (&sizeLabel);
	release (&startLabel);
	release (&endLabel);
	release (&loopCheckbox);
	release (&loopLabel);
	release (&noFileLabel);
	if (sample) delete (sample);

	waveform = that.waveform;
	scrollbar = that.scrollbar;
	startMarker = that. startMarker;
	endMarker = that.endMarker;
	sizeLabel = that.sizeLabel;
	startLabel = that.startLabel;
	endLabel = that.endLabel;
	loopCheckbox = that.loopCheckbox;
	loopLabel = that.loopLabel;
	noFileLabel = that.noFileLabel;
	try {sample = new Sample (*that.sample);}
	catch (std::exception& exc) {throw exc;}
	FileChooser::operator= (that);

	add (waveform);
	waveform.add (startMarker);
	waveform.add (endMarker);
	add (scrollbar);

	add (sizeLabel);
	add (startLabel);
	add (endLabel);
	add (loopCheckbox);
	add (loopLabel);
	add (noFileLabel);

	return *this;
}

BWidgets::Widget* SampleChooser::clone () const {return new SampleChooser (*this);}

void SampleChooser::setFileName (const std::string& filename)
{
	if (filename != fileNameBox.getText())
	{
		FileChooser::setFileName (filename);
		std::string newPath = getPath() + "/" + filename;
		char buf[PATH_MAX];
		char *rp = realpath(newPath.c_str(), buf);
		if (sample)
		{
			delete (sample);
			sample = nullptr;
		}
		try {sample = new Sample (rp);}
		catch (std::exception& exc)
		{
			std::cerr << exc.what() << "\n";
			noFileLabel.setText (exc.what());
		}

		if (sample)
		{
			noFileLabel.setText (labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_NO_FILE_INDEX]);

			sample->start = 0;
			sample->end = sample->info.frames;

			scrollbar.minButton.setValue (0.0);
			scrollbar.maxButton.setValue (1.0);
		}

		update();
	}
}

void SampleChooser::setStart (const int64_t start)
{
	if (!sample) return;
	sample->start = LIMIT (start, 0, sample->info.frames - 1);
	update();
}

int64_t SampleChooser::getStart() const {return (sample ? LIMIT (sample->start, 0, sample->info.frames - 1) : 0);}

void SampleChooser::setEnd (const int64_t end)
{
	if (!sample) return;
	sample->end = LIMIT (end, 0, sample->info.frames);
	update();
}

int64_t SampleChooser::getEnd() const {return (sample ? LIMIT (sample->end, 1, sample->info.frames) : 0);}

void SampleChooser::setLoop (const bool loop) {loopCheckbox.setValue (loop ? 1.0 : 0.0);}

bool SampleChooser::getLoop() const {return (loopCheckbox.getValue() != 0.0);}

void SampleChooser::resize () {resize (BWIDGETS_DEFAULT_SAMPLECHOOSER_WIDTH, BWIDGETS_DEFAULT_SAMPLECHOOSER_HEIGHT);}

void SampleChooser::resize (const double width, const double height)  {resize (BUtilities::Point (width, height));}

void SampleChooser::resize (const BUtilities::Point extends) {Widget::resize (extends);}

void SampleChooser::update ()
{
	double x0 = getXOffset();
	double y0 = getYOffset();
	double w = getEffectiveWidth();
	double h = getEffectiveHeight();

	if ((w >= 40) && (h >= 20))
	{
		double val = fileListBox.getValue();
		if ((val == UNSELECTED) || (val > dirs.size())) okButton.getLabel()->setText (labels[BWIDGETS_DEFAULT_FILECHOOSER_OK_INDEX]);
		else okButton.getLabel()->setText (labels[BWIDGETS_DEFAULT_FILECHOOSER_OPEN_INDEX]);
		cancelButton.getLabel()->setText(labels[BWIDGETS_DEFAULT_FILECHOOSER_CANCEL_INDEX]);
		loopLabel.setText(labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_PLAY_AS_LOOP_INDEX]);

		// Get extends first
		okButton.resize();
		cancelButton.resize ();
		double okWidth = (okButton.getWidth() > cancelButton.getWidth() ? okButton.getWidth() : cancelButton.getWidth());
		double okHeight = (okButton.getHeight() > cancelButton.getHeight() ? okButton.getHeight() : cancelButton.getHeight());
		pathNameBox.resize();
		double pathNameHeight = pathNameBox.getHeight();
		fileNameBox.resize();
		double fileNameHeight = fileNameBox.getHeight();
		fileNameLabel.resize();
		double fileNameWidth = fileNameLabel.getWidth();

		pathNameBox.moveTo (x0 + 10, y0 + 10);
		pathNameBox.resize (w - pathNameHeight - 30, pathNameHeight);

		newFolderButton.moveTo (x0 + w - 10 - pathNameHeight, y0 + 10);
		newFolderButton.resize (pathNameHeight, pathNameHeight);

		okButton.moveTo (x0 + w - okWidth - 10, y0 + h - okHeight - 10);
		okButton.resize (okWidth, okHeight);

		cancelButton.moveTo (x0 + w - 2 * okWidth - 20, y0 + h - okHeight - 10);
		cancelButton.resize (okWidth, okHeight);

		fileNameLabel.moveTo (x0 + 10, y0 + h - okHeight - fileNameHeight - 20);
		fileNameLabel.resize (fileNameWidth, fileNameHeight);

		fileNameBox.moveTo (x0 + fileNameWidth + 30, y0 + h - okHeight - fileNameHeight - 20);
		fileNameBox.resize (w - fileNameWidth - 40, fileNameHeight);

		filterPopupListBox.moveTo (x0 + 10, y0 + h - okHeight - 10);
		filterPopupListBox.resize (w - 2 * okWidth - 40, okHeight);
		filterPopupListBox.resizeListBox (BUtilities::Point (w - 2 * okWidth - 40, filters.size() * okHeight + 20)); // TODO Limit
		filterPopupListBox.resizeListBoxItems (BUtilities::Point (w - 2 * okWidth - 40, okHeight));

		okButton.show();
		cancelButton.show();
		fileNameLabel.show();
		fileNameBox.show();
		filterPopupListBox.show ();

		if (h > pathNameHeight + okHeight + fileNameHeight + 60)
		{
			const double fileListBoxHeight = h - pathNameHeight - okHeight - fileNameHeight - 50;

			fileListBox.moveTo (x0 + 10, y0 + pathNameHeight + 20);
			fileListBox.resize (0.4 * w - 15, fileListBoxHeight);
			fileListBox.resizeItems (BUtilities::Point (fileListBox.getEffectiveWidth(), 20));
			fileListBox.show();

			startMarker.resize (6.0, fileListBoxHeight);
			startMarker.show();
			endMarker.resize (6.0, fileListBoxHeight);
			endMarker.show();

			double waveformHeight = fileListBoxHeight;

			if (sample && (sample->info.frames > 0))
			{
				sizeLabel.resize();
				const double sizeHeight = sizeLabel.getHeight();
				startLabel.resize();
				const double startHeight = startLabel.getHeight();
				endLabel.resize();
				const double endHeight = endLabel.getHeight();
				loopLabel.resize();
				const double loopHeight = loopLabel.getHeight();

				if (fileListBoxHeight > sizeHeight + startHeight + endHeight + loopHeight + 50.0)
				{
					waveformHeight = fileListBoxHeight - sizeHeight - startHeight - endHeight - loopHeight - 10.0;
					sizeLabel.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20.0 + waveformHeight + 10.0);
					startLabel.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20.0 + waveformHeight + 10.0 + sizeHeight);
					endLabel.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20.0 + waveformHeight + 10.0 + sizeHeight + startHeight);
					loopCheckbox.resize (0.6 * loopHeight, 0.6 * loopHeight);
					loopCheckbox.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20.0 + waveformHeight + 10.0 + sizeHeight + startHeight + endHeight + 0.2 * loopHeight);
					loopLabel.moveTo (x0 + 0.4 * w + 5 + loopHeight, y0 + pathNameHeight + 20.0 + waveformHeight + 10.0 + sizeHeight + startHeight + endHeight);

					sizeLabel.show();
					startLabel.show();
					endLabel.show();
					loopCheckbox.show();
					loopLabel.show();
				}

				else
				{
					sizeLabel.hide();
					startLabel.hide();
					endLabel.hide();
					loopCheckbox.hide();
					loopLabel.hide();
					noFileLabel.hide();
				}
			}

			else
			{
				sizeLabel.hide();
				startLabel.hide();
				endLabel.hide();
				loopCheckbox.hide();
				loopLabel.hide();
			}

			waveform.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20);
			waveform.resize (0.6 * w - 15, waveformHeight);
			drawWaveform();
			waveform.show();

			scrollbar.moveTo (x0 + 0.4 * w + 5, y0 + pathNameHeight + 20 + waveformHeight - 12);
			scrollbar.resize (0.6 * w - 15, 10);
			if (sample && (sample->info.frames > 0))
			{
				startMarker.show();
				endMarker.show();
				scrollbar.show();
				noFileLabel.hide();
			}
			else
			{
				startMarker.hide();
				endMarker.hide();
				scrollbar.hide();
				noFileLabel.resize ();
				noFileLabel.moveTo
				(
					x0 + 0.4 * w + 5 + 0.3 * w - 7.5 - 0.5 * noFileLabel.getWidth(),
					y0 + pathNameHeight + 20 + 0.5 * waveformHeight - 0.5 * noFileLabel.getHeight()
				);
				noFileLabel.show();
			}
		}
		else
		{
			fileListBox.hide();
			waveform.hide();
			scrollbar.hide();
			startMarker.hide();
			endMarker.hide();
			sizeLabel.hide();
			startLabel.hide();
			endLabel.hide();
			loopCheckbox.hide();
			loopLabel.hide();
			noFileLabel.hide();
		}

		confirmLabel.resize();
		const double confirmBoxWidth = (confirmLabel.getWidth() + 40 > 2 * okWidth + 60 ? confirmLabel.getWidth() + 40 : 2 * okWidth + 60);
		const double confirmBoxHeight = confirmLabel.getHeight() + okHeight + 60;
		confirmBox.resize (confirmBoxWidth, confirmBoxHeight);
		confirmBox.moveTo (0.5 * getWidth() - 0.5 * confirmBoxWidth, 0.5 * getHeight() - 0.5 * confirmBoxHeight);
		confirmLabel.moveTo (20, 20);
		confirmCancelButton.moveTo (0.5 * confirmBoxWidth - okWidth - 10, confirmBoxHeight - okHeight - 10);
		confirmCancelButton.resize (okWidth, okHeight);
		confirmOkButton.moveTo (0.5 * confirmBoxWidth + 10, confirmBoxHeight - okHeight - 10);
		confirmOkButton.resize (okWidth, okHeight);
		confirmBox.show();

		createLabel.resize();
		createInput.resize();
		createError.resize();
		const double createLabelsWidth = (createLabel.getWidth() > createError.getWidth() ? createLabel.getWidth() : createError.getWidth());
		const double createBoxWidth = (createLabelsWidth + 40 > 2 * okWidth + 100 ? createLabelsWidth + 40 : 2 * okWidth + 100);
		const double createBoxHeight = createLabel.getHeight() + createInput.getHeight() + createError.getHeight() + okHeight + 60;
		createBox.resize (createBoxWidth, createBoxHeight);
		createBox.moveTo (0.5 * getWidth() - 0.5 * createBoxWidth, 0.5 * getHeight() - 0.5 * createBoxHeight);
		createLabel.moveTo (20, 20);
		createInput.resize (createBoxWidth - 40, createInput.getHeight());
		createInput.moveTo (20, 30 + createLabel.getHeight());
		createError.moveTo (20, 40 + createLabel.getHeight() + createInput.getHeight());
		createCancelButton.moveTo (0.5 * createBoxWidth - okWidth - 10, createBoxHeight - okHeight - 10);
		createCancelButton.resize (okWidth, okHeight);
		createOkButton.moveTo (0.5 * createBoxWidth + 10, createBoxHeight - okHeight - 10);
		createOkButton.resize (okWidth, okHeight);
		createBox.show();
	}

	else
	{
		okButton.hide();
		cancelButton.hide();
		fileListBox.hide();
		waveform.hide();
		scrollbar.hide();
		startMarker.hide();
		endMarker.hide();
		sizeLabel.hide();
		startLabel.hide();
		endLabel.hide();
		loopCheckbox.hide();
		loopLabel.hide();
		noFileLabel.hide();
		fileNameLabel.hide();
		fileNameBox.hide();
		filterPopupListBox.hide ();
		confirmBox.hide();
		createBox.hide();
	}

	Widget::update();
}

void SampleChooser::applyTheme (BStyles::Theme& theme) {applyTheme (theme, name_);}

void SampleChooser::applyTheme (BStyles::Theme& theme, const std::string& name)
{
	FileChooser::applyTheme (theme, name);
	waveform.applyTheme (theme, name + "/textbox");
	scrollbar.applyTheme (theme, name + "/scrollbar");
	startMarker.applyTheme (theme, name + "/marker");
	endMarker.applyTheme (theme, name + "/marker");
	sizeLabel.applyTheme (theme, name + "/label");
	startLabel.applyTheme (theme, name + "/label");
	endLabel.applyTheme (theme, name + "/label");
	loopCheckbox.applyTheme (theme, name + "/checkbox");
	loopLabel.applyTheme (theme, name + "/label");
	noFileLabel.applyTheme (theme, name + "/label");
}

void SampleChooser::sfileListBoxClickedCallback (BEvents::Event* event)
{
	if (!event) return;
	BWidgets::ListBox* w = (BWidgets::ListBox*)event->getWidget();
	if (!w) return;
	SampleChooser* fc = (SampleChooser*)w->getParent();
	if (!fc) return;

	double val = w->getValue();

	if ((val != UNSELECTED) && (!fc->fileNameBox.getEditMode()))
	{
		// Directory selected -> one click chdir
		if (val <= fc->dirs.size())
		{
			fc->fileNameBox.setText ("");
			if (fc->sample)
			{
				delete (fc->sample);
				fc->sample = nullptr;
			}
			BEvents::ValueChangedEvent dummyEvent = BEvents::ValueChangedEvent (&fc->okButton, 1.0);
			fc->noFileLabel.setText (fc->labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_NO_FILE_INDEX]);
			fc->okButtonClickedCallback (&dummyEvent);
			fc->update();
		}

		// File selected
		else
		{
			BItems::Item* ai = w->getActiveItem();
			if (ai)
			{
				BWidgets::Label* ail = (BWidgets::Label*)ai->getWidget();
				if (ail) fc->setFileName (ail->getText());
			}
		}
	}
}

void SampleChooser::scrollbarChangedCallback (BEvents::Event* event)
{
	if (!event) return;
	HRangeScrollbar::EndButton* w = (HRangeScrollbar::EndButton*)event->getWidget();
	if (!w) return;
	HRangeScrollbar* hrs = (HRangeScrollbar*)w->getParent();
	if (!hrs) return;
	SampleChooser* fc = (SampleChooser*)hrs->getParent();
	if (!fc) return;

	fc->drawWaveform();
}

void SampleChooser::lineDraggedCallback (BEvents::Event* event)
{
	if (!event) return;
	BEvents::PointerEvent* pev = (BEvents::PointerEvent*)event;
	VLine* w = (VLine*)event->getWidget();
	if (!w) return;
	BWidgets::DrawingSurface* ds = (BWidgets::DrawingSurface*)w->getParent();
	if (!ds) return;
	SampleChooser* fc = (SampleChooser*)ds->getParent();
	if ((!fc) || (!fc->sample) || (fc->sample->info.frames == 0) || (fc->waveform.getEffectiveWidth() <= 0.0)) return;

	const double start = fc->scrollbar.minButton.getValue();
	const double range = fc->scrollbar.maxButton.getValue() - start;
	const double dp = pev->getDelta().x / fc->waveform.getEffectiveWidth();
	const double df = dp * range * double (fc->sample->info.frames);

	if (w == &fc->startMarker) fc->sample->start = LIMIT (fc->sample->start + df, 0, fc->sample->info.frames - 1);
	else if (w == &fc->endMarker) fc->sample->end = LIMIT (fc->sample->end + df, 1, fc->sample->info.frames);

	if (fc->sample->start >= fc->sample->end) fc->sample->start = fc->sample->end - 1;
	fc->drawWaveform();
}

void SampleChooser::filenameEnteredCallback (BEvents::Event* event)
{
	if (!event) return;
	if (!event->getWidget()) return;
	BWidgets::Label* l = (BWidgets::Label*)event->getWidget();
	SampleChooser* p = (SampleChooser*)l->getParent();
	if (!p) return;

	const std::string s = l->getText();
	l->setText ("");
	p->setFileName (s);
}

void SampleChooser::drawWaveform()
{
	const double x0 = waveform.getXOffset();
	const double y0 = waveform.getYOffset();
	const double w = waveform.getEffectiveWidth();
	const double h = waveform.getEffectiveHeight();

	cairo_surface_clear (waveform.getDrawingSurface());
	cairo_t* cr = cairo_create (waveform.getDrawingSurface ());
	if (cr && cairo_status (cr) == CAIRO_STATUS_SUCCESS)
	{
		if (sample && (sample->info.frames) && (sample->info.samplerate) && (w >= 1.0))
		{
			// Scan for min/max
			const double start = scrollbar.minButton.getValue();
			const double range = scrollbar.maxButton.getValue() - start;
			double max = 1.0;
			for (double i = 0.0; i < 1.0; i += 0.25 / w)
			{
				if (fabs (sample->get (i * double (sample->info.frames), 0, sample->info.samplerate)) > max)
				{
					max = fabs (sample->get (i * double (sample->info.frames), 0, sample->info.samplerate));
				}
			}

			cairo_set_line_width (cr, 0.1);
			double lo = sample->get (start * double (sample->info.frames), 0, sample->info.samplerate);
			double hi = lo;
			const double step = 0.1 / w;
			const double minstep = LIMIT (1.0 / (range * double (sample->info.frames)), 0.01 * step, step);
			for (double x = 0; x < 1.0; x += step)
			{
				double s = 0;
				for (double xm = 0; xm < step; xm += minstep)
				{
					const double f = (start + (x + xm) * range) * double (sample->info.frames);
					s = sample->get (f, 0, sample->info.samplerate);
					if (s > hi) hi = s;
					if (s < lo) lo = s;
				}
				const double frame = (start + x * range) * double (sample->info.frames);
				if ((frame >= sample->start) && (frame <= sample->end)) cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
				else cairo_set_source_rgba (cr, 0.25, 0.25, 0.25, 1.0);
				cairo_move_to (cr, x0 + x * w, y0 + 0.5 * h - 0.5 * h * lo / max);
				cairo_line_to (cr, x0 + x * w, y0 + 0.5 * h - 0.5 * h * hi / max);
				cairo_stroke (cr);
				lo = s;
				hi = s + 2.0 * max / h;
			}

			// Set start and end line
			if (range > 0)
			{
				const double sp = (sample->start / double (sample->info.frames) - start) / range;
				startMarker.moveTo (x0 + sp * w - 0.5 * startMarker.getWidth(), 0.0);
				const double ep = (sample->end / double (sample->info.frames) - start) / range;
				endMarker.moveTo (x0 + ep * w - 0.5 * endMarker.getWidth(), 0.0);
			}

			else
			{
				startMarker.moveTo (-startMarker.getWidth(), 0.0);
				endMarker.moveTo (-startMarker.getWidth(), 0.0);
			}

			// Update labels
			sizeLabel.setText
			(
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_FILE_INDEX] + ": " +
				std::to_string (int (sample->info.frames / (sample->info.samplerate * 60))) +
				":" +
				std::to_string ((int (sample->info.frames / sample->info.samplerate) % 60) / 10) +
				std::to_string ((int (sample->info.frames / sample->info.samplerate) % 60) % 10) +
				" (" +
				std::to_string (sample->info.frames) +
				") " +
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_FRAMES_INDEX]
			);
			startLabel.setText
			(
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_SELECTION_START_INDEX] + ": " +
				std::to_string (int (sample->start / (sample->info.samplerate * 60))) +
				":" +
				std::to_string ((int (sample->start / sample->info.samplerate) % 60) / 10) +
				std::to_string ((int (sample->start / sample->info.samplerate) % 60) % 10) +
				" (" +
				std::to_string (sample->start) +
				") " +
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_FRAMES_INDEX]
			);
			endLabel.setText
			(
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_SELECTION_END_INDEX] + ": " +
				std::to_string (int (sample->end / (sample->info.samplerate * 60))) +
				":" +
				std::to_string ((int (sample->end / sample->info.samplerate) % 60) / 10) +
				std::to_string ((int (sample->end / sample->info.samplerate) % 60) % 10) +
				" (" +
				std::to_string (sample->end) +
				") " +
				labels[BWIDGETS_DEFAULT_SAMPLECHOOSER_FRAMES_INDEX]
			);
			sizeLabel.resize();
			startLabel.resize();
			endLabel.resize();
		}

		else
		{

		}
		cairo_destroy (cr);
	}
	waveform.update ();
}

std::function<void (BEvents::Event*)> SampleChooser::getFileListBoxClickedCallback()
{
	return sfileListBoxClickedCallback;
}
