//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/gfx.hpp>
#include <data/symbol_font.hpp>
#include <data/stylesheet.hpp>
#include <util/dynamic_arg.hpp>
#include <util/io/package_manager.hpp>
#include <util/rotation.hpp>
#include <util/error.hpp>
#include <util/window_id.hpp>
#include <render/text/element.hpp> // fot CharInfo
#include <script/image.hpp>

// ----------------------------------------------------------------------------- : SymbolFont

// SymbolFont that is used for SymbolInFonts constructed with the default constructor
DECLARE_DYNAMIC_ARG(SymbolFont*, symbol_font_for_reading);
IMPLEMENT_DYNAMIC_ARG(SymbolFont*, symbol_font_for_reading, nullptr);

SymbolFont::SymbolFont()
  : img_size(12)
  , spacing(1,1)
  , scale_text(false)
  , processed_insert_symbol_menu(nullptr)
{}

SymbolFont::~SymbolFont() {
  delete processed_insert_symbol_menu;
}

String SymbolFont::typeNameStatic() { return _("symbol-font"); }
String SymbolFont::typeName() const { return _("symbol-font"); }
Version SymbolFont::fileVersion() const { return file_version_symbol_font; }

SymbolFontP SymbolFont::byName(const String& name) {
  return package_manager.open<SymbolFont>(
    name.size() > 16 && is_substr(name, name.size() - 16, _(".mse-symbol-font"))
    ? name : name + _(".mse-symbol-font"));
}

IMPLEMENT_REFLECTION(SymbolFont) {
  REFLECT_BASE(Packaged);
  
  REFLECT_N("image_font_size",  img_size);
  REFLECT_N("horizontal_space", spacing.width);
  REFLECT_N("vertical_space",   spacing.height);
  WITH_DYNAMIC_ARG(symbol_font_for_reading, this);
    REFLECT(symbols);
    REFLECT(scale_text);
    REFLECT(insert_symbol_menu);
}

// ----------------------------------------------------------------------------- : SymbolInFont

/// A symbol in a symbol font
class SymbolInFont : public IntrusivePtrBase<SymbolInFont> {
public:
  SymbolInFont();
  
  /// Get a shrunk, zoomed image
  Image getImage(Package& pkg, double size);
  
  /// Get a shrunk, zoomed bitmap
  Bitmap getBitmap(Package& pkg, double size);
  
  /// Get a bitmap with the given size
  Bitmap getBitmap(Package& pkg, wxSize size);
  
  /// Size of a (zoomed) bitmap
  /** This is the size of the resulting image, it does NOT convert back to internal coordinates */
  RealSize size(Package& pkg, double size);
  
  void update(Context& ctx);
  
  String           code;      ///< Code for this symbol
  Scriptable<bool> enabled;    ///< Is this symbol enabled?
  bool             regex;      ///< Should this symbol be matched by a regex?
  int              draw_text;    ///< The index of the captured regex expression to draw, or -1 to not draw text
  Regex            code_regex;  ///< Regex for matching the symbol code
  FontP            text_font;    ///< Font to draw text in.
  Alignment        text_alignment;
  double           text_margin_left;
  double           text_margin_right;
  double           text_margin_top;
  double           text_margin_bottom;
private:
  ScriptableImage  image;      ///< The image for this symbol
  double           img_size;    ///< Font size used by the image
  wxSize           actual_size;  ///< Actual image size, only known after loading the image
  /// Cached bitmaps for different sizes
  map<double, Bitmap> bitmaps;
  
  DECLARE_REFLECTION();
};

SymbolInFont::SymbolInFont()
  : enabled(true)
  , regex(false)
  , draw_text(-1)
  , text_alignment(ALIGN_MIDDLE_CENTER)
  , text_margin_left(0), text_margin_right(0)
  , text_margin_top(0),  text_margin_bottom(0)
  , actual_size(0,0)
{
  assert(symbol_font_for_reading());
  img_size = symbol_font_for_reading()->img_size;
  if (img_size <= 0) img_size = 1;
}

