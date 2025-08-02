//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/package_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/field/symbol.hpp>
#include <data/action/set.hpp>
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>

// ----------------------------------------------------------------------------- : Helper functions

inline static Value* get_card_field_container(Game& game, IndexMap<FieldP, ValueP>& map, String& key_name, bool ignore_field_not_found) {
  // find value container to update
  IndexMap<FieldP, ValueP>::const_iterator it = map.find(key_name);
  if (it == map.end()) {
    // look among alternate names
    std::map<String, String>::iterator alt_name_it = game.card_fields_alt_names.find(unified_form(key_name));
    if (alt_name_it != game.card_fields_alt_names.end()) {
      it = map.find(alt_name_it->second);
    }
  }
  if (it == map.end()) {
    if (ignore_field_not_found) return nullptr;
    throw ScriptError(_ERROR_2_("no field with name", _TYPE_("card"), key_name));
  }
  return it->get();
}

inline static Value* get_container(IndexMap<FieldP, ValueP>& map, String& type, String& key_name, bool ignore_field_not_found) {
  // find value container to update
  IndexMap<FieldP, ValueP>::const_iterator it = map.find(key_name);
  if (it == map.end()) {
    it = map.find(key_name.Lower());
    if (it == map.end()) {
      if (ignore_field_not_found) return nullptr;
      throw ScriptError(_ERROR_2_("no field with name", _TYPE_V_(type), key_name));
    }
  }
  return it->get();
}

inline static void set_container(Value* container, ScriptValueP& value, String key_name) {
  // set the given value into the container
  if (TextValue* tvalue = dynamic_cast<TextValue*>(container)) {
    tvalue->value = value->toString();
  }
  else if (ChoiceValue* cvalue = dynamic_cast<ChoiceValue*>(container)) {
    cvalue->value = value->toString();
  }
  else if (PackageChoiceValue* pvalue = dynamic_cast<PackageChoiceValue*>(container)) {
    pvalue->package_name = value->toString();
  }
  else if (ColorValue* cvalue = dynamic_cast<ColorValue*>(container)) {
    cvalue->value = value->toColor();
  }
  else if (ImageValue* ivalue = dynamic_cast<ImageValue*>(container)) {
    if (ExternalImage* image = dynamic_cast<ExternalImage*>(value.get())) {
      wxFileName fname(image->toString());
      ivalue->filename = LocalFileName::fromReadString(fname.GetName(), "");
    } else if (value->type() == SCRIPT_STRING) {
      ivalue->filename = LocalFileName::fromReadString(value->toString(), "");
    } else {
      throw ScriptError(_ERROR_1_("cant set image value", key_name));
    }
  }
  else if (SymbolValue* svalue = dynamic_cast<SymbolValue*>(container)) {
    if (value->type() == SCRIPT_STRING) {
      svalue->filename = LocalFileName::fromReadString(value->toString(), "");
    } else {
      throw ScriptError(_ERROR_1_("cant set symbol value", key_name));
    }
  }
  else {
    throw ScriptError(_ERROR_1_("cant set value", key_name));
  }
}

inline static bool set_builtin_container(const Game& game, CardP& card, ScriptValueP& value, String key_name, bool ignore_field_not_found) {
  // check if the given value is for a built-in field, if found set it and return true
  key_name = unified_form(key_name);
  if (key_name == _("notes") || key_name == _("note")) {
    card->notes = value->toString();
    return true;
  } else if (key_name == _("style") || key_name == _("stylesheet") || key_name == _("template")) {
    if (!trim(value->toString()).empty()) {
      card->stylesheet = StyleSheet::byGameAndName(game, value->toString());
      if (card->stylesheet) card->styling_data.init(card->stylesheet->styling_fields);
    }
    return true;
  }
  //else if (key_name == _("id") || key_name == _("uid")) {
  //  card->uid = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_card") || key_name == _("linked_card_1")) {
  //  card->linked_card_1 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_card_2")) {
  //  card->linked_card_2 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_card_3")) {
  //  card->linked_card_3 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_card_4")) {
  //  card->linked_card_4 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_relation") || key_name == _("linked_relation_1")) {
  //  card->linked_relation_1 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_relation_2")) {
  //  card->linked_relation_2 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_relation_3")) {
  //  card->linked_relation_3 = value->toString();
  //  return true;
  //}
  //else if (key_name == _("linked_relation_4")) {
  //  card->linked_relation_4 = value->toString();
  //  return true;
  //}
  else if          (key_name == _("styling_data")   || key_name == _("style_data")   || key_name == _("stylesheet_data")   || key_name == _("template_data") || key_name == _("styling")
                 || key_name == _("styling_fields") || key_name == _("style_fields") || key_name == _("stylesheet_fields") || key_name == _("template_fields")
                 || key_name == _("extra_data")     || key_name == _("extra_fields") || key_name == _("extra_card_data")   || key_name == _("extra_card_fields")) {
    bool is_extra = key_name == _("extra_data")     || key_name == _("extra_fields") || key_name == _("extra_card_data")   || key_name == _("extra_card_fields");
    String type = is_extra ? _("extra") : _("styling");
    if (value->type() != SCRIPT_COLLECTION) {
      throw ScriptError(_ERROR_1_("styling data not map", type));
    }
    if (!card->stylesheet) {
      throw ScriptError(_ERROR_1_("styling data without stylesheet", type));
    }
    IndexMap<FieldP, ValueP>& data = is_extra ? card->extraDataFor(*card->stylesheet) : card->styling_data;
    ScriptValueP it = value->makeIterator();
    ScriptValueP key;
    while (ScriptValueP value = it->next(&key)) {
      assert(key);
      if (key == script_nil) continue;
      String key_name = key->toString();
      Value* container = get_container(data, type, key_name, ignore_field_not_found);
      set_container(container, value, key_name);
      if (!is_extra) card->has_styling = true;
    }
    return true;
  }
  return false;
}

