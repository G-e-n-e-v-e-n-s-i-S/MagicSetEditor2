//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/real_point.hpp>
#include <boost/json.hpp>
#include <wx/filename.h>
#include <fstream>
#include <filesystem>

// ----------------------------------------------------------------------------- : Crop Rect Encoding

/// Encode a rect in a std::string
inline static std::string encodeRectInStdString(RealRect rect, int degrees) {
  return "<mse-crop-data>" + std::to_string((int)std::ceil (rect.x)) +
         ";"               + std::to_string((int)std::ceil (rect.y)) +
         ";"               + std::to_string((int)std::floor(rect.width)) +
         ";"               + std::to_string((int)std::floor(rect.height)) +
         ";"               + std::to_string(degrees) +
         "</mse-crop-data>";
}

/// Encode a rect in a wxString
inline static String encodeRectInWxString(RealRect rect, int degrees) {
  return _("<mse-crop-data>") + wxString::Format(wxT("%i"), (int)std::ceil (rect.x)) +
         _(";")               + wxString::Format(wxT("%i"), (int)std::ceil (rect.y)) +
         _(";")               + wxString::Format(wxT("%i"), (int)std::floor(rect.width)) +
         _(";")               + wxString::Format(wxT("%i"), (int)std::floor(rect.height)) +
         _(";")               + wxString::Format(wxT("%i"), degrees) +
         _("</mse-crop-data>");
}

/// Retreive a rect encoded in a string, return true if "<mse-crop-data>" was found
inline static bool decodeRectFromString(const String& rectString, RealRect& rect_out, int& degrees_out) {
  size_t start = rectString.find(_("<mse-crop-data>"));
  if (start == String::npos) return false;
  size_t end = rectString.find(_("</mse-crop-data>"), start + 15);
  if (end == String::npos) return true;
  String string = rectString.substr(start + 15, end - (start + 15));
  if (string.empty()) return true;

  size_t divider = string.find(_(";"));
  if (divider == String::npos) return true;
  if (divider == 0) return true;
  int x;
  if(!string.substr(0, divider).ToInt(&x)) return true;
  string = string.substr(divider + 1);

  divider = string.find(_(";"));
  if (divider == String::npos) return true;
  if (divider == 0) return true;
  int y;
  if(!string.substr(0, divider).ToInt(&y)) return true;
  string = string.substr(divider + 1);

  divider = string.find(_(";"));
  if (divider == String::npos) return true;
  if (divider == 0) return true;
  int width;
  if(!string.substr(0, divider).ToInt(&width)) return true;
  string = string.substr(divider + 1);

  divider = string.find(_(";"));
  if (divider == String::npos) return true;
  if (divider == 0) return true;
  int height;
  if(!string.substr(0, divider).ToInt(&height)) return true;
  string = string.substr(divider + 1);

  if(!string.ToInt(&degrees_out)) return true;

  rect_out = RealRect(x, y, width, height);
  return true;
}

/// Retreive a rect encoded in a string, apply a transformation, then encode it back
inline static String transformEncodedRect(const String& rectString, RectTransform transform, double param_x, double param_y, int mode) {
  RealRect rect(0.0, 0.0, 0.0, 0.0);
  int degrees = 0;
  decodeRectFromString(rectString, rect, degrees);
  if (rect.width > 0.0 && rect.height > 0.0) {
    transform(rect, degrees, param_x, param_y, mode);
    return encodeRectInWxString(rect, degrees);
  }
  return _("");
}

/// Retreive all rects encoded in a string, apply a transformation, then encode them back
inline static String transformAllEncodedRects(const String& rectString, RectTransform transform, double param_x, double param_y, int mode = 0) {
  size_t start = rectString.find(_("<mse-crop-data>"));
  if (start == String::npos) return rectString;
  size_t end = 0;
  String result;
  while (start != String::npos) {
    result = result + rectString.substr(end, start - end);
    end = rectString.find(_("</mse-crop-data>"), start + 15);
    if (end == String::npos) return rectString;
    end += 16;
    result = result + transformEncodedRect(rectString.substr(start, end - start), transform, param_x, param_y, mode);
    start = rectString.find(_("<mse-crop-data>"), end);
  }
  result = result + rectString.substr(end);
  return result;
}

// ----------------------------------------------------------------------------- : File to UTF8 Encoding

inline static const char Base64Alphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";

inline static const std::vector<int> Base64ReverseAlphabet = [] {
  std::vector<int> table(256, -1);
  for (int i = 0; i < 64; i++) table[(uint8_t)Base64Alphabet[i]] = i;
  return table;
}();