Image SymbolInFont::getImage(Package& pkg, double size) {
  // generate new image
  if (!image.isReady()) {
    throw Error(_("No image specified for symbol with code '") + code + _("' in symbol font."));
  }
  Image img = image.generate(GeneratedImage::Options(0, 0, &pkg));
  actual_size = wxSize(img.GetWidth(), img.GetHeight());
  // scale to match expected size
  Image resampled_image((int) (actual_size.GetWidth()  * size / img_size),
                        (int) (actual_size.GetHeight() * size / img_size), false);
  if (!resampled_image.Ok()) return Image(1,1);
  resample(img, resampled_image);
  return resampled_image;
}
Bitmap SymbolInFont::getBitmap(Package& pkg, double size) {
  // is this bitmap already loaded/generated?
  Bitmap& bmp = bitmaps[size];
  if (!bmp.Ok()) {
    // generate image, convert to bitmap, store for later use
    bmp = Bitmap(getImage(pkg, size));
  }
  return bmp;
}
Bitmap SymbolInFont::getBitmap(Package& pkg, wxSize size) {
  // generate new bitmap
  if (!image.isReady()) {
    throw Error(_("No image specified for symbol with code '") + code + _("' in symbol font."));
  }
  return Bitmap( image.generate(GeneratedImage::Options(size.x, size.y, &pkg, nullptr, ASPECT_BORDER)) );
}

RealSize SymbolInFont::size(Package& pkg, double size) {
  if (actual_size.GetWidth() == 0) {
    // we don't know what size the image will be
    getBitmap(pkg, size);
  }
  return wxSize(actual_size * (int) (size) / (int) (img_size));
}

void SymbolInFont::update(Context& ctx) {
  if (image.update(ctx)) {
    // image has changed, cache is no longer valid
    bitmaps.clear();
  }
  enabled.update(ctx);
  if (text_font)
    text_font->update(ctx);
}
void SymbolFont::update(Context& ctx) const {
  // update all symbol-in-fonts
  FOR_EACH_CONST(sym, symbols) {
    sym->update(ctx);
  }
}

IMPLEMENT_REFLECTION(SymbolInFont) {
  REFLECT(code);
  REFLECT(regex);
  REFLECT_IF_READING
    if (regex)
      code_regex.assign(code);
  REFLECT(draw_text);
  REFLECT(text_font);
  REFLECT(text_alignment);
  REFLECT_COMPAT(<300,"text_align",text_alignment);
  REFLECT(text_margin_left);
  REFLECT(text_margin_right);
  REFLECT(text_margin_top);
  REFLECT(text_margin_bottom);
  REFLECT(image);
  REFLECT(enabled);
  REFLECT_N("image_font_size", img_size);
}

// ----------------------------------------------------------------------------- : SymbolFont : splitting

void SymbolFont::split(const String& text, SplitSymbols& out) const {
  // read a single symbol until we are done with the text
  for (size_t pos = 0 ; pos < text.size() ; ) {
    // check symbol list
    FOR_EACH_CONST(sym, symbols) {
      if (!sym->code.empty() && sym->enabled) {
        if (sym->regex) {
          if (sym->code_regex.empty()) {
            sym->code_regex.assign(sym->code);
          }
          Regex::Results results;
          if (sym->code_regex.matches(results,text.begin() + pos, text.end())
              && results.position() == 0 && results.length() > 0) { //Matches the regex
            if (sym->draw_text >= 0 && sym->draw_text < (int)results.size()) {
              out.push_back(DrawableSymbol(
                      results.str(),
                      results.str(sym->draw_text),
                      *sym));
            } else {
              out.push_back(DrawableSymbol(
                      results.str(),
                      _(""),
                      *sym));
            }
            pos += results.length();
            goto next_symbol;
          }
        } else {
          if (is_substr(text, pos, sym->code)) {
            out.push_back(DrawableSymbol(sym->code, sym->draw_text >= 0 ? sym->code : _(""), *sym));
            pos += sym->code.size();
            goto next_symbol; // continue two levels
          }
        }
      }
    }
    // unknown code, draw single character as text
    //out.push_back(DrawableSymbol(text.substr(pos, 1), _(""), defaultSymbol()));
    pos += 1;
next_symbol:;
  }
}

