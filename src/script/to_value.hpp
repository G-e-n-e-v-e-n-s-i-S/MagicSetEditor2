//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <script/value.hpp>
#include <script/script.hpp>
#include <script/profiler.hpp>
#include <util/reflect.hpp>
#include <util/error.hpp>
#include <util/io/get_member.hpp>
#include <gfx/generated_image.hpp> // we need the dtor of GeneratedImage
#if USE_SCRIPT_PROFILING
  #include <typeinfo>
#endif

// ----------------------------------------------------------------------------- : Overloadable templates

/// Number of items in some collection like object, can be overloaded
template <typename T>
int item_count(const T& v) {
  return -1;
}
/// Return an iterator for some collection, can be overloaded
template <typename T>
ScriptValueP make_iterator(const T& v) {
  return ScriptValueP();
}

/// Whether an iterated map entry actually holds a value. Used to skip unset slots when iterating an IndexMap
template <typename T> inline bool has_map_value(const T&) { return true; }
template <typename T> inline bool has_map_value(const intrusive_ptr<T>& p) { return (bool)p; }

/// Mark a dependency on a member of value, can be overloaded
template <typename T>
void mark_dependency_member(const T& value, const String& name, const Dependency& dep) {}

/// Mark a dependency on an object, can be overloaded
template <typename T>
void mark_dependency_value(const T& value, const Dependency& dep) {}

/// Type name of an object, for error messages
template <typename T> inline String type_name(const T&) {
  return _TYPE_("object");
}
template <typename T> inline String type_name(const intrusive_ptr<T>& p) {
  return type_name(*p.get());
}
template <typename K, typename V> inline String type_name(const pair<K,V>& p) {
  return type_name(p.second); // for maps
}

/// Script code for an object, or anything else we can show
template <typename T> inline String to_code(const T& v) {
  return format_string(_("<%s>"),type_name(v));
}
template <typename T> inline String to_code(const intrusive_ptr<T>& p) {
  return type_name(*p.get());
}

// ----------------------------------------------------------------------------- : Errors

ScriptValueP delay_error(const ScriptError& error);
inline ScriptValueP delay_error(const String& m) {
  return delay_error(ScriptError(m));
}

// ----------------------------------------------------------------------------- : Iterators

// Iterator over a collection
struct ScriptIterator : public ScriptValue, public IntrusiveFromThis<ScriptIterator> {
  ScriptType type() const override;
  String typeName() const override;
  CompareWhat compareAs(String&, void const*&) const override;

  /// Return the next item for this iterator, or ScriptValueP() if there is no such item
  ScriptValueP next(ScriptValueP* key_out = nullptr, int* index_out = nullptr) override = 0;
  ScriptValueP makeIterator() const override;
};

// make an iterator over a range
ScriptValueP rangeIterator(int start, int end);

// ----------------------------------------------------------------------------- : Collections

ScriptValueP to_script(int);

class ScriptCollectionBase : public ScriptValue {
public:
  ScriptType type() const override { return SCRIPT_COLLECTION; }
  String typeName() const override { return _TYPE_("collection"); }
  String toCode() const override;
};

// Iterator over a collection
template <typename Collection>
class ScriptCollectionIterator : public ScriptIterator {
public:
  ScriptCollectionIterator(const Collection* col) : pos(0), col(col) {}
  ScriptValueP next(ScriptValueP* key_out, int* index_out) override {
    if (pos < col->size()) {
      if (key_out) *key_out = to_script((int)pos);
      if (index_out) *index_out = (int)pos;
      return to_script(col->at(pos++));
    } else {
      return ScriptValueP();
    }
  }
private:
  size_t pos;
  const Collection* col;
};

/// Script value containing a collection (vector like)
template <typename Collection>
class ScriptCollection : public ScriptCollectionBase {
public:
  inline ScriptCollection(const Collection* v) : value(v) {}
  String typeName() const override {
    return _TYPE_1_("collection of", type_name(*value->begin()));
  }
  ScriptValueP getIndex(int index) const override {
    if (index >= 0 && index < (int)value->size()) {
      return to_script(value->at(index));
    } else {
      return ScriptValue::getIndex(index);
    }
  }
  ScriptValueP makeIterator() const override {
    return make_intrusive<ScriptCollectionIterator<Collection>>(value);
  }
  int itemCount() const override {
    return (int)value->size();
  }
  /// Collections can be compared by comparing pointers
  CompareWhat compareAs(String&, void const*& compare_ptr) const override {
    compare_ptr = value;
    return COMPARE_AS_POINTER;
  }
private:
  /// Store a pointer to a collection, collections are only ever used for structures owned outside the script
  const Collection* value;
};

// ----------------------------------------------------------------------------- : Collections : maps