/// Encode a file in a string
inline static std::string fileToUTF8(const std::string& filepath) {
  // Load file
  std::ifstream file(filepath, std::ios::binary);
  if (!file)  {
    queue_message(MESSAGE_WARNING, _("Could not find file: ") + String(filepath));
    return "";
  }
  size_t size = std::filesystem::file_size(filepath);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);
  // Base64 encode
  std::string out;
  out.reserve(((size + 2) / 3) * 4);
  int val = 0;
  int valb = -6;
  for (uint8_t c : data) {
    val = (val << 8) | c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(Base64Alphabet[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    out.push_back(Base64Alphabet[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  // Pad
  while (out.size() % 4) {
    out.push_back('=');
  }
  return out;
}

/// Retreive a file encoded in a string, return true if successful
inline static bool UTF8ToFile(const std::string& filepath, std::string& data) {
  // Base64 decode
  std::string out;
  out.reserve(data.size() * 3 / 4);
  int val = 0;
  int valb = -8;
  for (uint8_t c : data) {
    if (c == '=') break; // padding, we're done
    val = (val << 6) | Base64ReverseAlphabet[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  // Save file
  std::ofstream file(filepath, std::ios::binary);
  file.write(out.data(), out.size());
  return true;
}

/// Encode an image in a string
inline static std::string encodeImageInString(const Image& img) {
  String temppath = wxFileName::CreateTempFileName(_("mse")) + _(".png");
  img.SaveFile(temppath);
  std::string s = "<mse-image-data>" + fileToUTF8(temppath.ToStdString()) + "</mse-image-data>";
  wxRemoveFile(temppath);
  wxRemoveFile(temppath.substr(0, temppath.size() - 4));
  return s;
}

/// Retreive an image encoded in a string, return true if "<mse-image-data>" was found
inline static bool decodeImageFromString(const String& string, Image& img_out) {
  size_t first = string.find(_("<mse-image-data>"));
  if (first == String::npos) return false;
  size_t last = string.find(_("</mse-image-data>"), first + 16);
  if (last == String::npos) return true;
  std::string s = string.substr(first + 16, last - (first + 16)).ToStdString();
  if (s.empty()) return true;

  const std::string& temppath = (wxFileName::CreateTempFileName(_("mse")) + _(".png")).ToStdString();
  UTF8ToFile(temppath, s);
  img_out.LoadFile(temppath, wxBITMAP_TYPE_PNG);
  wxRemoveFile(temppath);
  wxRemoveFile(temppath.substr(0, temppath.size() - 4));
  return true;
}

// ----------------------------------------------------------------------------- : Metadata manipulation

inline static String metadata_merge(const Image& img1, const Image& img2, int offset_x1 = 0, int offset_y1 = 0, int offset_x2 = 0, int offset_y2 = 0)
{
  if (img1.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata1 = img1.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION);
    if (offset_x1 != 0 || offset_y1 != 0) metadata1 = transformAllEncodedRects(metadata1, RealRect::translate, offset_x1, offset_y1);
    if (img2.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
      String metadata2 = img2.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION);
      if (offset_x2 != 0 || offset_y2 != 0) metadata2 = transformAllEncodedRects(metadata2, RealRect::translate, offset_x2, offset_y2);
      size_t end1 = metadata1.find(_("</mse-card-data>"));
      size_t start2 = metadata2.find(_("<mse-card-data>"));
      if (end1 != String::npos && start2 != String::npos && end1 > 0 && start2 + 16 < metadata2.size()) {
        metadata1 = metadata1.substr(0, end1 - 1) + "," + metadata2.substr(start2 + 16);
      }
    }
    return metadata1;
  }
  else if (img2.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata2 = img2.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION);
    if (offset_x2 != 0 || offset_y2 != 0) metadata2 = transformAllEncodedRects(metadata2, RealRect::translate, offset_x2, offset_y2);
    return metadata2;
  }
  return _("");
}

inline static boost::json::array metadata_to_json(const String& metadata) {
  size_t start = metadata.find(_("<mse-card-data>"));
  if (start == String::npos) return boost::json::array();
  size_t end = metadata.find(_("</mse-card-data>"), start + 15);
  if (end == String::npos) return boost::json::array();
  String string = metadata.substr(start + 15, end - (start + 15));
  try {
    boost::system::error_code ec;
    boost::json::parse_options options;
    options.allow_invalid_utf8 = true;
    boost::json::value jv = boost::json::parse(string.ToStdString(), ec, {}, options);
    if(ec || !jv.is_array()) {
      queue_message(MESSAGE_ERROR, _ERROR_("json cant parse"));
      return boost::json::array();
    }
    return jv.as_array();
  }
  catch (...) {
    queue_message(MESSAGE_ERROR, _ERROR_("json cant parse"));
    return boost::json::array();
  }
}