size_t SymbolFont::recognizePrefix(const String& text, size_t start) const {
  size_t pos;
  for (pos = start ; pos < text.size() ; ) {
    // check symbol list
    FOR_EACH_CONST(sym, symbols) {
      if (!sym->code.empty() && sym->enabled) {
        if (sym->regex) {
          Regex::Results results;
          if (!sym->code_regex.empty() && sym->code_regex.matches(results,text.begin() + pos, text.end())
              && results.position() == 0 && results.length() > 0) { //Matches the regex
            pos += results.length();
            goto next_symbol;
          }
        } else {
          if (is_substr(text, pos, sym->code)) {
            pos += sym->code.size();
            goto next_symbol; // continue two levels
          }
        }
      }
    }
    // failed
    break;
next_symbol:;
  }
  return pos - start;
}

SymbolInFont* SymbolFont::defaultSymbol() const {
  FOR_EACH_CONST(sym, symbols) {
    if (sym->enabled && sym->regex && sym->code_regex.matches(_("0"))) return sym.get();
  }
  return nullptr;
}

// ----------------------------------------------------------------------------- : SymbolFont : drawing

void SymbolFont::draw(RotatedDC& dc, Context& ctx, const RealRect& rect, double scale, const SymbolFontRef& font, const String& text) {
  SplitSymbols symbols;
  update(ctx);
  split(text, symbols);
  draw(dc, rect, scale, font, symbols);
}

