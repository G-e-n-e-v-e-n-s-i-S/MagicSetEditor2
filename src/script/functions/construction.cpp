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
#include <data/card.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : new_card

SCRIPT_FUNCTION(new_card) {
  SCRIPT_PARAM(GameP, game);
  CardP new_card = make_intrusive<Card>(*game);
  // set field values
  SCRIPT_PARAM(ScriptValueP, input);
  ScriptValueP it = input->makeIterator();
  ScriptValueP key;
  while (ScriptValueP v = it->next(&key)) {
    assert(key);
    String name = key->toString();
    // find value to update
    IndexMap<FieldP,ValueP>::const_iterator value_it = new_card->data.find(name);
    if (value_it == new_card->data.end()) {
      // look among alternate names
      map<String, String>::iterator it2 = game->card_fields_alt_names.find(unified_form(name));
      if (it2 != game->card_fields_alt_names.end()) {
        value_it = new_card->data.find(it2->second);
      }
    }
    if (value_it == new_card->data.end()) {
      throw ScriptError(_ERROR_1_("no field with name", name));
    }
    Value* value = value_it->get();
    // set the value
    if (TextValue* tvalue = dynamic_cast<TextValue*>(value)) {
      tvalue->value = v->toString();
    } else if (ChoiceValue* cvalue = dynamic_cast<ChoiceValue*>(value)) {
      cvalue->value = v->toString();
    } else if (PackageChoiceValue* pvalue = dynamic_cast<PackageChoiceValue*>(value)) {
      pvalue->package_name = v->toString();
    } else if (ColorValue* cvalue = dynamic_cast<ColorValue*>(value)) {
      cvalue->value = v->toColor();
    } else if (ImageValue* ivalue = dynamic_cast<ImageValue*>(value)) {
      wxFileName fname( static_cast<ExternalImage*>(v.get())->toString() );
      ivalue->filename = LocalFileName::fromReadString( fname.GetName(), "");
    } else {
      throw ScriptError(_ERROR_1_("can't set value", name));
    }
  }
  SCRIPT_RETURN(new_card);
}

// ----------------------------------------------------------------------------- : Init

void init_script_construction_functions(Context& ctx) {
  ctx.setVariable(_("new_card"), script_new_card);
}
