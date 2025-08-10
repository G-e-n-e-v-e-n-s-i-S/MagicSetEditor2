//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <render/value/choice.hpp>
#include <render/card/viewer.hpp>

// ----------------------------------------------------------------------------- : ChoiceValueViewer

IMPLEMENT_VALUE_VIEWER(Choice);

void get_options(Rotation& rot, ValueViewer& viewer, const ChoiceStyle& style, GeneratedImage::Options& opts);

bool ChoiceValueViewer::prepare(RotatedDC& dc) {
  return prepare_choice_viewer(dc, *this, style(), value().value());
}
void ChoiceValueViewer::draw(RotatedDC& dc) {
  drawFieldBorder(dc);
  if (style().render_style & RENDER_HIDDEN) return;
  // render background
  if (nativeLook()) {
    dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(RealRect(0, 0, dc.getWidth(), dc.getHeight()));
  }
  draw_choice_viewer(dc, *this, style(), value().value());
}

void ChoiceValueViewer::onStyleChange(int changes) {
  if (changes & CHANGE_MASK) style().image.clearCache();
  ValueViewer::onStyleChange(changes);
}

// ----------------------------------------------------------------------------- : Generic draw/prepare

bool prepare_choice_viewer(RotatedDC& dc, ValueViewer& viewer, ChoiceStyle& style, const String& value) {
  if (style.render_style & RENDER_IMAGE) {
    style.initImage();
    CachedScriptableImage& img = style.image;
    Context& ctx = viewer.getContext();
    ctx.setVariable(SCRIPT_VAR_input, to_script(value));
    // generate to determine the size
    if (img.update(ctx) && img.isReady()) {
      GeneratedImage::Options img_options;
      get_options(dc, viewer, style, img_options);
      // Generate image/bitmap (whichever is available)
      // don't worry, we cache the image
      ImageCombine combine = style.combine;
      Bitmap bitmap; Image image;
      RealSize size;
      img.generateCached(img_options, &style.mask, &combine, &bitmap, &image, &size);
      // store content properties
      double zoom = dc.getZoom();
      if (style.content_width != size.width / zoom || style.content_height != size.height / zoom) {
        style.content_width  = size.width / zoom;
        style.content_height = size.height / zoom;
        return true;
      }
    }
  }
  return false;
}

void draw_choice_viewer(RotatedDC& dc, ValueViewer& viewer, ChoiceStyle& style, const String& value) {
  if (value.empty()) return;
  double margin = 0;
  if (style.render_style & RENDER_IMAGE) {
    // draw image
    CachedScriptableImage& img = style.image;
    if (style.content_dependent) {
      // re run script
      Context& ctx = viewer.getContext();
      ctx.setVariable(SCRIPT_VAR_input, to_script(value));
      img.update(ctx);
    }
    if (img.isReady()) {
      GeneratedImage::Options img_options;
      get_options(dc, viewer, style, img_options);
      // Generate image/bitmap
      ImageCombine combine = style.combine;
      Bitmap bitmap; Image image;
      RealSize size;
      img.generateCached(img_options, &style.mask, &combine, &bitmap, &image, &size);
      size = dc.trInvS(size);
      RealRect rect(align_in_rect(style.alignment, size, dc.getInternalRect()), size);
      if (bitmap.Ok()) {
        // just draw it
        dc.DrawPreRotatedBitmap(bitmap,rect);
      } else {
        // use combine mode
        assert(image.Ok());
        dc.DrawPreRotatedImage(image,rect,combine);
      }
      margin = size.width + 2;
    } else if (viewer.nativeLook()) {
      // always have the margin
      margin = 18;
    }
  }
  if (style.render_style & RENDER_TEXT) {
    String text = tr(viewer.getStylePackage(), value, capitalize_sentence);
    Alignment text_align = style.alignment;
    if (style.render_style & RENDER_IMAGE) {
      text_align = ALIGN_MIDDLE_LEFT; // can't align both text and image in the same way
    }
    Font& font = style.font;
    Color font_color = font.color;
    if (viewer.nativeLook()) {
      font.color = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
      margin += 1.;
    }
    dc.SetFont(font, 1.0);
    RealSize size = dc.GetTextExtent(text);
    RealPoint text_pos = align_in_rect(text_align, size, dc.getInternalRect()) + RealSize(margin, 0);
    dc.DrawTextWithShadow(text, font, text_pos);
    if (viewer.nativeLook()) font.color = font_color;
  }
}

void get_options(Rotation& rot, ValueViewer& viewer, const ChoiceStyle& style, GeneratedImage::Options& opts) {
  opts.package       = &viewer.getStylePackage();
  opts.local_package = &viewer.getLocalPackage();
  opts.angle         = rot.getAngle();
  if (viewer.nativeLook()) {
    opts.width = opts.height = 16;
    opts.preserve_aspect = ASPECT_BORDER;
  } else if(style.render_style & RENDER_TEXT) {
    // also drawing text, use original size
  } else {
    opts.width  = (int) rot.trX(style.width);
    opts.height = (int) rot.trY(style.height);
    opts.preserve_aspect = (style.alignment & ALIGN_STRETCH) ? ASPECT_STRETCH : ASPECT_FIT;
  }
}