void SymbolFont::draw(RotatedDC& dc, RealRect rect, double scale, const SymbolFontRef& font, const SplitSymbols& symbols) {
  vector<Bitmap> bmps;
  vector<RealSize> bmp_sizes;
  vector<RealPoint> bmp_poss;
  double stretch = dc.getStretch();
  double font_size = scale * font.size();
  double font_size_ext = dc.trS(font_size);
  double s_scale = font_size_ext / 15.;
  int count = symbols.size();
  for (int s = 0 ; s < count ; ++s) {
    // 1. find bitmap, size and pos
    auto const& dsym = symbols[s];
    RealSize size = dc.trInvS(symbolSize(font_size_ext, dsym));
    size = RealSize(size.width*stretch, size.height);
    RealRect sym_rect = split_left(rect, size);
    SymbolInFont& sym = *dsym.symbol;
    Bitmap bmp = sym.getBitmap(*this, font_size_ext);
    if (!almost_equal(stretch, 1.0)) {
      bmp = Bitmap(resample(bmp.ConvertToImage(), bmp.GetWidth() * stretch, bmp.GetHeight()));
    }
    RealSize  bmp_size = dc.trInvS(RealSize(bmp));
    RealPoint bmp_pos  = align_in_rect(font.alignment(), bmp_size, sym_rect);
    // 2. draw potential stroke or shadow
    if (font.hasStroke()) {
      int blur_radius = lround(font.stroke_blur() * s_scale);
      int stroke_radius = lround(font.stroke_radius() * s_scale);
      Image s_img = bmp.ConvertToImage();
      s_img = make_stroke_image(s_img, font.stroke_color(), stroke_radius, blur_radius);
      RealSize  s_size = dc.trInvS(RealSize(s_img));
      RealPoint s_pos(bmp_pos.x - (s_size.width - bmp_size.width)/2, bmp_pos.y - (s_size.height - bmp_size.height)/2);
      dc.DrawImage(s_img, s_pos);
    }
    else if (font.hasShadow()) {
      int blur_radius = lround(font.shadow_blur() * s_scale);
      Image s_img = bmp.ConvertToImage();
      s_img = make_stroke_image(s_img, font.shadow_color(), 0, blur_radius);
      RealSize  s_size = dc.trInvS(RealSize(s_img));
      RealPoint s_pos(bmp_pos.x - (s_size.width - bmp_size.width)/2, bmp_pos.y - (s_size.height - bmp_size.height)/2);
      RealSize s_displacement = dc.trInvS(RealSize(font.shadow_displacement_x, font.shadow_displacement_y) * s_scale);
      dc.DrawImage(s_img, s_pos + s_displacement);
    }
    bmps.push_back(std::move(bmp));
    bmp_sizes.push_back(bmp_size);
    bmp_poss.push_back(bmp_pos);
  }
  for (int s = 0 ; s < count ; ++s) {
    // 3. restart loop to draw bitmap
    auto const& dsym = symbols[s];
    const String& text = dsym.draw_text;
    SymbolInFont& sym = *dsym.symbol;
    Bitmap& bmp = bmps[s];
    RealSize& bmp_size = bmp_sizes[s];
    RealPoint& bmp_pos = bmp_poss[s];
    dc.DrawBitmap(bmp, bmp_pos);
    // 4. draw text
    if (text.empty() || !sym.text_font) continue;
    // only use the bitmap rectangle
    RealRect sym_rect = RealRect(bmp_pos, bmp_size);
    // subtract margins from size
    sym_rect.x      += font_size * sym.text_margin_left;
    sym_rect.y      += font_size * sym.text_margin_top;
    sym_rect.width  -= font_size * (sym.text_margin_left + sym.text_margin_right);
    sym_rect.height -= font_size * (sym.text_margin_top  + sym.text_margin_bottom);
    // setup text, shrink it
    double text_size = font_size * sym.text_font->size;
    double text_stretch = 1.0;
    RealSize ts;
    while (true) {
      if (text_size <= 0)
        goto continue_outer; // text too small
      dc.SetFont(*sym.text_font, text_size / sym.text_font->size);
      ts = dc.GetTextExtent(text);
      if (ts.height <= sym_rect.height) {
        if (ts.width <= sym_rect.width) {
          break; // text fits
        } else if (ts.width * sym.text_font->max_stretch <= sym_rect.width) {
          text_stretch = sym_rect.width / ts.width;
          ts.width = sym_rect.width; // for alignment
          break;
        }
      }
      // text doesn't fit
      text_size -= dc.getFontSizeStep();
    }
    {
    // align text
    RealPoint text_pos = align_in_rect(sym.text_alignment, ts, sym_rect);
    // draw text
    dc.DrawTextWithShadowOrStroke(text, *sym.text_font, text_pos, font_size, text_stretch);
    }
    continue_outer:;
  }
}

