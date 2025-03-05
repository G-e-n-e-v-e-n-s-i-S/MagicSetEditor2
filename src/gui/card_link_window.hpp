//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(Card);
DECLARE_POINTER_TYPE(ExportCardSelectionChoice);
class SelectCardList;

// ----------------------------------------------------------------------------- : CardLinkWindow

/// A window for selecting a subset of the cards from a set,
/** and selecting a link relation type.
/** this is used when linking cards
 */
class CardLinkWindow : public wxDialog {
public:
  CardLinkWindow(Window* parent, const SetP& set, const CardP& selected_card, bool sizer=true);
  
  /// Is the given card selected?
  bool isSelected(const CardP& card) const;
  /// Get a list of all selected cards
  void getSelection(vector<CardP>& out) const;
  /// Change which cards are selected
  void setSelection(const vector<CardP>& cards);
  /// Change the type of link relation
  void setRelationType();

protected:
  DECLARE_EVENT_TABLE();

  wxChoice*       relation_type;
  wxTextCtrl*     selected_relation, *linked_relation;
  SelectCardList* list;
  SetP            set;
  CardP           selected_card;
  wxButton*       sel_none;

  void onRelationTypeChange(wxCommandEvent&);

  void onOk(wxCommandEvent&);

  void onSelectNone(wxCommandEvent&);
};
