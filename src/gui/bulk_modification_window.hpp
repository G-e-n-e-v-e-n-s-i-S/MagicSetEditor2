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
  
  wxChoice*       modification_type, *field_type;
  wxStaticText*   modification_description, *predicate_description;
  wxTextCtrl*     modification, *predicate;
  SetP            set;
  Window*         parent;

  void onTypeChange(wxCommandEvent&);
  void setType();
  
  void onFieldChange(wxCommandEvent&);
  void setField();
  
  void onOk(wxCommandEvent&);
  
};
