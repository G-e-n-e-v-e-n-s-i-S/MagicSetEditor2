//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+


// ----------------------------------------------------------------------------- : Includes

#include <script/functions/json.hpp>

#include <util/delayed_index_maps.hpp>
#include <util/prec.hpp>
#include <data/card.hpp>
#include <data/card_link.hpp>
#include <data/word_list.hpp>
#include <data/statistics.hpp>
#include <data/field/information.hpp>
#include <data/field/boolean.hpp>
#include <data/field/multiple_choice.hpp>
#include <data/format/clipboard.hpp>
#include <data/update_cards_script.hpp>
#include <data/add_cards_script.hpp>
#include <render/symbol/filter.hpp>
#include <script/functions/construction_helper.hpp>
#include <sstream>
#include <wx/sstream.h>


// All this isn't great, but it will do for now. Idealy you would create JsonWriter and JsonReader classes
// that inherit from Writer and Reader, and just have a switch to go from normal mode to JSON mode...


// ----------------------------------------------------------------------------- : JSON to String

void pretty_print(std::ostringstream& os, const boost::json::value& jv, std::string* indent)
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
    os << jv;
    break;

  case boost::json::kind::double_: {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(10) << jv.as_double();
    std::string str = oss.str();
    // remove trailing zeros
    if (str.find('.') != std::string::npos) {
      str.erase(str.find_last_not_of('0') + 1);
      if (str.back() == '.') {
        str.pop_back();
      }
    }
    os << str;
    break;
  }

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

String json_pretty_print(const boost::json::value& jv, std::string* indent) {
  std::ostringstream stream;
  pretty_print(stream, jv, indent);
  std::string stdstring = stream.str();
  const char* data = stdstring.data();
  size_t size = stdstring.size();
  String wxstring = String::FromUTF8(data, size);
  if (wxstring.empty() && size > 0) {
    wxstring = String(data, wxConvWhateverWorks, size);
  }
  return wxstring;
}

String json_ugly_print(const boost::json::value& jv) {
  std::string stdstring = boost::json::serialize(jv);
  const char* data = stdstring.data();
  size_t size = stdstring.size();
  String wxstring = String::FromUTF8(data, size);
  if (wxstring.empty() && size > 0) {
    wxstring = String(data, wxConvWhateverWorks, size);
  }
  return wxstring;
}

// ----------------------------------------------------------------------------- : JSON to MSE

ScriptValueP json_to_mse(const boost::json::value& jv, Set* set);

template <typename T>
void read(T& out, boost::json::object& jv, const char value_name[]) {
  if (!jv.contains(value_name)) return;
  boost::json::string jstring = jv[value_name].as_string();
  const char* data = jstring.data();
  size_t size = jstring.size();
  String wxstring = String::FromUTF8(data, size);
  if (wxstring.empty() && size > 0) {
    wxstring = String(data, wxConvWhateverWorks, size);
  }
  wxStringInputStream stream(wxstring);
  Reader reader(stream, nullptr, _(""), false, true);
  reader.setValue(wxstring);
  reader.handle(out);
}

// templates don't work with enums? are you kidding me with this language?
void read(PackSelectType& out, boost::json::object& jv, const char value_name[]) {
  if (!jv.contains(value_name)) return;
  boost::json::string jstring = jv[value_name].as_string();
  const char* data = jstring.data();
  size_t size = jstring.size();
  String wxstring = String::FromUTF8(data, size);
  if (wxstring.empty() && size > 0) {
    wxstring = String(data, wxConvWhateverWorks, size);
  }
  wxStringInputStream stream(wxstring);
  Reader reader(stream, nullptr, _(""), false, true);
  reader.setValue(wxstring);
  reader.handle(out);
}

PackItemP json_to_mse_pack_item(boost::json::object& jv) {
  PackItemP pack_item = make_intrusive<PackItem>();
  read(pack_item->name,   jv, "name");
  read(pack_item->amount, jv, "amount");
  read(pack_item->weight, jv, "weight");
  return pack_item;
}

PackTypeP json_to_mse_pack_type(boost::json::object& jv) {
  PackTypeP pack_type = make_intrusive<PackType>();
  read(pack_type->name,       jv, "name");
  read(pack_type->enabled,    jv, "enabled");
  read(pack_type->selectable, jv, "selectable");
  read(pack_type->summary,    jv, "summary");
  read(pack_type->select,     jv, "select");
  if (jv.contains("items") && jv["items"].is_array()) {
    boost::json::array pack_itemsv = jv["items"].get_array();
    for (size_t i = 0; i < pack_itemsv.size(); i++) {
      boost::json::object pack_itemv = pack_itemsv[i].as_object();
      pack_type->items.emplace_back(json_to_mse_pack_item(pack_itemv));
    }
  }
  return pack_type;
}

KeywordP json_to_mse_keyword(boost::json::object& jv) {
  KeywordP keyword = make_intrusive<Keyword>();
  read(keyword->keyword,  jv, "keyword");
  read(keyword->match,    jv, "match");
  read(keyword->reminder, jv, "reminder");
  read(keyword->rules,    jv, "rules");
  read(keyword->mode,     jv, "mode");
  return keyword;
}

