//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>
#include <data/keyword.hpp>
#include <data/pack.hpp>
#include <data/field.hpp>
#include <data/update_cards_script.hpp>
#include <data/field/information.hpp>
#include <data/field/image.hpp>
#include <data/field/symbol.hpp>
#include <data/action/value.hpp>
#include <util/order_cache.hpp>
#include <util/delayed_index_maps.hpp>
#include <util/uid.hpp>
#include <script/script_manager.hpp>
#include <script/profiler.hpp>
#include <wx/sstream.h>

// ----------------------------------------------------------------------------- : Set

Set::Set()
  : vcs (make_intrusive<VCS>())
  , script_manager(new SetScriptManager(*this))
{}

Set::Set(const GameP& game)
  : game(game)
  , vcs (make_intrusive<VCS>())
  , script_manager(new SetScriptManager(*this))
{
  data.init(game->set_fields);
}

Set::Set(const StyleSheetP& stylesheet)
  : game(stylesheet->game)
  , stylesheet(stylesheet)
  , vcs (make_intrusive<VCS>())
  , script_manager(new SetScriptManager(*this))
{
  data.init(game->set_fields);
}

Set::~Set() {}


Context& Set::getContext() {
  assert(wxThread::IsMain());
  return script_manager->getContext(CardP());
}
Context& Set::getContext(const CardP& card) {
  assert(wxThread::IsMain());
  return script_manager->getContext(card);
}
void Set::updateStyles(const CardP& card, bool only_content_dependent) {
  script_manager->updateStyles(card, only_content_dependent);
}
void Set::updateDelayed() {
  script_manager->updateDelayed();
}
void Set::updateLinkScripts(const CardP& card) {
  script_manager->updateLinkScripts(card);
}
void Set::updateCardsScripts() {
  // first apply all the stylesheet update_cards_scripts that are older than the first game update_cards_scripts
  // then apply the first game update_cards_scripts
  // then apply all the stylesheet scripts that are older than the second game script
  // then apply the second game script, etc...
  Version previous_cutoff = Version();
  Version current_cutoff = Version();
  for (size_t g = 0; g < game->update_cards_scripts.size() + 1; ++g) {
    bool last_iteration = g == game->update_cards_scripts.size();
    UpdateCardsScriptP game_script = last_iteration ? nullptr : game->update_cards_scripts[g];
    previous_cutoff = current_cutoff;
    current_cutoff = last_iteration ? current_cutoff : game_script->before_version;
    // Apply stylesheet scripts that are older than the current game script
    for (size_t i = 0; i < cards.size(); ++i) {
      CardP& card = cards[i];
      StyleSheetP stylesheet = stylesheetForP(card);
      Version stylesheet_version = card->stylesheet_version.isZero() ? this->stylesheet_version : card->stylesheet_version;
      for (size_t j = 0; j < stylesheet->update_cards_scripts.size(); ++j) {
        UpdateCardsScriptP& script = stylesheet->update_cards_scripts[j];
        if (script->before_version >= current_cutoff && !last_iteration) continue;
        if (script->before_version < previous_cutoff) continue;
        if (stylesheet_version >= script->before_version) continue;
        vector<CardP> new_cards = script->perform(*this, card);
        if (!new_cards.empty()) {
          FOR_EACH(new_card, new_cards) {
            // Initialize the stylesheet_version if it wasn't defined, to prevent this script from applying again
            if (stylesheet == stylesheetForP(new_card) && new_card->stylesheet_version < script->before_version) {
              new_card->stylesheet_version = script->before_version;
            }
          }
          --i;
          break;
        }
      }
    }
    // Apply current game script
    if (last_iteration || game_version >= current_cutoff) continue;
    size_t size = cards.size();
    for (int i = 0; i < size; ++i) {
      CardP& card = cards[i];
      vector<CardP> new_cards = game_script->perform(*this, card);
      if (!new_cards.empty()) {
        --i;
        --size;
      }
    }
  }
}
void Set::buildUIDMap() {
  card_uids.clear();
  FOR_EACH(c, cards) {
    while (card_uids.find(c->uid) != card_uids.end()) {
      queue_message(MESSAGE_WARNING, _("Multiple cards found with same uid:\n") + c->identification() + _("\n") + card_uids[c->uid]->identification() + _("\nPlease notify someone on the Discord server."));
      c->uid = generate_uid();
    }
    card_uids[c->uid] = c;
  }
}

