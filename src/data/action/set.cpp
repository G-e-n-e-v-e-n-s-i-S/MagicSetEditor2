//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/action/set.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/pack.hpp>
#include <data/stylesheet.hpp>
#include <util/error.hpp>
#include <util/uid.hpp>

// ----------------------------------------------------------------------------- : Add card

AddCardAction::AddCardAction(Set& set)
  : CardListAction(set)
  , action(ADD, make_intrusive<Card>(*set.game), set.cards)
{}

AddCardAction::AddCardAction(AddingOrRemoving ar, Set& set, const CardP& card)
  : CardListAction(set)
  , action(ar, card, set.cards)
{}

AddCardAction::AddCardAction(AddingOrRemoving ar, Set& set, const vector<CardP>& cards)
  : CardListAction(set)
  , action(ar, cards, set.cards)
{}

String AddCardAction::getName(bool to_undo) const {
  return action.getName();
}

void AddCardAction::perform(bool to_undo) {
  action.perform(set.cards, to_undo);
  // Assign new unique ids if necessary
  if (action.adding) {
    for (size_t pos = 0; pos < action.steps.size(); ++pos) {
      CardP added_card = action.steps[pos].item;
      FOR_EACH(card, set.cards) {
        if (added_card == card) continue;
        String old_uid = added_card->uid;
        if (old_uid == card->uid) {
          String new_uid = generate_uid();
          added_card->uid = new_uid;
          for (size_t other_pos = 0; other_pos < action.steps.size(); ++other_pos) {
            if (pos == other_pos) continue;
            CardP other_added_card = action.steps[other_pos].item;
            if (other_added_card->linked_card_1 == old_uid) other_added_card->linked_card_1 = new_uid;
            if (other_added_card->linked_card_2 == old_uid) other_added_card->linked_card_2 = new_uid;
            if (other_added_card->linked_card_3 == old_uid) other_added_card->linked_card_3 = new_uid;
            if (other_added_card->linked_card_4 == old_uid) other_added_card->linked_card_4 = new_uid;
          }
          break;
        }
      }
    }
  }
}


// ----------------------------------------------------------------------------- : Reorder cards

ReorderCardsAction::ReorderCardsAction(Set& set, size_t card_id1, size_t card_id2)
  : CardListAction(set), card_id1(card_id1), card_id2(card_id2)
{}

String ReorderCardsAction::getName(bool to_undo) const {
  return _("Reorder cards");
}

void ReorderCardsAction::perform(bool to_undo) {
  #ifdef _DEBUG
    assert(card_id1 < set.cards.size());
    assert(card_id2 < set.cards.size());
  #endif
  if (card_id1 >= set.cards.size() || card_id2 >= set.cards.size()) {
    // TODO : Too lazy to fix this right now.
    return;
  }
  swap(set.cards[card_id1], set.cards[card_id2]);
}

// ----------------------------------------------------------------------------- : Link cards

LinkCardsAction::LinkCardsAction(Set& set, const CardP& selectedCard, vector<CardP>& linkedCards, const String& selectedRelation, const String& linkedRelation)
  : CardListAction(set), selectedCard(selectedCard), linkedCards(linkedCards), selectedRelation(selectedRelation), linkedRelation(linkedRelation)
{}

String LinkCardsAction::getName(bool to_undo) const {
  return _("Link cards");
}

void LinkCardsAction::perform(bool to_undo) {
  if (!to_undo) {
    selectedCard->link(linkedCards, selectedRelation, linkedRelation);
  } else {
    selectedCard->unlink(linkedCards);
  }
}

// ----------------------------------------------------------------------------- : Change stylesheet

String DisplayChangeAction::getName(bool to_undo) const {
  assert(false);
  return _("");
}
void DisplayChangeAction::perform(bool to_undo) {
  assert(false);
}


ChangeCardStyleAction::ChangeCardStyleAction(const CardP& card, const StyleSheetP& stylesheet)
  : card(card), stylesheet(stylesheet), has_styling(false) // styling_data(empty)
{}
String ChangeCardStyleAction::getName(bool to_undo) const {
  return _("Change style");
}
void ChangeCardStyleAction::perform(bool to_undo) {
  swap(card->stylesheet,   stylesheet);
  swap(card->has_styling,  has_styling);
  swap(card->styling_data, styling_data);
}


ChangeSetStyleAction::ChangeSetStyleAction(Set& set, const CardP& card)
  : set(set), card(card)
{}
String ChangeSetStyleAction::getName(bool to_undo) const {
  return _("Change style (all cards)");
}
void ChangeSetStyleAction::perform(bool to_undo) {
  if (!to_undo) {
    // backup has_styling
    has_styling.clear();
    FOR_EACH(card, set.cards) {
      has_styling.push_back(card->has_styling);
      if (!card->stylesheet) {
        card->has_styling = false; // this card has custom style options for the default stylesheet
      }
    }
    stylesheet       = set.stylesheet;
    set.stylesheet   = card->stylesheet;
    card->stylesheet = StyleSheetP();
  } else {
    card->stylesheet = set.stylesheet;
    set.stylesheet   = stylesheet;
    // restore has_styling
    FOR_EACH_2(card, set.cards, has, has_styling) {
      card->has_styling = has;
    }
  }
}

ChangeCardHasStylingAction::ChangeCardHasStylingAction(Set& set, const CardP& card)
  : set(set), card(card)
{
  if (!card->has_styling) {
    // copy of the set's styling data
    styling_data.cloneFrom( set.stylingDataFor(set.stylesheetFor(card)) );
  } else {
    // the new styling data is empty
  }
}
String ChangeCardHasStylingAction::getName(bool to_undo) const {
  return _("Use custom style");
}
void ChangeCardHasStylingAction::perform(bool to_undo) {
  card->has_styling = !card->has_styling;
  swap(card->styling_data, styling_data);
}

// ----------------------------------------------------------------------------- : Pack types

AddPackAction::AddPackAction(AddingOrRemoving ar, Set& set, const PackTypeP& pack)
  : PackTypesAction(set)
  , action(ar, pack, set.pack_types)
{}

String AddPackAction::getName(bool to_undo) const {
  return action.getName();
}

void AddPackAction::perform(bool to_undo) {
  action.perform(set.pack_types, to_undo);
}


ChangePackAction::ChangePackAction(Set& set, size_t pos, const PackTypeP& pack)
  : PackTypesAction(set)
  , pack(pack), pos(pos)
{}

String ChangePackAction::getName(bool to_undo) const {
  return _ACTION_1_("change",type_name(pack));
}

void ChangePackAction::perform(bool to_undo) {
  swap(set.pack_types.at(pos), pack);
}
