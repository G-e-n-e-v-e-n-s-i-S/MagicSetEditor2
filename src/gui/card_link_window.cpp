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

CardLinkWindow::CardLinkWindow(Window* parent, const SetP& set, const CardP& selected_card, bool sizer)
  : wxDialog(parent, wxID_ANY, _TITLE_("link cards"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , set(set), selected_card(selected_card)
{
  // init controls
  selected_relation = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  linked_relation = new wxTextCtrl(this, wxID_ANY, wxEmptyString);
  relation_type = new wxChoice(this, ID_CARD_LINK_TYPE, wxDefaultPosition, wxDefaultSize, 0, nullptr);
  relation_type->Clear();
  FOR_EACH(link, set->game->card_links) {
    relation_type->Append(link);
  }
  relation_type->Append(_LABEL_("custom link"));
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
      s->Add(selected_relation, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
      s->Add(new wxStaticText(this, -1, _("  ") + _LABEL_("linked cards")), 0, wxALL, 4);
      s->Add(linked_relation, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
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
    selected_relation->ChangeValue(_LABEL_("custom link selected"));
    selected_relation->Enable();
    linked_relation->ChangeValue(_LABEL_("custom link linked"));
    linked_relation->Enable();
  }
  else {
    String relation = relation_type->GetString(sel);
    int delimiter_pos = relation.find("//");
    selected_relation->ChangeValue(relation.substr(0, delimiter_pos).Trim().Trim(false));
    selected_relation->Enable(false);
    linked_relation->ChangeValue(delimiter_pos + 2 < relation.Length() ? relation.substr(delimiter_pos + 2).Trim().Trim(false) : _LABEL_("custom link undefined"));
    linked_relation->Enable(false);
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
  // The selected_card is the one selected on the main cards tab
  // The linked_cards are the ones selected in this dialogue window
  vector<CardP> linked_cards;
  getSelection(linked_cards);
  set->actions.addAction(make_unique<LinkCardsAction>(*set, selected_card, linked_cards, selected_relation->GetValue(), linked_relation->GetValue()));
  // Done
  EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(CardLinkWindow, wxDialog)
  EVT_BUTTON       (ID_SELECT_NONE, CardLinkWindow::onSelectNone)
  EVT_BUTTON       (wxID_OK, CardLinkWindow::onOk)
  EVT_CHOICE       (ID_CARD_LINK_TYPE, CardLinkWindow::onRelationTypeChange)
END_EVENT_TABLE  ()
