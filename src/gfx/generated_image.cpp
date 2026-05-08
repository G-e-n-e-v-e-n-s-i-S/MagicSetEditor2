//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/generated_image.hpp>
#include <util/io/package.hpp>
#include <util/error.hpp>
#include <data/set.hpp>
#include <data/symbol.hpp>
#include <data/field/symbol.hpp>
#include <script/functions/json.hpp>
#include <render/symbol/filter.hpp>
#include <gui/util.hpp> // load_resource_image
#include <gui/web_request_window.hpp>
#include <wx/wfstream.h>

// ----------------------------------------------------------------------------- : GeneratedImage

ScriptType GeneratedImage::type() const { return SCRIPT_IMAGE; }
String GeneratedImage::typeName() const { return _TYPE_("image"); }
GeneratedImageP GeneratedImage::toImage() const {
  return const_cast<GeneratedImage*>(this)->intrusive_from_this();
}

Image GeneratedImage::generateConform(const Options& options) {
  return conform_image(generate(options),options);
}

Image conform_image(const Image& img, const GeneratedImage::Options& options) {
  Image image = img;
  // resize?
  int iw = image.GetWidth(), ih = image.GetHeight();
  if ((iw == options.width && ih == options.height) || (options.width == 0 && options.height == 0)) {
    // zoom?
    if (!almost_equal(options.zoom, 1.0)) {
      image = resample(image, int(iw * options.zoom), int(ih * options.zoom));
    } else {
      // already the right size
    }
  } else if (options.height == 0) {
    // width is given, determine height
    int h = options.width * ih / iw;
    image = resample(image, options.width, h);
  } else if (options.width == 0) {
    // height is given, determine width
    int w = options.height * iw / ih;
    image = resample(image, w, options.height);
  } else if (options.preserve_aspect == ASPECT_FIT) {
    // determine actual size of resulting image
    int w, h;
    if (iw * options.height > ih * options.width) { // too much height requested
      w = options.width;
      h = options.width * ih / iw;
    } else {
      w = options.height * iw / ih;
      h = options.height;
    }
    image = resample(image, w, h);
  } else {
    if (options.preserve_aspect == ASPECT_BORDER && (options.width < options.height * 3) && (options.height < options.width * 3)) {
      // preserve the aspect ratio if there is not too much difference
      image = resample_preserve_aspect(image, options.width, options.height);
    } else {
      image = resample(image, options.width, options.height);
    }
  }
  // saturate?
  if (options.saturate) {
    saturate(image, .1);
  }
  options.width  = image.GetWidth();
  options.height = image.GetHeight();
  // rotate?
  if (!almost_equal(options.angle, 0)) {
    image = rotate_image(image, options.angle);
  }
  return image;
}

// ----------------------------------------------------------------------------- : BlankImage

Image BlankImage::generate(const Options& opt) {
  int w = max(1, opt.width >= 0  ? opt.width  : opt.height);
  int h = max(1, opt.height >= 0 ? opt.height : opt.width);
  Image img(w, h);
  assert(img.Ok());
  img.InitAlpha();
  memset(img.GetAlpha(), 0, w * h);
  return img;
}
bool BlankImage::operator == (const GeneratedImage& that) const {
  const BlankImage* that2 = dynamic_cast<const BlankImage*>(&that);
  return that2;
}

// ----------------------------------------------------------------------------- : LinearBlendImage

Image LinearBlendImage::generate(const Options& opt) {
  Image img = image1->generate(opt);
  linear_blend(img, image2->generate(opt), x1, y1, x2, y2);
  return img;
}
ImageCombine LinearBlendImage::combine() const {
  return image1->combine();
}
bool LinearBlendImage::operator == (const GeneratedImage& that) const {
  const LinearBlendImage* that2 = dynamic_cast<const LinearBlendImage*>(&that);
  return that2 && *image1 == *that2->image1
    && *image2 == *that2->image2
    && x1 == that2->x1 && y1 == that2->y1
    && x2 == that2->x2 && y2 == that2->y2;
}

// ----------------------------------------------------------------------------- : MaskedBlendImage