Image SymbolFont::getImage(double font_size, const DrawableSymbol& sym) {
  if (!sym.symbol) return Image(1,1);
  if (sym.draw_text.empty() || !sym.symbol->text_font) return sym.symbol->getImage(*this, font_size);
  // with text
  Bitmap bmp(sym.symbol->getImage(*this, font_size));
  // memory dc to work with
  wxMemoryDC dc;
  dc.SelectObject(bmp);
  RealRect sym_rect(0,0,bmp.GetWidth(),bmp.GetHeight());
  RotatedDC rdc(dc, 0, sym_rect, 1, QUALITY_AA);
  // subtract margins from size
  sym_rect.x      += font_size * sym.symbol->text_margin_left;
  sym_rect.y      += font_size * sym.symbol->text_margin_top;
  sym_rect.width  -= font_size * (sym.symbol->text_margin_left + sym.symbol->text_margin_right);
  sym_rect.height -= font_size * (sym.symbol->text_margin_top  + sym.symbol->text_margin_bottom);
  // setup text, shrink it
  double size = font_size * sym.symbol->text_font->size;
  double text_stretch = 1.0;
  RealSize ts;
  while (true) {
    if (size <= 0) return sym.symbol->getImage(*this, font_size); // text too small
    rdc.SetFont(*sym.symbol->text_font, size / sym.symbol->text_font->size);
    ts = rdc.GetTextExtent(sym.draw_text);
    if (ts.height <= sym_rect.height) {
      if (ts.width <= sym_rect.width) {
        break; // text fits
      } else if (ts.width * sym.symbol->text_font->max_stretch <= sym_rect.width) {
        text_stretch = sym_rect.width / ts.width;
        ts.width = sym_rect.width; // for alignment
        break;
      }
    }
    // text doesn't fit
    size -= rdc.getFontSizeStep();
  }
  // align text
  RealPoint text_pos = align_in_rect(sym.symbol->text_alignment, ts, sym_rect);
  // draw text
  rdc.DrawTextWithShadowOrStroke(sym.draw_text, *sym.symbol->text_font, text_pos, font_size, text_stretch);
  // done
  dc.SelectObject(wxNullBitmap);
  return bmp.ConvertToImage();
}

// ----------------------------------------------------------------------------- : SymbolFont : sizes

void SymbolFont::getCharInfo(RotatedDC& dc, Context& ctx, double font_size, const String& text, vector<CharInfo>& out) {
  SplitSymbols symbols;
  update(ctx);
  split(text, symbols);
  getCharInfo(dc, font_size, symbols, out);
}

void SymbolFont::getCharInfo(RotatedDC& dc, double font_size, const SplitSymbols& text, vector<CharInfo>& out) {
  FOR_EACH_CONST(sym, text) {
    size_t count = sym.text.size();
    RealSize size = dc.trInvS(symbolSize(dc.trS(font_size), sym));
    size.width /= count; // divide into count parts
    for (size_t i = 0 ; i < count ; ++i) {
      out.push_back(CharInfo(size, i == count - 1 ? LineBreak::MAYBE : LineBreak::NO));
    }
  }
}

RealSize SymbolFont::symbolSize(double font_size, const DrawableSymbol& sym) {
  if (sym.symbol) {
    return add_diagonal(sym.symbol->size(*this, font_size), spacingSize(font_size));
  } else {
    return defaultSymbolSize(font_size);
  }
}

RealSize SymbolFont::defaultSymbolSize(double font_size) {
  SymbolInFont* def = defaultSymbol();
  if (def) {
    return add_diagonal(def->size(*this, font_size), spacingSize(font_size));
  } else {
    return add_diagonal(RealSize(1,1), spacingSize(font_size));
  }
}

RealSize SymbolFont::spacingSize(double font_size) {
  return RealSize(spacing.width * font_size / 15.0, spacing.height * font_size / 15.0);
}

// ----------------------------------------------------------------------------- : InsertSymbolMenu

wxMenu* SymbolFont::insertSymbolMenu(Context& ctx) {
  if (!processed_insert_symbol_menu && insert_symbol_menu) {
    update(ctx);
    // Make menu
    processed_insert_symbol_menu = insert_symbol_menu->makeMenu(ID_INSERT_SYMBOL_MENU_MIN, *this);
  }
  return processed_insert_symbol_menu;
}

String SymbolFont::insertSymbolCode(int menu_id) const {
  // find item
  if (insert_symbol_menu) {
    return insert_symbol_menu->getCode(menu_id - ID_INSERT_SYMBOL_MENU_MIN, *this);
  } else {
    return _("");
  }
}


InsertSymbolMenu::InsertSymbolMenu()
  : type(Type::CODE)
{}

