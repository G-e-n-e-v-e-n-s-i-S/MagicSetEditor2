//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/game.hpp>
#include <gui/card_link_window.hpp>
#include <gui/control/select_card_list.hpp>
#include <util/window_id.hpp>
#include <data/action/set.hpp>
#include <wx/statline.h>

// ----------------------------------------------------------------------------- : ExportCardSelectionChoice

CardLinkWindow::CardLinkWindow(Window* parent, const SetP& set, const CardP& selectedCard, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("link cards"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set), selectedCard(selectedCard)
{
  // init controls
  selectedRelation = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  linkedRelation = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  relation_type = new wxChoice(this, ID_CARD_LINK_TYPE, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  relation_type->Clear();
  FOR_EACH(link, set->game->card_links) {
    relation_type->Append(link);
  }
  relation_type->Append(_("Custom..."));
  relation_type->SetSelection(0);
  setRelationType();
  list = new SelectCardList(this, wxID_ANY);
  list->setSet(set);
  list->selectNone();
  sel_none = new wxButton(this, ID_SELECT_NONE, _BUTTON_("select none"));
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
      s->Add(new wxStaticText(this, -1, _LABEL_("linked cards relation")), 0, wxALL, 8);
      s->Add(relation_type, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
      s->Add(new wxStaticText(this, -1, _("  ") + _LABEL_("selected card")), 0, wxALL, 4);
      s->Add(selectedRelation, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
      s->Add(new wxStaticText(this, -1, _("  ") + _LABEL_("linked cards")), 0, wxALL, 4);
      s->Add(linkedRelation, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
      s->Add(new wxStaticText(this, wxID_ANY, _LABEL_("select linked cards")), 0, wxALL & ~wxBOTTOM, 8);
      s->Add(list, 1, wxEXPAND | wxALL, 8);
      wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
        s2->Add(sel_none, 0, wxEXPAND | wxRIGHT, 8);
        s2->Add(CreateButtonSizer(wxOK | wxCANCEL), 1, wxEXPAND, 8);
      s->Add(s2, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
    s->SetSizeHints(this);
    SetSizer(s);
    SetSize(600,500);
  }
}

bool CardLinkWindow::isSelected(const CardP& card) const {
  return list->isSelected(card);
}

void CardLinkWindow::getSelection(vector<CardP>& out) const {
  list->getSelection(out);
}

void CardLinkWindow::setSelection(const vector<CardP>& cards) {
  list->setSelection(cards);
}
void CardLinkWindow::setRelationType() {
  int sel = relation_type->GetSelection();
  if (sel == relation_type->GetCount() - 1) { // Custom type
    selectedRelation->ChangeValue(_("Generator, Front Face, Meld Component, etc..."));
    selectedRelation->Enable();
    linkedRelation->ChangeValue(_("Token, Back Face, Meld Result, etc..."));
    linkedRelation->Enable();
  }
  else {
    String relation = relation_type->GetString(sel);
    int delimiter_pos = relation.find(" // ");
    selectedRelation->ChangeValue(relation.substr(0, delimiter_pos));
    selectedRelation->Enable(false);
    linkedRelation->ChangeValue(delimiter_pos + 4 < relation.Length() ? relation.substr(delimiter_pos + 4) : _("Undefined"));
    linkedRelation->Enable(false);
  }
}

void CardLinkWindow::onSelectNone(wxCommandEvent&) {
  list->selectNone();
}

void CardLinkWindow::onRelationTypeChange(wxCommandEvent&) {
  setRelationType();
}

void CardLinkWindow::onOk(wxCommandEvent&) {
  // Perform the linking
  // The selectedCard is the one selected on the main cards tab
  // The linkedCards are the ones selected in this dialogue window
  vector<CardP> linkedCards;
  getSelection(linkedCards);
  set->actions.addAction(make_unique<LinkCardsAction>(*set, selectedCard, linkedCards, selectedRelation->GetValue(), linkedRelation->GetValue()));
  // Done
  EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(CardLinkWindow, wxDialog)
  EVT_BUTTON       (ID_SELECT_NONE, CardLinkWindow::onSelectNone)
  EVT_BUTTON       (wxID_OK, CardLinkWindow::onOk)
  EVT_CHOICE       (ID_CARD_LINK_TYPE, CardLinkWindow::onRelationTypeChange)
END_EVENT_TABLE  ()
