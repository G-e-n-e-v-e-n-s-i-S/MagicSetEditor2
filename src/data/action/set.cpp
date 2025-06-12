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
  // If we are adding cards, resolve any uid conflicts
  // (If we are re-adding cards, from a remove undo, there shouldn't be any uid conflicts)
  // We always assume uid conflicts occur because a card was copy-pasted into the same set,
  // and never because two different cards randomly got assigned the same uid
  if (action.adding && !to_undo) {
    // Tally existing unique ids
    unordered_map<String, CardP> all_existing_uids;
    FOR_EACH(card, set.cards) {
      all_existing_uids.insert({ card->uid, card });
    }
    // Tally added unique ids
    unordered_map<String, CardP> all_added_uids;
    for (size_t pos = 0; pos < action.steps.size(); ++pos) {
      CardP card = action.steps[pos].item;
      all_added_uids.insert({ card->uid, card });
    }
    FOR_EACH(added_pair, all_added_uids) {
      String old_uid = added_pair.first;
      CardP added_card = added_pair.second;
      // Assign new unique ids
      if (all_existing_uids.find(old_uid) != all_existing_uids.end()) {
        String new_uid = generate_uid();
        added_card->uid = new_uid;
        all_added_uids.insert({ new_uid, added_card });
        // Update links on linked cards
        OTHER_LINKED_PAIRS(linked_pairs, added_card);
        FOR_EACH(linked_pair, linked_pairs) {
          String& linked_uid = linked_pair.first.get();
          String& linked_relation = linked_pair.second.get();
          if (linked_uid == wxEmptyString) continue;
          // If it's an added card, replace the link
          if (all_added_uids.find(linked_uid) != all_added_uids.end()) {
            all_added_uids.at(linked_uid)->updateLink(old_uid, new_uid);
          }
          // Otherwise, if it's an existing card, copy the link
          else if (all_existing_uids.find(linked_uid) != all_existing_uids.end()) {
            all_existing_uids.at(linked_uid)->copyLink(set, old_uid, new_uid);
          }
        }
      }
    }
  }

  // Add or remove cards
  action.perform(set.cards, to_undo);
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

LinkCardsAction::LinkCardsAction(Set& set, const CardP& selected_card, vector<CardP>& linked_cards, const String& selected_relation, const String& linked_relation)
  : CardListAction(set), selected_card(selected_card), linked_cards(linked_cards), selected_relation(selected_relation), linked_relation(linked_relation)
{}

String LinkCardsAction::getName(bool to_undo) const {
  return _("Link cards");
}

void LinkCardsAction::perform(bool to_undo) {
  if (!to_undo) {
    selected_card->link(set, linked_cards, selected_relation, linked_relation);
  } else {
    selected_card->unlink(linked_cards);
  }
}

UnlinkCardsAction::UnlinkCardsAction(Set& set, const CardP& selected_card, CardP& unlinked_card)
  : CardListAction(set), selected_card(selected_card), unlinked_card(unlinked_card)
{}

String UnlinkCardsAction::getName(bool to_undo) const {
  return _("Unlink card");
}

void UnlinkCardsAction::perform(bool to_undo) {
  if (!to_undo) {
    pair<String, String> relations = selected_card->unlink(unlinked_card);
    selected_relation = relations.first;
    unlinked_relation = relations.second;
  }
  else {
    selected_card->link(set, unlinked_card, selected_relation, unlinked_relation);
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
