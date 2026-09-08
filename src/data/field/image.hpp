//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field.hpp>
#include <data/set.hpp>
#include <script/scriptable.hpp>
#include <script/image.hpp>
#include <util/io/package.hpp>

// ----------------------------------------------------------------------------- : ImageField

DECLARE_POINTER_TYPE(ImageField);
DECLARE_POINTER_TYPE(ImageStyle);
DECLARE_POINTER_TYPE(ImageValue);

/// A field for image values
class ImageField : public Field {
public:
  ImageField() {
    show_statistics = false; // no statistics on image fields by default, that just leads to errors
  }
  // no extra data
  DECLARE_FIELD_TYPE(Image);
};

// ----------------------------------------------------------------------------- : ImageValue

/// The Value in a ImageField, i.e. an image
class ImageValue : public Value {
public:
  inline ImageValue(const ImageFieldP& field) : Value(field) {}
  DECLARE_VALUE_TYPE(Image, LocalFileName);

  inline Image getImage(const SetP& set) {
    auto imageInputStream = set->openIn(filename);
    Image img(*imageInputStream, wxBITMAP_TYPE_ANY);
    if (!img.IsOk()) throw ScriptError(_ERROR_2_("file not found", filename.toStringForKey(), set));
    return img;
  }

  void copyDataFrom(const Value& other) override;

  ValueType filename;    ///< Filename of the image (in the current package), or ""
  Age       last_update; ///< When was the image last changed?

  bool isDefault() override;

  void makeDefault(bool d = true) override;
};

// ----------------------------------------------------------------------------- : ImageStyle

/// The Style for an ImageField
class ImageStyle : public Style {
public:
  inline ImageStyle(const ImageFieldP& field) : Style(field), store_in_metadata(false) {}
  DECLARE_STYLE_TYPE(Image);
  
  ScriptableImage default_image;      ///< Placeholder image when the user hasn't set one.
  Scriptable<bool> store_in_metadata; ///< Is the image stored in full in the metadata when exporting?
  
  int update(Context&) override;
};
