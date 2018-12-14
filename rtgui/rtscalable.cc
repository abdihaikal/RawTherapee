/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2018 Jean-Christophe FRISCH <natureh.510@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rtscalable.h"
#include <glib/gstdio.h>
#include <regex>
#include <glibmm.h>
#include <iostream>
#include <librsvg/rsvg.h>
#include "options.h"

double RTScalable::dpi = 0.;
int RTScalable::scale = 0;

extern Glib::ustring argv0;
extern Options options;
extern float fontScale;
extern unsigned char scale;
Gtk::TextDirection RTScalable::direction = Gtk::TextDirection::TEXT_DIR_NONE;

void RTScalable::setDPInScale (const double newDPI, const int newScale)
{
    if (scale != newScale || (scale == 1 && dpi != newDPI)) {
        // reload all images
        scale = newScale;
        // HOMBRE: On windows, if scale = 2, the dpi is non significant, i.e. should be considered = 192 ; don't know for linux/macos
        dpi = newDPI;
        if (scale == 2 && newDPI < 192) {
            dpi *= 2;
        }
        //printf("RTScalable::setDPInScale  /  New scale = %d & new DPI = %.3f (%.3f asked) -> Reloading all RTScalable\n", scale, dpi, newDPI);
    }
}

double RTScalable::getDPI ()
{
    return dpi;
}

double RTScalable::getTweakedDPI ()
{
    return dpi * fontScale;
}

int RTScalable::getScale ()
{
    return scale;
}

Gtk::TextDirection RTScalable::getDirection()
{
    return direction;
}

void RTScalable::init(Gtk::Window *window)
{
    setDPInScale(window->get_screen()->get_resolution(), ::scale);
    direction = window->get_direction();
}

/*
 * This function try to find the svg file converted to png in a cache and return
 * the Cairo::ImageSurface. If it can't find it, it will generate it.
 *
 * If the provided filename doesn't end with ".svg" (and then we're assuming it's a png file),
 * it will try to load that file directly from the source images folder. Scaling is disabled
 * for anything else than svg files.
 *
 * This function will always return a usable value, but it might be a garbage image
 * if something went wrong.
 */
Cairo::RefPtr<Cairo::ImageSurface> RTScalable::loadImage(const Glib::ustring &fname, double dpi)
{
    // Magic color       : #2a7fff
    // Dark theme color  : #CCCCCC
    // Light theme color : #252525  -- not used

    Glib::ustring imagesFolder = Glib::build_filename (argv0, "images");
    Glib::ustring imagesCacheFolder = Glib::build_filename (options.cacheBaseDir, "svg2png");

    // -------------------- Looking for the cached PNG file first --------------------

    Glib::ustring imagesCacheFolderDPI = Glib::build_filename (imagesCacheFolder, Glib::ustring::compose("%1", (int)dpi));
    auto path = Glib::build_filename(imagesCacheFolderDPI, fname);

    if (Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS)) {
        return Cairo::ImageSurface::create_from_png(path);
    } else {

        // -------------------- Looking for the PNG file in install directory --------------------

        path = Glib::build_filename(imagesFolder, fname);
        if (Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS)) {
            return Cairo::ImageSurface::create_from_png(path);
        }
    }

    // Last chance: looking for the svg file and creating the cached image file

    // -------------------- Creating the cache folder for PNGs --------------------

    if (!Glib::file_test(imagesCacheFolderDPI.c_str(), Glib::FILE_TEST_EXISTS)) {
        auto error = g_mkdir_with_parents (imagesCacheFolderDPI.c_str(), 0777);
        if (error != 0) {
            std::cerr << "ERROR: Can't create \"" << imagesCacheFolderDPI << "\" cache folder: " << g_strerror(error)  << std::endl;
            Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, 10, 10);
            return surf;
        }
    }

    // -------------------- Loading the SVG file --------------------

    std::string svgFile;
    Glib::ustring iconNameSVG;
    if (fname.find(".png") != Glib::ustring::npos) {
        iconNameSVG = fname.substr(0, fname.length() - 3) + Glib::ustring("svg");
    }
    try {
        path = Glib::build_filename (imagesFolder, iconNameSVG);
        //printf("Trying to get content of %s\n", path.c_str());
        svgFile = Glib::file_get_contents(Glib::build_filename (imagesFolder, iconNameSVG));
    }
    catch (Glib::FileError &err) {
        std::cerr << "ERROR: " << err.what() << std::endl;
        Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, 10, 10);
        return surf;
    }

    // -------------------- Updating the the magic color --------------------

    std::string updatedSVG = std::regex_replace(svgFile, std::regex("#2a7fff"), "#CCCCCC");

    // -------------------- Creating the rsvg handle --------------------

    GError **error = nullptr;
    RsvgHandle *handle = rsvg_handle_new_from_data((unsigned const char*)updatedSVG.c_str(), updatedSVG.length(), error);

    if (handle == nullptr) {
        std::cerr << "ERROR: Can't use the provided data for \"" << fname << "\" to create a RsvgHandle:" << std::endl
                  << Glib::ustring((*error)->message) << std::endl;
        Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, 10, 10);
        return surf;
    }

    // -------------------- Drawing the image to a Cairo::ImageSurface --------------------

    RsvgDimensionData dim;
    rsvg_handle_get_dimensions(handle, &dim);
    double r = dpi / 96.;
    Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, (int)(dim.width * r + 0.499), (int)(dim.height * r + 0.499));
    Cairo::RefPtr<Cairo::Context> c = Cairo::Context::create(surf);
    c->set_source_rgba (0., 0., 0., 0.);
    c->set_operator (Cairo::OPERATOR_CLEAR);
    c->paint ();
    c->set_operator (Cairo::OPERATOR_OVER);
    c->scale(r, r);
    rsvg_handle_render_cairo(handle, c->cobj());
    rsvg_handle_free(handle);

    // -------------------- Saving the image in cache --------------------

    surf->write_to_png(Glib::build_filename(imagesCacheFolderDPI, fname));

    // -------------------- Finished! Pfeeew ! --------------------

    return surf;
}