Image MaskedBlendImage::generate(const Options& opt) {
  Image img = light->generate(opt);
  mask_blend(img, dark->generate(opt), mask->generate(opt));
  return img;
}
ImageCombine MaskedBlendImage::combine() const {
  return light->combine();
}
bool MaskedBlendImage::operator == (const GeneratedImage& that) const {
  const MaskedBlendImage* that2 = dynamic_cast<const MaskedBlendImage*>(&that);
  return that2 && *light == *that2->light
    && *dark  == *that2->dark
    && *mask  == *that2->mask;
}

// ----------------------------------------------------------------------------- : CombineBlendImage

Image CombineBlendImage::generate(const Options& opt) {
  Image img = image1->generate(opt);
  combine_image(img, image2->generate(opt), image_combine);
  return img;
}
ImageCombine CombineBlendImage::combine() const {
  return image1->combine();
}
bool CombineBlendImage::operator == (const GeneratedImage& that) const {
  const CombineBlendImage* that2 = dynamic_cast<const CombineBlendImage*>(&that);
  return that2 && *image1 == *that2->image1
    && *image2 == *that2->image2
    && image_combine == that2->image_combine;
}

// ----------------------------------------------------------------------------- : SetMaskImage

Image SetMaskImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  set_alpha(img, mask->generate(opt));
  return img;
}
bool SetMaskImage::operator == (const GeneratedImage& that) const {
  const SetMaskImage* that2 = dynamic_cast<const SetMaskImage*>(&that);
  return that2 && *image == *that2->image
    && *mask  == *that2->mask;
}

Image SetAlphaImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  set_alpha(img, alpha);
  return img;
}
bool SetAlphaImage::operator == (const GeneratedImage& that) const {
  const SetAlphaImage* that2 = dynamic_cast<const SetAlphaImage*>(&that);
  return that2 && *image == *that2->image
    && alpha  == that2->alpha;
}

// ----------------------------------------------------------------------------- : SetCombineImage

Image SetCombineImage::generate(const Options& opt) {
  return image->generate(opt);
}
ImageCombine SetCombineImage::combine() const {
  return image_combine;
}
bool SetCombineImage::operator == (const GeneratedImage& that) const {
  const SetCombineImage* that2 = dynamic_cast<const SetCombineImage*>(&that);
  return that2 && *image == *that2->image
    && image_combine == that2->image_combine;
}

// ----------------------------------------------------------------------------- : FillImage

Image FillImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  fill_image(img, color);
  return img;
}
bool FillImage::operator == (const GeneratedImage& that) const {
  const FillImage* that2 = dynamic_cast<const FillImage*>(&that);
  return that2 && *image == *that2->image
    && color == that2->color;
}

// ----------------------------------------------------------------------------- : SaturateImage

Image SaturateImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  saturate(img, amount);
  return img;
}
bool SaturateImage::operator == (const GeneratedImage& that) const {
  const SaturateImage* that2 = dynamic_cast<const SaturateImage*>(&that);
  return that2 && *image == *that2->image
    && amount == that2->amount;
}

// ----------------------------------------------------------------------------- : BrightenImage

Image BrightenImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  brighten(img, amount);
  return img;
}
bool BrightenImage::operator == (const GeneratedImage& that) const {
  const BrightenImage* that2 = dynamic_cast<const BrightenImage*>(&that);
  return that2 && *image == *that2->image
    && amount == that2->amount;
}

// ----------------------------------------------------------------------------- : InvertImage

Image InvertImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  invert(img);
  return img;
}
bool InvertImage::operator == (const GeneratedImage& that) const {
  const InvertImage* that2 = dynamic_cast<const InvertImage*>(&that);
  return that2 && *image == *that2->image;
}

// ----------------------------------------------------------------------------- : RecolorImage

Image RecolorImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  recolor(img, color);
  return img;
}
bool RecolorImage::operator == (const GeneratedImage& that) const {
  const RecolorImage* that2 = dynamic_cast<const RecolorImage*>(&that);
  return that2 && *image == *that2->image
    && color == that2->color;
}

