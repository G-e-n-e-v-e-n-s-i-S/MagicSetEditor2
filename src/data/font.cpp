//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/font.hpp>
#include <wx/stdpaths.h>
#include <wx/dir.h>
#include <wx/font.h>

// ----------------------------------------------------------------------------- : Font

FontRef::FontRef()
  : name()
  , size(1)
  , underline(false)
  , strikethrough(false)
  , scale_down_to(100000)
  , max_stretch(1.0)
  , color(Color(0,0,0))
  , shadow_color(Color(0,0,0))
  , shadow_displacement_x(0)
  , shadow_displacement_y(0)
  , shadow_blur(0)
  , stroke_color(Color(0,0,0))
  , stroke_radius(0)
  , stroke_blur(0)
  , separator_color(Color(0,0,0,128))
  , flags(FONT_NORMAL)
{}

bool FontRef::PreloadResourceFonts(bool recursive) {
#if wxUSE_PRIVATE_FONTS
  String pathSeparator(wxFileName::GetPathSeparator());
  String appPath(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath());
  wxDir appDir(appPath);
  if (!appDir.IsOpened()) return true;

  bool preloadHadErrors = false;
  wxString folder;
  bool cont = appDir.GetFirst(&folder, _(""), wxDIR_DIRS);
  while (cont)
  {
    if (folder.Lower().Contains("fonts")) {
      String folderPath = appPath + pathSeparator + folder + pathSeparator;
      
      // tally fonts
      vector<String> fontFilePaths;
      TallyResourceFonts(folderPath, fontFilePaths, recursive);

      // load fonts
      for (const String& fontFilePath : fontFilePaths) {
        if (!wxFont::AddPrivateFont(fontFilePath)) {
          preloadHadErrors = true;
        }
      }
    }
    cont = appDir.GetNext(&folder);
  }

  return preloadHadErrors;

#endif // wxUSE_PRIVATE_FONTS
  return false;
}

void FontRef::TallyResourceFonts(String fontsDirectoryPath, vector<String>& fontFilePaths, bool recursive) {
  wxDir fontsDirectory(fontsDirectoryPath);
  String fontFileName = _("");
  bool hasNext = fontsDirectory.GetFirst(&fontFileName);
  while (hasNext) {
    String fontFilePath = fontsDirectoryPath + fontFileName;
    if (wxDirExists(fontFilePath)) {
      if (recursive) {
        TallyResourceFonts(fontFilePath + wxFileName::GetPathSeparator(), fontFilePaths, true);
      }
    }
    else if (fontFilePath.EndsWith(_(".ttf")) || fontFilePath.EndsWith(_(".otf"))) {
      fontFilePaths.push_back(fontFilePath);
    }
    hasNext = fontsDirectory.GetNext(&fontFileName);
  }
}

bool FontRef::update(Context& ctx) {
  bool changes = false;
  changes |= name                 .update(ctx);
  changes |= italic_name          .update(ctx);
  changes |= size                 .update(ctx);
  changes |= weight               .update(ctx);
  changes |= style                .update(ctx);
  changes |= underline            .update(ctx);
  changes |= strikethrough        .update(ctx);
  changes |= color                .update(ctx);
  changes |= shadow_color         .update(ctx);
  changes |= shadow_displacement_x.update(ctx);
  changes |= shadow_displacement_y.update(ctx);
  changes |= shadow_blur          .update(ctx);
  changes |= stroke_color         .update(ctx);
  changes |= stroke_radius        .update(ctx);
  changes |= stroke_blur          .update(ctx);
  flags = (flags & ~FONT_BOLD & ~FONT_ITALIC)
        | (weight() == _("bold")   ? FONT_BOLD   : FONT_NORMAL)
        | (style()  == _("italic") ? FONT_ITALIC : FONT_NORMAL);
  return changes;
}
void FontRef::initDependencies(Context& ctx, const Dependency& dep) const {
  name                 .initDependencies(ctx, dep);
  italic_name          .initDependencies(ctx, dep);
  size                 .initDependencies(ctx, dep);
  weight               .initDependencies(ctx, dep);
  style                .initDependencies(ctx, dep);
  underline            .initDependencies(ctx, dep);
  strikethrough        .initDependencies(ctx, dep);
  color                .initDependencies(ctx, dep);
  shadow_color         .initDependencies(ctx, dep);
  shadow_displacement_x.initDependencies(ctx, dep);
  shadow_displacement_y.initDependencies(ctx, dep);
  shadow_blur          .initDependencies(ctx, dep);
  stroke_color         .initDependencies(ctx, dep);
  stroke_blur          .initDependencies(ctx, dep);
  stroke_radius        .initDependencies(ctx, dep);
}