template <typename Container>
ScriptValueP get_member(const Container& m, const String& name) {
  auto it = m.find(name);
  if (it != m.end()) {
    return to_script(it->second);
  } else {
    return delay_error(ScriptErrorNoMember(_TYPE_("collection"), name));
  }
}

template <typename K, typename V>
ScriptValueP get_member(const IndexMap<K,V>& m, const String& name) {
  typename IndexMap<K,V>::const_iterator it = m.find(name);
  if (it != m.end()) {
    return to_script(*it);
  } else {
    return delay_error(ScriptErrorNoMember(_TYPE_("collection"), name));
  }
}

// Iterator over a map-like collection, yields key/value pairs
template <typename Collection>
class ScriptMapIterator : public ScriptIterator {
public:
  ScriptMapIterator(const Collection* col) : it(col->begin()), end(col->end()) {}
  ScriptValueP next(ScriptValueP* key_out, int* index_out) override {
    while (it != end && !has_map_value(it->second)) ++it;
    if (it == end) return ScriptValueP();
    if (key_out) *key_out = to_script(it->first);
    ScriptValueP v = to_script(it->second);
    ++it;
    return v;
  }
private:
  typename Collection::const_iterator it, end;
};

template <typename Key, typename Value>
class ScriptIndexMapIterator : public ScriptIterator {
public:
  ScriptIndexMapIterator(const IndexMap<Key,Value>* col) : it(col->begin()), end(col->end()) {}
  ScriptValueP next(ScriptValueP* key_out, int* index_out) override {
    while (it != end && !has_map_value(*it)) ++it; // skip unset slots
    if (it == end) return ScriptValueP();
    if (key_out) *key_out = to_script(get_key_name(*it));
    ScriptValueP v = to_script(*it);
    ++it;
    return v;
  }
private:
  typename IndexMap<Key,Value>::const_iterator it, end;
};

/// Make an iterator for a map-like collection.
template <typename Collection>
inline ScriptValueP make_map_iterator(const Collection* col) {
  return make_intrusive<ScriptMapIterator<Collection>>(col);
}

template <typename Key, typename Value>
inline ScriptValueP make_map_iterator(const IndexMap<Key,Value>* col) {
  return make_intrusive<ScriptIndexMapIterator<Key,Value>>(col);
}

/// Count values that are set. the same count next() would yield.
template <typename Collection>
inline int map_item_count(const Collection& col) {
  return (int)col.size();
}

template <typename Key, typename Value>
inline int map_item_count(const IndexMap<Key,Value>& col) {
  // IndexMap can have holes, so it needs a scan.
  int n = 0;
  for (typename IndexMap<Key,Value>::const_iterator it = col.begin() ; it != col.end() ; ++it) {
    if (has_map_value(*it)) ++n;
  }
  return n;
}

/// Script value containing a map-like collection
template <typename Collection>
class ScriptMap : public ScriptValue {
public:
  inline ScriptMap(const Collection* v) : value(v) {}
  ScriptType type() const override { return SCRIPT_COLLECTION; }
  String typeName() const override {
    for (typename Collection::const_iterator it = value->begin() ; it != value->end() ; ++it) {
      if (has_map_value(*it)) return _TYPE_1_("collection of", type_name(*it));
    }
    return _TYPE_("collection");
  }
  ScriptValueP getMember(const String& name) const override {
    return get_member(*value, name);
  }
  ScriptValueP makeIterator() const override {
    return make_map_iterator(value);
  }
  int itemCount() const override { return map_item_count(*value); }
  ScriptValueP dependencyMember(const String& name, const Dependency& dep) const override {
    mark_dependency_member(*value, name, dep);
    return getMember(name);
  }
  /// Collections can be compared by comparing pointers
  CompareWhat compareAs(String&, void const*& compare_ptr) const override {
    compare_ptr = value;
    return COMPARE_AS_POINTER;
  }
  /// Store a pointer to a collection, collections are only ever used for structures owned outside the script
  const Collection* value;
};

// ----------------------------------------------------------------------------- : Collections : from script

inline void appendFromCollection(class ScriptCustomCollection* dest, const ScriptValueP& coll);

/// Script value containing a custom collection, returned from script functions
class ScriptCustomCollection : public ScriptCollectionBase, public IntrusiveFromThis<ScriptCustomCollection>{
public:
  inline ScriptCustomCollection() {}
  inline ScriptCustomCollection(ScriptValueP a, ScriptValueP b) {
      const int total = a->itemCount() + b->itemCount();
      if (total > 0) value.reserve((size_t)total);
      appendFromCollection(this, a);
      appendFromCollection(this, b);
  }
  ScriptValueP getMember(const String& name) const override;
  ScriptValueP getIndex(int index) const override;
  ScriptValueP makeIterator() const override;
  int itemCount() const override {
    return (int)(value.size() + key_value.size());
  }
  /// Collections can be compared by comparing pointers
  CompareWhat compareAs(String&, void const*& compare_ptr) const override {
    compare_ptr = this;
    return COMPARE_AS_POINTER;
  }

