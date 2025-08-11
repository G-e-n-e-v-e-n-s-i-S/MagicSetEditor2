//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/pack.hpp>
#include <data/format/clipboard.hpp>
#include <script/functions/construction_helper.hpp>
#include <boost/json.hpp>
#include <sstream>
#include <wx/sstream.h>


// All this isn't great, but it will do for now. Idealy you would create JsonWriter and JsonReader classes
// that inherit from Writer and Reader, and just have a switch to go from normal mode to JSON mode...


// ----------------------------------------------------------------------------- : JSON to String

inline static void pretty_print(std::ostream& os, const boost::json::value& jv, std::string* indent = nullptr)
{
  std::string indent_;
  if(! indent)
    indent = &indent_;
  switch(jv.kind())
  {
  case boost::json::kind::object:
  {
    os << "{\n";
    indent->append(4, ' ');
    auto const& obj = jv.get_object();
    if(! obj.empty())
    {
      auto it = obj.begin();
      for(;;)
      {
        os << *indent << boost::json::serialize(it->key()) << " : ";
        pretty_print(os, it->value(), indent);
        if(++it == obj.end())
          break;
        os << ",\n";
      }
    }
    os << "\n";
    indent->resize(indent->size() - 4);
    os << *indent << "}";
    break;
  }

  case boost::json::kind::array:
  {
    os << "[\n";
    indent->append(4, ' ');
    auto const& arr = jv.get_array();
    if(! arr.empty())
    {
      auto it = arr.begin();
      for(;;)
      {
        os << *indent;
        pretty_print( os, *it, indent);
        if(++it == arr.end())
          break;
        os << ",\n";
      }
    }
    os << "\n";
    indent->resize(indent->size() - 4);
    os << *indent << "]";
    break;
  }

  case boost::json::kind::string:
  {
    os << boost::json::serialize(jv.get_string());
    break;
  }

  case boost::json::kind::uint64:
  case boost::json::kind::int64:
  case boost::json::kind::double_:
    os << jv;
    break;

  case boost::json::kind::bool_:
    if(jv.get_bool())
      os << "true";
    else
      os << "false";
    break;

  case boost::json::kind::null:
    os << "null";
    break;
  }

  if(indent->empty())
    os << "\n";
}

inline static String json_pretty_print(const boost::json::value& jv, std::string* indent = nullptr) {
  std::ostringstream stream;
  pretty_print(stream, jv, indent);
  String string = wxString(stream.str().c_str());
  return string;
}

inline static String json_ugly_print(const boost::json::value& jv) {
  String string = wxString(boost::json::serialize(jv).c_str());
  return string;
}

// ----------------------------------------------------------------------------- : JSON to MSE

inline static ScriptValueP json_to_mse(const boost::json::value& jv, Set* set);

template <typename T>
static void read(T& out, boost::json::object& jv, const char value_name[]) {
  if (!jv.contains(value_name)) return;
  else {
    wxStringInputStream stream = {_("")};
    Reader reader(stream, nullptr, _(""));
    reader.setValue(wxString(jv[value_name].as_string().c_str()));
    reader.handle(out);
  }
}

// templates don't work with enums? are you kidding me with this language?
static void read(PackSelectType& out, boost::json::object& jv, const char value_name[]) {
  if (!jv.contains(value_name)) return;
  else {
    wxStringInputStream stream = {_("")};
    Reader reader(stream, nullptr, _(""));
    reader.setValue(wxString(jv[value_name].as_string().c_str()));
    reader.handle(out);
  }
}

inline static PackItemP json_to_mse_pack_item(boost::json::object& jv) {
  PackItemP pack_item = make_intrusive<PackItem>();
  read(pack_item->name,   jv, "name");
  read(pack_item->amount, jv, "amount");
  read(pack_item->weight, jv, "weight");
  return pack_item;
}

