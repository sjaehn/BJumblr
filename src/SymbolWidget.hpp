/* B.Jumblr
 * Pattern-controlled audio stream / sample re-sequencer LV2 plugin
 *
 * Copyright (C) 2018 - 2020 by Sven Jähnichen
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

#ifndef SYMBOLWIDGET_HPP_
#define SYMBOLWIDGET_HPP_

#include "BWidgets/Widget.hpp"
#include "BWidgets/Label.hpp"
#include "BWidgets/Focusable.hpp"
#include <cmath>

enum SWSymbol
{
        NOSYMBOL        = -1,
        ADDSYMBOL       = 0,
        CLOSESYMBOL     = 1,
        RIGHTSYMBOL     = 2,
        LEFTSYMBOL      = 3,
        PLAYSYMBOL      = 4,
        MIDISYMBOL      = 5
};

const std::string symboltxt[7] = {"", "Insert", "Delete", "Move forward", "Move backward", "Play", "MIDI"};

class SymbolWidget : public BWidgets::Widget, public BWidgets::Focusable
{
protected:
        BColors::ColorSet fgColors_;
        SWSymbol symbol_;
        BWidgets::Label focusLabel_;

public:
        SymbolWidget () : SymbolWidget (0.0, 0.0, 0.0, 0.0, "symbol", NOSYMBOL) {}
        SymbolWidget (const double x, const double y, const double width, const double height, const std::string& name, SWSymbol symbol) :
                Widget (x, y, width, height, name),
                Focusable (std::chrono::milliseconds (BWIDGETS_DEFAULT_FOCUS_IN_MS),
			   std::chrono::milliseconds (BWIDGETS_DEFAULT_FOCUS_OUT_MS)),
                fgColors_ (BColors::whites),
                symbol_ (symbol),
                focusLabel_ (0, 0, 80, 20, name_ + BWIDGETS_DEFAULT_FOCUS_NAME, symboltxt[symbol + 1])
        {
                focusLabel_.setStacking (BWidgets::STACKING_OVERSIZE);
   		focusLabel_.resize ();
   		focusLabel_.hide ();
   		add (focusLabel_);
        }

        SymbolWidget (const SymbolWidget& that) :
		Widget (that), Focusable (that),
                fgColors_ (that.fgColors_),
                symbol_ (that.symbol_),
		focusLabel_ (that.focusLabel_)
	{
		focusLabel_.hide();
		add (focusLabel_);
	}

	SymbolWidget& operator= (const SymbolWidget& that)
	{
		release (&focusLabel_);
		focusLabel_ = that.focusLabel_;
		focusLabel_.hide();

                fgColors_ = that.fgColors_;
                symbol_ = that.symbol_;

		Widget::operator= (that);
		Focusable::operator= (that);

		add (focusLabel_);

		return *this;

	}

	virtual BWidgets::Widget* clone () const override {return new SymbolWidget (*this);}

	virtual void applyTheme (BStyles::Theme& theme, const std::string& name) override
        {
                Widget::applyTheme (theme, name);
                focusLabel_.applyTheme (theme, name + BWIDGETS_DEFAULT_FOCUS_NAME);
		focusLabel_.resize();

        	void* colorsPtr = theme.getStyle(name, BWIDGETS_KEYWORD_FGCOLORS);
        	if (colorsPtr)
                {
                        fgColors_ = *((BColors::ColorSet*) colorsPtr);
                        update ();
                }
        }

        virtual void applyTheme (BStyles::Theme& theme) override {applyTheme (theme, name_);}

        virtual void onFocusIn (BEvents::FocusEvent* event) override
	{
		if (event && event->getWidget())
		{
			BUtilities::Point pos = event->getPosition();
			focusLabel_.moveTo (pos.x - 0.5 * focusLabel_.getWidth(), pos.y - focusLabel_.getHeight());
			focusLabel_.show();
		}
		Widget::onFocusIn (event);
	}

	virtual void onFocusOut (BEvents::FocusEvent* event) override
	{
		if (event && event->getWidget()) focusLabel_.hide();
		Widget::onFocusOut (event);
	}

protected:
	virtual void draw (const BUtilities::RectArea& area) override
	{
                if ((!widgetSurface_) || (cairo_surface_status (widgetSurface_) != CAIRO_STATUS_SUCCESS)) return;

		Widget::draw (area);

		cairo_t* cr = cairo_create (widgetSurface_);

		if (cairo_status (cr) == CAIRO_STATUS_SUCCESS)
		{
			// Limit cairo-drawing area
			cairo_rectangle (cr, area.getX(), area.getY(), area.getWidth(), area.getHeight());
			cairo_clip (cr);

                        double x0 = getXOffset ();
			double y0 = getYOffset ();
			double w = getEffectiveWidth ();
			double h = getEffectiveHeight ();
                        double ext = (w < h ? w : h);

                        switch (symbol_)
                        {
                                case ADDSYMBOL:         cairo_move_to (cr, x0 + w / 2 - ext / 2, y0 + h / 2);
                                                        cairo_line_to (cr, x0 + w / 2 + ext / 2, y0 + h / 2);
                                                        cairo_move_to (cr, x0 + w / 2, y0 + h / 2 - ext / 2);
                                                        cairo_line_to (cr, x0 + w / 2, y0 + h / 2 + ext / 2);
                                                        cairo_set_line_width (cr, 2.0);
                                                        cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                        cairo_stroke (cr);
                                                        break;

                                case CLOSESYMBOL:       cairo_move_to (cr, x0 + w / 2 - ext / 2, y0 + h / 2);
                                                        cairo_line_to (cr, x0 + w / 2 + ext / 2, y0 + h / 2);
                                                        cairo_set_line_width (cr, 2.0);
                                                        cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                        cairo_stroke (cr);
                                                        break;

                                case LEFTSYMBOL:        cairo_move_to (cr, x0 + w / 2 + 0.25 * ext, y0 + h / 2 - ext / 2);
                                                        cairo_line_to (cr, x0 + w / 2 - 0.25 * ext, y0 + h / 2);
                                                        cairo_line_to (cr, x0 + w / 2 + 0.25 * ext, y0 + h / 2 + ext / 2);
                                                        cairo_set_line_width (cr, 2.0);
                                                        cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                        cairo_stroke (cr);
                                                        break;

                                case RIGHTSYMBOL:       cairo_move_to (cr, x0 + w / 2 - 0.25 * ext, y0 + h / 2 - ext / 2);
                                                        cairo_line_to (cr, x0 + w / 2 + 0.25 * ext, y0 + h / 2);
                                                        cairo_line_to (cr, x0 + w / 2 - 0.25 * ext, y0 + h / 2 + ext / 2);
                                                        cairo_set_line_width (cr, 2.0);
                                                        cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                        cairo_stroke (cr);
                                                        break;

                                case PLAYSYMBOL:        cairo_move_to (cr, x0 + 0.25 * w, y0);
                                                        cairo_line_to (cr, x0 + 0.75 * w, y0 + 0.5 * h);
                                                        cairo_line_to (cr, x0 + 0.25 * w, y0 + h);
                                                        cairo_close_path (cr);
                                                        cairo_set_line_width (cr, 0.0);
                                                        if (getState() == BColors::NORMAL)
                                                        {
                                                                cairo_set_line_width (cr, 1.0);
                                                                cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (BColors::ACTIVE)));
                                                                cairo_stroke (cr);
                                                        }
                                                        else
                                                        {
                                                                cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                                cairo_fill (cr);
                                                        }
                                                        break;

                                case MIDISYMBOL:        cairo_set_line_width (cr, 1.0);
                                                        cairo_set_source_rgba (cr, CAIRO_RGBA (*fgColors_.getColor (getState ())));
                                                        for (int i = 0; i < 3; ++i) cairo_rectangle (cr, x0 + 0.5 * w - 0.375 * ext + i * 0.25 * ext, y0 + 0.5 * h - 0.45 * ext, 0.25 * ext, 0.9 * ext);
                                                        cairo_stroke (cr);
                                                        cairo_set_line_width (cr, 0.0);
                                                        for (int i = 0; i < 2; ++i)
                                                        {
                                                                cairo_rectangle (cr, x0 + 0.5 * w - 0.2 * ext + i * 0.25 * ext, y0 + 0.5 * h - 0.45 * ext, 0.15 * ext, 0.6 * ext);
                                                                cairo_fill (cr);
                                                        }

                                default:                break;
                        }

                        cairo_destroy (cr);
		}
	}

};

#endif /* SYMBOLWIDGET_HPP_ */
