//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/card_link.hpp>

// ----------------------------------------------------------------------------- : CardLink

CardLink::CardLink() {}

String CardLink::name() {
  return selected.get() + _(" // ") + linked.get();
}

IMPLEMENT_REFLECTION_NO_GET_MEMBER(CardLink) {
  REFLECT_LOCALIZED(selected);
  REFLECT_LOCALIZED(linked);
}