inline static PackTypeP json_to_mse_pack_type(boost::json::object& jv) {
  PackTypeP pack_type = make_intrusive<PackType>();
  read(pack_type->name,       jv, "name");
  read(pack_type->enabled,    jv, "enabled");
  read(pack_type->selectable, jv, "selectable");
  read(pack_type->summary,    jv, "summary");
  read(pack_type->select,     jv, "select");
  if (jv.contains("items") && jv["items"].is_array()) {
    boost::json::array pack_itemsv = jv["items"].as_array();
    for (int i = 0; i < pack_itemsv.size(); i++) {
      boost::json::object pack_itemv = pack_itemsv[i].as_object();
      pack_type->items.emplace_back(json_to_mse_pack_item(pack_itemv));
    }
  }
  return pack_type;
}

inline static KeywordP json_to_mse_keyword(boost::json::object& jv) {
  KeywordP keyword = make_intrusive<Keyword>();
  read(keyword->keyword,  jv, "keyword");
  read(keyword->match,    jv, "match");
  read(keyword->reminder, jv, "reminder");
  read(keyword->rules,    jv, "rules");
  read(keyword->mode,     jv, "mode");
  return keyword;
}

inline static CardP json_to_mse_card(boost::json::object& jv, Set* set) {
  CardP card = make_intrusive<Card>(*set->game);
  read(card->time_created,                     jv, "time_created");
  read(card->time_modified,                    jv, "time_modified");
  read(card->notes,                            jv, "notes");
  //read(card->uid,                              jv, "uid");
  //read(card->linked_card_1,                    jv, "linked_card_1");
  //read(card->linked_card_2,                    jv, "linked_card_2");
  //read(card->linked_card_3,                    jv, "linked_card_3");
  //read(card->linked_card_4,                    jv, "linked_card_4");
  //read(card->linked_relation_1,                jv, "linked_relation_1");
  //read(card->linked_relation_2,                jv, "linked_relation_2");
  //read(card->linked_relation_3,                jv, "linked_relation_3");
  //read(card->linked_relation_4,                jv, "linked_relation_4");
  // card fields
  if (jv.contains("data") && jv["data"].is_object()) {
    boost::json::object datav = jv["data"].as_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      String key_name = wxString(it->key_c_str());
      Value* container = get_card_field_container(*set->game, card->data, key_name, false);
      ScriptValueP value = json_to_mse(it->value(), set);
      set_container(container, value, key_name);
    }
  }
  // stylesheet
  if (jv.contains("stylesheet")) card->stylesheet = StyleSheet::byGameAndName(*set->game, wxString(jv["stylesheet"].as_string().c_str()));
  if (card->stylesheet) {
    // styling fields
    card->styling_data.init(card->stylesheet->styling_fields);
    if (jv.contains("styling_data") && jv["styling_data"].is_object()) {
      boost::json::object datav = jv["styling_data"].as_object();
      for (auto it = datav.begin(); it != datav.end(); ++it) {
        String key_name = wxString(it->key_c_str());
        Value* container = get_container(card->styling_data, wxString("styling"), key_name, false);
        ScriptValueP value = json_to_mse(it->value(), set);
        set_container(container, value, key_name);
        card->has_styling = true;
      }
    }
    // extra card fields
    if (jv.contains("extra_data") && jv["extra_data"].is_object()) {
      boost::json::object datav = jv["extra_data"].as_object();
      for (auto it = datav.begin(); it != datav.end(); ++it) {
        StyleSheetP& stylesheet = StyleSheet::byGameAndName(*set->game, it->key_c_str());
        if (!stylesheet) continue;
        IndexMap<FieldP, ValueP>& stylesheet_data = card->extraDataFor(*stylesheet);
        boost::json::object stylesheet_datav = it->value().as_object();
        for (auto stylesheet_it = stylesheet_datav.begin(); stylesheet_it != stylesheet_datav.end(); ++stylesheet_it) {
          String key_name = wxString(stylesheet_it->key_c_str());
          Value* container = get_container(stylesheet_data, wxString("extra card"), key_name, false);
          ScriptValueP value = json_to_mse(stylesheet_it->value(), set);
          set_container(container, value, key_name);
        }
      }
    }
  }
  return card;
}

