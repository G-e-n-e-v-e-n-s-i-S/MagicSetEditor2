//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/tagged_string.hpp>
#include <data/format/formats.hpp>
#include <data/format/clipboard.hpp>
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/settings.hpp>
#include <script/functions/json.hpp>
#include <gui/util.hpp>
#include <render/card/viewer.hpp>
#include <wx/filename.h>

// ----------------------------------------------------------------------------- : Card export

class UnzoomedDataViewer : public DataViewer {
public:
  UnzoomedDataViewer();
  UnzoomedDataViewer(double zoom, Radians angle);
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

  double export_zoom = settings.exportZoomSettingsFor(set->stylesheetFor(card));
  bool use_viewer_rotation = !settings.stylesheetSettingsFor(set->stylesheetFor(card)).card_normal_export();

  if (use_viewer_rotation) {
    return Rotation(DataViewer::getRotation().getAngle(), stylesheet->getCardRect(), export_zoom, 1.0, ROTATION_ATTACH_TOP_LEFT);
  } else {
    return Rotation(angle, stylesheet->getCardRect(), export_zoom, 1.0, ROTATION_ATTACH_TOP_LEFT);
  }
}

// ----------------------------------------------------------------------------- : wxBitmap export

Bitmap export_bitmap(const SetP& set, const CardP& card, const double zoom, const Radians angle_radians) {
  if (!set) throw Error(_("no set"));
  UnzoomedDataViewer viewer = UnzoomedDataViewer(zoom, angle_radians);
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

Bitmap export_bitmap(const SetP& set, const vector<CardP>& cards, bool scale_to_lowest_dpi, int padding, const double zoom, const Radians angle_radians, vector<double>& scales_out, vector<int>& offsets_out) {
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
    double scale = zoom;
    if (scale_to_lowest_dpi) {
      double dpi = max(set->stylesheetFor(card).card_dpi, 150.0);
      scale *= lowest_dpi / dpi;
    }
    scales_out.push_back(scale);
    UnzoomedDataViewer viewer = UnzoomedDataViewer(scale, angle_radians);
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
    offsets_out.push_back(offset);
    globalDC.SetDeviceOrigin(offset, 0);
    globalDC.DrawBitmap(bitmap, 0, 0);
    offset += bitmap.GetWidth() + padding;
  }
  globalDC.SelectObject(wxNullBitmap);
  return global_bitmap;
}

// ----------------------------------------------------------------------------- : wxImage export

Image export_image(const SetP& set, const CardP& card, const double zoom, const Radians angle_radians) {
  Bitmap bitmap = export_bitmap(set, card, zoom, angle_radians);
  Image img = bitmap.ConvertToImage();
  String data = _("<mse-data-start>[");
  IndexMap<FieldP, ValueP>& card_data = card->data;
  boost::json::object& cardv = mse_to_json(card, set.get());
  boost::json::object& cardv_data = cardv["data"].as_object();
  StyleSheetP stylesheet = set->stylesheetForP(card);
  if (!settings.stylesheetSettingsFor(*stylesheet).card_notes_export()) cardv["notes"] = "";
  for(IndexMap<FieldP, ValueP>::iterator it = card_data.begin() ; it != card_data.end() ; ++it) {
    ImageValue* value = dynamic_cast<ImageValue*>(it->get());
    if (value && !value->filename.empty()) {
      FieldP field = (*it)->fieldP;
      StyleP style = stylesheet->card_style.at(field->index);
      if (style) {
        style->update(set->getContext(card));
        std::string rect = style->getExternalRectString(zoom, 0).ToStdString();
        cardv_data[field->name.ToStdString()] = rect;
      }
    }
  }
  data += json_ugly_print(cardv) + _("]<mse-data-end>");
  img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, data);
  return img;
}

Image export_image(const SetP& set, const vector<CardP>& cards, bool scale_to_lowest_dpi, int padding, const double zoom, const Radians angle_radians) {
  vector<double> scales;
  vector<int> offsets;
  Bitmap bitmap = export_bitmap(set, cards, scale_to_lowest_dpi, padding, zoom, angle_radians, scales, offsets);
  Image img = bitmap.ConvertToImage();
  String data = _("<mse-data-start>[");
  for (int i = 0; i < cards.size(); ++i) {
    if (i > 0) data += _(",");
    CardP card = cards[i];
    IndexMap<FieldP, ValueP>& card_data = card->data;
    boost::json::object& cardv = mse_to_json(card, set.get());
    boost::json::object& cardv_data = cardv["data"].as_object();
    StyleSheetP stylesheet = set->stylesheetForP(card);
    if (!settings.stylesheetSettingsFor(*stylesheet).card_notes_export()) cardv["notes"] = "";
    for(IndexMap<FieldP, ValueP>::iterator it = card_data.begin() ; it != card_data.end() ; ++it) {
      ImageValue* value = dynamic_cast<ImageValue*>(it->get());
      if (value && !value->filename.empty()) {
        FieldP field = (*it)->fieldP;
        StyleP style = stylesheet->card_style.at(field->index);
        if (style) {
          style->update(set->getContext(card));
          std::string rect = style->getExternalRectString(scales[i], offsets[i]).ToStdString();
          cardv_data[field->name.ToStdString()] = rect;
        }
      }
    }
    data += json_ugly_print(cardv);
  }
  data += _("]<mse-data-end>");
  img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, data);
  return img;
}

void export_image(const SetP& set, const CardP& card, const String& filename) {
  const StyleSheet& stylesheet = set->stylesheetFor(card);
  StyleSheetSettings& stylesheet_settings = settings.stylesheetSettingsFor(stylesheet);
  double zoom = settings.exportZoomSettingsFor(stylesheet);
  Radians angle = stylesheet_settings.card_normal_export() ? 0.0 : stylesheet_settings.card_angle() / 360.0 * 2.0 * M_PI;
  Image img = export_image(set, card, zoom, angle);
  img.SaveFile(filename);
}

void export_image(const SetP& set, const vector<CardP>& cards,
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