  /// The collection as a list (contains only the values that don't have a key)
  vector<ScriptValueP> value;
  /// The collection as a map (contains only the values that have a key)
  map<String,ScriptValueP> key_value;
};

inline void appendFromCollection(ScriptCustomCollection* dest, const ScriptValueP& coll) {
  ScriptValueP it = coll->makeIterator();
  ScriptValueP keyV;
  int index = -1;
  while (ScriptValueP v = it->next(&keyV, &index)) {
    if (index >= 0) {
      dest->value.push_back(v);
    } else {
      String key = keyV->toString();
      if (dest->key_value.find(key) == dest->key_value.end())
        dest->key_value.emplace(std::move(key), v);
    }
  }
}

DECLARE_POINTER_TYPE(ScriptCustomCollection);

// ----------------------------------------------------------------------------- : Collections : concatenation

/// Script value containing the concatenation of two collections
class ScriptConcatCollection : public ScriptCollectionBase {
public:
  inline ScriptConcatCollection(ScriptValueP a, ScriptValueP b) : a(a), b(b) {}
  ScriptValueP getMember(const String& name) const override;
  ScriptValueP getIndex(int index) const override;
  ScriptValueP makeIterator() const override;
  ScriptValueP getA() { return a; }
  ScriptValueP getB() { return b; }

  int itemCount() const override { return a->itemCount() + b->itemCount(); }
  /// Collections can be compared by comparing pointers
  CompareWhat compareAs(String&, void const*& compare_ptr) const override {
    compare_ptr = this;
    return COMPARE_AS_POINTER;
  }

private:
  ScriptValueP a,b;
  friend class ScriptConcatCollectionIterator;
};

// ----------------------------------------------------------------------------- : Objects

/// Script value containing an object (pointer)
template <typename T>
class ScriptObject : public ScriptValue {
public:
  inline ScriptObject(const T& v) : value(v) {}
  ScriptType type() const override { ScriptValueP d = getDefault(); return d ? d->type() : SCRIPT_OBJECT; }
  String typeName() const override { return type_name(*value); }
  String toString() const override {
    ScriptValueP d = getDefault(); return d ? d->toString() : ScriptValue::toString();
  }
  double toDouble() const override {
    ScriptValueP d = getDefault(); return d ? d->toDouble() : ScriptValue::toDouble();
  }
  int toInt() const override {
    ScriptValueP d = getDefault(); return d ? d->toInt() : ScriptValue::toInt();
  }
  bool toBool() const override {
    ScriptValueP d = getDefault(); return d ? d->toBool() : ScriptValue::toBool();
  }
  Color toColor() const override {
    ScriptValueP d = getDefault(); return d ? d->toColor() : ScriptValue::toColor();
  }
  String toCode() const override {
    ScriptValueP d = getDefault(); return d ? d->toCode() : to_code(*value);
  }
  GeneratedImageP toImage() const override {
    ScriptValueP d = getDefault(); return d ? d->toImage() : ScriptValue::toImage();
  }
  ScriptValueP getMember(const String& name) const override {
    #if USE_SCRIPT_PROFILING
      Timer t;
      Profiler prof(t, (void*)mangled_name(typeid(T)), _("get member of ") + type_name(*value));
    #endif
    // Use reflection to find the member of the object
    GetMember gm(name);
    gm.handle(*value);
    if (gm.result()) return gm.result();
    else {
      // try nameless member
      ScriptValueP d = getDefault();
      if (d) {
        return d->getMember(name);
      } else {
        return ScriptValue::getMember(name);
      }
    }
  }
  ScriptValueP getIndex(int index) const override {
    ScriptValueP d = getDefault(); return d ? d->getIndex(index) : ScriptValue::getIndex(index);
  }
  ScriptValueP dependencyMember(const String& name, const Dependency& dep) const override {
    mark_dependency_member(*value, name, dep);
    return getMember(name);
  }
  void dependencyThis(const Dependency& dep) override {
    mark_dependency_value(*value, dep);
  }
  ScriptValueP makeIterator() const override {
    ScriptValueP it = make_iterator(*value);
    if (it) return it;
    ScriptValueP d = getDefault();
    if (d) return d->makeIterator();
    return ScriptValue::makeIterator();
  }
  int itemCount() const override {
    int i = item_count(*value);
    if (i >= 0) return i;
    ScriptValueP d = getDefault();
    if (d) return d->itemCount();
    return ScriptValue::itemCount();
  }
  /// Objects can be compared by comparing pointers
  CompareWhat compareAs(String& compare_str, void const*& compare_ptr) const override {
    ScriptValueP d = getDefault();
    if (d) {
      return d->compareAs(compare_str, compare_ptr);
    } else {
      compare_ptr = &*value;
      return COMPARE_AS_POINTER;
    }
  }
  /// Get access to the value
  inline T getValue() const { return value; }
private:
  T value; ///< The object
  ScriptValueP getDefault() const {
    GetDefaultMember gdm;
    gdm.handle(*value);
    return gdm.result();
  }
};

