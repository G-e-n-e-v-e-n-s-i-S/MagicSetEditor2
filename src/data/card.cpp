//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/card.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/field.hpp>
#include <util/error.hpp>
#include <util/reflect.hpp>
#include <util/delayed_index_maps.hpp>
#include <util/uid.hpp>
#include <unordered_set>

// ----------------------------------------------------------------------------- : Card

Card::Card()
    // for files made before we saved these, set the time to 'yesterday', generate a uid
  : time_created (wxDateTime::Now().Subtract(wxDateSpan::Day()).ResetTime())
  , time_modified(wxDateTime::Now().Subtract(wxDateSpan::Day()).ResetTime())
  , uid(generate_uid())
  , has_styling(false)
{
  if (!game_for_reading()) {
    throw InternalError(_("game_for_reading not set"));
  }
  data.init(game_for_reading()->card_fields);
}

Card::Card(const Game& game)
  : time_created (wxDateTime::Now())
  , time_modified(wxDateTime::Now())
  , uid(generate_uid())
  , has_styling(false)
{
  data.init(game.card_fields);
}

String Card::identification() const {
  // an identifying field
  FOR_EACH_CONST(v, data) {
    if (v->fieldP->identifying) {
      return v->toString();
    }
  }
  // otherwise the first field
  if (!data.empty()) {
    return data.at(0)->toString();
  } else {
    return wxEmptyString;
  }
}

bool Card::contains(QuickFilterPart const& query) const {
  FOR_EACH_CONST(v, data) {
    if (query.match(v->fieldP->name, v->toString())) return true;
  }
  if (query.match(_("notes"), notes)) return true;
  return false;
}

void Card::link(const Set& set, const vector<CardP>& linked_cards, const String& selected_relation, const String& linked_relation)
{
  unlink(linked_cards);

  unordered_set<String> all_existing_uids;
  FOR_EACH(card, set.cards) {
    all_existing_uids.insert(card->uid);
  }
  int free_link_count = 0;
  THIS_LINKED_PAIRS(this_linked_pairs);
  FOR_EACH(this_linked_pair, this_linked_pairs) {
    String& this_linked_uid = this_linked_pair.first.get();
    if (
      this_linked_uid == wxEmptyString ||                                // Not a reference
      all_existing_uids.find(this_linked_uid) == all_existing_uids.end() // Reference to nonexistent card
    ) free_link_count++;
  }
  if (free_link_count < linked_cards.size()) {
    queue_message(MESSAGE_WARNING, _ERROR_("not enough free links"));
    return;
  }

  vector<CardP> all_missed_cards;
  FOR_EACH(linked_card, linked_cards) {
    bool written = false;
    // Try to write to a free spot
    FOR_EACH(this_linked_pair, this_linked_pairs) {
      String& this_linked_uid = this_linked_pair.first.get();
      String& this_linked_relation = this_linked_pair.second.get();
      if (this_linked_uid == wxEmptyString) {
        this_linked_uid = linked_card->uid;
        this_linked_relation = linked_relation;
        written = true;
        break;
      }
    }
    // Try to write to an erasable spot
    if (!written) {
      FOR_EACH(this_linked_pair, this_linked_pairs) {
        String& this_linked_uid = this_linked_pair.first.get();
        String& this_linked_relation = this_linked_pair.second.get();
        if (all_existing_uids.find(this_linked_uid) == all_existing_uids.end()) {
          this_linked_uid = linked_card->uid;
          this_linked_relation = linked_relation;
          written = true;
          break;
        }
      }
    }
    if (!written) {
      // Should be impossible to end up here?
    }

    OTHER_LINKED_PAIRS(linked_pairs, linked_card);
    written = false;
    // Try to write to a free spot
    FOR_EACH(linked_pair, linked_pairs) {
      String& linked_uid = linked_pair.first.get();
      String& linked_relation = linked_pair.second.get();
      if (linked_uid == wxEmptyString) {
        linked_uid = uid;
        linked_relation = selected_relation;
        written = true;
        break;
      }
    }
    // Try to write to an erasable spot
    if (!written) {
      FOR_EACH(linked_pair, linked_pairs) {
        String& linked_uid = linked_pair.first.get();
        String& linked_relation = linked_pair.second.get();
        if (all_existing_uids.find(linked_uid) == all_existing_uids.end()) {
          linked_uid = uid;
          linked_relation = selected_relation;
          written = true;
          break;
        }
      }
    }
    // Notify we couldn't write
    if (!written) {
      all_missed_cards.push_back(linked_card);
    }
  }
  if (all_missed_cards.size() > 0) {
    std::stringstream ss;
    ss << _ERROR_("could not link");
    for (size_t pos = 0; pos < all_missed_cards.size(); ++pos) {
      ss << all_missed_cards[pos]->identification();
      if (pos < all_missed_cards.size() - 1) ss << ", ";
    };
    String wxString(ss.str().c_str(), wxConvUTF8);
    queue_message(MESSAGE_WARNING, wxString);
  }
}

void Card::link(const Set& set, CardP& linked_card, const String& selected_relation, const String& linked_relation)
{
  vector<CardP> linked_cards { linked_card };
  link(set, linked_cards, selected_relation, linked_relation);
}

void Card::unlink(const vector<CardP>& unlinked_cards)
{
  for (size_t pos = 0; pos < unlinked_cards.size(); ++pos) {
    CardP unlinked_card = unlinked_cards[pos];
    unlink(unlinked_card);
  }
}