inline static SetP json_to_mse_set(boost::json::object& jv) {
  if (!jv.contains("game")) {
    throw ScriptError(_ERROR_("json set without game"));
  }
  if (!jv.contains("stylesheet")) {
    throw ScriptError(_ERROR_("json set without stylesheet"));
  }
  GameP& game = Game::byName(wxString(jv["game"].as_string().c_str()));
  StyleSheetP& stylesheet = StyleSheet::byGameAndName(*game, wxString(jv["stylesheet"].as_string().c_str()));
  SetP& set = make_intrusive<Set>(stylesheet);
  // set fields
  if (jv.contains("set_info") && jv["set_info"].is_object()) {
    boost::json::object datav = jv["set_info"].as_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      String key_name = wxString(it->key_c_str());
      Value* container = get_container(set->data, wxString("set"), key_name, false);
      ScriptValueP value = json_to_mse(it->value(), set.get());
      set_container(container, value, key_name);
    }
  }
  // styling
  if (jv.contains("styling") && jv["styling"].is_object()) {
    boost::json::object datav = jv["styling"].as_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      StyleSheetP& stylesheet = StyleSheet::byGameAndName(*set->game, it->key_c_str());
      if (!stylesheet) continue;
      IndexMap<FieldP, ValueP>& stylesheet_data = set->stylingDataFor(*stylesheet);
      boost::json::object stylesheet_datav = it->value().as_object();
      for (auto stylesheet_it = stylesheet_datav.begin(); stylesheet_it != stylesheet_datav.end(); ++stylesheet_it) {
        String key_name = wxString(stylesheet_it->key_c_str());
        Value* container = get_container(stylesheet_data, wxString("styling"), key_name, false);
        ScriptValueP value = json_to_mse(stylesheet_it->value(), set.get());
        set_container(container, value, key_name);
      }
    }
  }
  // cards
  if (jv.contains("cards") && jv["cards"].is_array()) {
    boost::json::array cardsv = jv["cards"].as_array();
    for (int i = 0; i < cardsv.size(); i++) {
      boost::json::object cardv = cardsv[i].as_object();
      set->cards.emplace_back(json_to_mse_card(cardv, set.get()));
    }
  }
  // keywords
  if (jv.contains("keywords") && jv["keywords"].is_array()) {
    boost::json::array keywordsv = jv["keywords"].as_array();
    for (int i = 0; i < keywordsv.size(); i++) {
      boost::json::object keywordv = keywordsv[i].as_object();
      set->keywords.emplace_back(json_to_mse_keyword(keywordv));
    }
  }
  // pack types
  if (jv.contains("pack_types") && jv["pack_types"].is_array()) {
    boost::json::array pack_typesv = jv["pack_types"].as_array();
    for (int i = 0; i < pack_typesv.size(); i++) {
      boost::json::object pack_typev = pack_typesv[i].as_object();
      set->pack_types.emplace_back(json_to_mse_pack_type(pack_typev));
    }
  }
  return set;
}