int InsertSymbolMenu::size() const {
  if (type == Type::CODE || type == Type::CUSTOM) {
    return 1;
  } else if (type == Type::SUBMENU) {
    int count = 0;
    FOR_EACH_CONST(i, items) {
      count += i->size();
    }
    return count;
  } else {
    return 0;
  }
}
String InsertSymbolMenu::getCode(int id, const SymbolFont& font) const {
  if (type == Type::SUBMENU) {
    FOR_EACH_CONST(i, items) {
      int id2 = id - i->size();
      if (id2 < 0) {
        return i->getCode(id, font);
      }
      id = id2;
    }
  } else if (id == 0 && type == Type::CODE) {
    return name;
  } else if (id == 0 && type == Type::CUSTOM) {
    String title = this->label.get();
    title.Replace(_("&"), _("")); // remove underlines
    return wxGetTextFromUser(prompt.get(), title);
  }
  return String();
}

wxMenu* InsertSymbolMenu::makeMenu(int id, SymbolFont& font) const {
  if (type == Type::SUBMENU) {
    wxMenu* menu = new wxMenu();
    FOR_EACH_CONST(i, items) {
      menu->Append(i->makeMenuItem(menu, id, font));
      id += i->size();
    }
    return menu;
  }
  return nullptr;
}

wxMenuItem* InsertSymbolMenu::makeMenuItem(wxMenu* parent, int first_id, SymbolFont& font) const {
  String label = this->label.get();
  // ensure that we are not defining an accelerator...
  // everything after a tab is considered to be an accelerator by wxMenuItem
  int accel_pos = label.find_last_of('\t');
  if (accel_pos != label.npos && accel_pos > 0) {
    String accel = label.substr(accel_pos+1);
#ifdef __WXMSW__
    // if there is a + or - in the accelerator, replace the tab with spaces (simply adding a space does not work)
    if (accel.Contains("+") || accel.Contains("-")) {
      label = label.substr(0, accel_pos) + _("      ") + accel;
    }
    // otherwise simply add a space after the tab if there isn't one
    else {
      label.Replace(_("\t "), _("\t"));
      label.Replace(_("\t"), _("\t "));
    }
#else
    label = label.substr(0, accel_pos) + _("      ") + accel; // replace the tab with spaces
#endif
  }
  if (type == Type::SUBMENU) {
    wxMenuItem* item = new wxMenuItem(parent, wxID_ANY, label,
                                      _(""), wxITEM_NORMAL,
                                      makeMenu(first_id, font));
    item->SetBitmap(wxNullBitmap);
    return item;
  } else if (type == Type::LINE) {
    wxMenuItem* item = new wxMenuItem(parent, wxID_SEPARATOR);
    return item;
  } else {
    wxMenuItem* item = new wxMenuItem(parent, first_id, label);
    // Generate bitmap for use on this item
    SymbolInFont* symbol = nullptr;
    if (type == Type::CUSTOM) {
      symbol = font.defaultSymbol();
    } else {
      FOR_EACH(sym, font.symbols) {
        if (!sym->code.empty() && sym->enabled && name == sym->code) { 
          symbol = sym.get();
          break;
        }
      }
    }
    if (symbol) {
      item->SetBitmap(symbol->getBitmap(font, wxSize(16,16)));
    } else {
      item->SetBitmap(wxNullBitmap);
    }
    return item;
  }
}


IMPLEMENT_REFLECTION_ENUM(InsertSymbolMenu::Type) {
  VALUE_N("code",    InsertSymbolMenu::Type::CODE);
  VALUE_N("custom",  InsertSymbolMenu::Type::CUSTOM);
  VALUE_N("line",    InsertSymbolMenu::Type::LINE);
  VALUE_N("submenu", InsertSymbolMenu::Type::SUBMENU);
}

IMPLEMENT_REFLECTION(InsertSymbolMenu) {
  REFLECT_IF_READING_SINGLE_VALUE_AND(items.empty()) {
    REFLECT_NAMELESS(name);
  } else {
    // complex values are groups
    REFLECT(type);
    REFLECT(name);
    REFLECT_LOCALIZED(label);
    REFLECT_LOCALIZED(prompt);
    REFLECT(items);
    if (Handler::isReading && !items.empty()) type = Type::SUBMENU;
  }
}