Image RecolorImage2::generate(const Options& opt) {
  Image img = image->generate(opt);
  recolor(img, red,green,blue,white);
  return img;
}
bool RecolorImage2::operator == (const GeneratedImage& that) const {
  const RecolorImage2* that2 = dynamic_cast<const RecolorImage2*>(&that);
  return that2 && *image == *that2->image
    && red == that2->red
    && green == that2->green
    && blue == that2->blue
    && white == that2->white;
}

// ----------------------------------------------------------------------------- : FlipImage

Image FlipImageHorizontal::generate(const Options& opt) {
  Image img = image->generate(opt);
  return flip_image_horizontal(img);
}
bool FlipImageHorizontal::operator == (const GeneratedImage& that) const {
  const FlipImageHorizontal* that2 = dynamic_cast<const FlipImageHorizontal*>(&that);
  return that2 && *image == *that2->image;
}

Image FlipImageVertical::generate(const Options& opt) {
  Image img = image->generate(opt);
  return flip_image_vertical(img);
}
bool FlipImageVertical::operator == (const GeneratedImage& that) const {
  const FlipImageVertical* that2 = dynamic_cast<const FlipImageVertical*>(&that);
  return that2 && *image == *that2->image;
}

Image RotateImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  return rotate_image(img,angle);
}
bool RotateImage::operator == (const GeneratedImage& that) const {
  const RotateImage* that2 = dynamic_cast<const RotateImage*>(&that);
  return that2 && *image == *that2->image
    && angle == that2->angle;
}

// ----------------------------------------------------------------------------- : EnlargeImage

Image EnlargeImage::generate(const Options& opt) {
  // generate 'sub' image
  Options sub_opt
  ( int(opt.width  * (border_size < 0.5 ? 1 - 2 * border_size : 0))
    , int(opt.height * (border_size < 0.5 ? 1 - 2 * border_size : 0))
    , opt.package
    , opt.local_package
    , opt.preserve_aspect);
  Image img = image->generate(sub_opt);
  // size of generated image
  int w  = img.GetWidth(),  h = img.GetHeight();  // original image size
  int dw = int(w * border_size), dh = int(h * border_size); // delta
  int w2 = w + dw + dw,     h2 = h + dh + dh;     // new image size
  Image larger(w2,h2);
  larger.InitAlpha();
  memset(larger.GetAlpha(),0,w2*h2); // blank
  // copy to sub-part of larger image
  Byte* data1 = img.GetData(), *data2 = larger.GetData();
  for (int y = 0 ; y < h ; ++y) {
    memcpy(data2 + 3*(dw + (y+dh)*w2), data1 + 3*y*w, 3*w); // copy a line
  }
  if (img.HasAlpha()) {
    data1 = img.GetAlpha(), data2 = larger.GetAlpha();
    for (int y = 0 ; y < h ; ++y) {
      memcpy(data2 + dw + (y+dh)*w2, data1 + y*w, w); // copy a line
    }
  }
  // transfer metadata
  if (img.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata = transformAllEncodedRects(img.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION), RealRect::translate, dw, dh);
    larger.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }
  // done
  return larger;
}
bool EnlargeImage::operator == (const GeneratedImage& that) const {
  const EnlargeImage* that2 = dynamic_cast<const EnlargeImage*>(&that);
  return that2 && *image      == *that2->image
    && border_size == that2->border_size;
}

// ----------------------------------------------------------------------------- : ResizeImage

Image ResizeImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  return resample(img, width, height);
}
bool ResizeImage::operator == (const GeneratedImage& that) const {
  const ResizeImage* that2 = dynamic_cast<const ResizeImage*>(&that);
  return that2 && *image == *that2->image
    && width == that2->width
    && height == that2->height;
}

// ----------------------------------------------------------------------------- : StrokeImage

Image StrokeImage::generate(const Options& opt) {
  // create enlarged image
  Image base_img = base_image->generate(opt);
  Image s_img = make_stroke_image(base_img, color, radius, blur);
  if (include_image) {
    int offset = radius + blur;
    s_img.Paste(base_img, offset, offset, wxIMAGE_ALPHA_BLEND_COMPOSE);
  }
  return s_img;
}
bool StrokeImage::operator == (const GeneratedImage& that) const {
  const StrokeImage* that2 = dynamic_cast<const StrokeImage*>(&that);
  return that2 && *base_image == *that2->base_image
    && radius == that2->radius
    && blur == that2->blur
    && color == that2->color;
}

