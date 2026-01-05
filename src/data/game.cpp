//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/game.hpp>
#include <data/field.hpp>
#include <data/field/choice.hpp>
#include <data/card_link.hpp>
#include <data/keyword.hpp>
#include <data/statistics.hpp>
#include <data/pack.hpp>
#include <data/word_list.hpp>
#include <data/add_cards_script.hpp>
#include <util/io/package_manager.hpp>
#include <script/script.hpp>

// ----------------------------------------------------------------------------- : Game

IMPLEMENT_DYNAMIC_ARG(Game*, game_for_reading, nullptr);

Game::Game()
  : has_keywords(false)
  , dependencies_initialized(false)
{}

GameP Game::byName(const String& name) {
  return package_manager.open<Game>(name + _(".mse-game"));
}

bool Game::isMagic() const {
  return name() == _("magic");
}

String Game::typeNameStatic() { return _("game"); }
String Game::typeName() const { return _("game"); }
Version Game::fileVersion() const { return file_version_game; }

IMPLEMENT_REFLECTION(Game) {
  REFLECT_BASE(Packaged);
  REFLECT_NO_SCRIPT(init_script);
  REFLECT_NO_SCRIPT(set_fields);
  REFLECT_IF_READING {
    default_set_style.init(set_fields);
  }
  REFLECT_NO_SCRIPT(default_set_style);
  REFLECT_NO_SCRIPT(card_fields);
  REFLECT_NO_SCRIPT(card_links);
  REFLECT_NO_SCRIPT(card_list_color_script);
  REFLECT_NO_SCRIPT(import_script);
  REFLECT_NO_SCRIPT(json_paths);
  REFLECT_NO_SCRIPT(statistics_dimensions);
  REFLECT_NO_SCRIPT(statistics_categories);
  REFLECT_COMPAT(<308, "pack_item", pack_types);
  REFLECT_NO_SCRIPT(pack_types);
  REFLECT_NO_SCRIPT(keyword_match_script);
  REFLECT(has_keywords);
  REFLECT(keyword_modes);
  REFLECT(keyword_parameter_types);
  REFLECT_NO_SCRIPT(keywords);
  REFLECT_NO_SCRIPT(word_lists);
  REFLECT_NO_SCRIPT(add_cards_scripts);
  REFLECT_NO_SCRIPT(auto_replaces);
}