FontRefP FontRef::make(int add_flags, bool add_underline, bool add_strikethrough, String const* other_family, Color const* other_color, double const* other_size) const {
  FontRefP f(new FontRef(*this));
  f->flags |= add_flags;
  if (add_flags & FONT_CODE_STRING) {
    f->color = Color(0,0,100);
  }
  if (add_flags & FONT_CODE) {
    f->color = Color(128,0,0);
  }
  if (add_flags & FONT_CODE_KW) {
    f->color = Color(158,100,0);
    f->flags |= FONT_BOLD;
  }
  if (add_flags & FONT_SOFT) {
    f->color = f->separator_color;
    f->shadow_displacement_x = 0; // no shadow
    f->shadow_displacement_y = 0; // no shadow
  }
  if (add_underline) {
    f->underline = true;
  }
  if (add_strikethrough) {
    f->strikethrough = true;
  }
  if (other_color) {
    f->color = *other_color;
  }
  if (other_size) {
    f->size = *other_size;
  }
  if (other_family && !other_family->empty()) {
    f->name = *other_family;
  }
  return f;
}

static const String BOLD_STRING   = _(" Bold");

// Construct a wxFont without going through the legacy 6-argument wxFont(...)
// That constructor silently treats a pointSize of exactly 70 as a request for wxDEFAULT
static wxFont make_font(int point_size, wxFontFamily family, wxFontStyle style, wxFontWeight weight, bool underlined, const String& face) {
  wxFont font;
  font.SetFamily(family);
  font.SetStyle(style);
  font.SetWeight(weight);
  font.SetUnderlined(underlined);
  if (!face.empty()) font.SetFaceName(face);
  font.SetPointSize(point_size);
  return font;
}

wxFont FontRef::toWxFont(double scale) const {
  int size_i = to_int(scale * size);
  wxFontWeight weight_i = flags & FONT_BOLD   ? wxFONTWEIGHT_BOLD  : wxFONTWEIGHT_NORMAL;
  wxFontStyle style_i  = flags & FONT_ITALIC ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
  // make font
  wxFont font;

  if (flags & FONT_CODE) {
    if (size_i < 2) {
      font = make_font(wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, weight_i, underline(), _("Courier New"));
      if (strikethrough()) font.MakeStrikethrough();
      return font;
    } else {
      font = make_font(size_i, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, weight_i, underline(), _("Courier New"));
    }
  } else if (name().empty()) {
    font = *wxNORMAL_FONT;
    font.SetPointSize(size > 1 ? size_i : int(scale * font.GetPointSize()));
    if (strikethrough()) font.MakeStrikethrough();
    return font;
  } else if (!(flags & FONT_FROM_TAG) && (flags & FONT_ITALIC) && !italic_name().empty()) {
    font = make_font(size_i, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, weight_i, underline(), italic_name());
  } else {
    String familyName = name();
    if(familyName.EndsWith(BOLD_STRING)) {
      familyName = familyName.Left(familyName.length() - BOLD_STRING.length());
      weight_i = wxFONTWEIGHT_BOLD;
    }
    font = make_font(size_i, wxFONTFAMILY_DEFAULT, style_i, weight_i, underline(), familyName);
  }
  // fix size
#if defined(__WXMSW__) || defined(__WXGTK__)
  // make it independent of screen dpi, always use 96 dpi
  // TODO: do something more sensible, and more portable
  font.SetPixelSize(wxSize(0, -(int)(scale*size*96.0/72.0 + 0.5) ));
#endif
  if (strikethrough()) font.MakeStrikethrough();
  return font;
}

IMPLEMENT_REFLECTION_NO_SCRIPT(FontRef) {
  REFLECT(name);
  REFLECT(size);
  REFLECT(weight);
  REFLECT(style);
  REFLECT(underline);
  REFLECT(strikethrough);
  REFLECT(italic_name);
  REFLECT(color);
  REFLECT(scale_down_to);
  REFLECT(max_stretch);
  REFLECT(shadow_color);
  REFLECT(shadow_displacement_x);
  REFLECT(shadow_displacement_y);
  REFLECT(shadow_blur);
  REFLECT(stroke_color);
  REFLECT(stroke_radius);
  REFLECT(stroke_blur);
  REFLECT(separator_color);
}