void after_reading(InsertSymbolMenu& m, Version ver) {
  assert(symbol_font_for_reading());
  if (m.label.empty()) m.label.default_ = tr(*symbol_font_for_reading(), _("menu_item"), m.name, capitalize);
  if (m.prompt.empty()) m.prompt.default_ = tr(*symbol_font_for_reading(), _("message"), m.name, capitalize_sentence);
}

// ----------------------------------------------------------------------------- : SymbolFontRef

SymbolFontRef::SymbolFontRef()
  : size(12)
  , scale_down_to(1)
  , underline(false)
  , strikethrough(false)
  , alignment(ALIGN_MIDDLE_CENTER)
  , shadow_color(Color(0,0,0))
  , shadow_displacement_x(0)
  , shadow_displacement_y(0)
  , shadow_blur(0)
  , stroke_color(Color(0,0,0))
  , stroke_radius(0)
  , stroke_blur(0)
{}

bool SymbolFontRef::valid() const {
  return !!font;
}

bool SymbolFontRef::update(Context& ctx) {
  bool changes = false;
  if (name.update(ctx)) {
    // font name changed, load another font
    loadFont(ctx);
    changes = true;
  } else if (!font) {
    loadFont(ctx);
  }
  changes |= size                 .update(ctx);
  changes |= underline            .update(ctx);
  changes |= strikethrough        .update(ctx);
  changes |= alignment            .update(ctx);
  changes |= shadow_color         .update(ctx);
  changes |= shadow_displacement_x.update(ctx);
  changes |= shadow_displacement_y.update(ctx);
  changes |= shadow_blur          .update(ctx);
  changes |= stroke_color         .update(ctx);
  changes |= stroke_radius        .update(ctx);
  changes |= stroke_blur          .update(ctx);
  return changes;
}
void SymbolFontRef::initDependencies(Context& ctx, const Dependency& dep) const {
  name                 .initDependencies(ctx, dep);
  size                 .initDependencies(ctx, dep);
  underline            .initDependencies(ctx, dep);
  strikethrough        .initDependencies(ctx, dep);
  alignment            .initDependencies(ctx, dep);
  shadow_color         .initDependencies(ctx, dep);
  shadow_displacement_x.initDependencies(ctx, dep);
  shadow_displacement_y.initDependencies(ctx, dep);
  shadow_blur          .initDependencies(ctx, dep);
  stroke_color         .initDependencies(ctx, dep);
  stroke_radius        .initDependencies(ctx, dep);
  stroke_blur          .initDependencies(ctx, dep);
}

void SymbolFontRef::loadFont(Context& ctx) {
  if (name().empty()) {
    font = SymbolFontP();
  } else {
    font = SymbolFont::byName(name);
    if (starts_with(name(),_("/:NO-WARN-DEP:"))) {
      // ensure the dependency on the font is present in the stylesheet this ref is in
      // Getting this stylesheet from the context is a bit of a hack
      // If the name starts with a ' ', no dependency is needed;
      //  this is for packages selected with a PackageChoiceList
      StyleSheetP stylesheet = from_script<StyleSheetP>(ctx.getVariable(_("stylesheet")));
      stylesheet->requireDependency(font.get());
    }
  }
}

IMPLEMENT_REFLECTION(SymbolFontRef) {
  REFLECT(name);
  REFLECT(size);
  REFLECT(scale_down_to);
  REFLECT(underline);
  REFLECT(strikethrough);
  REFLECT(alignment);
  REFLECT(shadow_color);
  REFLECT(shadow_displacement_x);
  REFLECT(shadow_displacement_y);
  REFLECT(shadow_blur);
  REFLECT(stroke_color);
  REFLECT(stroke_radius);
  REFLECT(stroke_blur);
}
