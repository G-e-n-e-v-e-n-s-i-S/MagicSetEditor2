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
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <gui/add_csv_window.hpp>
#include <util/window_id.hpp>
#include <data/action/set.hpp>
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
  separator_type->SetSelection(0);
  setSeparatorType();
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(this, -1, _LABEL_("add card csv sep")), 0, wxALL, 8);
    s->Add(separator_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(new wxStaticText(this, -1, _("  ") + _LABEL_("add card csv file")), 0, wxALL | (wxALL & ~wxTOP), 8);
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
  else  separator = ',';
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
  // escape " { }
  for (f = 0; f < fields.size(); ++f) {
    fields[f] = std::regex_replace(fields[f], std::regex("\""), "\\\"");
    fields[f] = std::regex_replace(fields[f], std::regex("\\{"), "\\{");
    fields[f] = std::regex_replace(fields[f], std::regex("\\}"), "\\}");
  }
  return fields;
}

void AddCSVWindow::onOk(wxCommandEvent&) {
  /// Perform the import
  // Read the file, put it into a table
  auto in = std::ifstream(file_path->GetValue().ToStdString());
  if (in.fail()) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card csv file not found"));
    EndModal(wxID_ABORT);
    return;
  }
  std::vector<std::vector<std::string>> table;
  std::string row;
  while (!in.eof()) {
    std::getline(in, row);
    if (in.bad() || in.fail()) {
      break;
    }
    auto fields = readCSVRow(row);
    table.push_back(fields);
  }
  // ensure table is square
  int count = table[0].size();
  for (int y = 1; y < table.size(); ++y) {
    if (table[y].size() != count) {
      queue_message(MESSAGE_ERROR, _ERROR_1_("add card csv file malformed", wxString::Format(wxT("%i"), y+1)));
      EndModal(wxID_ABORT);
      return;
    }
  }
  // produce script from table
  std::ostringstream stream;
  stream << "[";
  for (int y = 1; y < table.size(); ++y) {
    stream << "new_card([";
    for (int x = 0; x < count; ++x) {
      stream << "\"" << table[0][x] << "\": \"" << table[y][x] << "\"";
      if (x < count - 1) stream << ",";
    }
    stream << "])";
    if (y < table.size() - 1) stream << ",";
  }
  stream << "]";
  String string(stream.str().c_str(), wxConvUTF8);
  ScriptP script = parse(string, nullptr, false);
  Context& ctx = set->getContext();
  ScriptValueP result = script->eval(ctx, false);
  // create cards
  vector<CardP> cards;
  ScriptValueP it = result->makeIterator();
  while (ScriptValueP item = it->next()) {
    CardP card = from_script<CardP>(item);
    // is this a new card?
    if (contains(set->cards, card) || contains(cards, card)) {
      // make copy
      card = make_intrusive<Card>(*card);
    }
    cards.push_back(card);
  }
  // add to set
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