// ----------------------------------------------------------------------------- : BleedEdgedImage

Image BleedEdgedImage::generate(const Options& opt) {
  // create enlarged image
  Image base_img = base_image->generate(opt);
  int w = base_img.GetWidth(), h = base_img.GetHeight();
  if (w <= 3 || h <= 3) {
    queue_message(MESSAGE_ERROR, _("Image too small to add bleed edge"));
    return base_img;
  }
  bool is_landscape = w > h;
  int dw = int(w * (horizontal_size > 0.0 ? horizontal_size : is_landscape ? 0.037 : 0.048));
  int dh = int(h * (vertical_size   > 0.0 ? vertical_size   : is_landscape ? 0.048 : 0.037));
  dw = min(w-1, max(0, dw));
  dh = min(h-1, max(0, dh));
  if (dw <= 0 && dh <= 0) {
    return base_img;
  }
  int width = w + dw + dw, height = h + dh + dh;
  UInt size = width * height;
  Image img = wxImage(width, height, false);
  img.InitAlpha();
  Byte* pixels = img.GetData();
  Byte* alpha = img.GetAlpha();
  // fill with background color
  for (UInt i = 0; i < size; ++i) {
    pixels[3 * i + 0] = background_color.Red();
    pixels[3 * i + 1] = background_color.Green();
    pixels[3 * i + 2] = background_color.Blue();
    alpha[i] = background_color.Alpha();
  }
  // paste original image
  img.Paste(base_img, dw, dh, wxIMAGE_ALPHA_BLEND_COMPOSE);
  int pixel, mirror, x_start, y_start, x_size, y_size;
  // fill left border
  x_start = 0;
  y_start = dh;
  x_size = dw;
  y_size = height - dh - dh;
  for (int y = 0; y < y_size; ++y) {
    for (int x = 0; x < x_size; ++x) {
      pixel = x_start + x + (y_start + y) * width;
      mirror = 2 * dw - x + (y_start + y) * width;
      pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
      pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
      pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
      alpha[pixel] = alpha[mirror];
    }
  }
  // fill right border
  x_start = width - dw;
  y_start = dh;
  x_size = dw;
  y_size = height - dh - dh;
  for (int y = 0; y < y_size; ++y) {
    for (int x = 0; x < x_size; ++x) {
      pixel =        x_start + x + (y_start + y) * width;
      mirror = - 2 + x_start - x + (y_start + y) * width;
      pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
      pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
      pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
      alpha[pixel] = alpha[mirror];
    }
  }
  // fill top border
  x_start = 0;
  y_start = 0;
  x_size = width;
  y_size = dh;
  for (int y = 0; y < y_size; ++y) {
    for (int x = 0; x < x_size; ++x) {
      pixel =  x_start + x + (y_start + y) * width;
      mirror = x_start + x + ( 2 * dh - y) * width;
      pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
      pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
      pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
      alpha[pixel] = alpha[mirror];
    }
  }
  // fill bottom border
  x_start = 0;
  y_start = height - dh;
  x_size = width;
  y_size = dh;
  for (int y = 0; y < y_size; ++y) {
    for (int x = 0; x < x_size; ++x) {
      pixel =  x_start + x + (      y_start + y) * width;
      mirror = x_start + x + (- 2 + y_start - y) * width;
      pixels[3 * pixel + 0] = pixels[3 * mirror + 0];
      pixels[3 * pixel + 1] = pixels[3 * mirror + 1];
      pixels[3 * pixel + 2] = pixels[3 * mirror + 2];
      alpha[pixel] = alpha[mirror];
    }
  }
  // transfer metadata
  if (base_img.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata = transformAllEncodedRects(base_img.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION), RealRect::translate, dw, dh);
    img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }
  // done
  return img;
}
bool BleedEdgedImage::operator == (const GeneratedImage& that) const {
  const BleedEdgedImage* that2 = dynamic_cast<const BleedEdgedImage*>(&that);
  return that2 && *base_image == *that2->base_image
    && horizontal_size == that2->horizontal_size
    && vertical_size == that2->vertical_size
    && background_color == that2->background_color;
}

