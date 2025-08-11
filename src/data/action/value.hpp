//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file data/action/value.hpp
 *
 *  Actions operating on Values (and derived classes, "*Value")
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/action_stack.hpp>
#include <util/defaultable.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/multiple_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/field/symbol.hpp>
#include <data/field/package_choice.hpp>

class StyleSheet;
class LocalFileName;
DECLARE_POINTER_TYPE(Card);
DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(Value);
DECLARE_POINTER_TYPE(Style);
DECLARE_POINTER_TYPE(TextValue);
DECLARE_POINTER_TYPE(ChoiceValue);
DECLARE_POINTER_TYPE(MultipleChoiceValue);
DECLARE_POINTER_TYPE(ColorValue);
DECLARE_POINTER_TYPE(ImageValue);
DECLARE_POINTER_TYPE(SymbolValue);
DECLARE_POINTER_TYPE(PackageChoiceValue);

// ----------------------------------------------------------------------------- : ValueAction (based class)

/// An Action the changes a Value
class ValueAction : public Action {
public:
  ValueAction(const ValueP& value);
  ~ValueAction();
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  
  /// We know that the value is on the given card, add that information
  void setCard(CardP const& card);
  
  const ValueP valueP; ///< The modified value
  const CardP  card;   ///< The card the value is on, or null if it is not a card value
private:
  wxDateTime old_time_modified;
};

// ----------------------------------------------------------------------------- : Simple

/// Swap the value in a Value object with a new one
inline void swap_value(ChoiceValue&         a, ChoiceValue        ::ValueType& b) { swap(a.value,    b); }
inline void swap_value(ColorValue&          a, ColorValue         ::ValueType& b) { swap(a.value,    b); }
inline void swap_value(ImageValue&          a, ImageValue         ::ValueType& b) { swap(a.filename, b); a.last_update.update(); }
inline void swap_value(SymbolValue&         a, SymbolValue        ::ValueType& b) { swap(a.filename, b); a.last_update.update(); }
inline void swap_value(TextValue&           a, TextValue          ::ValueType& b) { swap(a.value,    b); a.last_update.update(); }
inline void swap_value(PackageChoiceValue&  a, PackageChoiceValue ::ValueType& b) { swap(a.package_name, b); }
inline void swap_value(MultipleChoiceValue& a, MultipleChoiceValue::ValueType& b) {
  swap(a.value,       b.value);
  swap(a.last_change, b.last_change);
}

/// A ValueAction that swaps between old and new values
template <typename T, bool ALLOW_MERGE>
class SimpleValueAction : public ValueAction {
public:
  inline SimpleValueAction(const intrusive_ptr<T>& value, const typename T::ValueType& new_value)
    : ValueAction(value), new_value(new_value)
  {}
  
  void perform(bool to_undo) override {
    ValueAction::perform(to_undo);
    swap_value(static_cast<T&>(*valueP), new_value);
    valueP->onAction(*this, to_undo); // notify value
  }
  
  bool merge(const Action& action) override {
    if (!ALLOW_MERGE) return false;
    TYPE_CASE(action, SimpleValueAction) {
      if (action.valueP == valueP) {
        // adjacent actions on the same value, discard the other one,
        // because it only keeps an intermediate value
        return true;
      }
    }
    return false;
  }
  
  typename T::ValueType new_value;
};

/// Action that updates a Value to a new value
unique_ptr<ValueAction> value_action(const ChoiceValueP&         value, const Defaultable<String>& new_value);
unique_ptr<ValueAction> value_action(const MultipleChoiceValueP& value, const Defaultable<String>& new_value, const String& last_change);
unique_ptr<ValueAction> value_action(const ColorValueP&          value, const Defaultable<Color>&  new_value);
unique_ptr<ValueAction> value_action(const ImageValueP&          value, const LocalFileName&       new_value);
unique_ptr<ValueAction> value_action(const SymbolValueP&         value, const LocalFileName&       new_value);
unique_ptr<ValueAction> value_action(const PackageChoiceValueP&  value, const String&              new_value);

// ----------------------------------------------------------------------------- : Text

/// An action that changes a TextValue
class TextValueAction : public ValueAction {
public:
  TextValueAction(const TextValueP& value, size_t start, size_t end, size_t new_end, const Defaultable<String>& new_value, const String& name);
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  bool merge(const Action& action) override;
  
  inline const String& newValue() const { return new_value(); }
  
  /// The modified selection
  size_t selection_start, selection_end;
private:
  inline TextValue& value() const;
  
  size_t new_selection_end;
  Defaultable<String> new_value;
  String name;
};

/// Action for toggling some formating tag on or off in some range
unique_ptr<TextValueAction> toggle_format_action(const TextValueP& value, const String& tag, size_t start_i, size_t end_i, size_t start, size_t end, const String& action_name);

/// Typing in a TextValue, replace the selection [start...end) with replacement
/** start and end are cursor positions, start_i and end_i are indices*/
unique_ptr<TextValueAction> typing_action(const TextValueP& value, size_t start_i, size_t end_i, size_t start, size_t end, const String& replacement, const String& action_name);

// ----------------------------------------------------------------------------- : Reminder text

/// Toggle reminder text for a keyword on or off
class TextToggleReminderAction : public ValueAction {
public:
  TextToggleReminderAction(const TextValueP& value, size_t pos);
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  
private:
  size_t pos;  ///< Position of "<kw-"
  bool enable; ///< Should the reminder text be turned on or off?
  wxUniChar old; ///< Old value of the <kw- tag
};

// ----------------------------------------------------------------------------- : Replace all

/// A TextValueAction without the start and end stuff
class SimpleTextValueAction : public ValueAction {
public:
  SimpleTextValueAction(const Card* card, const TextValueP& value, const Defaultable<String>& new_value);
  void perform(bool to_undo) override;
  bool merge(const SimpleTextValueAction& action);
private:
  Defaultable<String> new_value;
};

/// An action from "Replace All"; just a bunch of value actions performed in sequence
class ReplaceAllAction : public Action {
public:
  ~ReplaceAllAction();
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  
  vector<SimpleTextValueAction> actions;
};

// ----------------------------------------------------------------------------- : Event

/// Notification that a script caused a value to change
class ScriptValueEvent : public Action {
public:
  inline ScriptValueEvent(const Card* card, const Value* value) : card(card), value(value) {}
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  
  const Card* card;   ///< Card the value is on
  const Value* value; ///< The modified value
};

/// Notification that a script caused a style to change
class ScriptStyleEvent : public Action {
public:
  inline ScriptStyleEvent(const StyleSheet* stylesheet, const Style* style)
    : stylesheet(stylesheet), style(style)
  {}
  
  String getName(bool to_undo) const override;
  void perform(bool to_undo) override;
  
  const StyleSheet* stylesheet; ///< StyleSheet the style is for
  const Style*      style;      ///< The modified style
};


// ----------------------------------------------------------------------------- : Action performer

/// A loose object for performing ValueActions on a certain value.
/** Used to reduce coupling */
class ValueActionPerformer {
public:
  ValueActionPerformer(const ValueP& value, CardP const& card, const SetP& set);
  ~ValueActionPerformer();
  /// Perform an action. The performer takes ownerwhip of the action.
  void addAction(unique_ptr<ValueAction>&& action);
  
  const ValueP value; ///< The value
  Package& getLocalPackage();
private:
  CardP card; ///< Card the value is on (if any)
  SetP set; ///< Set for the actions
};