CardP json_to_mse_card(boost::json::object& jv, Set* set) {
  CardP card = make_intrusive<Card>(*set->game);
  read(card->time_created,      jv, "time_created");
  read(card->time_modified,     jv, "time_modified");
  read(card->notes,             jv, "notes");
  read(card->uid,               jv, "uid");
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    std::string card_key     = "linked_card_"     + std::to_string(i + 1);
    std::string relation_key = "linked_relation_" + std::to_string(i + 1);
    read(card->getLinkedUID(i),      jv, card_key.c_str());
    read(card->getLinkedRelation(i), jv, relation_key.c_str());
  }
  // card fields
  if (jv.contains("data") && jv["data"].is_object()) {
    boost::json::object datav = jv["data"].get_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      boost::json::string_view key_view = it->key();
      String key_name = String::FromUTF8(key_view.data(), key_view.size());
      Value* container = get_card_field_container(*set->game, card->data, key_name, false);
      ScriptValueP value = json_to_mse(it->value(), set);
      set_container(set, container, _("card"), value, key_name);
    }
  }
  // stylesheet
  if (jv.contains("stylesheet")) {
    boost::json::string stylesheet_name = jv["stylesheet"].as_string();
    card->stylesheet = StyleSheet::byGameAndName(*set->game, String::FromUTF8(stylesheet_name.data(), stylesheet_name.size()));
  }
  if (card->stylesheet) {
    // styling fields
    card->styling_data.init(card->stylesheet->styling_fields);
    if (jv.contains("styling_data") && jv["styling_data"].is_object()) {
      boost::json::object datav = jv["styling_data"].get_object();
      for (auto it = datav.begin(); it != datav.end(); ++it) {
        boost::json::string_view key_view = it->key();
        String key_name = String::FromUTF8(key_view.data(), key_view.size());
        Value* container = get_container(card->styling_data, String("styling"), key_name, false);
        ScriptValueP value = json_to_mse(it->value(), set);
        set_container(set, container, _("styling"), value, key_name);
        card->has_styling = true;
      }
    }
    // extra card fields
    if (jv.contains("extra_data") && jv["extra_data"].is_object()) {
      boost::json::object datav = jv["extra_data"].get_object();
      for (auto it = datav.begin(); it != datav.end(); ++it) {
        boost::json::string_view stylesheet_view = it->key();
        StyleSheetP stylesheet = StyleSheet::byGameAndName(*set->game, String::FromUTF8(stylesheet_view.data(), stylesheet_view.size()));
        if (!stylesheet) continue;
        IndexMap<FieldP, ValueP>& stylesheet_data = card->extraDataFor(*stylesheet);
        stylesheet_data.init(stylesheet->extra_card_fields);
        boost::json::object stylesheet_datav = it->value().as_object();
        for (auto stylesheet_it = stylesheet_datav.begin(); stylesheet_it != stylesheet_datav.end(); ++stylesheet_it) {
          boost::json::string_view key_view = stylesheet_it->key();
          String key_name = String::FromUTF8(key_view.data(), key_view.size());
          Value* container = get_container(stylesheet_data, String("extra card"), key_name, false);
          ScriptValueP value = json_to_mse(stylesheet_it->value(), set);
          set_container(set, container, _("extra"), value, key_name);
        }
      }
    }
  }
  return card;
}

SetP json_to_mse_set(boost::json::object& jv) {
  if (!jv.contains("game")) {
    throw ScriptError(_ERROR_("json set without game"));
  }
  if (!jv.contains("stylesheet")) {
    throw ScriptError(_ERROR_("json set without stylesheet"));
  }
  boost::json::string game_name = jv["game"].as_string();
  GameP game = Game::byName(String::FromUTF8(game_name.data(), game_name.size()));
  boost::json::string stylesheet_name = jv["stylesheet"].as_string();
  StyleSheetP stylesheet = StyleSheet::byGameAndName(*game, String::FromUTF8(stylesheet_name.data(), stylesheet_name.size()));
  SetP set = make_intrusive<Set>(stylesheet);
  // set fields
  if (jv.contains("set_info") && jv["set_info"].is_object()) {
    boost::json::object datav = jv["set_info"].get_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      boost::json::string_view key_view = it->key();
      String key_name = String::FromUTF8(key_view.data(), key_view.size());
      Value* container = get_container(set->data, String("set"), key_name, false);
      ScriptValueP value = json_to_mse(it->value(), set.get());
      set_container(set.get(), container, _("set"), value, key_name);
    }
  }
  // styling
  if (jv.contains("styling") && jv["styling"].is_object()) {
    boost::json::object datav = jv["styling"].get_object();
    for (auto it = datav.begin(); it != datav.end(); ++it) {
      boost::json::string_view stylesheet_view = it->key();
      StyleSheetP stylesheet = StyleSheet::byGameAndName(*set->game, String::FromUTF8(stylesheet_view.data(), stylesheet_view.size()));
      if (!stylesheet) continue;
      IndexMap<FieldP, ValueP>& stylesheet_data = set->stylingDataFor(*stylesheet);
      boost::json::object stylesheet_datav = it->value().as_object();
      for (auto stylesheet_it = stylesheet_datav.begin(); stylesheet_it != stylesheet_datav.end(); ++stylesheet_it) {
        boost::json::string_view key_view = stylesheet_it->key();
        String key_name = String::FromUTF8(key_view.data(), key_view.size());
        Value* container = get_container(stylesheet_data, String("styling"), key_name, false);
        ScriptValueP value = json_to_mse(stylesheet_it->value(), set.get());
        set_container(set.get(), container, _("styling"), value, key_name);
      }
    }
  }
  // cards
  if (jv.contains("cards") && jv["cards"].is_array()) {
    boost::json::array cardsv = jv["cards"].get_array();
    for (size_t i = 0; i < cardsv.size(); i++) {
      boost::json::object cardv = cardsv[i].as_object();
      set->cards.emplace_back(json_to_mse_card(cardv, set.get()));
    }
  }
  // keywords
  if (jv.contains("keywords") && jv["keywords"].is_array()) {
    boost::json::array keywordsv = jv["keywords"].get_array();
    for (size_t i = 0; i < keywordsv.size(); i++) {
      boost::json::object keywordv = keywordsv[i].as_object();
      set->keywords.emplace_back(json_to_mse_keyword(keywordv));
    }
  }
  // pack types
  if (jv.contains("pack_types") && jv["pack_types"].is_array()) {
    boost::json::array pack_typesv = jv["pack_types"].get_array();
    for (size_t i = 0; i < pack_typesv.size(); i++) {
      boost::json::object pack_typev = pack_typesv[i].as_object();
      set->pack_types.emplace_back(json_to_mse_pack_type(pack_typev));
    }
  }
  return set;
}