// ----------------------------------------------------------------------------- : InsertedImage

Image InsertedImage::generate(const Options& opt) {
  Image base_img =     base_image->generate(opt);
  Image inserted_img = inserted_image->generate(opt);
  int base_x =     offset_x < 0 ? -offset_x : 0;
  int base_y =     offset_y < 0 ? -offset_y : 0;
  int inserted_x = offset_x < 0 ? 0         : offset_x;
  int inserted_y = offset_y < 0 ? 0         : offset_y;
  int width =  max(base_x + base_img.GetWidth(),  inserted_x + inserted_img.GetWidth());
  int height = max(base_y + base_img.GetHeight(), inserted_y + inserted_img.GetHeight());
  if (width <= 0) throw ScriptError(_ERROR_1_("negative image width", "insert_image"));
  if (height <= 0) throw ScriptError(_ERROR_1_("negative image height", "insert_image"));
  UInt size = width * height;
  Image img = wxImage(width, height, false);
  img.InitAlpha();
  Byte* data = img.GetData();
  Byte* alpha = img.GetAlpha();
  Byte r = background_color.Red();
  Byte g = background_color.Green();
  Byte b = background_color.Blue();
  Byte a = background_color.Alpha();
  for (UInt i = 0; i < size; ++i) {
    data[0] = r;
    data[1] = g;
    data[2] = b;
    data += 3;
    alpha[0] = a;
    alpha += 1;
  }
  img.Paste(base_img, base_x, base_y, wxIMAGE_ALPHA_BLEND_COMPOSE);
  img.Paste(inserted_img, inserted_x, inserted_y, wxIMAGE_ALPHA_BLEND_COMPOSE);
  // transfer metadata
  img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata_merge(base_img, inserted_img, base_x, base_y, inserted_x, inserted_y));
  return img;
}

ImageCombine InsertedImage::combine() const {
  return base_image->combine();
}
bool InsertedImage::operator == (const GeneratedImage& that) const {
  const InsertedImage* that2 = dynamic_cast<const InsertedImage*>(&that);
  return that2
    && *base_image == *that2->base_image
    && *inserted_image == *that2->inserted_image
    && offset_x == that2->offset_x
    && offset_y == that2->offset_y;
}

// ----------------------------------------------------------------------------- : CropImage

Image CropImage::generate(const Options& opt) {
  if (width <= 0) throw ScriptError(_ERROR_1_("negative image width", "crop_image"));
  if (height <= 0) throw ScriptError(_ERROR_1_("negative image height", "crop_image"));
  Image base_img = image->generate(opt);
  Image img = base_img.Size(wxSize((int)width, (int)height), wxPoint(-(int)offset_x, -(int)offset_y)); //Image img = base_img.Size(wxSize((int)width, (int)height), wxPoint(-(int)offset_x, -(int)offset_y), background_color.Red(), background_color.Green(), background_color.Blue());
  // transfer metadata
  if (base_img.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata = transformAllEncodedRects(base_img.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION), RealRect::translate, -offset_x, -offset_y);
    // prune out of bounds cards
    boost::json::array cardsv = metadata_to_json(metadata);
    boost::json::array inbounds_cardsv;
    for (size_t i = 0; i < cardsv.size(); i++) {
      boost::json::object cardv = cardsv[i].as_object();
      if (cardv.contains("bounds")) {
        String bounds = String(cardv["bounds"].as_string().c_str());
        RealRect rect(0.0, 0.0, 0.0, 0.0);
        int degrees = 0;
        if (decodeRectFromString(bounds, rect, degrees)) {
          rect = rect.intersect(RealRect(0.0, 0.0, width, height));
          if (rect.width <= 0.0 || rect.height <= 0.0 ) continue;
        }
      }
      inbounds_cardsv.emplace_back(cardv);
    }
    metadata = "<mse-card-data>" + json_ugly_print(inbounds_cardsv) + "</mse-card-data>";
    img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }
  return img;
}
bool CropImage::operator == (const GeneratedImage& that) const {
  const CropImage* that2 = dynamic_cast<const CropImage*>(&that);
  return that2 && *image      == *that2->image
    && width    == that2->width    && height   == that2->height
    && offset_x == that2->offset_x && offset_y == that2->offset_y
    && background_color == that2->background_color;
}

