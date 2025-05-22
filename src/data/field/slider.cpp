//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/slider.hpp>
#include <iomanip>
#include <sstream>

// ----------------------------------------------------------------------------- : SliderField

SliderField::SliderField()
  : minimum(0)
  , maximum(100)
  , increment(1)
{}

IMPLEMENT_FIELD_TYPE(Slider, "slider");

IMPLEMENT_REFLECTION(SliderField) {
  REFLECT_BASE(Field); // NOTE: don't reflect as a ChoiceField
  REFLECT(script);
  REFLECT_N("default", default_script);
  REFLECT(initial);
  REFLECT(minimum);
  REFLECT(maximum);
  REFLECT(increment);
}

void SliderField::after_reading(Version ver) {
  Field::after_reading(ver);

  if (maximum < minimum) {
    int temp = maximum;
    maximum = minimum;
    minimum = temp;
  }
  if (increment < 1) increment = 1;

  for (int i = minimum; i < maximum; i += increment) {
    choices->choices.push_back(make_intrusive<Choice>(wxString::Format(wxT("%i"), i)));
  }
  choices->choices.push_back(make_intrusive<Choice>(wxString::Format(wxT("%i"), maximum)));

  int initial_int;
  try {
    initial_int = std::stoi(initial.ToStdString());
    if (initial_int < minimum || initial_int > maximum) initial = wxString::Format(wxT("%i"), minimum);
  }
  catch (...) {
    initial = wxString::Format(wxT("%i"), minimum);
  }
  choices->initIds();
}
// ----------------------------------------------------------------------------- : SliderStyle

SliderStyle::SliderStyle(const ChoiceFieldP& field)
  : ChoiceStyle(field)
{
  render_style = RENDER_TEXT;
}

IMPLEMENT_REFLECTION(SliderStyle) {
  REFLECT_BASE(ChoiceStyle);
}

// ----------------------------------------------------------------------------- : SliderValue

IMPLEMENT_REFLECTION_NAMELESS(SliderValue) {
  REFLECT_BASE(ChoiceValue);
}