ScriptValueP json_to_mse(const boost::json::value& jv, Set* set) {
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
    boost::json::string jstring = jv.get_string();
    if (jstring.empty()) return to_script(String());
    const char* data = jstring.data();
    size_t size = jstring.size();
    String wxstring = String::FromUTF8(data, size);
    if (!wxstring.empty()) return to_script(wxstring);
    return to_script(String(data, wxConvWhateverWorks, size));
  }
  else if (jv.is_array()) {
    boost::json::array array = jv.get_array();
    ScriptCustomCollectionP result = make_intrusive<ScriptCustomCollection>();
    for (size_t i = 0; i < array.size(); ++i) {
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
      queue_message(MESSAGE_ERROR, _ERROR_("json unknown type") + _("(") + String(mse_object_type.c_str()) + _(")"));
      return script_nil;
    }
    ScriptCustomCollectionP result = make_intrusive<ScriptCustomCollection>();
    for (auto it = object.begin(); it != object.end(); ++it) {
      boost::json::string_view jview = it->key();
      String key_name = String::FromUTF8(jview.data(), jview.size());
      boost::json::value jvalue = it->value();
      ScriptValueP value = json_to_mse(jvalue, set);
      result->key_value[key_name] = value;
    }
    return result;
  }
  else {
    queue_message(MESSAGE_ERROR, _ERROR_("json unknown type"));
    return script_nil;
  }
}
ScriptValueP json_to_mse(const String& string, Set* set) {
  try {
    boost::system::error_code ec;
    boost::json::parse_options options;
    options.allow_invalid_utf8 = true;
    wxScopedCharBuffer buffer = string.ToUTF8();
    boost::json::value jv = boost::json::parse(boost::json::string_view(buffer.data(), buffer.length()), ec, {}, options);
    //if(ec && buffer.length() > 0) queue_message(MESSAGE_ERROR, _ERROR_("json cant parse") + _("\n\n") + ec.message());
    if(ec) return script_nil;
    return json_to_mse(jv, set);
  }
  catch (...) {
    queue_message(MESSAGE_ERROR, _ERROR_("json cant parse"));
    return script_nil;
  }
}
ScriptValueP json_to_mse(const ScriptValueP& sv, Set* set) {
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

inline boost::json::string to_json_string(const String& s) {
  wxScopedCharBuffer buffer = s.ToUTF8();
  return boost::json::string(buffer.data(), buffer.length());
}

template <typename T>
void write(boost::json::object& out, const String& name, const T& value) {
  wxStringOutputStream stream;
  Writer writer(stream);
  writer.indentation = -1000;
  writer.handle(name, value);
  String string = stream.GetString();
  if (!string.empty()) {
    if (string.StartsWith(name + ":")) string = string.substr(name.length() + 1).Trim(false);
    if (string.EndsWith("\n")) string = string.substr(0, string.length() - 1);
    out.emplace(name.ToStdString(), to_json_string(string));
  }
}

void write(boost::json::object& out, const String& name, IndexMap<FieldP, ValueP>& map) {
  boost::json::object indexmapv;
  for (IndexMap<FieldP, ValueP>::iterator it = map.begin(); it != map.end(); ++it) {
    write(indexmapv, (*it)->fieldP->name, *it);
  }
  if (!indexmapv.empty()) out.emplace(name.ToStdString(), indexmapv);
}

void write(boost::json::object& out, const String& name, DelayedIndexMaps<FieldP,ValueP>& map) {
  boost::json::object delayedindexmapv;
  for (auto it = map.data.begin() ; it != map.data.end() ; ++it) {
    write(delayedindexmapv, it->first, it->second->read_data);
  }
  if (!delayedindexmapv.empty()) out.emplace(name.ToStdString(), delayedindexmapv);
}

boost::json::object mse_to_json(const PackItemP& item) {
  boost::json::object itemv;
  itemv.emplace("mse_object_type", "pack_item");
  write(itemv, "name",   item->name);
  write(itemv, "amount", item->amount);
  write(itemv, "weight", item->weight);
  return itemv;
}

boost::json::object mse_to_json(const PackTypeP& pack) {
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

boost::json::object mse_to_json(const KeywordP& keyword) {
  boost::json::object keywordv;
  keywordv.emplace("mse_object_type", "keyword");
  write(keywordv, "keyword",  keyword->keyword);
  write(keywordv, "match",    keyword->match);
  write(keywordv, "reminder", keyword->reminder);
  write(keywordv, "rules",    keyword->rules);
  write(keywordv, "mode",     keyword->mode);
  return keywordv;
}

boost::json::object mse_to_json(const CardP& card, const Set* set) {
  boost::json::object cardv;
  cardv.emplace("mse_object_type", "card");
  // built-in values
  write(cardv, "time_created",      card->time_created);
  write(cardv, "time_modified",     card->time_modified);
  write(cardv, "notes",             card->notes);
  write(cardv, "uid",               card->uid);
  for (int i = 0; i < Card::MAX_LINKS; ++i) {
    const String& uid = card->getLinkedUID(i);
    if (uid.empty()) continue;
    write(cardv, String::Format(_("linked_card_%d"), i + 1),     uid);
    write(cardv, String::Format(_("linked_relation_%d"), i + 1), card->getLinkedRelation(i));
  }
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

boost::json::object mse_to_json(const StyleP& style) {
  //Context ctx = set->getContext();
  //style->update(ctx);

  boost::json::object stylev;
  stylev.emplace("mse_object_type", "style");
  stylev.emplace("z_index",   String::Format(wxT("%i"),   style->z_index));
  stylev.emplace("tab_index", String::Format(wxT("%i"),   style->tab_index));
  stylev.emplace("left",      String::Format(wxT("%.2f"), style->left()));
  stylev.emplace("top",       String::Format(wxT("%.2f"), style->top()));
  stylev.emplace("right",     String::Format(wxT("%.2f"), style->right()));
  stylev.emplace("bottom",    String::Format(wxT("%.2f"), style->bottom()));
  stylev.emplace("width",     String::Format(wxT("%.2f"), style->width()));
  stylev.emplace("height",    String::Format(wxT("%.2f"), style->height()));
  stylev.emplace("angle",     String::Format(wxT("%.2f"), style->angle()));
  stylev.emplace("visible",                               style->visible());
  stylev.emplace("mask",                                  to_json_string(style->mask.toScriptString()));

  if (TextStyle* s = dynamic_cast<TextStyle*>(style.get())) {
    stylev.emplace("field_type", "text");

    boost::json::object fontv;
    fontv.emplace("name",                                              to_json_string(s->font.name()));
    fontv.emplace("italic_name",                                       to_json_string(s->font.italic_name()));
    fontv.emplace("size",                  String::Format(wxT("%.2f"), s->font.size()));
    fontv.emplace("weight",                                            s->font.weight());
    fontv.emplace("style",                                             s->font.style());
    fontv.emplace("underline",                                         s->font.underline());
    fontv.emplace("strikethrough",                                     s->font.strikethrough());
    fontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->font.scale_down_to));
    fontv.emplace("max_stretch",           String::Format(wxT("%.2f"), s->font.max_stretch));
    fontv.emplace("color",                 format_color(               s->font.color()));
    fontv.emplace("shadow_color",          format_color(               s->font.shadow_color()));
    fontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->font.shadow_displacement_x()));
    fontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->font.shadow_displacement_y()));
    fontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->font.shadow_blur()));
    fontv.emplace("stroke_color",          format_color(               s->font.stroke_color()));
    fontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->font.stroke_radius()));
    fontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->font.stroke_blur()));
    fontv.emplace("separator_color",       format_color(               s->font.separator_color));
    fontv.emplace("flags",                 String::Format(wxT("%i"),   s->font.flags));
    stylev.emplace("font",                                             fontv);

    boost::json::object symbolfontv;
    symbolfontv.emplace("name",                                              to_json_string(s->symbol_font.name()));
    symbolfontv.emplace("size",                  String::Format(wxT("%.2f"), s->symbol_font.size()));
    symbolfontv.emplace("underline",                                         s->symbol_font.underline());
    symbolfontv.emplace("strikethrough",                                     s->symbol_font.strikethrough());
    symbolfontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->symbol_font.scale_down_to));
    symbolfontv.emplace("shadow_color",          format_color(               s->symbol_font.shadow_color()));
    symbolfontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->symbol_font.shadow_displacement_x()));
    symbolfontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->symbol_font.shadow_displacement_y()));
    symbolfontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->symbol_font.shadow_blur()));
    symbolfontv.emplace("stroke_color",          format_color(               s->symbol_font.stroke_color()));
    symbolfontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->symbol_font.stroke_radius()));
    symbolfontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->symbol_font.stroke_blur()));
    stylev.emplace("symbol_font",                                      symbolfontv);

    stylev.emplace("always_symbol",                                    s->always_symbol);
    stylev.emplace("allow_formating",                                  s->allow_formating);
    stylev.emplace("alignment",            alignment_to_string(        s->alignment()));
    stylev.emplace("direction",            direction_to_string(        s->direction()));
    stylev.emplace("padding_left",         String::Format(wxT("%.2f"), s->padding_left()));
    stylev.emplace("padding_right",        String::Format(wxT("%.2f"), s->padding_right()));
    stylev.emplace("padding_top",          String::Format(wxT("%.2f"), s->padding_top()));
    stylev.emplace("padding_bottom",       String::Format(wxT("%.2f"), s->padding_bottom()));
    stylev.emplace("padding_left_min",     String::Format(wxT("%.2f"), s->padding_left_min()));
    stylev.emplace("padding_right_min",    String::Format(wxT("%.2f"), s->padding_right_min()));
    stylev.emplace("padding_top_min",      String::Format(wxT("%.2f"), s->padding_top_min()));
    stylev.emplace("padding_bottom_min",   String::Format(wxT("%.2f"), s->padding_bottom_min()));
    stylev.emplace("line_height_soft",     String::Format(wxT("%.2f"), s->line_height_soft()));
    stylev.emplace("line_height_hard",     String::Format(wxT("%.2f"), s->line_height_hard()));
    stylev.emplace("line_height_line",     String::Format(wxT("%.2f"), s->line_height_line()));
    stylev.emplace("line_height_soft_max", String::Format(wxT("%.2f"), s->line_height_soft_max()));
    stylev.emplace("line_height_hard_max", String::Format(wxT("%.2f"), s->line_height_hard_max()));
    stylev.emplace("line_height_line_max", String::Format(wxT("%.2f"), s->line_height_line_max()));
    stylev.emplace("paragraph_height",     String::Format(wxT("%.2f"), s->paragraph_height()));

    boost::json::object layoutv;
    layoutv.emplace("content_top",         String::Format(wxT("%.2f"), s->layout->top));
    layoutv.emplace("content_middle",      String::Format(wxT("%.2f"), s->layout->middle()));
    layoutv.emplace("content_bottom",      String::Format(wxT("%.2f"), s->layout->bottom()));
    layoutv.emplace("content_width",       String::Format(wxT("%.2f"), s->layout->width));
    layoutv.emplace("content_height",      String::Format(wxT("%.2f"), s->layout->height));
    layoutv.emplace("content_lines",       String::Format(wxT("%i"),   (int)s->layout->lines.size()));
    layoutv.emplace("content_clauses",     String::Format(wxT("%i"),   (int)s->layout->clauses.size()));
    layoutv.emplace("content_paragraphs",  String::Format(wxT("%i"),   (int)s->layout->paragraphs.size()));
    layoutv.emplace("content_blocks",      String::Format(wxT("%i"),   (int)s->layout->blocks.size()));
    boost::json::array separatorsv;
    int size = s->layout->separators.size();
    for (int i = 0; i < size; i++) {
      separatorsv.emplace_back(String::Format(wxT("%.2f"), s->layout->separators[i]));
    }
    if (size > 0) layoutv.emplace("content_separators", separatorsv);

    stylev.emplace("layout", layoutv);
  }

  else if (ImageStyle* s = dynamic_cast<ImageStyle*>(style.get())) {
    stylev.emplace("field_type", "image");
    stylev.emplace("default",           to_json_string(s->default_image.toScriptString()));
    stylev.emplace("store_in_metadata", s->store_in_metadata());
  }

  else if (MultipleChoiceStyle* s = dynamic_cast<MultipleChoiceStyle*>(style.get())) {
    stylev.emplace("field_type", "multiple_choice");
    stylev.emplace("popup_style",          popup_style_to_string(      s->popup_style));
    stylev.emplace("render_style",         render_style_to_string(     s->render_style));
    stylev.emplace("image",                                            to_json_string(s->image.toScriptString()));
    stylev.emplace("combine",              combine_to_string(          s->combine));
    stylev.emplace("alignment",            alignment_to_string(        s->alignment));
    stylev.emplace("direction",            direction_to_string(        s->direction()));
    stylev.emplace("spacing",              String::Format(wxT("%.2f"), s->spacing()));

    boost::json::object fontv;
    fontv.emplace("name",                                              to_json_string(s->font.name()));
    fontv.emplace("italic_name",                                       to_json_string(s->font.italic_name()));
    fontv.emplace("size",                  String::Format(wxT("%.2f"), s->font.size()));
    fontv.emplace("weight",                                            s->font.weight());
    fontv.emplace("style",                                             s->font.style());
    fontv.emplace("underline",                                         s->font.underline());
    fontv.emplace("strikethrough",                                     s->font.strikethrough());
    fontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->font.scale_down_to));
    fontv.emplace("max_stretch",           String::Format(wxT("%.2f"), s->font.max_stretch));
    fontv.emplace("color",                 format_color(               s->font.color()));
    fontv.emplace("shadow_color",          format_color(               s->font.shadow_color()));
    fontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->font.shadow_displacement_x()));
    fontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->font.shadow_displacement_y()));
    fontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->font.shadow_blur()));
    fontv.emplace("stroke_color",          format_color(               s->font.stroke_color()));
    fontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->font.stroke_radius()));
    fontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->font.stroke_blur()));
    fontv.emplace("separator_color",       format_color(               s->font.separator_color));
    fontv.emplace("flags",                 String::Format(wxT("%i"),   s->font.flags));
    stylev.emplace("font",                                             fontv);

    boost::json::object choiceimagesv;
    for (auto choice_image : s->choice_images) {
      String image = choice_image.second.toScriptString();
      if (!image.empty()) choiceimagesv.emplace(choice_image.first.ToStdString(), to_json_string(image));
    }
    if (choiceimagesv.size() > 0) stylev.emplace("choice_images", choiceimagesv);
  }

  else if (ChoiceStyle* s = dynamic_cast<ChoiceStyle*>(style.get())) {
    stylev.emplace("field_type", dynamic_cast<BooleanStyle*>(style.get()) ? "boolean" : "choice");
    stylev.emplace("popup_style",          popup_style_to_string(      s->popup_style));
    stylev.emplace("render_style",         render_style_to_string(     s->render_style));
    stylev.emplace("image",                                            to_json_string(s->image.toScriptString()));
    stylev.emplace("combine",              combine_to_string(          s->combine));
    stylev.emplace("alignment",            alignment_to_string(        s->alignment));

    boost::json::object fontv;
    fontv.emplace("name",                                              to_json_string(s->font.name()));
    fontv.emplace("italic_name",                                       to_json_string(s->font.italic_name()));
    fontv.emplace("size",                  String::Format(wxT("%.2f"), s->font.size()));
    fontv.emplace("weight",                                            s->font.weight());
    fontv.emplace("style",                                             s->font.style());
    fontv.emplace("underline",                                         s->font.underline());
    fontv.emplace("strikethrough",                                     s->font.strikethrough());
    fontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->font.scale_down_to));
    fontv.emplace("max_stretch",           String::Format(wxT("%.2f"), s->font.max_stretch));
    fontv.emplace("color",                 format_color(               s->font.color()));
    fontv.emplace("shadow_color",          format_color(               s->font.shadow_color()));
    fontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->font.shadow_displacement_x()));
    fontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->font.shadow_displacement_y()));
    fontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->font.shadow_blur()));
    fontv.emplace("stroke_color",          format_color(               s->font.stroke_color()));
    fontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->font.stroke_radius()));
    fontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->font.stroke_blur()));
    fontv.emplace("separator_color",       format_color(               s->font.separator_color));
    fontv.emplace("flags",                 String::Format(wxT("%i"),   s->font.flags));
    stylev.emplace("font",                                             fontv);

    boost::json::object choiceimagesv;
    for (auto choice_image : s->choice_images) {
      String image = choice_image.second.toScriptString();
      if (!image.empty()) choiceimagesv.emplace(choice_image.first.ToStdString(), to_json_string(image));
    }
    if (choiceimagesv.size() > 0) stylev.emplace("choice_images", choiceimagesv);
  }

  else if (PackageChoiceStyle* s = dynamic_cast<PackageChoiceStyle*>(style.get())) {
    stylev.emplace("field_type", "package_choice");

    boost::json::object fontv;
    fontv.emplace("name",                                              to_json_string(s->font.name()));
    fontv.emplace("italic_name",                                       to_json_string(s->font.italic_name()));
    fontv.emplace("size",                  String::Format(wxT("%.2f"), s->font.size()));
    fontv.emplace("weight",                                            s->font.weight());
    fontv.emplace("style",                                             s->font.style());
    fontv.emplace("underline",                                         s->font.underline());
    fontv.emplace("strikethrough",                                     s->font.strikethrough());
    fontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->font.scale_down_to));
    fontv.emplace("max_stretch",           String::Format(wxT("%.2f"), s->font.max_stretch));
    fontv.emplace("color",                 format_color(               s->font.color()));
    fontv.emplace("shadow_color",          format_color(               s->font.shadow_color()));
    fontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->font.shadow_displacement_x()));
    fontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->font.shadow_displacement_y()));
    fontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->font.shadow_blur()));
    fontv.emplace("stroke_color",          format_color(               s->font.stroke_color()));
    fontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->font.stroke_radius()));
    fontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->font.stroke_blur()));
    fontv.emplace("separator_color",       format_color(               s->font.separator_color));
    fontv.emplace("flags",                 String::Format(wxT("%i"),   s->font.flags));
    stylev.emplace("font",                                             fontv);
  }

  else if (ColorStyle* s = dynamic_cast<ColorStyle*>(style.get())) {
    stylev.emplace("field_type", "color");
    stylev.emplace("radius",               String::Format(wxT("%.2f"), s->radius()));
    stylev.emplace("left_width",           String::Format(wxT("%.2f"), s->left_width()));
    stylev.emplace("right_width",          String::Format(wxT("%.2f"), s->right_width()));
    stylev.emplace("top_width",            String::Format(wxT("%.2f"), s->top_width()));
    stylev.emplace("bottom_width",         String::Format(wxT("%.2f"), s->bottom_width()));
    stylev.emplace("combine",              combine_to_string(          s->combine));
  }

  else if (SymbolStyle* s = dynamic_cast<SymbolStyle*>(style.get())) {
    stylev.emplace("field_type", "symbol");
    stylev.emplace("min_aspect_ratio", String::Format(wxT("%.2f"), s->min_aspect_ratio));
    stylev.emplace("max_aspect_ratio", String::Format(wxT("%.2f"), s->max_aspect_ratio));
    boost::json::array variationsv;
    int size = s->variations.size();
    for (int i = 0; i < size; i++) {
      boost::json::object variationv;
      variationv.emplace("name",                                         to_json_string(s->variations[i]->name));
      variationv.emplace("border_radius",    String::Format(wxT("%.2f"), s->variations[i]->border_radius));
      SymbolFilterP filter = s->variations[i]->filter;
      if (SolidFillSymbolFilter* f = dynamic_cast<SolidFillSymbolFilter*>(filter.get())) {
        variationv.emplace("fill_type",                                  f->fillType());
        variationv.emplace("fill_color",     format_color(               f->fill_color));
        variationv.emplace("border_color",   format_color(               f->border_color));
      }
      else if (RadialGradientSymbolFilter* f = dynamic_cast<RadialGradientSymbolFilter*>(filter.get())) {
        variationv.emplace("fill_type",                                  f->fillType());
        variationv.emplace("fill_color_1",   format_color(               f->fill_color_1));
        variationv.emplace("fill_color_2",   format_color(               f->fill_color_2));
        variationv.emplace("border_color_1", format_color(               f->border_color_1));
        variationv.emplace("border_color_2", format_color(               f->border_color_2));
      }
      else if (LinearGradientSymbolFilter* f = dynamic_cast<LinearGradientSymbolFilter*>(filter.get())) {
        variationv.emplace("fill_type",                                  f->fillType());
        variationv.emplace("fill_color_1",   format_color(               f->fill_color_1));
        variationv.emplace("fill_color_2",   format_color(               f->fill_color_2));
        variationv.emplace("border_color_1", format_color(               f->border_color_1));
        variationv.emplace("border_color_2", format_color(               f->border_color_2));
        variationv.emplace("center_x",       String::Format(wxT("%.2f"), f->center_x));
        variationv.emplace("center_y",       String::Format(wxT("%.2f"), f->center_y));
        variationv.emplace("end_x",          String::Format(wxT("%.2f"), f->end_x));
        variationv.emplace("end_y",          String::Format(wxT("%.2f"), f->end_y));
      }
      variationsv.emplace_back(variationv);
    }
    if (size > 0) stylev.emplace("variations", variationsv);
  }

  else if (InfoStyle* s = dynamic_cast<InfoStyle*>(style.get())) {
    stylev.emplace("field_type", "info");
    stylev.emplace("alignment",            alignment_to_string(        s->alignment));
    stylev.emplace("padding_left",         String::Format(wxT("%.2f"), s->padding_left));
    stylev.emplace("padding_right",        String::Format(wxT("%.2f"), s->padding_right));
    stylev.emplace("padding_top",          String::Format(wxT("%.2f"), s->padding_top));
    stylev.emplace("padding_bottom",       String::Format(wxT("%.2f"), s->padding_bottom));
    stylev.emplace("background_color",     format_color(               s->background_color));

    boost::json::object fontv;
    fontv.emplace("name",                                              to_json_string(s->font.name()));
    fontv.emplace("italic_name",                                       to_json_string(s->font.italic_name()));
    fontv.emplace("size",                  String::Format(wxT("%.2f"), s->font.size()));
    fontv.emplace("weight",                                            s->font.weight());
    fontv.emplace("style",                                             s->font.style());
    fontv.emplace("underline",                                         s->font.underline());
    fontv.emplace("strikethrough",                                     s->font.strikethrough());
    fontv.emplace("scale_down_to",         String::Format(wxT("%.2f"), s->font.scale_down_to));
    fontv.emplace("max_stretch",           String::Format(wxT("%.2f"), s->font.max_stretch));
    fontv.emplace("color",                 format_color(               s->font.color()));
    fontv.emplace("shadow_color",          format_color(               s->font.shadow_color()));
    fontv.emplace("shadow_displacement_x", String::Format(wxT("%.2f"), s->font.shadow_displacement_x()));
    fontv.emplace("shadow_displacement_y", String::Format(wxT("%.2f"), s->font.shadow_displacement_y()));
    fontv.emplace("shadow_blur",           String::Format(wxT("%.2f"), s->font.shadow_blur()));
    fontv.emplace("stroke_color",          format_color(               s->font.stroke_color()));
    fontv.emplace("stroke_radius",         String::Format(wxT("%.2f"), s->font.stroke_radius()));
    fontv.emplace("stroke_blur",           String::Format(wxT("%.2f"), s->font.stroke_blur()));
    fontv.emplace("separator_color",       format_color(               s->font.separator_color));
    fontv.emplace("flags",                 String::Format(wxT("%i"),   s->font.flags));
    stylev.emplace("font",                                             fontv);
  }

  return stylev;
}