// ----------------------------------------------------------------------------- : DropShadowImage

/// Preform a gaussian blur, from the image in of w*h bytes to out
/** out is scaled some scaling, this is the return value */
UInt gaussian_blur(Byte* in, UInt* out, int w, int h, double radius) {
  // blur horizontally
  auto blur_x = make_unique<UInt[]>(w*h); // scaled by total_x, so in [0..255*total_x]
  memset(blur_x.get(), 0, w*h*sizeof(UInt));
  UInt total_x = 0;
  {
    double sigma = radius * w;
    double mult = (1 << 8) / (sqrt(2 * M_PI) * sigma);
    double sigsqr2 = 1 / (2 * sigma * sigma);
    int range = min(w, (int)(3*sigma));
    for (int d = -range ; d <= range ; ++d) {
      UInt factor = (int)( mult * exp(-d * d * sigsqr2) );
      total_x += factor;
      if (factor > 0) {
        int x_start = max(0, -d), x_end = min(w, w-d);
        for (int y = 0 ; y < h ; ++y) {
          for (int x = x_start ; x < x_end ; ++x) {
            blur_x[x + y*w] += in[x + d + y*w] * factor;
          }
        }
      }
    }
  }
  // blur vertically
  memset(out, 0, w*h*sizeof(UInt));
  UInt total_y = 0;
  {
    double sigma = radius * h;
    double mult = (1 << 8) / (sqrt(2 * M_PI) * sigma);
    double sigsqr2 = 1 / (2 * sigma * sigma);
    int range = min(h, (int)(3*sigma));
    for (int d = -range ; d <= range ; ++d) {
      UInt factor = (UInt)( mult * exp(-d * d * sigsqr2) );
      total_y += factor;
      if (factor > 0) {
        int y_start = max(0, -d), y_end = min(h, h-d);
        for (int y = y_start ; y < y_end ; ++y) {
          for (int x = 0 ; x < w ; ++x) {
            out[x + y*w] += blur_x[x + (d + y)*w] * factor;
          }
        }
      }
    }
  }
  return total_x * total_y;
}

Image DropShadowImage::generate(const Options& opt) {
  // sub image
  Image img = image->generate(opt);
  if (!img.HasAlpha()) {
    // no alpha, there is nothing we can do
    return img;
  }
  int w = img.GetWidth(), h = img.GetHeight();
  Byte* alpha = img.GetAlpha();
  // blur
  auto shadow = make_unique<UInt[]>(w*h);
  UInt total = 255 * gaussian_blur(alpha, shadow.get(), w, h, shadow_blur_radius);
  // combine
  Byte* data = img.GetData();
  int dw = int(w * offset_x), dh = int(h * offset_y);
  int x_start = max(0,   dw), y_start = max(0,   dh);
  int x_end   = min(w, w+dw), y_end   = min(h, h+dh);
  int delta = dw + w * dh;
  int sa = (int)(shadow_alpha * (1 << 16));
  for (int y = y_start ; y < y_end ; ++y) {
    for (int x = x_start ; x < x_end ; ++x) {
      int p  = x + y * w; // pixel we are working on
      int a = alpha[p];
      int shad = ((((255 - a)*sa)>>16) * shadow[p - delta]) / total; // amount of shadow to add
      int factor = max(1, a + shad); // divide by this
      data[3 * p    ] = (a * data[3 * p    ] + shad * shadow_color.Red()  ) / factor;
      data[3 * p + 1] = (a * data[3 * p + 1] + shad * shadow_color.Green()) / factor;
      data[3 * p + 2] = (a * data[3 * p + 2] + shad * shadow_color.Blue() ) / factor;
      alpha[p] = a + shad;
    }
  }
  return img;
}
bool DropShadowImage::operator == (const GeneratedImage& that) const {
  const DropShadowImage* that2 = dynamic_cast<const DropShadowImage*>(&that);
  return that2 && *image   == *that2->image
    && offset_x == that2->offset_x && offset_y == that2->offset_y
    && shadow_alpha == that2->shadow_alpha && shadow_blur_radius == that2->shadow_blur_radius
    && shadow_color == that2->shadow_color;
}

