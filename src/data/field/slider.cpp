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
  REFLECT_N("choices", choices->choices);
  REFLECT(initial);
  REFLECT(minimum);
  REFLECT(maximum);
  REFLECT(increment);
}

void SliderField::after_reading(Version ver) {
  Field::after_reading(ver);

  int choice_count = choices->choices.size();

  if (maximum < minimum) swap(minimum, maximum);
  if (increment < 1) increment = 1;

  for (int i = minimum; i < maximum; i += increment) {
    choices->choices.push_back(make_intrusive<Choice>(wxString::Format(wxT("%i"), i)));
  }
  choices->choices.push_back(make_intrusive<Choice>(wxString::Format(wxT("%i"), maximum)));

  // Move individual choices that are numbers greater than maximum to the end
  for (int i = 0; i < choice_count; ++i) {
    try {
      int choice_int = std::stoi(choices->choices[i].get()->name.ToStdString());
      if (choice_int >= maximum) {
        auto it = choices->choices.begin() + i;
        std::rotate(it, it + 1, choices->choices.end());
        --i;
        --choice_count;
      }
    }
    catch (...) {}
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
