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
#include <script/functions/construction_helpers.hpp>
#include <gui/bulk_modification_window.hpp>
#include <gui/control/card_list.hpp>
#include <util/window_id.hpp>
#include <data/action/set.hpp>
#include <wx/statline.h>

// ----------------------------------------------------------------------------- : AddCSV

BulkModificationWindow::BulkModificationWindow(Window* parent, const SetP& set, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("bulk modify"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set), parent(parent)
{
  // init controls
  predicate_description = new wxStaticText(this, -1, _LABEL_("bulk modify predicate description"));
  predicate = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  modification_description = new wxStaticText(this, -1, _LABEL_("bulk modify modification description"));
  modification = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  modification_type = new wxChoice(this, ID_CARD_BULK_TYPE, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  modification_type->Clear();
  modification_type->Append(_LABEL_("bulk modify selected"));
  modification_type->Append(_LABEL_("bulk modify predicate"));
  modification_type->SetSelection(0);
  setType();
  field_type = new wxChoice(this, ID_CARD_BULK_FIELD, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  field_type->Clear();
  FOR_EACH(field, set->game->card_fields) {
    field_type->Append(field->name);
  }
  field_type->SetSelection(0);
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
  if (modification_type->GetSelection() == 0) { // Modify from selection
    card_list_window->getSelection(cards);
  }
  else { // Modify from predicate
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
  // Make modifications
  String field_name = field_type->GetString(field_type->GetSelection());
  ScriptP modification_script = parse(modification->GetValue(), nullptr, false);
  FOR_EACH(card, cards) {
    Value* container = get_container(set->game, card, field_name);
    ctx.setVariable(SCRIPT_VAR_card, to_script(card));
    ctx.setVariable(SCRIPT_VAR_stylesheet, card->stylesheet ? to_script(card->stylesheet) : to_script(set->stylesheet));
    ScriptValueP value = modification_script->eval(ctx, false);
    set_container(container, value, field_name);
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
