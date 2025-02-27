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

void Card::link(const vector<CardP>& linkedCards, const String& selectedRelation, const String& linkedRelation)
{
  int free_link_count = 0;
  if (linked_card_1 == wxEmptyString) free_link_count++;
  if (linked_card_2 == wxEmptyString) free_link_count++;
  if (linked_card_3 == wxEmptyString) free_link_count++;
  if (linked_card_4 == wxEmptyString) free_link_count++;
  if (free_link_count < linkedCards.size())
  {
    throw Error(_ERROR_("Card does not have enough free links available. Can only link up to 4 cards."));
  }

  vector<CardP> missedCards;
  for (size_t pos = 0; pos < linkedCards.size(); ++pos) {
    CardP linkedCard = linkedCards[pos];

    if (linked_card_1 == wxEmptyString)
    {
      linked_card_1 = linkedCard->uid;
      linked_relation_1 = linkedRelation;
    }
    else if (linked_card_2 == wxEmptyString)
    {
      linked_card_2 = linkedCard->uid;
      linked_relation_2 = linkedRelation;
    }
    else if (linked_card_3 == wxEmptyString)
    {
      linked_card_3 = linkedCard->uid;
      linked_relation_3 = linkedRelation;
    }
    else
    {
      linked_card_4 = linkedCard->uid;
      linked_relation_4 = linkedRelation;
    }

    if (linkedCard->linked_card_1 == wxEmptyString)
    {
      linkedCard->linked_card_1 = uid;
      linkedCard->linked_relation_1 = selectedRelation;
    }
    else if (linkedCard->linked_card_2 == wxEmptyString)
    {
      linkedCard->linked_card_2 = uid;
      linkedCard->linked_relation_2 = selectedRelation;
    }
    else if (linkedCard->linked_card_3 == wxEmptyString)
    {
      linkedCard->linked_card_3 = uid;
      linkedCard->linked_relation_3 = selectedRelation;
    }
    else if (linkedCard->linked_card_4 == wxEmptyString)
    {
      linkedCard->linked_card_4 = uid;
      linkedCard->linked_relation_4 = selectedRelation;
    }
    else
    {
      missedCards.push_back(linkedCard);
    }
  }
  if (missedCards.size() > 0)
  {
    std::stringstream ss;
    ss << "The following cards could not be linked, as they already have 4 links: ";
    for (size_t pos = 0; pos < missedCards.size(); ++pos) {
      ss << missedCards[pos]->identification();
      if (pos < missedCards.size() - 1) ss << ", ";
    };
    String wxString(ss.str().c_str(), wxConvUTF8);
    throw Error(tr(LOCALE_CAT_ERROR, wxString));
  }
}

void Card::unlink(const vector<CardP>& linkedCards)
{
  for (size_t pos = 0; pos < linkedCards.size(); ++pos) {
    CardP linkedCard = linkedCards[pos];

    if (linked_card_1 == linkedCard->uid)
    {
      linked_card_1 = wxEmptyString;
      linked_relation_1 = wxEmptyString;
    }
    if (linked_card_2 == linkedCard->uid)
    {
      linked_card_2 = wxEmptyString;
      linked_relation_2 = wxEmptyString;
    }
    if (linked_card_3 == linkedCard->uid)
    {
      linked_card_3 = wxEmptyString;
      linked_relation_3 = wxEmptyString;
    }
    if (linked_card_4 == linkedCard->uid)
    {
      linked_card_4 = wxEmptyString;
      linked_relation_4 = wxEmptyString;
    }

    if (linkedCard->linked_card_1 == uid)
    {
      linkedCard->linked_card_1 = wxEmptyString;
      linkedCard->linked_relation_1 = wxEmptyString;
    }
    if (linkedCard->linked_card_2 == uid)
    {
      linkedCard->linked_card_2 = wxEmptyString;
      linkedCard->linked_relation_2 = wxEmptyString;
    }
    if (linkedCard->linked_card_3 == uid)
    {
      linkedCard->linked_card_3 = wxEmptyString;
      linkedCard->linked_relation_3 = wxEmptyString;
    }
    if (linkedCard->linked_card_4 == uid)
    {
      linkedCard->linked_card_4 = wxEmptyString;
      linkedCard->linked_relation_4 = wxEmptyString;
    }
  }
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
