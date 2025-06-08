//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <script/functions/functions.hpp>
#include <script/functions/util.hpp>
#include <data/field.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/package_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : Helper functions

static Value* get_container(GameP& game, CardP& card, String key_name) {
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
  if (key_name == _("notes")) {
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

// ----------------------------------------------------------------------------- : new_card

SCRIPT_FUNCTION(new_card) {
  SCRIPT_PARAM(GameP, game);
  // create a new card object
  CardP new_card = make_intrusive<Card>(*game);
  // iterate on the given key/value pairs
  SCRIPT_PARAM(ScriptValueP, input);
  ScriptValueP it = input->makeIterator();
  ScriptValueP key;
  while (ScriptValueP value = it->next(&key)) {
    assert(key);
    String key_name = key->toString();
    // check if the given value is for a built-in field
    if (set_builtin_container(game, new_card, value, key_name)) continue;
    // find the field value (container) that corresponds to the given value
    Value* container = get_container(game, new_card, key_name);
    FieldP field = container->fieldP;
    // if the field has a construction script, set the value and card context variables to be the given value and this card, run script
    if (field->construction_script) {
      ScriptValueP ctx_value = ctx.getVariableOpt(SCRIPT_VAR_value);
      ScriptValueP ctx_card =  ctx.getVariableOpt(SCRIPT_VAR_card);
      ctx.setVariable(SCRIPT_VAR_value, value);
      ctx.setVariable(SCRIPT_VAR_card,  to_script(new_card));
      ScriptValueP script_input = field->construction_script.invoke(ctx, true);
      // iterate on the key/value pairs given by the script
      ScriptValueP script_it = script_input->makeIterator();
      ScriptValueP script_key;
      while (ScriptValueP script_value = script_it->next(&script_key)) {
        assert(script_key);
        String script_key_name = script_key->toString();
        // check if the script value is for a built-in field
        if (set_builtin_container(game, new_card, script_value, script_key_name)) continue;
        // find the field value that corresponds to the script value
        Value* script_container = get_container(game, new_card, script_key_name);
        // set the field value to the script value
        set_container(script_container, script_value, script_key_name);
      }
      // restore old value and card context variables
      if (ctx_value) ctx.setVariable(SCRIPT_VAR_value, ctx_value);
      if (ctx_card)  ctx.setVariable(SCRIPT_VAR_card,  ctx_card);
    }
    // if the field has no construction script, simply set the field value to the given value
    else {
      set_container(container, value, key_name);
    }
  }
  // if the game has a construction script, set the card context variable to be this card, run script
  if (game->construction_script) {
    ScriptValueP ctx_card = ctx.getVariableOpt(SCRIPT_VAR_card);
    ctx.setVariable(SCRIPT_VAR_card, to_script(new_card));
    ScriptValueP script_input = game->construction_script.invoke(ctx, true);
    // iterate on the key/value pairs given by the script
    ScriptValueP script_it = script_input->makeIterator();
    ScriptValueP script_key;
    while (ScriptValueP script_value = script_it->next(&script_key)) {
      assert(script_key);
      String script_key_name = script_key->toString();
      // check if the script value is for a built-in field
      if (set_builtin_container(game, new_card, script_value, script_key_name)) continue;
      // find the field value that corresponds to the script value
      Value* script_container = get_container(game, new_card, script_key_name);
      // set the field value to the script value
      set_container(script_container, script_value, script_key_name);
    }
    // restore old context card
    if (ctx_card) ctx.setVariable(SCRIPT_VAR_card, ctx_card);
  }
  SCRIPT_RETURN(new_card);
}

// ----------------------------------------------------------------------------- : Init

void init_script_construction_functions(Context& ctx) {
  ctx.setVariable(_("new_card"), script_new_card);
}
