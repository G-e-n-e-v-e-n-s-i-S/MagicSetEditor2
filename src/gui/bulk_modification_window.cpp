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
#include <data/field/multiple_choice.hpp>
#include <data/field/symbol.hpp>
#include <data/action/set.hpp>
#include <data/action/value.hpp>
#include <script/functions/construction_helper.hpp>
#include <gui/bulk_modification_window.hpp>
#include <gui/control/card_list.hpp>
#include <util/window_id.hpp>
#include <wx/statline.h>

// ----------------------------------------------------------------------------- : AddCSV

BulkModificationWindow::BulkModificationWindow(Window* parent, const SetP& set, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("bulk modify"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set), parent(parent)
{
  // init controls
  predicate_description = new wxStaticText(this, -1, _LABEL_("bulk modify predicate description"));
  predicate = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
  predicate->SetHint("example (tiny creatures):\ncard.cmc <= 3 and contains(card.type, match:\"Creature\")");
  modification_description = new wxStaticText(this, -1, _LABEL_("bulk modify modification description"));
  modification = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
  modification_type = new wxChoice(this, ID_CARD_BULK_TYPE, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  modification_type->Clear();
  modification_type->Append(_LABEL_("bulk modify all"));
  modification_type->Append(_LABEL_("bulk modify selected"));
  modification_type->Append(_LABEL_("bulk modify predicate"));
  modification_type->SetSelection(0);
  setType();
  field_type = new wxChoice(this, ID_CARD_BULK_FIELD, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_SORT);
  field_type->Clear();
  String default_selection = "";
  field_type->Append("stylesheet");
  field_type->Append("notes");
  FOR_EACH(field, set->game->card_fields) {
    field_type->Append(field->name);
    if (field->identifying) default_selection = field->name;
  }
  int default_index = field_type->FindString(default_selection);
  if (default_index == wxNOT_FOUND) default_index = 0;
  field_type->SetSelection(default_index);
  setField();
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(this, -1, _LABEL_("bulk modify type")), 0, wxALL, 8);
    s->Add(modification_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(predicate_description, 0, wxEXPAND | wxALL, 8);
    s->Add(predicate, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(new wxStaticText(this, -1, _LABEL_("bulk modify field")), 0, wxALL, 8);
    s->Add(field_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(modification_description, 0, wxEXPAND | wxALL, 8);
    s->Add(modification, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(CreateButtonSizer(wxOK | wxCANCEL), 1, wxEXPAND | wxALL, 12);
    s->SetSizeHints(this);
    SetSizer(s);
    SetSize(500, 500);
  }
}

void BulkModificationWindow::setType() {
  if (modification_type->GetSelection() <= 1) {
    predicate_description->Hide();
    predicate->Hide();
  } else {
    predicate_description->Show();
    predicate->Show();
  }
  Layout();
}

void BulkModificationWindow::onTypeChange(wxCommandEvent&) {
  setType();
}

void BulkModificationWindow::setField() {

}

void BulkModificationWindow::onFieldChange(wxCommandEvent&) {
  setField();
}

void BulkModificationWindow::onOk(wxCommandEvent&) {
  wxBusyCursor wait;
  // get the context
  CardListBase* card_list_window = dynamic_cast<CardListBase*>(parent);
  if (!card_list_window) {
    queue_message(MESSAGE_ERROR, _("Bulk modification must be called from a card list window!"));
    EndModal(wxID_ABORT);
    return;
  }
  Context& ctx = set->getContext();
  ScriptValueP ctx_card = ctx.getVariableOpt(SCRIPT_VAR_card);
  ScriptValueP ctx_stylesheet = ctx.getVariableOpt(SCRIPT_VAR_stylesheet);
  // get the cards
  vector<CardP> cards;
  if (modification_type->GetSelection() == 0) { // all
    cards = set->cards;
  } else if (modification_type->GetSelection() == 1) { // selection
    card_list_window->getSelection(cards);
  } else { // predicate
    ScriptP predicate_script = parse(predicate->GetValue(), nullptr, false);
    FOR_EACH(card, set->cards) {
      ctx.setVariable(SCRIPT_VAR_card, to_script(card));
      ctx.setVariable(SCRIPT_VAR_stylesheet, card->stylesheet ? to_script(card->stylesheet) : to_script(set->stylesheet));
      ScriptValueP result = predicate_script->eval(ctx, false);
      if (result->type() != SCRIPT_BOOL) {
        queue_message(MESSAGE_ERROR, _ERROR_("bulk modify predicate is not bool"));
        EndModal(wxID_ABORT);
        return;
      }
      if (result->toBool()) {
        cards.push_back(card);
      }
    }
  }
  // get the new script values
  if (!cards.empty()) {
    vector<shared_ptr<Action>> actions;
    String field_name = field_type->GetString(field_type->GetSelection());
    ScriptP modification_script = parse(modification->GetValue(), nullptr, false);
    vector<Value*> values;
    vector<ScriptValueP> new_values;
    FOR_EACH(card, cards) {
      Value* value = get_container(set->game, card, field_name, false);
      values.push_back(value);
      ctx.setVariable(SCRIPT_VAR_card, to_script(card));
      ctx.setVariable(SCRIPT_VAR_stylesheet, card->stylesheet ? to_script(card->stylesheet) : to_script(set->stylesheet));
      ScriptValueP new_value = modification_script->eval(ctx, false);
      new_values.push_back(new_value);
    }
    int count = cards.size();
    assert(count == values.size());
    assert(count == new_values.size());
    // make the modifications (I have lost my battle with c++ templates)
    if (dynamic_cast<TextValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        TextValue* value = dynamic_cast<TextValue*>(values[i]);
        TextValue::ValueType new_value = new_values[i]->toString();
        shared_ptr<Action> action = make_shared<SimpleValueAction<TextValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<MultipleChoiceValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        MultipleChoiceValue* value = dynamic_cast<MultipleChoiceValue*>(values[i]);
        MultipleChoiceValue::ValueType new_value = { new_values[i]->toString(), _("") };
        shared_ptr<Action> action = make_shared<SimpleValueAction<MultipleChoiceValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<ChoiceValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        ChoiceValue* value = dynamic_cast<ChoiceValue*>(values[i]);
        ChoiceValue::ValueType new_value = new_values[i]->toString();
        shared_ptr<Action> action = make_shared<SimpleValueAction<ChoiceValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<PackageChoiceValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        PackageChoiceValue* value = dynamic_cast<PackageChoiceValue*>(values[i]);
        PackageChoiceValue::ValueType new_value = new_values[i]->toString();
        shared_ptr<Action> action = make_shared<SimpleValueAction<PackageChoiceValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<ColorValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        ColorValue* value = dynamic_cast<ColorValue*>(values[i]);
        ColorValue::ValueType new_value = new_values[i]->toColor();
        shared_ptr<Action> action = make_shared<SimpleValueAction<ColorValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<ImageValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        ImageValue* value = dynamic_cast<ImageValue*>(values[i]);
        wxFileName fname(static_cast<ExternalImage*>(new_values[i].get())->toString());
        ImageValue::ValueType new_value = LocalFileName::fromReadString(fname.GetName(), "");
        shared_ptr<Action> action = make_shared<SimpleValueAction<ImageValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else if (dynamic_cast<SymbolValue*>(values.front())) {
      for (int i = 0; i < count; ++i) {
        SymbolValue* value = dynamic_cast<SymbolValue*>(values[i]);
        wxFileName fname(static_cast<ExternalImage*>(new_values[i].get())->toString());
        SymbolValue::ValueType new_value = LocalFileName::fromReadString(fname.GetName(), "");
        shared_ptr<Action> action = make_shared<SimpleValueAction<SymbolValue, false>>(value, new_value);
        actions.push_back(action);
      }
    }
    else {
      queue_message(MESSAGE_ERROR, _ERROR_("bulk modify script type unknown"));
    }
    set->actions.addAction(make_unique<BulkAction>(actions, set), false);
  }
  // restore context variables
  if (ctx_card) ctx.setVariable(SCRIPT_VAR_card, ctx_card);
  if (ctx_stylesheet) ctx.setVariable(SCRIPT_VAR_card, ctx_stylesheet);
  // Done
  EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(BulkModificationWindow, wxDialog)
  EVT_BUTTON(wxID_OK, BulkModificationWindow::onOk)
  EVT_CHOICE(ID_CARD_BULK_TYPE, BulkModificationWindow::onTypeChange)
  EVT_CHOICE(ID_CARD_BULK_FIELD, BulkModificationWindow::onFieldChange)
END_EVENT_TABLE()