boost::json::object mse_to_json(const Set* set) {
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
  for (const CardP& card : set->cards) {
    cardsv.emplace_back(mse_to_json(card, set));
  }
  setv.emplace("cards", cardsv);
  // keywords
  boost::json::array keywordsv;
  for (const KeywordP& keyword : set->keywords) {
    keywordsv.emplace_back(mse_to_json(keyword));
  }
  if (!keywordsv.empty()) setv.emplace("keywords", keywordsv);
  // pack types
  boost::json::array pack_typesv;
  for (const PackTypeP& pack_type : set->pack_types) {
    pack_typesv.emplace_back(mse_to_json(pack_type));
  }
  if (!pack_typesv.empty()) setv.emplace("pack_types", pack_typesv);
  // done
  return setv;
}

boost::json::object mse_to_json(const StyleSheetP stylesheet) {
  boost::json::object stylesheetv;
  stylesheetv.emplace("mse_object_type",    "stylesheet");
  // built-in values
  write(stylesheetv, "mse_version",        stylesheet->fileVersion());
  write(stylesheetv, "short_name",         stylesheet->short_name);
  write(stylesheetv, "full_name",          stylesheet->full_name);
  write(stylesheetv, "folder_name",        stylesheet->folder_name + _(".mse-style"));
  write(stylesheetv, "version",            stylesheet->version);
  write(stylesheetv, "installer_group",    stylesheet->installer_group);
  write(stylesheetv, "icon",               stylesheet->icon_filename);
  write(stylesheetv, "dark_icon",          stylesheet->dark_icon_filename);
  write(stylesheetv, "position_hint",      stylesheet->position_hint);
  write(stylesheetv, "game",               stylesheet->game);
  write(stylesheetv, "card_width",         stylesheet->card_width);
  write(stylesheetv, "card_height",        stylesheet->card_height);
  write(stylesheetv, "card_dpi",           stylesheet->card_dpi);
  write(stylesheetv, "card_background",    format_color(stylesheet->card_background));
  // update scripts
  boost::json::array updatescriptsv;
  for (const UpdateCardsScriptP& script : stylesheet->update_cards_scripts) {
    updatescriptsv.emplace_back(to_json_string(script->before_version.toString()));
  }
  if (!updatescriptsv.empty()) stylesheetv.emplace("update_cards_scripts", updatescriptsv);
  // styling fields
  boost::json::array stylingfieldsv;
  for (const FieldP& field : stylesheet->styling_fields) {
    stylingfieldsv.emplace_back(to_json_string(field->name));
  }
  if (!stylingfieldsv.empty()) stylesheetv.emplace("styling_fields", stylingfieldsv);
  // extra fields
  boost::json::array extrafieldsv;
  for (const FieldP& field : stylesheet->extra_card_fields) {
    extrafieldsv.emplace_back(to_json_string(field->name));
  }
  if (!extrafieldsv.empty()) stylesheetv.emplace("extra_card_fields", extrafieldsv);
  // done
  return stylesheetv;
}