// ----------------------------------------------------------------------------- : Default arguments / closure

/// A wrapper around a function that gives default arguments
class ScriptClosure : public ScriptValue {
public:
  ScriptClosure(ScriptValueP fun) : fun(fun) {}

  ScriptType type() const override;
  String typeName() const override;
  ScriptValueP dependencies(Context& ctx, const Dependency& dep) const override;

  /// Add a binding
  void addBinding(Variable v, const ScriptValueP& value);
  /// Is there a binding for the given variable? If so, retrieve it
  ScriptValueP getBinding(Variable v) const;

  /// Try to simplify this closure, returns a value if successful
  ScriptValueP simplify();

  ScriptValueP eval(Context& ctx, bool openScope) const override;

  /// The wrapped function
  ScriptValueP                          fun;
  /// The default argument bindings
  vector<pair<Variable,ScriptValueP>>  bindings;

private:
  /// Apply the bindings in a context
  void applyBindings(Context& ctx) const;
};

/// Turn a script function into a rule, a.k.a. a delayed closure
class ScriptRule : public ScriptValue {
public:
  inline ScriptRule(const ScriptValueP& fun) : fun(fun) {}
  ScriptType type() const override;
  String typeName() const override;
  ScriptValueP eval(Context& ctx, bool openScope) const override;

private:
  ScriptValueP fun;
};

// ----------------------------------------------------------------------------- : Creating

extern ScriptValueP script_nil; ///< The preallocated nil value
extern ScriptValueP script_true; ///< The preallocated true value
extern ScriptValueP script_false; ///< The preallocated false value
extern ScriptValueP dependency_dummy; ///< Dummy value used during dependency analysis

/// Convert a value to a script value
ScriptValueP to_script(int           v);
ScriptValueP to_script(double        v);
ScriptValueP to_script(const String& v);
ScriptValueP to_script(Color         v);
ScriptValueP to_script(wxDateTime    v);

inline ScriptValueP to_script(long v) {
  return to_script((int) v);
}
inline ScriptValueP to_script(bool v) {
  return v ? script_true : script_false;
}

template <typename T>
inline ScriptValueP to_script(const vector<T>* v) {
  return make_intrusive<ScriptCollection<vector<T>>>(v);
}
template <typename K, typename V>
inline ScriptValueP to_script(const map<K,V>* v) {
  return make_intrusive<ScriptMap<map<K,V>>>(v);
}
template <typename K, typename V>
inline ScriptValueP to_script(const unordered_map<K,V>* v) {
  return make_intrusive<ScriptMap<unordered_map<K,V>>>(v);
}
template <typename K, typename V>
inline ScriptValueP to_script(const IndexMap<K,V>* v) {
  return make_intrusive<ScriptMap<IndexMap<K,V>>>(v);
}
template <typename T>
inline ScriptValueP to_script(const intrusive_ptr<T>& v) {
  if (v) {
    return make_intrusive<ScriptObject<intrusive_ptr<T>>>(v);
  } else {
    return script_nil;
  }
}
template <typename T>
inline ScriptValueP to_script(const Defaultable<T>& v) {
  return to_script(v());
}

// ----------------------------------------------------------------------------- : Destructing

/// Convert a value from a script value to a normal value
template <typename T> inline T from_script(const ScriptValueP& value) {
  ScriptObject<T>* o = dynamic_cast<ScriptObject<T>*>(value.get());
  if (!o) {
    throw ScriptErrorConversion(value->typeName(), _TYPE_("object" ));
  }
  return o->getValue();
}
template <> inline ScriptValueP from_script<ScriptValueP>(const ScriptValueP& value) { return value; }
template <> inline String       from_script<String>      (const ScriptValueP& value) { return value->toString(); }
template <> inline int          from_script<int>         (const ScriptValueP& value) { return value->toInt(); }
template <> inline double       from_script<double>      (const ScriptValueP& value) { return value->toDouble(); }
template <> inline bool         from_script<bool>        (const ScriptValueP& value) { return value->toBool(); }
template <> inline Color        from_script<Color>       (const ScriptValueP& value) { return value->toColor(); }
template <> inline wxDateTime   from_script<wxDateTime>  (const ScriptValueP& value) { return value->toDateTime(); }
template <> inline GeneratedImageP from_script<GeneratedImageP>(const ScriptValueP& value) { return value->toImage(); }