pair<String, String> Card::unlink(CardP& unlinked_card)
{
  String old_selected_relation = wxEmptyString;
  THIS_LINKED_PAIRS(this_linked_pairs);
  FOR_EACH(this_linked_pair, this_linked_pairs) {
    String& this_linked_uid = this_linked_pair.first.get();
    String& this_linked_relation = this_linked_pair.second.get();
    if (this_linked_uid == unlinked_card->uid) {
      old_selected_relation = this_linked_relation;
      this_linked_uid = wxEmptyString;
      this_linked_relation = wxEmptyString;
    }
  }
  String old_unlinked_relation = wxEmptyString;
  OTHER_LINKED_PAIRS(unlinked_pairs, unlinked_card);
  FOR_EACH(unlinked_pair, unlinked_pairs) {
    String& unlinked_uid = unlinked_pair.first.get();
    String& unlinked_relation = unlinked_pair.second.get();
    if (unlinked_uid == uid) {
      old_unlinked_relation = unlinked_relation;
      unlinked_uid = wxEmptyString;
      unlinked_relation = wxEmptyString;
    }
  }
  return make_pair(old_selected_relation, old_unlinked_relation);
}

void Card::copyLink(const Set& set, String old_uid, String new_uid) {
  // Find what relation we need to copy
  String relation_copy = wxEmptyString;
  THIS_LINKED_PAIRS(this_linked_pairs);
  FOR_EACH(this_linked_pair, this_linked_pairs) {
    String& this_linked_uid = this_linked_pair.first.get();
    String& this_linked_relation = this_linked_pair.second.get();
    if (this_linked_uid == old_uid) {
      relation_copy = this_linked_relation;
      break;
    }
  }
  // Nothing to copy
  if (relation_copy == wxEmptyString) {
    return;
  }

  // Try to copy to a free spot
  bool written = false;
  FOR_EACH(this_linked_pair, this_linked_pairs) {
    String& this_linked_uid = this_linked_pair.first.get();
    String& this_linked_relation = this_linked_pair.second.get();
    if (this_linked_uid == wxEmptyString) {
      this_linked_uid = new_uid;
      this_linked_relation = relation_copy;
      written = true;
      break;
    }
  }
  // Try to copy to an erasable spot
  if (!written) {
    unordered_set<String> all_existing_uids;
    FOR_EACH(card, set.cards) {
      all_existing_uids.insert(card->uid);
    }
    FOR_EACH(this_linked_pair, this_linked_pairs) {
      String& this_linked_uid = this_linked_pair.first.get();
      String& this_linked_relation = this_linked_pair.second.get();
      if (all_existing_uids.find(this_linked_uid) == all_existing_uids.end()) {
        this_linked_uid = new_uid;
        this_linked_relation = relation_copy;
        written = true;
        break;
      }
    }
  }
  // Notify we couldn't copy
  if (!written) {
    queue_message(MESSAGE_WARNING, _ERROR_("not enough free links for copy"));
  }
}

void Card::updateLink(String old_uid, String new_uid) {
  THIS_LINKED_PAIRS(this_linked_pairs);
  FOR_EACH(this_linked_pair, this_linked_pairs) {
    String& this_linked_uid = this_linked_pair.first.get();
    if (this_linked_uid == old_uid) {
      this_linked_uid = new_uid;
      return;
    }
  }
}

vector<pair<CardP, String>> Card::getLinkedCards(const Set& set) {
  unordered_map<String, String> links{
    { linked_card_1, linked_relation_1 },
    { linked_card_2, linked_relation_2 },
    { linked_card_3, linked_relation_3 },
    { linked_card_4, linked_relation_4 }
  };
  vector<pair<CardP, String>> linked_cards;
  FOR_EACH(other_card, set.cards) {
    if (links.find(other_card->uid) != links.end()) {
      linked_cards.push_back(make_pair(other_card, links.at(other_card->uid)));
    }
  }
  return linked_cards;
}

IndexMap<FieldP, ValueP>& Card::extraDataFor(const StyleSheet& stylesheet) {
  return extra_data.get(stylesheet.name(), stylesheet.extra_card_fields);
}

void mark_dependency_member(const Card& card, const String& name, const Dependency& dep) {
  mark_dependency_member(card.data, name, dep);
}

void reflect_version_check(Reader& handler, const Char* key, intrusive_ptr<Packaged> const& package);
void reflect_version_check(Writer& handler, const Char* key, intrusive_ptr<Packaged> const& package);
void reflect_version_check(GetMember& handler, const Char* key, intrusive_ptr<Packaged> const& package);
void reflect_version_check(GetDefaultMember& handler, const Char* key, intrusive_ptr<Packaged> const& package);

IMPLEMENT_REFLECTION(Card) {
  REFLECT(stylesheet);
  reflect_version_check(handler, _("stylesheet_version"), stylesheet);
  REFLECT(has_styling);
  if (has_styling) {
    if (stylesheet) {
      REFLECT_IF_READING styling_data.init(stylesheet->styling_fields);
      REFLECT(styling_data);
    } else if (stylesheet_for_reading()) {
      REFLECT_IF_READING styling_data.init(stylesheet_for_reading()->styling_fields);
      REFLECT(styling_data);
    } else if (Handler::isReading) {
      has_styling = false; // We don't know the style, this can be because of copy/pasting
    }
  }
  REFLECT(notes);
  REFLECT(uid);
  REFLECT(linked_card_1);
  REFLECT(linked_card_2);
  REFLECT(linked_card_3);
  REFLECT(linked_card_4);
  REFLECT(linked_relation_1);
  REFLECT(linked_relation_2);
  REFLECT(linked_relation_3);
  REFLECT(linked_relation_4);
  REFLECT(time_created);
  REFLECT(time_modified);
  REFLECT(extra_data); // don't allow scripts to depend on style specific data
  REFLECT_NAMELESS(data);
}
