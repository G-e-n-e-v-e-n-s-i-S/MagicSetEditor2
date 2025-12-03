//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

DECLARE_POINTER_TYPE(Game);
DECLARE_POINTER_TYPE(Set);

// ----------------------------------------------------------------------------- : BulkModificationWindow

/// A window for modifying multiple cards at once.
class BulkModificationWindow : public wxDialog {
public:
  BulkModificationWindow(Window* parent, const SetP& set, bool sizer);

protected:
  DECLARE_EVENT_TABLE();

  wxChoice*       modification_selection, *field_type;
  wxStaticText*   modification_description, *modification_errors, *predicate_description, *predicate_errors;
  wxTextCtrl*     modification, *predicate;
  bool            modification_parsed, predicate_parsed;
  wxButton*       ok_button;
  SetP            set;
  Window*         parent;
  ScriptP         modification_script, predicate_script;

  void onSelectionChange(wxCommandEvent&);
  void changeSelection();

  void onPredicateChange(wxCommandEvent&);
  void parsePredicate();

  void onModificationChange(wxCommandEvent&);
  void parseModification();

  void updateOkButton();

  void onOk(wxCommandEvent&);

};