Context& Set::getContextForThumbnails() {
  assert(!wxThread::IsMain());
  if (!thumbnail_script_context) {
    thumbnail_script_context.reset(new SetScriptContext(*this));
  }
  return thumbnail_script_context->getContext(CardP());
}
Context& Set::getContextForThumbnails(const CardP& card) {
  assert(!wxThread::IsMain());
  if (!thumbnail_script_context) {
    thumbnail_script_context.reset(new SetScriptContext(*this));
  }
  return thumbnail_script_context->getContext(card);
}
Context& Set::getContextForThumbnails(const StyleSheetP& stylesheet) {
  assert(!wxThread::IsMain());
  if (!thumbnail_script_context) {
    thumbnail_script_context.reset(new SetScriptContext(*this));
  }
  return thumbnail_script_context->getContext(stylesheet);
}

const StyleSheet& Set::stylesheetFor(const CardP& card) {
  if (card && card->stylesheet) return *card->stylesheet;
  else                          return *stylesheet;
}
StyleSheetP Set::stylesheetForP(const CardP& card) {
  if (card && card->stylesheet) return card->stylesheet;
  else                          return stylesheet;
}

IndexMap<FieldP, ValueP>& Set::stylingDataFor(const StyleSheet& stylesheet) {
  return styling_data.get(stylesheet.name(), stylesheet.styling_fields);
}
IndexMap<FieldP, ValueP>& Set::stylingDataFor(const CardP& card) {
  if (card && card->has_styling) return card->styling_data;
  else                           return stylingDataFor(stylesheetFor(card));
}

void Set::referenceActionStackFiles() {
  referenceActionStackFiles(true);
  referenceActionStackFiles(false);
}
void Set::referenceActionStackFiles(bool undo) {
  for (auto&& action : undo ? actions.undo_actions : actions.redo_actions) {
    try {
      SimpleValueAction<ImageValue, false>& v = dynamic_cast<SimpleValueAction<ImageValue, false>&>(*action);
      if (ImageValue* v2 = dynamic_cast<ImageValue*>(v.valueP.get())) {
        referenceFile(v.new_value.toStringForWriting());
        referenceFile(v2->filename.toStringForWriting());
      }
    } catch (...) { try {
      SimpleValueAction<SymbolValue, false>& v = dynamic_cast<SimpleValueAction<SymbolValue, false>&>(*action);
      if (SymbolValue* v2 = dynamic_cast<SymbolValue*>(v.valueP.get())) {
        referenceFile(v.new_value.toStringForWriting());
        referenceFile(v2->filename.toStringForWriting());
      }
    } catch (...) {} }
  }
}

String Set::identification() const {
  // an identifying field
  FOR_EACH_CONST(v, data) {
    if (v->fieldP->identifying) {
      return v->toString();
    }
  }
  // otherwise the first non-information field
  FOR_EACH_CONST(v, data) {
    if (!dynamic_pointer_cast<InfoValue>(v)) {
      return v->toString();
    }
  }
  return _("");
}


String Set::typeName() const { return _("set"); }
Version Set::fileVersion() const { return file_version_set; }

void Set::validate(Version file_app_version) {
  Packaged::validate(file_app_version);
  // are the game and stylesheet defined?
  if (!game) {
    throw Error(_ERROR_1_("no game specified",_TYPE_("set")));
  }
  if (!stylesheet) {
    // TODO : Allow user to select a different style
    throw Error(_ERROR_("no stylesheet specified for the set"));
  }
  if (stylesheet->game != game) {
    throw Error(_ERROR_("stylesheet and set refer to different game"));
  }

  bool empty = cards.empty();
  // we want at least one card
  if (empty) cards.push_back(make_intrusive<Card>(*game));
  // build uid map
  buildUIDMap();
  // update scripts
  script_manager->updateAll();
  // don't run update_cards_scripts if the only card in the set was just created
  if (empty) return;
  updateCardsScripts();
}

void reflect_version_check(Reader& handler, const Char* key, intrusive_ptr<Packaged> const& package) {
  if (!package) return;
  Version v = package->version;
  handler.handle(key, v);
  if (package->version < v) {
    queue_message(MESSAGE_WARNING, "This set file is made with a newer version of the '" + package->name() + "' template. Please update the template files.");
  }
}
void reflect_version_check(Writer& handler, const Char* key, intrusive_ptr<Packaged> const& package) {
  if (!package) return;
  handler.handle(key, package->version);
}
void reflect_version_check(GetMember& handler, const Char* key, intrusive_ptr<Packaged> const& package) {}
void reflect_version_check(GetDefaultMember& handler, const Char* key, intrusive_ptr<Packaged> const& package) {}