// ----------------------------------------------------------------------------- : PackagedImage

Image PackagedImage::generate(const Options& opt) {
  // TODO : use opt.width and opt.height?
  // open file from package
  if (!opt.package) throw ScriptError(_("Can only load images in a context where an image is expected"));
  auto file_stream = opt.package->openIn(filename);
  Image img;
  if (image_load_file(img, *file_stream)) {
    if (img.HasMask()) img.InitAlpha(); // we can't handle masks
    return img;
  } else {
    throw ScriptError(_("Unable to load image '") + filename + _("' from '") + opt.package->name() + _("'"));
  }
}
bool PackagedImage::operator == (const GeneratedImage& that) const {
  const PackagedImage* that2 = dynamic_cast<const PackagedImage*>(&that);
  return that2 && filename == that2->filename;
}

// ----------------------------------------------------------------------------- : BuiltInImage

Image BuiltInImage::generate(const Options& opt) {
  // TODO : use opt.width and opt.height?
  try {
    Image img = load_resource_image(name);
    if (img.Ok()) return img;
  } catch (...) {}
  throw ScriptError(_("There is no built in image '") + name + _("'"));
}
bool BuiltInImage::operator == (const GeneratedImage& that) const {
  const BuiltInImage* that2 = dynamic_cast<const BuiltInImage*>(&that);
  return that2 && name == that2->name;
}

// ----------------------------------------------------------------------------- : ArbitraryImage

Image ArbitraryImage::generate(const Options& opt) {
  return image;
}
bool ArbitraryImage::operator == (const GeneratedImage& that) const {
  const ArbitraryImage* that2 = dynamic_cast<const ArbitraryImage*>(&that);
  return that2 && image.IsSameAs(that2->image);
}


// ----------------------------------------------------------------------------- : SymbolToImage

SymbolToImage::SymbolToImage(bool is_local, const LocalFileName& filename, Age age, const SymbolVariationP& variation)
  : is_local(is_local), filename(filename), age(age), variation(variation)
{}
SymbolToImage::~SymbolToImage() {}

Image SymbolToImage::generate(const Options& opt) {
  // TODO : use opt.width and opt.height?
  Package* package = is_local ? opt.local_package : opt.package;
  if (!package) throw ScriptError(_("Can only load images in a context where an image is expected"));
  SymbolP the_symbol;
  if (filename.empty()) {
    the_symbol = default_symbol();
  } else {
    the_symbol = package->readFile<SymbolP>(filename);
  }
  int size = max(100, 3*max(opt.width,opt.height));
  if (opt.width <= 1 || opt.height <= 1) {
    return render_symbol(the_symbol, *variation->filter, variation->border_radius, size, size);
  } else {
    int width  = size * opt.width  / max(opt.width,opt.height);
    int height = size * opt.height / max(opt.width,opt.height);
    return render_symbol(the_symbol, *variation->filter, variation->border_radius, width, height, false, true);
  }
}
bool SymbolToImage::operator == (const GeneratedImage& that) const {
  const SymbolToImage* that2 = dynamic_cast<const SymbolToImage*>(&that);
  return that2 && is_local  == that2->is_local
    && filename  == that2->filename
    && age       == that2->age
    && (variation == that2->variation ||
      *variation == *that2->variation // custom variation
      );
}

// ----------------------------------------------------------------------------- : ImageValueToImage

ImageValueToImage::ImageValueToImage(const LocalFileName& filename, Age age)
  : filename(filename), age(age)
{}
ImageValueToImage::~ImageValueToImage() {}

Image ImageValueToImage::generate(const Options& opt) {
  // TODO : use opt.width and opt.height?
  if (!opt.local_package) throw ScriptError(_("Can only load images in a context where an image is expected"));
  Image image;
  if (!filename.empty()) {
    auto image_file_stream = opt.local_package->openIn(filename);
    image_load_file(image, *image_file_stream);
  }
  if (!image.Ok()) {
    image = Image(max(1,opt.width), max(1,opt.height));
  }
  return image;
}
bool ImageValueToImage::operator == (const GeneratedImage& that) const {
  const ImageValueToImage* that2 = dynamic_cast<const ImageValueToImage*>(&that);
  return that2 && filename == that2->filename
    && age      == that2->age;
}