inline static ScriptValueP json_to_mse(const boost::json::value& jv, Set* set) {
  if (jv == nullptr) return script_nil;
  else if (jv.is_null()) return script_nil;
  else if (jv.is_bool()) return to_script(jv.get_bool());
  else if (jv.is_double()) return to_script(jv.get_double());
  else if (jv.is_int64()) {
    int integer = jv.get_int64();
    return to_script(integer);
  }
  else if (jv.is_uint64()) {
    int integer = jv.get_uint64();
    return to_script(integer);
  }
  else if (jv.is_string()) {
    std::string stdstring = boost::json::value_to<std::string>(jv);
    String wxstring(stdstring.c_str(), wxConvUTF8);
    return to_script(wxstring);
  }
  else if (jv.is_array()) {
    boost::json::array array = jv.get_array();
    ScriptCustomCollectionP result = make_intrusive<ScriptCustomCollection>();
    for (int i = 0; i < array.size(); ++i) {
      boost::json::value jvalue = array[i];
      ScriptValueP value = json_to_mse(jvalue, set);
      result->value.push_back(value);
    }
    return result;
  }
  else if (jv.is_object()) {
    boost::json::object object = jv.get_object();
    if (object.contains("mse_object_type")) {
      boost::json::string mse_object_type = object["mse_object_type"].as_string();
      if (mse_object_type == "set")       return make_intrusive<ScriptObject<SetP>>     (json_to_mse_set(object));
      if (mse_object_type == "card")      return make_intrusive<ScriptObject<CardP>>    (json_to_mse_card(object, set));
      if (mse_object_type == "keyword")   return make_intrusive<ScriptObject<KeywordP>> (json_to_mse_keyword(object));
      if (mse_object_type == "pack_type") return make_intrusive<ScriptObject<PackTypeP>>(json_to_mse_pack_type(object));
      if (mse_object_type == "pack_item") return make_intrusive<ScriptObject<PackItemP>>(json_to_mse_pack_item(object));
      queue_message(MESSAGE_ERROR, _ERROR_("json unknown type") + _("(") + wxString(mse_object_type.c_str()) + _(")"));
      return script_nil;
    }
    ScriptCustomCollectionP result = make_intrusive<ScriptCustomCollection>();
    for (auto it = object.begin(); it != object.end(); ++it) {
      boost::json::string_view jview = it->key();
      std::string_view stdview = std::string_view(jview.data(), jview.size());
      std::string stdstring = { stdview.begin(), stdview.end() };
      String key(stdstring.c_str(), wxConvUTF8);
      boost::json::value jvalue = it->value();
      ScriptValueP value = json_to_mse(jvalue, set);
      result->key_value[key] = value;
    }
    return result;
  }
  else {
    queue_message(MESSAGE_ERROR, _ERROR_("json unknown type"));
    return script_nil;
  }
}
inline static ScriptValueP json_to_mse(const String& string, Set* set) {
  try {
    boost::system::error_code ec;
    boost::json::parse_options options;
    options.allow_invalid_utf8 = true;
    boost::json::value jv = boost::json::parse(string.ToStdString(), ec, {}, options);
    if(ec) queue_message(MESSAGE_ERROR, _ERROR_("json cant parse") + _("\n\n") + ec.message());
    return json_to_mse(jv, set);
  }
  catch (...) {
    queue_message(MESSAGE_ERROR, _ERROR_("json cant parse"));
    return script_nil;
  }
}
inline static ScriptValueP json_to_mse(const ScriptValueP& sv, Set* set) {
  try {
    String string = sv->toString();
    return json_to_mse(string, set);
  }
  catch (...) {
    queue_message(MESSAGE_ERROR, _ERROR_("json cant convert"));
    return script_nil;
  }
}

// ----------------------------------------------------------------------------- : MSE to JSON

template <typename T>
static void write(boost::json::object& out, const String& name, const T& value) {
  wxStringOutputStream stream;
  Writer writer(stream);
  writer.indentation = -1000;
  writer.handle(name, value);
  String string = stream.GetString();
  if (!string.empty()) {
    if (string.StartsWith(name + ":")) string = string.substr(name.length() + 1).Trim(false);
    if (string.EndsWith("\n")) string = string.substr(0, string.length() - 1);
    out.emplace(name.ToStdString(), string);
  }
}

static void write(boost::json::object& out, const String& name, IndexMap<FieldP, ValueP>& map) {
  boost::json::object indexmapv;
  for (IndexMap<FieldP, ValueP>::iterator it = map.begin(); it != map.end(); ++it) {
    write(indexmapv, (*it)->fieldP->name, *it);
  }
  if (!indexmapv.empty()) out.emplace(name.ToStdString(), indexmapv);
}

static void write(boost::json::object& out, const String& name, DelayedIndexMaps<FieldP,ValueP>& map) {
  boost::json::object delayedindexmapv;
  for (auto it = map.data.begin() ; it != map.data.end() ; ++it) {
    write(delayedindexmapv, it->first, it->second->read_data);
  }
  if (!delayedindexmapv.empty()) out.emplace(name.ToStdString(), delayedindexmapv);
}

