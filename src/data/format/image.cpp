//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
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

Image export_image(const SetP& set, const CardP& card, bool write_metadata, double zoom, Radians angle_radians, double bleed_pixels, Bitmap* out_bitmap) {
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

  /// return bitmap if needed
  if (out_bitmap) *out_bitmap = std::move(bitmap);

  /// rotate
  img = rotate_image(img, angle_radians);

  /// add print bleed edge
  int width = img.GetWidth(), height = img.GetHeight();
  bleed = max(0, min((width-1)/2, min((height-1)/2, bleed)));
  if (width < 2*bleed + 2 || height < 2*bleed + 2) {
    queue_message(MESSAGE_ERROR, _("Image too small to add bleed edge"));
  }
  else {
    if (!img.HasAlpha()) img.InitAlpha();
    Byte* pixels = img.GetData();
    Byte* alpha = img.GetAlpha();
    int pixel, mirror, x_start, y_start, x_size, y_size;
    // fill left border
    x_start = 0;
    y_start = bleed;
    x_size = bleed;
    y_size = height - bleed - bleed;
    for (int y = 0; y < y_size; ++y) {
      for (int x = 0; x < x_size; ++x) {
        pixel =    x_start + x + (y_start + y) * width;
        mirror = 2 * bleed - x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
        pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
        pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
        alpha[pixel] = alpha[mirror];
      }
    }
    // fill right border
    x_start = width - bleed;
    y_start = bleed;
    x_size = bleed;
    y_size = height - bleed - bleed;
    for (int y = 0; y < y_size; ++y) {
      for (int x = 0; x < x_size; ++x) {
        pixel =        x_start + x + (y_start + y) * width;
        mirror = - 2 + x_start - x + (y_start + y) * width;
        pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
        pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
        pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
        alpha[pixel] = alpha[mirror];
      }
    }
    // fill top border
    x_start = 0;
    y_start = 0;
    x_size = width;
    y_size = bleed;
    for (int y = 0; y < y_size; ++y) {
      for (int x = 0; x < x_size; ++x) {
        pixel =  x_start + x + (  y_start + y) * width;
        mirror = x_start + x + (2 * bleed - y) * width;
        pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
        pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
        pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
        alpha[pixel] = alpha[mirror];
      }
    }
    // fill bottom border
    x_start = 0;
    y_start = height - bleed;
    x_size = width;
    y_size = bleed;
    for (int y = 0; y < y_size; ++y) {
      for (int x = 0; x < x_size; ++x) {
        pixel =  x_start + x + (      y_start + y) * width;
        mirror = x_start + x + (- 2 + y_start - y) * width;
        pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
        pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
        pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
        alpha[pixel] = alpha[mirror];
      }
    }
  }

  /// add metadata
  if (write_metadata) {
    bool rotated = is_rad90(angle_radians) || is_rad270(angle_radians); // we stored width and height after rotation, but export_metadata expects them before rotation
    String metadata = _("<mse-card-data>[")
      + export_metadata(set, card, zoom, angle_radians, rotated ? height : width, rotated ? width : height, bleed_pixels, bleed_pixels)
      + _("]</mse-card-data>");
    img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }
  
  return img;
}