boost::json::object mse_to_json(const Game* game) {
  boost::json::object gamev;
  gamev.emplace("mse_object_type",    "game");
  // built-in values
  write(gamev, "mse_version",        game->fileVersion());
  write(gamev, "short_name",         game->short_name);
  write(gamev, "full_name",          game->full_name);
  write(gamev, "folder_name",        game->folder_name + _(".mse-game"));
  write(gamev, "version",            game->version);
  write(gamev, "installer_group",    game->installer_group);
  write(gamev, "icon",               game->icon_filename);
  write(gamev, "dark_icon",          game->dark_icon_filename);
  write(gamev, "position_hint",      game->position_hint);
  // update scripts
  boost::json::array updatescriptsv;
  for (const UpdateCardsScriptP& script : game->update_cards_scripts) {
    updatescriptsv.emplace_back(to_json_string(script->before_version.toString()));
  }
  if (!updatescriptsv.empty()) gamev.emplace("update_cards_scripts", updatescriptsv);
  // add scripts
  boost::json::array addscriptsv;
  for (const AddCardsScriptP& script : game->add_cards_scripts) {
    addscriptsv.emplace_back(script->name);
  }
  if (!addscriptsv.empty()) gamev.emplace("add_cards_scripts", addscriptsv);
  // set fields
  boost::json::array setfieldsv;
  for (const FieldP& field : game->set_fields) {
    setfieldsv.emplace_back(to_json_string(field->name));
  }
  if (!setfieldsv.empty()) gamev.emplace("set_fields", setfieldsv);
  // card fields
  boost::json::array cardfieldsv;
  for (const FieldP& field : game->card_fields) {
    cardfieldsv.emplace_back(to_json_string(field->name));
  }
  if (!cardfieldsv.empty()) gamev.emplace("card_fields", cardfieldsv);
  // card links
  boost::json::array cardlinksv;
  for (const CardLinkP& link : game->card_links) {
    cardlinksv.emplace_back(to_json_string(link->name()));
  }
  if (!cardlinksv.empty()) gamev.emplace("card_links", cardlinksv);
  // json paths
  boost::json::array jsonpathsv;
  for (const String& path : game->json_paths) {
    jsonpathsv.emplace_back(to_json_string(path));
  }
  if (!jsonpathsv.empty()) gamev.emplace("json_paths", jsonpathsv);
  // word lists
  boost::json::array wordlistsv;
  for (const WordListP& list : game->word_lists) {
    wordlistsv.emplace_back(to_json_string(list->name));
  }
  if (!wordlistsv.empty()) gamev.emplace("word_lists", wordlistsv);
  // keywords
  boost::json::array keywordmodesv;
  for (const KeywordModeP& mode : game->keyword_modes) {
    keywordmodesv.emplace_back(to_json_string(mode->name));
  }
  if (!keywordmodesv.empty()) gamev.emplace("keyword_modes", keywordmodesv);
  boost::json::array keywordparametersv;
  for (const KeywordParamP& mode : game->keyword_parameter_types) {
    keywordparametersv.emplace_back(to_json_string(mode->name));
  }
  if (!keywordparametersv.empty()) gamev.emplace("keyword_parameters", keywordparametersv);
  boost::json::array keywordsv;
  for (const KeywordP& keyword : game->keywords) {
    keywordsv.emplace_back(to_json_string(keyword->keyword));
  }
  if (!keywordsv.empty()) gamev.emplace("keywords", keywordsv);
  // pack types
  boost::json::array pack_typesv;
  for (const PackTypeP& pack_type : game->pack_types) {
    pack_typesv.emplace_back(to_json_string(pack_type->name));
  }
  if (!pack_typesv.empty()) gamev.emplace("pack_types", pack_typesv);
  // statistics
  boost::json::array statisticsdimensionsv;
  for (const StatsDimensionP& stat : game->statistics_dimensions) {
    statisticsdimensionsv.emplace_back(to_json_string(stat->name));
  }
  if (!statisticsdimensionsv.empty()) gamev.emplace("statistics_dimensions", statisticsdimensionsv);
  // done
  return gamev;
}

