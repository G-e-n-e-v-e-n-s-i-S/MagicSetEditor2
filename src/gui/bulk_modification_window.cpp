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
  ok_button = new wxButton(this, wxID_OK);
  predicate_description = new wxStaticText(this, -1, _LABEL_("bulk modify predicate description"));
  predicate = new wxTextCtrl(this, ID_CARD_BULK_PREDICATE, wxEmptyString, wxDefaultPosition, wxSize(400, 70), wxTE_MULTILINE);
  predicate->SetHint(_LABEL_("bulk modify predicate example") + _("\ncard.cmc <= 3 and contains(card.type, match:\"Creature\")"));
  predicate_errors = new wxStaticText(this, wxID_ANY, _(""));
  predicate_errors->SetForegroundColour(*wxRED);
  modification_description = new wxStaticText(this, -1, _LABEL_("bulk modify mod description"));
  modification = new wxTextCtrl(this, ID_CARD_BULK_MODIFICATION, wxEmptyString, wxDefaultPosition, wxSize(400, 70), wxTE_MULTILINE);
  modification_errors = new wxStaticText(this, wxID_ANY, _(""));
  modification_errors->SetForegroundColour(*wxRED);
  modification_parsed = true;
  modification_selection = new wxChoice(this, ID_CARD_BULK_TYPE, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  modification_selection->Clear();
  modification_selection->Append(_LABEL_("bulk modify all"));
  modification_selection->Append(_LABEL_("bulk modify filtered"));
  modification_selection->Append(_LABEL_("bulk modify selected"));
  modification_selection->Append(_LABEL_("bulk modify predicate"));
  modification_selection->SetSelection(0);
  changeSelection();
  parseModification();
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
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
    s->Add(new wxStaticText(this, -1, _LABEL_("bulk modify selection")), 0, wxALL, 8);
    s->Add(modification_selection, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(predicate_description, 0, wxEXPAND | wxALL, 8);
    s->Add(predicate, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(predicate_errors, 0, wxALL & ~wxTOP, 8);
    s->AddSpacer(20);
    s->Add(new wxStaticText(this, -1, _LABEL_("bulk modify field")), 0, wxALL, 8);
    s->Add(field_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(modification_description, 0, wxEXPAND | wxALL, 8);
    s->Add(modification, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->Add(modification_errors, 0, wxALL & ~wxTOP, 8);
    s->AddStretchSpacer(1);
    wxStdDialogButtonSizer* s1 = new wxStdDialogButtonSizer();
      s1->AddStretchSpacer(1);
      s1->AddButton(ok_button);
      s1->AddButton(new wxButton(this, wxID_CANCEL));
      s1->Realize();
    s->Add(s1, 1, wxEXPAND | wxALL, 12);
    s->SetSizeHints(this);
    SetSizer(s);
    SetSize(600, 400);
    Layout();
  }
}

void BulkModificationWindow::setContextVariables(CardP& card, Context& ctx)
{
  StyleSheetP stylesheet = set->stylesheetForP(card);
  ctx.setVariable(SCRIPT_VAR_stylesheet,       to_script(stylesheet));
  ctx.setVariable(SCRIPT_VAR_card_style,       to_script(&stylesheet->card_style));
  ctx.setVariable(SCRIPT_VAR_card,             to_script(card));
  ctx.setVariable(SCRIPT_VAR_styling,          to_script(&set->stylingDataFor(card)));
  ctx.setVariable(SCRIPT_VAR_extra_card_style, to_script(&stylesheet->extra_card_style));
  ctx.setVariable(SCRIPT_VAR_extra_card,       to_script(&card->extraDataFor(*stylesheet)));
}

void BulkModificationWindow::updateOkButton() {
  if (predicate_parsed && modification_parsed) {
    ok_button->Enable(true);
  }
  else {
    ok_button->Enable(false);
  }
}

void BulkModificationWindow::changeSelection() {
  if (modification_selection->GetSelection() <= 2) {
    predicate_description->Hide();
    predicate->Hide();
    predicate_errors->Hide();
    predicate_parsed = true;
    SetSize(600, 400);
  } else {
    predicate_description->Show();
    predicate->Show();
    predicate_errors->Show();
    parsePredicate();
    SetSize(600, 560);
  }
  updateOkButton();
  Layout();
}

void BulkModificationWindow::onSelectionChange(wxCommandEvent&) {
  changeSelection();
}

void BulkModificationWindow::parsePredicate() {
  vector<ScriptParseError> parse_errors;
  String value = predicate->GetValue();
  if (value.StartsWith("{") && value.EndsWith("}")) value = value.substr(1, value.length()-2);
  predicate_script = parse(value, nullptr, false, parse_errors);
  if (parse_errors.empty()) {
    predicate_errors->SetLabel(_(""));
    predicate_parsed = true;
  }
  else {
    predicate_errors->SetLabel(ScriptParseErrors(parse_errors).what());
    predicate_parsed = false;
  }
  updateOkButton();
}

void BulkModificationWindow::onPredicateChange(wxCommandEvent&) {
  parsePredicate();
}

void BulkModificationWindow::parseModification() {
  vector<ScriptParseError> parse_errors;
  modification_script = parse(modification->GetValue(), nullptr, true, parse_errors);
  if (parse_errors.empty()) {
    modification_errors->SetLabel(_(""));
    modification_parsed = true;
  }
  else {
    modification_errors->SetLabel(ScriptParseErrors(parse_errors).what());
    modification_parsed = false;
  }
  updateOkButton();
}

void BulkModificationWindow::onModificationChange(wxCommandEvent&) {
  parseModification();
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
  ScriptValueP ctx_stylesheet =       ctx.getVariableOpt(SCRIPT_VAR_stylesheet);
  ScriptValueP ctx_card_style =       ctx.getVariableOpt(SCRIPT_VAR_card_style);
  ScriptValueP ctx_card =             ctx.getVariableOpt(SCRIPT_VAR_card);
  ScriptValueP ctx_styling =          ctx.getVariableOpt(SCRIPT_VAR_styling);
  ScriptValueP ctx_extra_card_style = ctx.getVariableOpt(SCRIPT_VAR_extra_card_style);
  ScriptValueP ctx_extra_card =       ctx.getVariableOpt(SCRIPT_VAR_extra_card);

  // get the cards
  vector<CardP> cards;
  int selection_type = modification_selection->GetSelection();
  if (selection_type == 0) { // all
    for (int i = 0; i < set->cards.size(); ++i) {
      cards.push_back(set->cards[i]);
    }
  } else if (selection_type == 1) { // currently filtered
    for (int i = 0; i < card_list_window->GetItemCount(); ++i) {
      cards.push_back(card_list_window->getCard(i));
    }
  } else if (selection_type == 2) { // currently selected
    card_list_window->getSelection(cards);
  } else { // predicate
    FOR_EACH(card, set->cards) {
      setContextVariables(card, ctx);
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
    const String& field_name = field_type->GetString(field_type->GetSelection());
    vector<Value*> values;
    vector<ScriptValueP> new_values;
    FOR_EACH(card, cards) {
      Value* value = get_container(set->game, card, field_name, false);
      values.push_back(value);
      setContextVariables(card, ctx);
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
  if (ctx_stylesheet)       ctx.setVariable(SCRIPT_VAR_stylesheet,       ctx_stylesheet);
  if (ctx_card_style)       ctx.setVariable(SCRIPT_VAR_card_style,       ctx_card_style);
  if (ctx_card)             ctx.setVariable(SCRIPT_VAR_card,             ctx_card);
  if (ctx_styling)          ctx.setVariable(SCRIPT_VAR_styling,          ctx_styling);
  if (ctx_extra_card_style) ctx.setVariable(SCRIPT_VAR_extra_card_style, ctx_extra_card_style);
  if (ctx_extra_card)       ctx.setVariable(SCRIPT_VAR_extra_card,       ctx_extra_card);
  // Done
  EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(BulkModificationWindow, wxDialog)
  EVT_BUTTON (wxID_OK,                   BulkModificationWindow::onOk)
  EVT_CHOICE (ID_CARD_BULK_TYPE,         BulkModificationWindow::onSelectionChange)
  EVT_TEXT   (ID_CARD_BULK_PREDICATE,    BulkModificationWindow::onPredicateChange)
  EVT_TEXT   (ID_CARD_BULK_MODIFICATION, BulkModificationWindow::onModificationChange)
END_EVENT_TABLE()