inline static boost::json::object mse_to_json(PackItemP& item) {
  boost::json::object itemv;
  itemv.emplace("mse_object_type", "pack_item");
  write(itemv, "name",   item->name);
  write(itemv, "amount", item->amount);
  write(itemv, "weight", item->weight);
  return itemv;
}

inline static boost::json::object mse_to_json(PackTypeP& pack) {
  boost::json::object packv;
  packv.emplace("mse_object_type", "pack_type");
  write(packv, "name",       pack->name);
  write(packv, "enabled",    pack->enabled);
  write(packv, "selectable", pack->selectable);
  write(packv, "summary",    pack->summary);
  write(packv, "select",     pack->select);
  write(packv, "filter",     pack->filter);
  boost::json::array itemsv;
  for (auto item : pack->items) {
    itemsv.emplace_back(mse_to_json(item));
  }
  packv.emplace("items", itemsv);
  return packv;
}

inline static boost::json::object mse_to_json(KeywordP& keyword) {
  boost::json::object keywordv;
  keywordv.emplace("mse_object_type", "keyword");
  write(keywordv, "keyword",  keyword->keyword);
  write(keywordv, "match",    keyword->match);
  write(keywordv, "reminder", keyword->reminder);
  write(keywordv, "rules",    keyword->rules);
  write(keywordv, "mode",     keyword->mode);
  return keywordv;
}

inline static boost::json::object mse_to_json(CardP& card, Set* set) {
  boost::json::object cardv;
  cardv.emplace("mse_object_type", "card");
  // built-in values
  write(cardv, "time_created",      card->time_created);
  write(cardv, "time_modified",     card->time_modified);
  write(cardv, "notes",             card->notes);
  //write(cardv, "uid",               card->uid);
  //write(cardv, "linked_card_1",     card->linked_card_1);
  //write(cardv, "linked_card_2",     card->linked_card_2);
  //write(cardv, "linked_card_3",     card->linked_card_3);
  //write(cardv, "linked_card_4",     card->linked_card_4);
  //write(cardv, "linked_relation_1", card->linked_relation_1);
  //write(cardv, "linked_relation_2", card->linked_relation_2);
  //write(cardv, "linked_relation_3", card->linked_relation_3);
  //write(cardv, "linked_relation_4", card->linked_relation_4);
  // card fields
  write(cardv, "data",              card->data);
  // stylesheet
  bool change_stylesheet = set && !card->stylesheet;
  if (change_stylesheet) {
    card->stylesheet = set->stylesheet;
  }
  if (card->stylesheet) {
    write(cardv, "stylesheet",         card->stylesheet);
    write(cardv, "stylesheet_version", card->stylesheet->version);
    // extra card fields
    write(cardv, "extra_data",         card->extra_data);
  }
  // style
  write(cardv, "has_styling",       card->has_styling);
  if (card->has_styling) {
    write(cardv, "styling_data",       card->styling_data);
  }
  // restore stylesheet
  if (change_stylesheet) {
    card->stylesheet = StyleSheetP();
  }
  // done
  return cardv;
}

inline static boost::json::object mse_to_json(Set* set) {
  boost::json::object setv;
  setv.emplace("mse_object_type",    "set");
  // built-in values
  write(setv, "mse_version",        set->fileVersion());
  write(setv, "game",               set->game);
  write(setv, "game_version",       set->game->version);
  write(setv, "stylesheet",         set->stylesheet);
  write(setv, "stylesheet_version", set->stylesheet->version);
  // set fields
  write(setv, "set_info",           set->data);
  // styling
  write(setv, "styling",            set->styling_data);
  // cards
  boost::json::array cardsv;
  for (auto card : set->cards) {
    cardsv.emplace_back(mse_to_json(card, set));
  }
  setv.emplace("cards", cardsv);
  // keywords
  boost::json::array keywordsv;
  for (auto keyword : set->keywords) {
    keywordsv.emplace_back(mse_to_json(keyword));
  }
  if (!keywordsv.empty()) setv.emplace("keywords", keywordsv);
  // pack types
  boost::json::array pack_typesv;
  for (auto pack_type : set->pack_types) {
    pack_typesv.emplace_back(mse_to_json(pack_type));
  }
  if (!pack_typesv.empty()) setv.emplace("pack_types", pack_typesv);
  // done
  return setv;
}

