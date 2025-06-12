//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/tagged_string.hpp>
#include <data/format/formats.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/settings.hpp>
#include <gui/util.hpp>
#include <render/card/viewer.hpp>
#include <wx/filename.h>

// ----------------------------------------------------------------------------- : Single card export

void export_image(const SetP& set, const CardP& card, const String& filename) {
  Image img = export_bitmap(set, card).ConvertToImage();
  img.SaveFile(filename);  // can't use Bitmap::saveFile, it wants to know the file type
              // but image.saveFile determines it automagicly
}

class UnzoomedDataViewer : public DataViewer {
public:
  UnzoomedDataViewer();
  UnzoomedDataViewer(double zoom, double angle);
  virtual ~UnzoomedDataViewer() {};
  Rotation getRotation() const override;
private:
  double zoom;
  double angle;
  bool declared_values;
};

UnzoomedDataViewer::UnzoomedDataViewer()
  : zoom(1.0)
  , angle(0.0)
  , declared_values(false)
{}

UnzoomedDataViewer::UnzoomedDataViewer(const double zoom, const Radians angle = 0.0)
  : zoom(zoom)
  , angle(angle)
  , declared_values(true)
{}

Rotation UnzoomedDataViewer::getRotation() const {
  if (!stylesheet) stylesheet = set->stylesheet;
  if (declared_values) {
    return Rotation(angle, stylesheet->getCardRect(), zoom, 1.0, ROTATION_ATTACH_TOP_LEFT);
  }

  double export_zoom = settings.stylesheetSettingsFor(set->stylesheetFor(card)).export_zoom();
  bool use_viewer_rotation = !settings.stylesheetSettingsFor(set->stylesheetFor(card)).card_normal_export();

  if (use_viewer_rotation) {
    return Rotation(DataViewer::getRotation().getAngle(), stylesheet->getCardRect(), export_zoom, 1.0, ROTATION_ATTACH_TOP_LEFT);
  } else {
    return Rotation(angle, stylesheet->getCardRect(), export_zoom, 1.0, ROTATION_ATTACH_TOP_LEFT);
  }
}

Bitmap export_bitmap(const SetP& set, const CardP& card) {
  if (!set) throw Error(_("no set"));
  UnzoomedDataViewer viewer = UnzoomedDataViewer();
  viewer.setSet(set);
  viewer.setCard(card);
  // size of cards
  RealSize size = viewer.getRotation().getExternalSize();
  // create bitmap & dc
  Bitmap bitmap((int)size.width, (int)size.height);
  if (!bitmap.Ok()) throw InternalError(_("Unable to create bitmap"));
  wxMemoryDC dc;
  dc.SelectObject(bitmap);
  // draw
  viewer.draw(dc);
  dc.SelectObject(wxNullBitmap);
  return bitmap;
}

Bitmap export_bitmap(const SetP& set, const CardP& card, const double zoom, const Radians angle = 0.0) {
  if (!set) throw Error(_("no set"));
  UnzoomedDataViewer viewer = UnzoomedDataViewer(zoom, angle);
  viewer.setSet(set);
  viewer.setCard(card);
  // size of cards
  RealSize size = viewer.getRotation().getExternalSize();
  // create bitmap & dc
  Bitmap bitmap((int)size.width, (int)size.height);
  if (!bitmap.Ok()) throw InternalError(_("Unable to create bitmap"));
  wxMemoryDC dc;
  dc.SelectObject(bitmap);
  // draw
  viewer.draw(dc);
  dc.SelectObject(wxNullBitmap);
  return bitmap;
}

// put multiple card images into one bitmap
Bitmap export_bitmap(const SetP& set, const vector<CardP>& cards, bool scale_to_lowest_dpi, int padding, const double zoom, const Radians angle = 0.0) {
  if (!set) throw Error(_("no set"));
  vector<Bitmap> bitmaps;
  int width = 0;
  int height = 0;
  double lowest_dpi = 1200.0;
  if (scale_to_lowest_dpi) {
    FOR_EACH(card, cards) {
      lowest_dpi = min(lowest_dpi, set->stylesheetFor(card).card_dpi);
    }
    lowest_dpi = max(lowest_dpi, 150.0);
  }
  // Draw card bitmaps
  FOR_EACH(card, cards) {
    double scaled_zoom = zoom;
    if (scale_to_lowest_dpi) {
      double dpi = max(set->stylesheetFor(card).card_dpi, 150.0);
      scaled_zoom *= lowest_dpi / dpi;
    }
    UnzoomedDataViewer viewer = UnzoomedDataViewer(scaled_zoom, angle);
    viewer.setSet(set);
    viewer.setCard(card);
    RealSize size = viewer.getRotation().getExternalSize();
    Bitmap bitmap((int)size.width, (int)size.height);
    if (!bitmap.Ok()) throw InternalError(_("Unable to create bitmap"));
    wxMemoryDC bufferDC;
    bufferDC.SelectObject(bitmap);
    clearDC(bufferDC, *wxWHITE_BRUSH);
    viewer.draw(bufferDC);
    bufferDC.SelectObject(wxNullBitmap);
    width += (int)size.width;
    height = max(height, (int)size.height);
    bitmaps.push_back(bitmap);
  }
  // Draw global bitmap
  Bitmap global_bitmap(width + (bitmaps.size()-1) * padding, height);
  if (!global_bitmap.Ok()) throw InternalError(_("Unable to create bitmap"));
  wxMemoryDC globalDC;
  globalDC.SelectObject(global_bitmap);
  clearDC(globalDC, *wxWHITE_BRUSH);
  int offset = 0;
  FOR_EACH(bitmap, bitmaps) {
    globalDC.SetDeviceOrigin(offset, 0);
    globalDC.DrawBitmap(bitmap, 0, 0);
    offset += bitmap.GetWidth() + padding;
  }
  globalDC.SelectObject(wxNullBitmap);
  return global_bitmap;
}

// ----------------------------------------------------------------------------- : Multiple card export


void export_images(const SetP& set, const vector<CardP>& cards,
                   const String& path, const String& filename_template, FilenameConflicts conflicts)
{
  wxBusyCursor busy;
  // Script
  ScriptP filename_script = parse(filename_template, nullptr, true);
  // Path
  wxFileName fn(path);
  // Export
  std::set<String> used; // for CONFLICT_NUMBER_OVERWRITE
  FOR_EACH_CONST(card, cards) {
    // filename for this card
    Context& ctx = set->getContext(card);
    String filename = clean_filename(untag(ctx.eval(*filename_script)->toString()));
    if (!filename) continue; // no filename -> no saving
    // full path
    fn.SetFullName(filename);
    // does the file exist?
    if (!resolve_filename_conflicts(fn, conflicts, used)) continue;
    // write image
    filename = fn.GetFullPath();
    used.insert(filename);
    export_image(set, card, filename);
  }
}
