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
#include <data/field/image.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/settings.hpp>
#include <script/functions/json.hpp>
#include <gui/util.hpp>
#include <render/card/viewer.hpp>
#include <wx/filename.h>

class ZoomedUnrotatedDataViewer : public DataViewer {
public:
  ZoomedUnrotatedDataViewer(double zoom) : zoom(zoom) {};
  virtual ~ZoomedUnrotatedDataViewer() {};
  Rotation getRotation() const override;
private:
  double zoom;
};

Rotation ZoomedUnrotatedDataViewer::getRotation() const {
  return Rotation(0.0, stylesheet->getCardRect(), zoom);
}

// ----------------------------------------------------------------------------- : wxImage export

Image export_image(const SetP& set, const CardP& card, const bool write_metadata, const double zoom, const Radians angle_radians, const double bleed_pixels) {
  if (!set) throw Error(_("no set"));
  /// create and zoom
  ZoomedUnrotatedDataViewer viewer = ZoomedUnrotatedDataViewer(zoom);
  viewer.setSet(set);
  viewer.setCard(card);
  RealSize size = viewer.getRotation().getExternalSize();
  int bleed = lround(bleed_pixels);
  Bitmap bitmap((int)size.width + 2 * bleed, (int)size.height + 2 * bleed);
  if (!bitmap.Ok()) throw InternalError(_("Unable to create bitmap"));
  wxMemoryDC dc;
  dc.SelectObject(bitmap);
  dc.SetDeviceOrigin(bleed, bleed);
  viewer.draw(dc);
  dc.SelectObject(wxNullBitmap);
  Image img = bitmap.ConvertToImage();

  /// rotate
  img = rotate_image(img, angle_radians);
  int width = img.GetWidth(), height = img.GetHeight();

  /// add print bleed edge
  if (size.width < 1.0 || size.height < 1.0) {
    queue_message(MESSAGE_ERROR, _("Cannot add bleed edge to empty image"));
  }
  else {
    if (!img.HasAlpha()) img.InitAlpha();
    Byte* pixels = img.GetData();
    Byte* alpha = img.GetAlpha();
    // fill top left corner
    int pixel;
    int x_start = 0;
    int y_start = 0;
    int ref = bleed + bleed * width;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill top right corner
    x_start = width - bleed;
    y_start = 0;
    ref = x_start - 1 + bleed * width;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill bottom left corner
    x_start = 0;
    y_start = height - bleed;
    ref = bleed + (y_start - 1) * width;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill bottom right corner
    x_start = width - bleed;
    y_start = height - bleed;
    ref = (x_start - 1) + (y_start - 1) * width;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill left border
    x_start = 0;
    y_start = bleed;
    for (int y = 0; y < height - bleed - bleed; ++y) {
      ref = bleed + (y_start + y) * width;
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill top border
    x_start = bleed;
    y_start = 0;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < width - bleed - bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        ref = x_start + x + bleed * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill right border
    x_start = width - bleed;
    y_start = bleed;
    for (int y = 0; y < height - bleed - bleed; ++y) {
      ref = width - bleed - 1 + (y_start + y) * width;
      for (int x = 0; x < bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
    // fill bottom border
    x_start = bleed;
    y_start = height - bleed;
    for (int y = 0; y < bleed; ++y) {
      for (int x = 0; x < width - bleed - bleed; ++x) {
        pixel = x_start + x + (y_start + y) * width;
        ref = x_start + x + (height - bleed - 1) * width;
        pixels[3 * pixel + 0] = pixels[3 * ref + 0];
        pixels[3 * pixel + 1] = pixels[3 * ref + 1];
        pixels[3 * pixel + 2] = pixels[3 * ref + 2];
        alpha[pixel] = alpha[ref];
      }
    }
  }

  /// add metadata
  if (write_metadata) {
    String metadata = _("<mse-card-data>[");
    IndexMap<FieldP, ValueP>& card_data = card->data;
    boost::json::object cardv = mse_to_json(card, set.get());
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
          std::string rect = style->getExternalRectString(zoom, angle_radians, bleed_pixels, width, height, 0).ToStdString();
          cardv_data[field->name.ToStdString()] = rect;
        }
      }
    }
    metadata += json_ugly_print(cardv) + _("]</mse-card-data>");
    img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }

  return img;
}

Image export_image( const SetP& set, const vector<CardP>& cards,
                    const int padding,
                    const double global_zoom,
                    const bool use_zoom_setting,
                    const bool use_rotation_setting,
                    const bool use_bleed_setting) {
  if (!set) throw Error(_("no set"));
  if (cards.size() == 0) throw Error(_("no cards"));
  vector<Image> imgs;
  vector<int> offsets;
  vector<double> zooms;
  vector<double> angles;
  vector<double> bleeds;
  // Draw card images
  FOR_EACH(card, cards) {
    Settings::ExportSettings card_settings = settings.exportSettingsFor(set->stylesheetFor(card));
    double zoom = use_zoom_setting ? global_zoom * card_settings.zoom : global_zoom;
    double angle = use_rotation_setting ? card_settings.angle_radians : 0.0;
    double bleed = use_bleed_setting ? card_settings.bleed_pixels : 0.0;
    imgs.push_back(export_image(set, card, false, zoom, angle, bleed));
    zooms.push_back(zoom);
    angles.push_back(angle);
    bleeds.push_back(bleed);
  }
  int global_width = 0;
  int global_height = 0;
  vector<int> widths;
  vector<int> heights;
  FOR_EACH(img, imgs) {
    int width = img.GetWidth();
    int height = img.GetHeight();
    widths.push_back(width);
    heights.push_back(height);
    offsets.push_back(global_width);
    global_width += padding + width;
    global_height = max(global_height, height);
  }
  global_width -= padding;
  // Draw global image
  Image global_img = Image(global_width, global_height);
  if (!global_img.Ok()) throw InternalError(_("Unable to create image"));
  global_img.InitAlpha();
  Byte* pixels = global_img.GetData();
  Byte* alpha = global_img.GetAlpha();
  // fill with transparent
  for (UInt i = 0; i < global_width*global_height; ++i) {
    pixels[3 * i + 0] = 0;
    pixels[3 * i + 1] = 0;
    pixels[3 * i + 2] = 0;
    alpha[i] = 0;
  }
  // Paste card images
  FOR_EACH_2(img, imgs, offset, offsets) {
    global_img.Paste(img, offset, 0);
  }
  // Write metadata
  String metadata = _("<mse-card-data>[");
  for (int i = 0; i < cards.size(); ++i) {
    if (i > 0) metadata += _(",");
    CardP card = cards[i];
    IndexMap<FieldP, ValueP>& card_data = card->data;
    boost::json::object cardv = mse_to_json(card, set.get());
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
          Rotation rotation();
          std::string rect = style->getExternalRectString(zooms[i], angles[i], bleeds[i], widths[i], heights[i], offsets[i]).ToStdString();
          cardv_data[field->name.ToStdString()] = rect;
        }
      }
    }
    metadata += json_ugly_print(cardv);
  }
  metadata += _("]</mse-card-data>");
  global_img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);

  return global_img;
}

void export_image(const SetP& set, const CardP& card, const String& filename) {
  const StyleSheet& stylesheet = set->stylesheetFor(card);
  StyleSheetSettings& stylesheet_settings = settings.stylesheetSettingsFor(stylesheet);
  Settings::ExportSettings export_settings = settings.exportSettingsFor(stylesheet);
  Image img = export_image(set, card, true, export_settings.zoom, export_settings.angle_radians, export_settings.bleed_pixels);
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
