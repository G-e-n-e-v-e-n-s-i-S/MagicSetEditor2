//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/format/clipboard.hpp>
#include <data/format/formats.hpp>
#include <data/card.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/keyword.hpp>
#include <util/io/package.hpp>
#include <script/scriptable.hpp>
#include <wx/filename.h>
#include <wx/sstream.h>

// ----------------------------------------------------------------------------- : Clipboard serialization

/// Serialize an object to a string, clipboard_package will be set to the given package.
template <typename T>
String serialize_for_clipboard(Package& package, T& object) {
  wxStringOutputStream stream;
  Writer writer(stream, file_version_clipboard);
  WITH_DYNAMIC_ARG(clipboard_package, &package);
    writer.handle(object);
  return stream.GetString();
}

template <typename T>
void deserialize_from_clipboard(T& object, Package& package, const String& data) {
  wxStringInputStream stream = {data};
  Reader reader(stream, nullptr, _("clipboard"));
  WITH_DYNAMIC_ARG(clipboard_package, &package);
    reader.handle_greedy(object);
}

// ----------------------------------------------------------------------------- : CardDataObject

/// A wrapped cards for storing on the clipboard
struct WrappedCards {
  Game*         expected_game;
  String        game_name;
  String        id;
  vector<CardP> cards;
  
  DECLARE_REFLECTION();
};

IMPLEMENT_REFLECTION(WrappedCards) {
  REFLECT(game_name);
  REFLECT(id);
  if (game_name == expected_game->name()) {
    WITH_DYNAMIC_ARG(game_for_reading, expected_game);
    REFLECT(cards);
  }
}


wxDataFormat CardsDataObject::format(wxString("application/x-mse-cards"));

CardsDataObject::CardsDataObject(const SetP& set, const String& id, const vector<CardP>& cards)
  : wxCustomDataObject(format)
{
  // set the stylesheet, so when deserializing we know whos style options we are reading
  vector<bool> has_styling;
  for (size_t i = 0 ; i < cards.size() ; ++i) {
    has_styling.push_back(cards[i]->has_styling && !cards[i]->stylesheet);
    if (has_styling[i]) {
      cards[i]->stylesheet = set->stylesheet;
    }
  }

  // store as raw bytes
  WrappedCards data = { set->game.get(), set->game->name(), id, cards };
  String serialized = serialize_for_clipboard(*set, data);
  wxScopedCharBuffer utf8 = serialized.utf8_str();
  buffer.assign(utf8.data(), utf8.length());
  SetData(buffer.size(), buffer.data());

  // restore styling
  for (size_t i = 0 ; i < cards.size() ; ++i) {
    if (has_styling[i]) {
      cards[i]->stylesheet = StyleSheetP();
    }
  }
  SetFormat(format);
}

CardsDataObject::CardsDataObject()
  : wxCustomDataObject(format)
{}

bool CardsDataObject::getCards(const SetP& set, const String& id, vector<CardP>& out) {
  size_t size = GetSize();
  if (size == 0) return false;
  const void* raw = GetData();
  if (!raw) return false;

  String text(wxString::FromUTF8(static_cast<const char*>(raw), size));
  WrappedCards data = { set->game.get(), set->game->name() };
  try {
    deserialize_from_clipboard(data, *set, text);
  } catch (...) {
    queue_message(MESSAGE_ERROR, _("DEBUG: Card deserialization failed"));
    return false;
  }

  if (data.cards.empty()) return false;
  if (!id.empty() && data.id == id) return false;
  if (data.game_name != set->game->name()) return false;
  // Cards are from the same game
  out = data.cards;
  return true;
}

// ----------------------------------------------------------------------------- : KeywordDataObject

/// A wrapped keyword for storing on the clipboard
struct WrappedKeyword {
  Game*    expected_game;
  String   game_name;
  KeywordP keyword;
  
  DECLARE_REFLECTION();
};

IMPLEMENT_REFLECTION(WrappedKeyword) {
  REFLECT(game_name);
  if (game_name == expected_game->name()) {
    WITH_DYNAMIC_ARG(game_for_reading, expected_game);
    REFLECT(keyword);
  }
}


wxDataFormat KeywordDataObject::format(wxString("application/x-mse-keyword"));

KeywordDataObject::KeywordDataObject(const SetP& set, const KeywordP& keyword) {
  WrappedKeyword data = { set->game.get(), set->game->name(), keyword };
  SetText(serialize_for_clipboard(*set, data));
  SetFormat(format);
}

KeywordDataObject::KeywordDataObject() {
  SetFormat(format);
}

KeywordP KeywordDataObject::getKeyword(const SetP& set) {
  KeywordP keyword(new Keyword());
  WrappedKeyword data = { set->game.get(), set->game->name(), keyword};
  deserialize_from_clipboard(data, *set, GetText());
  if (data.game_name != set->game->name()) return KeywordP(); // Keyword is from a different game
  else                                     return keyword;
}

// ----------------------------------------------------------------------------- : Card on clipboard

CardsOnClipboard::CardsOnClipboard(const SetP& set, const String id, vector<CardP> cards) {
  wxBusyCursor busy;
  // Add other faces if necessary
  if (settings.card_dfc_copy) {
    for (size_t i = 0; i < cards.size(); ++i) {
      CardP card = cards[i]; // copy, not reference, since insert() can reallocate the vector
      CardP other_face = card->getBackFaceCard(*set);
      if (other_face) {
        if (find(cards.begin(), cards.end(), other_face) == cards.end()) {
          // back face goes right after the card
          cards.insert(cards.begin() + i + 1, other_face);
          ++i;
        }
        continue;
      }
      other_face = card->getFrontFaceCard(*set);
      if (other_face) {
        if (find(cards.begin(), cards.end(), other_face) == cards.end()) {
          // front face goes right before the card
          cards.insert(cards.begin() + i, other_face);
          ++i;
        }
        continue;
      }
    }
  }
  // Conversion to image file
  if (cards.size() < 6) {
    Image img = export_image(set, cards, 2, ExportImageMode::CLIPBOARD);
    //wxFileDataObject* fileData = new wxFileDataObject(); // needed for pasting on desktop, but slow
    //String temp_path = wxFileName::CreateTempFileName(_("mse")) + _(".png");
    //img.SaveFile(temp_path, wxBITMAP_TYPE_PNG);
    //fileData->AddFile(temp_path);
    //Add(fileData);
    wxImageDataObject* imgData = new wxImageDataObject(); // needed for metadata
    imgData->SetImage(img);
    Add(imgData);
    wxBitmapDataObject* bmpData = new wxBitmapDataObject(); // needed for pasting in MSPaint
    bmpData->SetBitmap(Bitmap(img));
    Add(bmpData);
  }
  // Conversion to serialized card format (preferred)
  Add(new CardsDataObject(set, id, cards), true);
}