IMPLEMENT_REFLECTION(Set) {
  REFLECT(game);
  if (game) {
    REFLECT_IF_READING {
      data.init(game->set_fields);
    }
    if (Handler::isReading) {
      REFLECT_NO_SCRIPT(game_version);
    }
    else {
      reflect_version_check(handler, _("game_version"), game);
    }
    WITH_DYNAMIC_ARG(game_for_reading, game.get());
    REFLECT(stylesheet);
    if (Handler::isReading) {
      REFLECT_NO_SCRIPT(stylesheet_version);
    }
    else {
      reflect_version_check(handler, _("stylesheet_version"), stylesheet);
    }
    WITH_DYNAMIC_ARG(stylesheet_for_reading, stylesheet.get());
    REFLECT_N("set_info", data);
    if (stylesheet) {
      REFLECT_N("styling", styling_data);
    }
    // Experimental: save each card to a different file
    //reflect_cards(handler); // people seem to not like this, so move back to normal REFLECT
    REFLECT(cards);
    REFLECT(keywords);
    REFLECT(pack_types);
  }
  reflect_set_info_get_member(handler,data);
  REFLECT_NO_SCRIPT_N("version_control", vcs);
  REFLECT(apprentice_code);
}

// TODO: make this a more generic function to be used elsewhere
template <typename Handler>
void Set::reflect_cards (Handler& handler) {
  REFLECT(cards);
}

template <>
void Set::reflect_cards<Writer> (Writer& handler) {
  // When writing to a directory, we write each card in a separate file.
  // We don't do this in zipfiles because it leads to bloat.
  if (isZipfile()) {
    REFLECT(cards);
  } else {
    set<String> used;
    FOR_EACH(card, cards) {
      // pick a unique filename for this card
      // can't use Package::newFileName, because then we get conflicts with the previous save of the same card
      String filename = _("card ") + normalize_internal_filename(clean_filename(card->identification()));
      String full_name = filename;
      int i = 0;

      while (used.find(full_name) != used.end()) {
        full_name = String(filename) << _(".") << ++i;
      }
      used.insert(full_name);

      // writeFile won't quite work because we'd need
      // include file: card: filename
      // to do that
      auto stream = openOut(full_name);
      Writer writer(*stream, app_version);
      writer.handle(_("card"), card);
      referenceFile(full_name);
      REFLECT_N("include_file", full_name);
    }
  }
}

// ----------------------------------------------------------------------------- : Script utilities

ScriptValueP make_iterator(const Set& set) {
  return make_intrusive<ScriptCollectionIterator<vector<CardP>>>(&set.cards);
}

void mark_dependency_member(const Set& set, const String& name, const Dependency& dep) {
  // is it the card list?
  if (name == _("cards")) {
    set.game->dependent_scripts_cards.add(dep);
    return;
  }
  // is it the keywords?
  if (name == _("keywords")) {
    set.game->dependent_scripts_keywords.add(dep);
    return;
  }
  // is it in the set data?
  mark_dependency_member(set.data, name, dep);
}

// in scripts, set.something is read from the set_info
template <typename Handler>
void reflect_set_info_get_member(Handler&   handler, const IndexMap<FieldP, ValueP>& data) {}
void reflect_set_info_get_member(GetMember& handler, const IndexMap<FieldP, ValueP>& data) {
  REFLECT_NAMELESS(data);
}

int Set::positionOfCard(const CardP& card, const ScriptValueP& order_by, const ScriptValueP& filter) {
  // TODO : Lock the map?
  assert(order_by);
  OrderCacheP& order = order_cache[make_pair(order_by,filter)];
  if (!order) {
    // 1. make a list of the order value for each card
    vector<String> values; values.reserve(cards.size());
    vector<int>    keep;   if(filter) keep.reserve(cards.size());
    FOR_EACH_CONST(c, cards) {
      Context& ctx = getContext(c);
      values.push_back(order_by->eval(ctx)->toString());
      if (filter) {
        keep.push_back(filter->eval(ctx)->toBool());
      }
    }
    #if USE_SCRIPT_PROFILING
      Timer t;
      Profiler prof(t, order_by.get(), _("init order cache"));
    #endif
    // 3. initialize order cache
    order = make_intrusive<OrderCache<CardP>>(cards, values, filter ? &keep : nullptr);
  }
  return order->find(card);
}
int Set::numberOfCards(const ScriptValueP& filter) {
  if (!filter) return (int)cards.size();
  map<ScriptValueP,int>::const_iterator it = filter_cache.find(filter);
  if (it !=filter_cache.end()) {
    return it->second;
  } else {
    int n = 0;
    FOR_EACH_CONST(c, cards) {
      if (filter->eval(getContext(c))->toBool()) ++n;
    }
    filter_cache.insert(make_pair(filter,n));
    return n;
  }
}
void Set::clearOrderCache() {
  order_cache.clear();
  filter_cache.clear();
}

// ----------------------------------------------------------------------------- : SetView

SetView::SetView() {}

SetView::~SetView() {
  if (set) set->actions.removeListener(this);
}

void SetView::setSet(const SetP& newSet) {
  // no longer listening to old set
  if (set) {
    onBeforeChangeSet();
    set->actions.removeListener(this);
  }
  set = newSet;
  // start listening to new set
  if (set) set->actions.addListener(this);
  onChangeSet();
}