// ----------------------------------------------------------------------------- : SetMetadataImage

Image SetMetadataImage::generate(const Options& opt) {
  Image img = image->generate(opt);
  img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  return img;
}
bool SetMetadataImage::operator == (const GeneratedImage& that) const {
  const SetMetadataImage* that2 = dynamic_cast<const SetMetadataImage*>(&that);
  return that2 && *image == *that2->image
    && metadata  == that2->metadata;
}

// ----------------------------------------------------------------------------- : ImportedImage

ImportedImage::ImportedImage(Set* set, const String& filepath)
{
  // has the set already been saved at least once?
  if (set->needSaveAs()) throw ScriptError(_ERROR_1_("can't import image without set", filepath));

  // determine save name
  loadpath = filepath;
  savename = normalize_internal_filename(loadpath + _(".png"));
  savename.Replace(":",  "-");
  savename.Replace("/",  "-");

  // does the file pointed to by filepath exist?
  if (!wxFileName(loadpath, wxPATH_UNIX).FileExists()) {
    if (set->contains(savename)) return;
    else throw ScriptError(_ERROR_1_("import not found", loadpath));
  }
  
  // is the file an image?
  Image img;
  img.LoadFile(loadpath);
  if (!img.IsOk()) throw ScriptError(_ERROR_1_("import not image", loadpath));

  // add the file to the set (or overwrite it if pre-existing), save set
  auto outStream = set->openOut(savename);
  img.SaveFile(*outStream, wxBITMAP_TYPE_PNG);
  if (!outStream->IsOk()) throw ScriptError(_ERROR_1_("can't write image to set", loadpath));
  outStream->Close();
  set->save(false);
}

Image ImportedImage::generate(const Options& opt) {
  auto imageInputStream = opt.local_package->openIn(savename);
  Image img(*imageInputStream, wxBITMAP_TYPE_PNG);

  if (!img.IsOk()) throw ScriptError(_ERROR_1_("can't import image", loadpath));

  return img;
}

bool ImportedImage::operator == (const GeneratedImage& that) const {
  const ImportedImage* that2 = dynamic_cast<const ImportedImage*>(&that);
  return that2 && that2->loadpath == loadpath;
}

// ----------------------------------------------------------------------------- : DownloadedImage

DownloadedImage::DownloadedImage(Set* set, const String& url)
{
  // has the set already been saved at least once?
  if (set->needSaveAs()) throw ScriptError(_ERROR_1_("can't download image without set", url));

  // determine save name
  loadpath = url;
  savename = normalize_internal_filename(loadpath + _(".png"));
  savename.Replace(":",  "-");
  savename.Replace("/",  "-");

  // can we download the data?
  WebRequestWindow wnd(loadpath);
  if (wnd.ShowModal() != wxID_OK) {
    if (set->contains(savename)) return;
    else throw ScriptError(_ERROR_1_("can't download image", loadpath));
  }

  // is the data an image?
  if (!wnd.content_type.StartsWith(_("image/"))) throw ScriptError(_ERROR_1_("download not image", loadpath));
  
  // add the file to the set (or overwrite it if pre-existing), save set
  auto outStream = set->openOut(savename);
  wnd.image_out.SaveFile(*outStream, wxBITMAP_TYPE_PNG);
  if (!outStream->IsOk()) throw ScriptError(_ERROR_1_("can't write image to set", loadpath));
  outStream->Close();
  set->save(false);
}

Image DownloadedImage::generate(const Options& opt) {
  auto imageInputStream = opt.local_package->openIn(savename);
  Image img(*imageInputStream, wxBITMAP_TYPE_PNG);

  if (!img.IsOk()) throw ScriptError(_ERROR_1_("can't download image", loadpath));

  return img;
}

bool DownloadedImage::operator == (const GeneratedImage& that) const {
  const DownloadedImage* that2 = dynamic_cast<const DownloadedImage*>(&that);
  return that2 && that2->loadpath == loadpath;
}
