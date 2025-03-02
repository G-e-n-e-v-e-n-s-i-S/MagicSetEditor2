//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/card.hpp>
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

void Card::link(const vector<CardP>& linked_cards, const String& selected_relation, const String& linked_relation)
{
  vector<String> already_linked_uids { linked_card_1, linked_card_2, linked_card_3, linked_card_4 };
  unordered_set<String> linked_uids;
  FOR_EACH(linked_card, linked_cards) {
    linked_uids.insert(linked_card->uid);
  }
  int free_link_count = 0;
  FOR_EACH(already_linked_uid, already_linked_uids) {
    if (already_linked_uid == wxEmptyString || linked_uids.find(already_linked_uid) != linked_uids.end()) free_link_count++;
  }
  if (free_link_count < linked_cards.size())
  {
    throw Error(_ERROR_("Card does not have enough free links available. Can only link up to 4 cards."));
  }

  unlink(linked_cards);

  vector<CardP> missed_cards;
  for (size_t pos = 0; pos < linked_cards.size(); ++pos) {
    CardP linked_card = linked_cards[pos];

    if (linked_card_1 == wxEmptyString)
    {
      linked_card_1 = linked_card->uid;
      linked_relation_1 = linked_relation;
    }
    else if (linked_card_2 == wxEmptyString)
    {
      linked_card_2 = linked_card->uid;
      linked_relation_2 = linked_relation;
    }
    else if (linked_card_3 == wxEmptyString)
    {
      linked_card_3 = linked_card->uid;
      linked_relation_3 = linked_relation;
    }
    else
    {
      linked_card_4 = linked_card->uid;
      linked_relation_4 = linked_relation;
    }

    if (linked_card->linked_card_1 == wxEmptyString)
    {
      linked_card->linked_card_1 = uid;
      linked_card->linked_relation_1 = selected_relation;
    }
    else if (linked_card->linked_card_2 == wxEmptyString)
    {
      linked_card->linked_card_2 = uid;
      linked_card->linked_relation_2 = selected_relation;
    }
    else if (linked_card->linked_card_3 == wxEmptyString)
    {
      linked_card->linked_card_3 = uid;
      linked_card->linked_relation_3 = selected_relation;
    }
    else if (linked_card->linked_card_4 == wxEmptyString)
    {
      linked_card->linked_card_4 = uid;
      linked_card->linked_relation_4 = selected_relation;
    }
    else
    {
      missed_cards.push_back(linked_card);
    }
  }
  if (missed_cards.size() > 0)
  {
    std::stringstream ss;
    ss << "The following cards could not be linked, as they already have 4 links: ";
    for (size_t pos = 0; pos < missed_cards.size(); ++pos) {
      ss << missed_cards[pos]->identification();
      if (pos < missed_cards.size() - 1) ss << ", ";
    };
    String wxString(ss.str().c_str(), wxConvUTF8);
    throw Error(tr(LOCALE_CAT_ERROR, wxString));
  }
}

void Card::link(CardP& linked_card, const String& selected_relation, const String& linked_relation)
{
  vector<CardP> linked_cards { linked_card };
  link(linked_cards, selected_relation, linked_relation);
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
  String selected_relation = wxEmptyString;
  String unlinked_relation = wxEmptyString;
  if (linked_card_1 == unlinked_card->uid)
  {
    selected_relation = linked_relation_1;
    unlinked_relation = unlinked_card->linked_relation_1;
    linked_card_1 = wxEmptyString;
    linked_relation_1 = wxEmptyString;
  }
  if (linked_card_2 == unlinked_card->uid)
  {
    selected_relation = linked_relation_1;
    unlinked_relation = unlinked_card->linked_relation_1;
    linked_card_2 = wxEmptyString;
    linked_relation_2 = wxEmptyString;
  }
  if (linked_card_3 == unlinked_card->uid)
  {
    selected_relation = linked_relation_1;
    unlinked_relation = unlinked_card->linked_relation_1;
    linked_card_3 = wxEmptyString;
    linked_relation_3 = wxEmptyString;
  }
  if (linked_card_4 == unlinked_card->uid)
  {
    selected_relation = linked_relation_1;
    unlinked_relation = unlinked_card->linked_relation_1;
    linked_card_4 = wxEmptyString;
    linked_relation_4 = wxEmptyString;
  }

  if (unlinked_card->linked_card_1 == uid)
  {
    unlinked_card->linked_card_1 = wxEmptyString;
    unlinked_card->linked_relation_1 = wxEmptyString;
  }
  if (unlinked_card->linked_card_2 == uid)
  {
    unlinked_card->linked_card_2 = wxEmptyString;
    unlinked_card->linked_relation_2 = wxEmptyString;
  }
  if (unlinked_card->linked_card_3 == uid)
  {
    unlinked_card->linked_card_3 = wxEmptyString;
    unlinked_card->linked_relation_3 = wxEmptyString;
  }
  if (unlinked_card->linked_card_4 == uid)
  {
    unlinked_card->linked_card_4 = wxEmptyString;
    unlinked_card->linked_relation_4 = wxEmptyString;
  }
  return make_pair(selected_relation, unlinked_relation);
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
