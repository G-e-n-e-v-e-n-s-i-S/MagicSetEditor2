//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <fstream>
#include <sstream>
#include <regex>
#include <util/prec.hpp>
#include <util/window_id.hpp>
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/action/set.hpp>
#include <gui/add_csv_window.hpp>
#include <script/functions/construction_helper.hpp>
#include <wx/statline.h>

// ----------------------------------------------------------------------------- : AddCSV

AddCSVWindow::AddCSVWindow(Window* parent, const SetP& set, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("add card csv"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set)
{
  // init controls
  file_path = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  file_browse = new wxButton(this, ID_CARD_ADD_CSV_BROWSE, _BUTTON_("browse"));
  separator_type = new wxChoice(this, ID_CARD_ADD_CSV_SEP, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  separator_type->Clear();
  separator_type->Append(_LABEL_("add card csv tab"));
  separator_type->Append(_LABEL_("add card csv comma"));
  separator_type->Append(_LABEL_("add card csv semicolon"));
  separator_type->SetSelection(0);
  setSeparatorType();
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(this, -1, _LABEL_("add card csv sep")), 0, wxALL, 8);
    s->Add(separator_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(new wxStaticText(this, -1, _("  ") + _LABEL_("add card csv file")), 0, wxALL, 8);
    s->Add(file_path, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
      s2->Add(file_browse, 0, wxEXPAND | wxRIGHT, 8);
      s2->Add(CreateButtonSizer(wxOK | wxCANCEL), 1, wxEXPAND, 8);
    s->Add(s2, 0, wxEXPAND | wxALL, 12);
    file_browse->SetFocus();
    s->SetSizeHints(this);
    SetSizer(s);
    SetSize(500, 110);
  }
}

void AddCSVWindow::setSeparatorType() {
  int sel = separator_type->GetSelection();
  if (sel == 0) separator = '	';
  else if (sel == 1) separator = ',';
  else  separator = ';';
}

void AddCSVWindow::onSeparatorTypeChange(wxCommandEvent&) {
  setSeparatorType();
}

void AddCSVWindow::onBrowseFiles(wxCommandEvent&) {
  wxFileDialog* dlg = new wxFileDialog(this, _TITLE_("add card csv file"), settings.default_set_dir, wxEmptyString, _("CSV files|*.csv;*.tsv|All files (*.*)|*"), wxFD_OPEN);
  if (dlg->ShowModal() == wxID_OK) {
    file_path->SetValue(dlg->GetPath());
  }
}

std::vector<std::string> AddCSVWindow::readCSVRow(const std::string& row) {
  CSVState state = CSVState::UnquotedField;
  std::vector<std::string> fields{ "" };
  size_t f = 0; // index of the current field
  for (char c : row) {
    switch (state) {
    case CSVState::UnquotedField:
      if (c == separator) { // end of field
        fields.push_back(""); f++;
      }
      else if (c == '"') {
        state = CSVState::QuotedField;
      }
      else {
        fields[f].push_back(c);
      }
      break;
    case CSVState::QuotedField:
      switch (c) {
      case '"': state = CSVState::QuotedQuote;
        break;
      default:  fields[f].push_back(c);
        break;
      }
      break;
    case CSVState::QuotedQuote:
      if (c == separator) { // separator after closing quote
        fields.push_back(""); f++;
        state = CSVState::UnquotedField;
      }
      else if (c == '"') { // "" -> "
        fields[f].push_back('"');
        state = CSVState::QuotedField;
      }
      else { // end of quote
        state = CSVState::UnquotedField;
      }
      break;
    }
  }
  return fields;
}

bool AddCSVWindow::readCSV(std::ifstream& in, std::vector<String> headers_out, std::vector<std::vector<ScriptValueP>>& table_out) {
  // Get the rows
  vector<std::string> raw_rows;
  std::string raw_row;
  while (std::getline(in, raw_row)) {
    raw_rows.push_back(raw_row);
  }
  // Check for quoted new line characters
  vector<std::string> rows;
  std::string row = "";
  for (int y = 0; y < raw_rows.size(); ++y) {
    row = row + raw_rows[y];
    int quote_count = 0;
    std::string::size_type pos = 0;
    while ((pos = row.find("\"", pos)) != std::string::npos) {
      ++quote_count;
      ++pos;
    }
    if (quote_count % 2 == 0) {
      if (row.find_first_not_of(' ') != std::string::npos) rows.push_back(row);
      row = "";
    } else {
      row = row + "\n";
    }
  }
  if (rows.size() == 0) {
    queue_message(MESSAGE_ERROR, _ERROR_1_("import empty file", _("CSV / TSV")));
    return false;
  }
  // Parse headers
  std::vector<std::string> headers = readCSVRow(rows[0]);
  for (int x = 0; x < headers.size(); ++x) {
    String wxstring(headers[x].c_str(), wxConvUTF8);
    headers_out.push_back(wxstring);
  }
  // Parse rows, add to table
  for (int y = 0; y < rows.size(); ++y) {
    auto fields = readCSVRow(rows[y]);
    std::vector<ScriptValueP> values;
    for (int x = 0; x < fields.size(); ++x) {
      String wxstring(fields[x].c_str(), wxConvUTF8);
      values.push_back(to_script(wxstring));
    }
    table_out.push_back(values);
  }
  return true;
}

void AddCSVWindow::onOk(wxCommandEvent&) {
  /// Perform the import
  wxBusyCursor wait;
  // Read the file
  auto file = std::ifstream(file_path->GetValue().ToStdString());
  if (file.fail()) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card csv file not found"));
    EndModal(wxID_ABORT);
    return;
  }
  // Put values into a table
  std::vector<String> headers;
  std::vector<std::vector<ScriptValueP>> table;
  if (!readCSV(file, headers, table)) {
    EndModal(wxID_ABORT);
    return;
  }
  // Check for missing fields
  String missing_fields;
  check_table_headers(set->game, headers, _("CSV / TSV"), missing_fields);
  if (missing_fields.size() > 0) {
    queue_message(MESSAGE_WARNING, _ERROR_2_("import missing fields", _("CSV / TSV"), missing_fields));
  }
  // Produce cards from the table
  vector<CardP> cards;
  if (!cards_from_table(set, headers, table, true, _("CSV / TSV"), cards)) {
    EndModal(wxID_ABORT);
    return;
  }
  // Add cards to set
  if (!cards.empty()) {
    // TODO: change the name of the action somehow
    set->actions.addAction(make_unique<AddCardAction>(ADD, *set, cards));
  }
  // Done
  EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(AddCSVWindow, wxDialog)
  EVT_BUTTON(wxID_OK, AddCSVWindow::onOk)
  EVT_BUTTON(ID_CARD_ADD_CSV_BROWSE, AddCSVWindow::onBrowseFiles)
  EVT_CHOICE(ID_CARD_ADD_CSV_SEP, AddCSVWindow::onSeparatorTypeChange)
END_EVENT_TABLE()