Image export_image(const SetP& set,
                   const vector<CardP>& cards,
                   int padding,
                   ExportImageMode mode) {
  if (!set) throw Error(_("no set"));
  if (cards.size() == 0) throw Error(_("no cards"));
  vector<Image> imgs;
  vector<int> offsets;
  vector<double> zooms;
  vector<double> angles;
  vector<double> bleeds;
  // Draw card images
  FOR_EACH(card, cards) {
    double zoom, angle, bleed;
    if (mode == ExportImageMode::DEFAULT) {
      zoom = 1.0;
      angle = 0.0;
      bleed = 0.0;
    } else {
      Settings::ExportSettings card_settings = mode == ExportImageMode::CLIPBOARD ?
        settings.clipboardSettingsFor(set->stylesheetFor(card)) :
        settings.exportSettingsFor(set->stylesheetFor(card));
      zoom = card_settings.zoom;
      angle = card_settings.angle_radians;
      bleed = card_settings.bleed_pixels;
    }
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
  for (int i = 0; i < global_width*global_height; ++i) {
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
  for (size_t i = 0; i < cards.size(); ++i) {
    if (i > 0) metadata += _(",");
    CardP card = cards[i];
    bool rotated = is_rad90(angles[i]) || is_rad270(angles[i]); // we stored width and height after rotation, but export_metadata expects them before rotation
    metadata += export_metadata(set, card, zooms[i], angles[i], rotated ? heights[i] : widths[i], rotated ? widths[i] : heights[i], bleeds[i] + offsets[i], bleeds[i]);
  }
  metadata += _("]</mse-card-data>");
  global_img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);

  return global_img;
}

void export_image(const SetP& set, const CardP& card, const String& filename) {
  const StyleSheet& stylesheet = set->stylesheetFor(card);
  // is this card part of a front/back pair that should be combined?
  pair<CardP, CardP> faces = settings.stylesheetSettingsFor(stylesheet).card_dfc_export() ?
                             card->getFrontFaceBackFacePair(*set) :
                             make_pair(CardP(), CardP());
  if (faces.first && faces.second) {
    vector<CardP> combo{faces.first, faces.second};
    Image img = export_image(set, combo);
    img.SaveFile(filename);
    return;
  }
  Settings::ExportSettings export_settings = settings.exportSettingsFor(stylesheet);
  Image img = export_image(set, card, true, export_settings.zoom, export_settings.angle_radians, export_settings.bleed_pixels);
  img.SaveFile(filename);
}

void export_image(const SetP& set, const vector<CardP>& cards, const String& path, const String& filename_template, FilenameConflicts conflicts) {
  wxBusyCursor busy;
  // Script
  ScriptP filename_script = parse(filename_template, nullptr, true);
  // Path
  wxFileName fn(path);
  // Export
  std::set<String> used; // for CONFLICT_NUMBER_OVERWRITE
  std::set<Card*> processed; // cards already written as part of a front/back pair, skip if hit again
  FOR_EACH_CONST(card, cards) {
    if (processed.count(card.get())) continue;
    // is this card part of a front/back pair that should be combined?
    const StyleSheet& stylesheet = set->stylesheetFor(card);
    pair<CardP, CardP> faces = settings.stylesheetSettingsFor(stylesheet).card_dfc_export() ?
                               card->getFrontFaceBackFacePair(*set) :
                               make_pair(CardP(), CardP());
    if (faces.first && faces.second) {
      // filename is "<front name> -- <back name>"
      Context& ctx_front = set->getContext(faces.first);
      String front_name = clean_filename(untag(ctx_front.eval(*filename_script)->toString()));
      Context& ctx_back = set->getContext(faces.second);
      String back_name  = clean_filename(untag(ctx_back.eval(*filename_script)->toString()));
      if (!front_name || !back_name) continue; // no filename -> no saving
      wxFileName front_fn(front_name);
      String combined_name = front_fn.GetName() + _(" -- ") + wxFileName(back_name).GetName();
      String ext = front_fn.GetExt();
      if (!ext.empty()) combined_name += _(".") + ext;
      // full path
      fn.SetFullName(combined_name);
      // does the file exist?
      if (!resolve_filename_conflicts(fn, conflicts, used)) continue;
      // write image
      String filename = fn.GetFullPath();
      used.insert(filename);
      vector<CardP> combo{faces.first, faces.second};
      Image img = export_image(set, combo);
      img.SaveFile(filename);
      processed.insert((faces.first == card ? faces.second : faces.first).get());
    } else {
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
}

String export_metadata(const SetP& set, const CardP& card, double zoom, Radians angle_radians, int width, int height, double offset_x, double offset_y) {
  IndexMap<FieldP, ValueP>& card_data = card->data;
  boost::json::object cardv = mse_to_json(card, set.get());
  StyleSheetP stylesheet = set->stylesheetForP(card);
  if (!settings.stylesheetSettingsFor(*stylesheet).card_notes_export()) cardv["notes"] = "";
  RealRect bounds_rect = RealRect(0, 0, width, height);
  int bounds_degrees = 0;
  RealRect::rotate(bounds_rect, bounds_degrees, width, height, lround(rad_to_deg(angle_radians)));
  RealRect::translate(bounds_rect, bounds_degrees, offset_x, offset_y);
  cardv.emplace("bounds", encodeRectInStdString(bounds_rect, bounds_degrees));
  if (!cardv.contains("data")) {
    cardv["data"] = boost::json::object();
  }
  // cardv.emplace may trigger a re-allocation, so only take a reference
  // to an object inside the map AFTER we're done emplacing
  boost::json::object& cardv_data = cardv["data"].as_object();
  // iterate over all image fields
  for (IndexMap<FieldP, ValueP>::iterator it = card_data.begin(); it != card_data.end(); ++it) {
    ImageValue* value = dynamic_cast<ImageValue*>(it->get());
    if (value && !value->filename.empty()) {
      FieldP field = (*it)->fieldP;
      ImageStyle* style = dynamic_cast<ImageStyle*>(stylesheet->card_style.at(field->index).get());
      if (style) {
        style->update(set->getContext(card));
        // store the entire image in the metadata
        if (style->store_in_metadata() && settings.stylesheetSettingsFor(*stylesheet).card_metaimage_export()) {
          Image img = value->getImage(set);
          cardv_data[field->name.ToStdString()] = encodeImageInString(img);
        }
        // store only crop coordinates
        else {
          RealRect rect = style->getCanonicalExternalRect();
          int degrees = lround(style->angle());
          RealRect::scale(rect, degrees, zoom, zoom);
          RealRect::rotate(rect, degrees, width, height, lround(rad_to_deg(angle_radians))); // width and height are already scaled
          RealRect::translate(rect, degrees, offset_x, offset_y); // offset_x and offset_y are already scaled and rotated
          cardv_data[field->name.ToStdString()] = encodeRectInStdString(rect, degrees);
        }
      }
    }
  }
  return json_ugly_print(cardv);
}
