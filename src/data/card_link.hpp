//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/reflect.hpp>
#include <data/localized_string.hpp>

DECLARE_POINTER_TYPE(CardLink);

// ----------------------------------------------------------------------------- : Card Link

/// Information on a link between two cards in a set
class CardLink : public IntrusivePtrBase<CardLink> {
public:
  CardLink();
  
  LocalizedString selected; ///< Type of link for the selected card
  LocalizedString linked;   ///< Type of link for the linked card
  
  String name();
private:
  DECLARE_REFLECTION();
};
