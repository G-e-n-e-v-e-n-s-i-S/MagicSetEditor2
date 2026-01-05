//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/image.hpp>
#include <gfx/generated_image.hpp>

// ----------------------------------------------------------------------------- : ImageField

IMPLEMENT_FIELD_TYPE(Image, "image");

IMPLEMENT_REFLECTION(ImageField) {
  REFLECT_BASE(Field);
}

// ----------------------------------------------------------------------------- : ImageStyle

IMPLEMENT_REFLECTION(ImageStyle) {
  REFLECT_BASE(Style);
  REFLECT_N("default", default_image);
  REFLECT(store_in_metadata);
}

int ImageStyle::update(Context& ctx) {
  int changes = Style::update(ctx);
  changes |= default_image.update(ctx) * CHANGE_DEFAULT;
  changes |= store_in_metadata.update(ctx) * CHANGE_OTHER;
  return changes;
}

// ----------------------------------------------------------------------------- : ImageValue

String ImageValue::toString() const {
  return filename.empty() ? _("") : _("<image>");
}

// custom reflection: convert to ScriptImageP for scripting

void ImageValue::reflect(Reader& handler) {
  handler.handle(filename);
}
void ImageValue::reflect(Writer& handler) {
  if (fieldP->save_value) handler.handle(filename);
}
void ImageValue::reflect(GetMember& handler) {}
void ImageValue::reflect(GetDefaultMember& handler) {
  // convert to ScriptImageP for scripting
  handler.handle( (ScriptValueP)make_intrusive<ImageValueToImage>(filename, last_update) );
}