void Game::validate(Version v) {
  // check that we have at least one card field
  if (card_fields.size() < 1) {
    throw Error(_ERROR_1_("no card fields", name()));
  }
  Packaged::validate(v);
  // automatic statistics dimensions
  {
    vector<StatsDimensionP> dims;
    FOR_EACH(f, card_fields) {
      if (f->show_statistics) {
        dims.push_back(make_intrusive<StatsDimension>(*f));
      }
    }
    statistics_dimensions.insert(statistics_dimensions.begin(), dims.begin(), dims.end()); // push front
  }
  // automatic statistics categories
  {
    vector<StatsCategoryP> cats;
    FOR_EACH(dim, statistics_dimensions) {
      cats.push_back(make_intrusive<StatsCategory>(dim));
    }
    statistics_categories.insert(statistics_categories.begin(), cats.begin(), cats.end()); // push front
  }
  // automatic pack if there are none
  if (pack_types.empty()) {
    PackTypeP pack(new PackType);
    pack->name = _("Any card");
    pack->enabled = true;
    pack->selectable = true;
    pack->summary = true;
    pack->filter = OptionalScript(_("true"));
    pack->select = SELECT_NO_REPLACE;
    pack_types.push_back(pack);
  }
  // alternate card field names map
  for (auto it = card_fields.begin(); it != card_fields.end(); ++it) {
    FieldP field = *it;
    String unified_name = unified_form(field->name);
    if (card_fields_alt_names.count(unified_name)) {
      queue_message(MESSAGE_WARNING, _("Duplicate alternate card field name: ") + unified_name);
    }
    else {
      card_fields_alt_names.emplace(unified_name, field->name);
    }
    //String column_name = field->card_list_name.get();
    //card_fields_alt_names.emplace(unified_form(column_name), field->name);
    for (auto it2 = field->alt_names.begin(); it2 != field->alt_names.end(); ++it2) {
      unified_name = unified_form(*it2);
      if (card_fields_alt_names.count(unified_name)) {
        queue_message(MESSAGE_WARNING, _("Duplicate alternate card field name: ") + unified_name);
      }
      else {
        card_fields_alt_names.emplace(unified_name, field->name);
      }
    }
  }
  // front face/back face card link
  card_links.insert(card_links.begin(), make_intrusive<CardLink>());
  card_links[0]->selected.default_ = _("Front Face");
  card_links[0]->selected.translations = std::unordered_map<String, String>{
    {_("ch-s"),  _("卡片正面")},
    {_("ch-t"),  _("卡片正面")},
    {_("da"),    _("Forside")},
    {_("de"),    _("Vorderseite")},
    {_("en"),    _("Front Face")},
    {_("es"),    _("Cara frontal")},
    {_("fr"),    _("Face Avant")},
    {_("it"),    _("Fronte")},
    {_("jp"),    _("カードの表面")},
    {_("ko"),    _("카드 앞면")},
    {_("pl"),    _("Przód")},
    {_("pt-br"), _("Frente")},
    {_("ru"),    _("Лицевая сторона")}
  };
  card_links[0]->linked.default_ = _("Back Face");
  card_links[0]->linked.translations = std::unordered_map<String, String>{
    {_("ch-s"),  _("卡片背面")},
    {_("ch-t"),  _("卡片背面")},
    {_("da"),    _("Bagside")},
    {_("de"),    _("Rückseite")},
    {_("en"),    _("Back Face")},
    {_("es"),    _("Cara posterior")},
    {_("fr"),    _("Face Arrière")},
    {_("it"),    _("Retro")},
    {_("jp"),    _("カードの裏面")},
    {_("ko"),    _("카드 뒷면")},
    {_("pl"),    _("Tył")},
    {_("pt-br"), _("Verso")},
    {_("ru"),    _("Обратная сторона")}
  };
  // localized card link names map
  for (auto it = card_links.begin(); it != card_links.end(); ++it) {
    CardLinkP link = *it;
    String selected_default = link->selected.default_;
    for (auto selected_it = link->selected.translations.begin(); selected_it != link->selected.translations.end(); selected_it++) {
      String selected_tr = unified_form(selected_it->second);
      if (card_links_alt_names.find(selected_tr) != card_links_alt_names.end() && card_links_alt_names[selected_tr] != selected_default) {
        queue_message(MESSAGE_WARNING, _ERROR_3_("link duplicate", selected_tr, card_links_alt_names[selected_tr], selected_default));
      }
      card_links_alt_names.emplace(selected_tr, selected_default);
    }
    String linked_default = link->linked.default_;
    for (auto linked_it = link->linked.translations.begin(); linked_it != link->linked.translations.end(); linked_it++) {
      String linked_tr = unified_form(linked_it->second);
      if (card_links_alt_names.find(linked_tr) != card_links_alt_names.end() && card_links_alt_names[linked_tr] != linked_default) {
        queue_message(MESSAGE_WARNING, _ERROR_3_("link duplicate", linked_tr, card_links_alt_names[linked_tr], linked_default));
      }
      card_links_alt_names.emplace(linked_tr, linked_default);
    }
  }
}

void Game::initCardListColorScript() {
  if (card_list_color_script) return; // already done
  // find a field with choice_colors_cardlist
  FOR_EACH(s, card_fields) {
    ChoiceFieldP cf = dynamic_pointer_cast<ChoiceField>(s);
    if (cf && !cf->choice_colors_cardlist.empty()) {
      // found the field to use
      // initialize script:  field.colors[card.field-name] or else rgb(0,0,0)
      Script& s = card_list_color_script.getMutableScript();
      s.addInstruction(I_PUSH_CONST, to_script(&cf->choice_colors_cardlist));
      s.addInstruction(I_GET_VAR,    SCRIPT_VAR_card);
      s.addInstruction(I_MEMBER_C,   cf->name);
      s.addInstruction(I_BINARY,     I_MEMBER);
      s.addInstruction(I_PUSH_CONST, to_script(Color(0,0,0)));
      s.addInstruction(I_BINARY,     I_OR_ELSE);
      return;
    }
  }
}

// special behaviour of reading/writing GamePs: only read/write the name

void Reader::handle(GameP& game) {
  game = Game::byName(getValue());
}
void Writer::handle(const GameP& game) {
  if (game) handle(game->name());
}
