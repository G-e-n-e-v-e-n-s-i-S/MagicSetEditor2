//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/package_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/action/set.hpp>
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : Helper functions

static Value* get_container(GameP& game, CardP& card, String key_name, bool ignore_field_not_found) {
  // find value container to update
  IndexMap<FieldP, ValueP>::const_iterator value_it = card->data.find(key_name);
  if (value_it == card->data.end()) {
    // look among alternate names
    map<String, String>::iterator alt_name_it = game->card_fields_alt_names.find(unified_form(key_name));
    if (alt_name_it != game->card_fields_alt_names.end()) {
      value_it = card->data.find(alt_name_it->second);
    }
  }
  if (value_it == card->data.end()) {
    if (ignore_field_not_found) return nullptr;
    throw ScriptError(_ERROR_1_("no field with name", key_name));
  }
  return value_it->get();
}

static void set_container(Value* container, ScriptValueP& value, String key_name) {
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
    wxFileName fname(static_cast<ExternalImage*>(value.get())->toString());
    ivalue->filename = LocalFileName::fromReadString(fname.GetName(), "");
  }
  else {
    throw ScriptError(_ERROR_1_("can't set value", key_name));
  }
}

static bool set_builtin_container(GameP& game, CardP& card, ScriptValueP& value, String key_name) {
  // check if the given value is for a built-in field, if found set it and return true
  key_name = unified_form(key_name);
  if (key_name == _("notes") || key_name == _("note")) {
    card->notes = value->toString();
    return true;
  } else if (key_name == _("style") || key_name == _("stylesheet") || key_name == _("template")) {
    if (trim(value->toString()) != wxEmptyString) card->stylesheet = StyleSheet::byGameAndName(*game, value->toString());
    return true;
  }
  // else if (key_name == _("id") || key_name == _("multiverse_id")) {
  //  card->id = value->toString();
  //  return true;
  //}
  
  //styling_data;
  //linked_card;
  //linked_relation_1;
  return false;
}

static bool check_table_headers(GameP& game, std::vector<String>& headers, const String& file_extension, String& missing_fields_out) {
  if (headers.size() == 0) {
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

static bool cards_from_table(SetP& set, vector<String>& headers, std::vector<std::vector<ScriptValueP>>& table, bool ignore_field_not_found, const String& file_extension, vector<CardP>& cards_out) {
  // ensure table is square
  int count = headers.size();
  for (int y = 0; y < table.size(); ++y) {
    if (table[y].size() != count) {
      queue_message(MESSAGE_ERROR, _ERROR_2_("import file malformed", file_extension, wxString::Format(wxT("%i"), y+1)));
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


