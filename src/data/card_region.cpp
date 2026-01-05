//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#include <util/prec.hpp>
#include <data/card_region.hpp>

CardRegion::CardRegion()
  : name("")
  , x(0.0)
  , y(0.0)
  , width(0)
  , height(0)
{}
CardRegion::~CardRegion() {}

IMPLEMENT_REFLECTION(CardRegion) {
  REFLECT(name);
  REFLECT(x);
  REFLECT(y);
  REFLECT(width);
  REFLECT(height);
}
