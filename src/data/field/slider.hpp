//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/choice.hpp>

// ----------------------------------------------------------------------------- : SliderField

DECLARE_POINTER_TYPE(SliderField);
DECLARE_POINTER_TYPE(SliderStyle);
DECLARE_POINTER_TYPE(SliderValue);

/// A field whos value is an integer from a range
class SliderField : public ChoiceField {
public:
  SliderField();
  DECLARE_FIELD_TYPE(SliderField);
  
  int minimum, maximum, increment;

  void after_reading(Version ver) override;
};

// ----------------------------------------------------------------------------- : SliderStyle

/// The Style for a SliderField
class SliderStyle : public ChoiceStyle {
public:
  SliderStyle(const ChoiceFieldP& field);
  DECLARE_HAS_FIELD(Slider); // not DECLARE_STYLE_TYPE, because we use a normal ChoiceValueViewer/Editor
  StyleP clone() const override;
  
  // no extra data
  
private:
  DECLARE_REFLECTION();
};

// ----------------------------------------------------------------------------- : SliderValue

/// The Value in a SliderField
class SliderValue : public ChoiceValue {
public:
  inline SliderValue(const ChoiceFieldP& field) : ChoiceValue(field) {}
  DECLARE_HAS_FIELD(Slider);
  ValueP clone() const override;
  
  // no extra data
  
private:
  DECLARE_REFLECTION();
};

