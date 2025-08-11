//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <iomanip>
#include <iostream>
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
#include <data/format/formats.hpp>
#include <gui/add_json_window.hpp>
#include <script/functions/json.hpp>
#include <script/functions/construction_helper.hpp>
#include <wx/statline.h>
#include <boost/json.hpp>
#include <boost/json/src.hpp>

// ----------------------------------------------------------------------------- : AddJSON

AddJSONWindow::AddJSONWindow(Window* parent, const SetP& set, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("add card json"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set)
{
  // init controls
  file_path = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  file_browse = new wxButton(this, ID_CARD_ADD_JSON_BROWSE, _BUTTON_("browse"));
  json_type = new wxChoice(this, ID_CARD_ADD_JSON_ARRAY, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  json_type->Clear();
  FOR_EACH(type, set->game->json_paths) {
    int delimiter_pos = type.find("//");
    json_type->Append(type.substr(0, delimiter_pos).Trim().Trim(false));
  }
  json_type->Append(_LABEL_("add card json custom"));
  json_type->SetSelection(0);
  card_array_path = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  setJSONType();
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(this, -1, _LABEL_("add card json type")), 0, wxALL, 8);
    s->Add(json_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(new wxStaticText(this, -1, _LABEL_("add card json path")), 0, wxALL & ~wxTOP, 8);
    s->Add(card_array_path, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(new wxStaticText(this, -1, _LABEL_("add card json file")), 0, wxALL, 8);
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

void AddJSONWindow::setJSONType() {
  int sel = json_type->GetSelection();
  if (sel == json_type->GetCount() - 1) { // Custom type
    card_array_path->ChangeValue(wxEmptyString);
    card_array_path->Enable();
  }
  else {
    String selection = json_type->GetString(sel);
    FOR_EACH(type, set->game->json_paths) {
      if (type.starts_with(selection)) {
        int delimiter_pos = type.find("//");
        card_array_path->ChangeValue(delimiter_pos + 2 < type.Length() ? type.substr(delimiter_pos + 2).Trim().Trim(false) : wxString());
        card_array_path->Enable(false);
        break;
      }
    }
  }
}

void AddJSONWindow::onJSONTypeChange(wxCommandEvent&) {
  setJSONType();
}

void AddJSONWindow::onBrowseFiles(wxCommandEvent&) {
  wxFileDialog* dlg = new wxFileDialog(this, _TITLE_("add card json file"), settings.default_set_dir, wxEmptyString, _("JSON files|*.json|All files (*.*)|*"), wxFD_OPEN);
  if (dlg->ShowModal() == wxID_OK) {
    file_path->SetValue(dlg->GetPath());
  }
}

 bool AddJSONWindow::readJSON(std::ifstream& in, std::vector<String>& headers_out, std::vector<std::vector<ScriptValueP>>& table_out) {
  // Read file
  std::string input(std::istreambuf_iterator<char>(in), {});
  boost::json::value jv;
  try {
    jv = boost::json::parse(input);
  } catch (...) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card json failed to parse"));
    return false;
  }
  // Split path into tokens
  std::string path = card_array_path->GetValue().ToStdString();
  std::vector<std::string> tokens;
  size_t pos = 0;
  std::string token;
  while ((pos = path.find(":")) != std::string::npos) {
    token = path.substr(0, pos);
    tokens.push_back(token);
    path.erase(0, pos + 1);
  }
  tokens.push_back(path);
  // Navigate path to card array
  for (int t = 0; t < tokens.size(); ++t) {
    if (tokens[t] == "") break;
    if (!jv.is_object()) {
      queue_message(MESSAGE_ERROR, _ERROR_("add card json path not valid"));
      return false;
    }
    auto& ov = jv.as_object();
    jv = ov[tokens[t]];
    if (jv == nullptr) {
      queue_message(MESSAGE_ERROR, _ERROR_("add card json path not valid"));
      return false;
    }
  }
  if (!jv.is_array()) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card json path not valid"));
    return false;
  }
  auto& card_array = jv.as_array();
  size_t count = card_array.size();
  if (count == 0) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card json empty array"));
    return false;
  }
  const auto& first_card = card_array[0];
  if (!first_card.is_object()) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card json path not valid"));
    return false;
  }
  // Get headers
  for (int i = 0; i < count; ++i) {
    const auto& card = card_array[i].as_object();
    for (auto it = card.begin(); it != card.end(); ++it) {
      boost::json::string_view jview = it->key();
      std::string_view stdview = std::string_view(jview.data(), jview.size());
      std::string stdstring = { stdview.begin(), stdview.end() };
      String key(stdstring.c_str(), wxConvUTF8);
      if (std::find(headers_out.begin(), headers_out.end(), key) == headers_out.end()) {
        headers_out.push_back(key);
      }
    }
  }
  // Parse cards, add to table
  for (int i = 0; i < count; ++i) {
    std::vector<ScriptValueP> row;
    auto& card = card_array[i].as_object();
    for (int h = 0; h < headers_out.size(); ++h) {
      const auto& value = card[headers_out[h].ToStdString()];
      row.push_back(json_to_mse(value, set.get()));
    }
    table_out.push_back(row);
  }
  return true;
}

void AddJSONWindow::onOk(wxCommandEvent&) {



  // debug test shit
  export_image(set, set->cards.front(), "C:\\Users\\Oli\\Desktop\\tetest\\test.png");
  auto extImg = make_intrusive<ExternalImage>("C:/Users/Oli/Desktop/tetest/test.png");
  Image img = extImg->generate(GeneratedImage::Options(0, 0, set->stylesheet.get(), set.get()));
  if (img.HasOption(wxIMAGE_OPTION_FILENAME))  queue_message(MESSAGE_ERROR, img.GetOption(wxIMAGE_OPTION_FILENAME));
  else queue_message(MESSAGE_ERROR, _("no dice"));
  return;



  /// Perform the import
  wxBusyCursor wait;
  // Read the file
  auto& file = std::ifstream(file_path->GetValue().ToStdString());
  if (file.fail()) {
    queue_message(MESSAGE_ERROR, _ERROR_("add card json file not found"));
    EndModal(wxID_ABORT);
    return;
  }
  // Put values into a table
  std::vector<String> headers;
  std::vector<std::vector<ScriptValueP>> table;
  if (!readJSON(file, headers, table)) {
    EndModal(wxID_ABORT);
    return;
  }
  // Check for missing fields
  String missing_fields;
  check_table_headers(set->game, headers, _("JSON"), missing_fields);
  if (!missing_fields.empty()) {
    queue_message(MESSAGE_WARNING, _ERROR_2_("import missing fields", _("JSON"), missing_fields));
  }
  // Produce cards from the table
  vector<CardP> cards;
  if (!cards_from_table(set, headers, table, true, _("JSON"), cards)) {
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

BEGIN_EVENT_TABLE(AddJSONWindow, wxDialog)
  EVT_BUTTON(wxID_OK, AddJSONWindow::onOk)
  EVT_BUTTON(ID_CARD_ADD_JSON_BROWSE, AddJSONWindow::onBrowseFiles)
  EVT_CHOICE(ID_CARD_ADD_JSON_ARRAY, AddJSONWindow::onJSONTypeChange)
END_EVENT_TABLE()