boost::json::object mse_to_json(const IndexMap<FieldP, ValueP>& map) {
  boost::json::object indexmapv;
  indexmapv.emplace("mse_object_type", "value_index_map");
  for (auto it = map.begin(); it != map.end(); ++it) {
    write(indexmapv, (*it)->fieldP->name, *it);
  }
  return indexmapv;
}

boost::json::object mse_to_json(const IndexMap<FieldP, StyleP>& map) {
  boost::json::object indexmapv;
  indexmapv.emplace("mse_object_type", "style_index_map");
  for (auto it = map.begin(); it != map.end(); ++it) {
    indexmapv.emplace((*it)->fieldP->name.ToStdString(), mse_to_json(*it));
  }
  return indexmapv;
}

boost::json::value mse_to_json(const ScriptValueP& sv, Set* set, bool suppress_warnings) {
  // special types
  if (ScriptObject<PackItemP>*            o = dynamic_cast<ScriptObject<PackItemP>*>           (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<PackTypeP>*            o = dynamic_cast<ScriptObject<PackTypeP>*>           (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<KeywordP>*             o = dynamic_cast<ScriptObject<KeywordP>*>            (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<StyleP>*               o = dynamic_cast<ScriptObject<StyleP>*>              (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<CardP>*                o = dynamic_cast<ScriptObject<CardP>*>               (sv.get())) return mse_to_json( o->getValue(), set);
  if (ScriptObject<SetP>*                 o = dynamic_cast<ScriptObject<SetP>*>                (sv.get())) return mse_to_json( o->getValue().get());
  if (ScriptObject<Set*>*                 o = dynamic_cast<ScriptObject<Set*>*>                (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<StyleSheetP>*          o = dynamic_cast<ScriptObject<StyleSheetP>*>         (sv.get())) return mse_to_json( o->getValue());
  if (ScriptObject<GameP>*                o = dynamic_cast<ScriptObject<GameP>*>               (sv.get())) return mse_to_json( o->getValue().get());
  if (ScriptObject<Game*>*                o = dynamic_cast<ScriptObject<Game*>*>               (sv.get())) return mse_to_json( o->getValue());
  if (ScriptMap<IndexMap<FieldP,ValueP>>* o = dynamic_cast<ScriptMap<IndexMap<FieldP,ValueP>>*>(sv.get())) return mse_to_json(*o->value);
  if (ScriptMap<IndexMap<FieldP,StyleP>>* o = dynamic_cast<ScriptMap<IndexMap<FieldP,StyleP>>*>(sv.get())) return mse_to_json(*o->value);

  // primitive types
  ScriptType type = sv->type();
  if (type == SCRIPT_NIL)      return boost::json::value(nullptr);
  if (type == SCRIPT_INT)      return boost::json::value(sv->toInt());
  if (type == SCRIPT_DOUBLE)   return boost::json::value(sv->toDouble());
  if (type == SCRIPT_BOOL)     return boost::json::value(sv->toBool());
  if (type == SCRIPT_STRING)   return boost::json::value(to_json_string(sv->toString()));
  if (type == SCRIPT_REGEX)    return boost::json::value(to_json_string(sv->toString()));
  if (type == SCRIPT_COLOR)    return boost::json::value(format_color(sv->toColor()));
  if (type == SCRIPT_DATETIME) return boost::json::value(sv->toDateTime().FormatISOCombined(' '));
  if (type == SCRIPT_COLLECTION) {
    ScriptCustomCollection* custom = dynamic_cast<ScriptCustomCollection*>(sv.get());
    if (custom) {
      if (custom->key_value.size() > 0) {
        boost::json::object object;
        map<String, ScriptValueP>::iterator it;
        for (it = custom->key_value.begin(); it != custom->key_value.end(); it++) {
          const std::string& key = it->first.ToStdString();
          if (object.contains(key)) queue_message(MESSAGE_WARNING, _ERROR_1_("mse map duplicate key", it->first));
          object.emplace(key, mse_to_json(it->second, set));
        }
        return object;
      }
      else {
        boost::json::array array;
        for (size_t i = 0; i < custom->value.size(); i++) {
          array.emplace_back(mse_to_json(custom->value[i], set));
        }
        return array;
      }
    } else {
      ScriptConcatCollection* concat = dynamic_cast<ScriptConcatCollection*>(sv.get());
      if (concat) {
        boost::json::value a = mse_to_json(concat->getA(), set);
        boost::json::value b = mse_to_json(concat->getB(), set);
        if (a.is_array() && b.is_array()) {
          boost::json::array array_a = a.get_array();
          boost::json::array array_b = b.get_array();
          for (size_t i = 0; i < array_b.size(); i++) {
            array_a.emplace_back(array_b[i]);
          }
          return array_a;
        } else if (a.is_object() && b.is_object()) {
          boost::json::object object_a = a.get_object();
          boost::json::object object_b = b.get_object();
          for (auto it = object_b.begin(); it != object_b.end(); ++it) {
            boost::json::string_view key_view = it->key();
            if (object_a.contains(key_view)) queue_message(MESSAGE_WARNING, _ERROR_1_("mse map duplicate key", String::FromUTF8(key_view.data(), key_view.size())));
            object_a.emplace(key_view, it->value());
          }
          return object_a;
        } else if (a.is_object() && b.is_array() && b.as_array().size() == 0) {
          return a.get_object();
        } else if (b.is_object() && a.is_array() && a.as_array().size() == 0) {
          return b.get_object();
        } else {
          queue_message(MESSAGE_ERROR, _ERROR_("json cant concat"));
          return boost::json::value(nullptr);
        }
      }
    }
  }
  if (!suppress_warnings) queue_message(MESSAGE_WARNING, _ERROR_1_("json unknown script type", sv->typeName()));
  return boost::json::value(nullptr);
}