inline static bool check_table_headers(GameP& game, std::vector<String>& headers, const String& file_extension, String& missing_fields_out) {
  if (headers.empty()) {
    queue_message(MESSAGE_ERROR, _("Empty headers given"));
    return false;
  }
  for (int x = 0; x < headers.size(); ++x) {
    String key_name = headers[x];
    if ( game->card_fields_alt_names.find(unified_form(key_name)) == game->card_fields_alt_names.end()
      || key_name == _("notes")
      || key_name == _("note")
      || key_name == _("style")
      || key_name == _("stylesheet")
      || key_name == _("template")
      || key_name == _("id")
      || key_name == _("uid")
      || key_name == _("multiverse_id")
      || key_name == _("linked_card")
      || key_name == _("linked_card_1")
      || key_name == _("linked_card_2")
      || key_name == _("linked_card_3")
      || key_name == _("linked_card_4")
      || key_name == _("linked_relation")
      || key_name == _("linked_relation_1")
      || key_name == _("linked_relation_2")
      || key_name == _("linked_relation_3")
      || key_name == _("linked_relation_4")
      || key_name == _("link_relation")
      || key_name == _("link_relation_1")
      || key_name == _("link_relation_2")
      || key_name == _("link_relation_3")
      || key_name == _("link_relation_4")
    ) {
      missing_fields_out += _("\n   ") + key_name;
    }
  }
  return true;
}

inline static bool cards_from_table(SetP& set, vector<String>& headers, std::vector<std::vector<ScriptValueP>>& table, bool ignore_field_not_found, const String& file_extension, vector<CardP>& cards_out) {
  // ensure table is square
  int count = headers.size();
  for (int y = 0; y < table.size(); ++y) {
    if (table[y].size() != count) {
      queue_message(MESSAGE_ERROR, _ERROR_1_("add card csv file malformed", wxString::Format(wxT("%i"), y+1)));
      return false;
    }
  }
  // produce cards from table
  Context& ctx = set->getContext();
  ScriptValueP new_card_function = ctx.getVariable("new_card");
  ScriptValueP ctx_input = ctx.getVariableOpt(SCRIPT_VAR_input);
  ScriptValueP ctx_ignore = ctx.getVariableOpt("ignore_field_not_found");
  ctx.setVariable("ignore_field_not_found", to_script(ignore_field_not_found));
  for (int y = 0; y < table.size(); ++y) {
    ScriptCustomCollectionP field_map = make_intrusive<ScriptCustomCollection>();
    for (int x = 0; x < count; ++x) {
      // check if value is worth writing
      if (table[y][x] != script_nil) {
        field_map->key_value[headers[x]] = table[y][x];
      }
    }
    ctx.setVariable(SCRIPT_VAR_input, field_map);
    CardP card = from_script<CardP>(new_card_function->eval(ctx));
    // is this a new card?
    if (contains(set->cards, card) || contains(cards_out, card)) {
      // make copy
      card = make_intrusive<Card>(*card);
    }
    cards_out.push_back(card);
  }
  if (ctx_input) ctx.setVariable(SCRIPT_VAR_input, ctx_input);
  if (ctx_ignore) ctx.setVariable("ignore_field_not_found", ctx_ignore);
  return true;
}
