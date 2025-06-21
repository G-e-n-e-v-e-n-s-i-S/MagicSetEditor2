//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

DECLARE_POINTER_TYPE(Set);

// ----------------------------------------------------------------------------- : AddJSONWindow

/// A window for selecting a JSON file, and importing cards from it.
class AddJSONWindow : public wxDialog {
public:
  AddJSONWindow(Window* parent, const SetP& set, bool sizer);

protected:
  DECLARE_EVENT_TABLE();

  wxChoice*       json_type;
  wxTextCtrl*     card_array_path, *file_path;
  wxButton*       file_browse;
  SetP            set;

  bool readJSON(std::ifstream& in, std::vector<String>& headers_out, std::vector<std::vector<ScriptValueP>>& table_out);

  void onJSONTypeChange(wxCommandEvent&);
  void setJSONType();

  void onBrowseFiles(wxCommandEvent&);

  void onOk(wxCommandEvent&);

};