inline static boost::json::value mse_to_json(ScriptValueP& sv, Set* set) {
  ScriptType type = sv->type();
  // special types
  if (ScriptObject<PackItemP>* i = dynamic_cast<ScriptObject<PackItemP>*>(sv.get())) return mse_to_json(i->getValue());
  if (ScriptObject<PackTypeP>* t = dynamic_cast<ScriptObject<PackTypeP>*>(sv.get())) return mse_to_json(t->getValue());
  if (ScriptObject<KeywordP>*  k = dynamic_cast<ScriptObject<KeywordP>*> (sv.get())) return mse_to_json(k->getValue());
  if (ScriptObject<CardP>*     c = dynamic_cast<ScriptObject<CardP>*>    (sv.get())) return mse_to_json(c->getValue(), set);
  if (ScriptObject<SetP>*      z = dynamic_cast<ScriptObject<SetP>*>     (sv.get())) return mse_to_json(z->getValue().get());
  if (ScriptObject<Set*>*      s = dynamic_cast<ScriptObject<Set*>*>     (sv.get())) return mse_to_json(s->getValue());

  // primitive types
  if (type == SCRIPT_NIL)      return boost::json::value(nullptr);
  if (type == SCRIPT_INT)      return boost::json::value(sv->toInt());
  if (type == SCRIPT_DOUBLE)   return boost::json::value(sv->toDouble());
  if (type == SCRIPT_BOOL)     return boost::json::value(sv->toBool());
  if (type == SCRIPT_STRING)   return boost::json::value(sv->toString());
  if (type == SCRIPT_REGEX)    return boost::json::value(sv->toString());
  if (type == SCRIPT_COLOR)    return boost::json::value(format_color(sv->toColor()));
  if (type == SCRIPT_DATETIME) return boost::json::value(sv->toDateTime().FormatISOCombined(' '));
  if (type == SCRIPT_COLLECTION) {
    ScriptCustomCollection* custom = dynamic_cast<ScriptCustomCollection*>(sv.get());
    if (custom) {
      if (custom->value.size() > 0) {
        boost::json::array array;
        for (int i = 0; i < custom->value.size(); i++) {
          array.emplace_back(mse_to_json(custom->value[i], set));
        }
        return array;
      } else if (custom->key_value.size() > 0) {
        boost::json::object object;
        map<String, ScriptValueP>::iterator it;
        for (it = custom->key_value.begin(); it != custom->key_value.end(); it++) {
          object.emplace(it->first.ToStdString(), mse_to_json(it->second, set));
        }
        return object;
      }
    } else {
      ScriptConcatCollection* concat = dynamic_cast<ScriptConcatCollection*>(sv.get());
      if (concat) {
        boost::json::value a = mse_to_json(concat->getA(), set);
        boost::json::value b = mse_to_json(concat->getB(), set);
        if (a.is_array() && b.is_array()) {
          boost::json::array array_a = a.get_array();
          boost::json::array array_b = b.get_array();
          for (int i = 0; i < array_b.size(); i++) {
            array_a.emplace_back(array_b[i]);
          }
          return array_a;
        } else if (a.is_object() && b.is_object()) {
          boost::json::object object_a = a.get_object();
          boost::json::object object_b = b.get_object();
          for (auto it = object_b.begin(); it != object_b.end(); ++it) {
            object_a.emplace(it->key(), it->value());
          }
          return object_a;
        } else {
          queue_message(MESSAGE_ERROR, _ERROR_("json cant concat"));
          return boost::json::value(nullptr);
        }
      }
    }
  }
  queue_message(MESSAGE_ERROR, _ERROR_1_("json unknown script type", sv->typeName()));
  return boost::json::value(nullptr);
}
