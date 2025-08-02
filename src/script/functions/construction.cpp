//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <script/functions/functions.hpp>
#include <script/functions/util.hpp>
#include <script/functions/construction_helper.hpp>
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

// ----------------------------------------------------------------------------- : new_card

SCRIPT_FUNCTION(new_card) {
  SCRIPT_PARAM(GameP, game);
  SCRIPT_OPTIONAL_PARAM_(bool, ignore_field_not_found);
  // create a new card object
  CardP new_card = make_intrusive<Card>(*game);
  // iterate on the given key/value pairs
  SCRIPT_PARAM(ScriptValueP, input);
  ScriptValueP it = input->makeIterator();
  ScriptValueP key;
  while (ScriptValueP value = it->next(&key)) {
    assert(key);
    if (key == script_nil) continue;
    String key_name = key->toString();
    // check if the given value is for a built-in field
    if (set_builtin_container(*game, new_card, value, key_name, ignore_field_not_found)) continue;
    // find the field value (container) that corresponds to the given value
    Value* container = get_card_field_container(*game, new_card->data, key_name, ignore_field_not_found);
    if (container == nullptr) continue;
    FieldP field = container->fieldP;
    // if the field has a construction script, set the value and card context variables to be the given value and this card, run script
    if (field->import_script) {
      ScriptValueP ctx_value = ctx.getVariableOpt(SCRIPT_VAR_value);
      ScriptValueP ctx_card =  ctx.getVariableOpt(SCRIPT_VAR_card);
      ctx.setVariable(SCRIPT_VAR_value, value);
      ctx.setVariable(SCRIPT_VAR_card,  to_script(new_card));
      ScriptValueP script_input = field->import_script.invoke(ctx, true);
      // if the script result is a collection, iterate on the key/value pairs
      // treat the keys as field names and the values as what to populate those fields with
      if (script_input->type() == SCRIPT_COLLECTION) {
        ScriptValueP script_it = script_input->makeIterator();
        ScriptValueP script_key;
        while (ScriptValueP script_value = script_it->next(&script_key)) {
          assert(script_key);
          if (script_key == script_nil) continue;
          String script_key_name = script_key->toString();
          // check if the script value is for a built-in field
          if (set_builtin_container(*game, new_card, script_value, script_key_name, ignore_field_not_found)) continue;
          // find the field value that corresponds to the script value
          Value* script_container = get_card_field_container(*game, new_card->data, script_key_name, ignore_field_not_found);
          if (script_container == nullptr) continue;
          // set the field value to the script value
          set_container(script_container, script_value, script_key_name);
        }
      }
      // if the script result is not a collection, simply set the field value to the script value
      else {
        set_container(container, script_input, key_name);
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
  if (game->import_script) {
    ScriptValueP ctx_card = ctx.getVariableOpt(SCRIPT_VAR_card);
    ctx.setVariable(SCRIPT_VAR_card, to_script(new_card));
    ScriptValueP script_input = game->import_script.invoke(ctx, true);
    if (script_input->type() == SCRIPT_COLLECTION) {
      // iterate on the key/value pairs given by the script
      ScriptValueP script_it = script_input->makeIterator();
      ScriptValueP script_key;
      while (ScriptValueP script_value = script_it->next(&script_key)) {
        assert(script_key);
        if (script_key == script_nil) continue;
        String script_key_name = script_key->toString();
        // check if the script value is for a built-in field
        if (set_builtin_container(*game, new_card, script_value, script_key_name, ignore_field_not_found)) continue;
        // find the field value that corresponds to the script value
        Value* script_container = get_card_field_container(*game, new_card->data, script_key_name, ignore_field_not_found);
        if (script_container == nullptr) continue;
        // set the field value to the script value
        set_container(script_container, script_value, script_key_name);
      }
    }
    else {
      queue_message(MESSAGE_ERROR, _ERROR_("game import script not map"));
    }
    // restore old context card
    if (ctx_card) ctx.setVariable(SCRIPT_VAR_card, ctx_card);
  }
  SCRIPT_RETURN(new_card);
}

SCRIPT_FUNCTION(add_card_to_set) {
  SCRIPT_PARAM_C(ScriptValueP, input);
  SCRIPT_PARAM_C(ScriptValueP, set);
  ScriptObject<Set*>* s = dynamic_cast<ScriptObject<Set*>*>(set.get());
  if (s) {
    Set& _set = *s->getValue();
    ScriptObject<CardP>* c = dynamic_cast<ScriptObject<CardP>*>(input.get());
    if (c) {
      CardP _card = c->getValue();
      _set.actions.addAction(make_unique<AddCardAction>(ADD, _set, _card));
      SCRIPT_RETURN(true);
    }
    if (input->type() == SCRIPT_COLLECTION) {
      vector<CardP> _cards;
      ScriptValueP it = input->makeIterator();
      ScriptValueP key;
      while (ScriptValueP value = it->next(&key)) {
        c = dynamic_cast<ScriptObject<CardP>*>(value.get());
        if (c) {
          _cards.push_back(c->getValue());
        }
      }
      if (!_cards.empty()) {
        _set.actions.addAction(make_unique<AddCardAction>(ADD, _set, _cards));
        SCRIPT_RETURN(true);
      }
    }
  }
  SCRIPT_RETURN(false);
}

// ----------------------------------------------------------------------------- : Init

void init_script_construction_functions(Context& ctx) {
  ctx.setVariable(_("new_card"), script_new_card);
  ctx.setVariable(_("add_card_to_set"), script_add_card_to_set);
}
