//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/format/image_encoding.hpp>
#include <gfx/gfx.hpp>
#include <util/error.hpp>
#if defined(__WXMSW__) && wxUSE_WXDIB
#include <wx/msw/dib.h>
#endif

// ----------------------------------------------------------------------------- : Saturation

void saturate(Image& image, double amount) {
  Byte* pix = image.GetData();
  Byte* end = pix + image.GetWidth() * image.GetHeight() * 3;
  // the formula for saturation is
  //   rgb' = (rgb - amount * avg) / (1 - amount)
  // if amount >= 1 then this is some kind of inversion
  // if amount >  0 then use formula directly
  // if amount <  0 then de-saturate instead:
  //   rgb = rgb' + -amount*avg - -amount*rgb'
  //       = rgb' * (1 - -amount) + -amount*avg
  // if amount < -1 then we are left with just the average
  int factor = int(256 * amount);
  if (factor == 0) {
    return; // nothing to do
  } else if (factor == 256) {
    // super crazy saturation: division by zero
    // if we take infty to be 255, then it is a >avg test
    while (pix != end) {
      int r = pix[0], g = pix[1], b = pix[2];
      pix[0] = r+r > g+b ? 255 : 0;
      pix[1] = g+g > b+r ? 255 : 0;
      pix[2] = b+b > r+g ? 255 : 0;
      pix += 3;
    }
  } else if (factor > 0) {
    int div = 768 - 3 * factor;
    assert(div > 0);
    while (pix != end) {
      int r = pix[0], g = pix[1], b = pix[2];
      int avg = factor*(r+g+b);
      pix[0] = col((768*r - avg) / div);
      pix[1] = col((768*g - avg) / div);
      pix[2] = col((768*b - avg) / div);
      pix += 3;
    }
  } else {
    int factor1 = -factor;
    int factor2 = 768 - 3*factor1;
    while (pix != end) {
      int r = pix[0], g = pix[1], b = pix[2];
      int avg = factor1*(r+g+b);
      pix[0] = (factor2*r + avg) / 768;
      pix[1] = (factor2*g + avg) / 768;
      pix[2] = (factor2*b + avg) / 768;
      pix += 3;
    }
  }
}

// ----------------------------------------------------------------------------- : Brightness

void brighten(Image& image, double amount) {
  if (almost_equal(amount, 0.0)) return;
  Byte* pix = image.GetData();
  Byte* end = pix + image.GetWidth() * image.GetHeight() * 3;
  amount = min(max(amount, -1.0), 1.0);
  if (amount > 0.0) {
    double s = 255 * amount;
    double t = 1.0 - amount;
    while (pix != end) {
      pix[0] = s + pix[0] * t;
      pix++;
    }
  } else {
    double t = 1.0 + amount;
    while (pix != end) {
      pix[0] = pix[0] * t;
      pix++;
    }
  }
  
}

// ----------------------------------------------------------------------------- : Color inversion

void invert(Image& img) {
  Byte* data = img.GetData();
  int n = 3 * img.GetWidth() * img.GetHeight();
  for (int i = 0 ; i < n ; ++i) {
    data[i] = 255 - data[i];
  }
}

// ----------------------------------------------------------------------------- : Image mask

Image make_visibility_mask(Image& img, int threshold, int radius) {
  int w = img.GetWidth(), h = img.GetHeight();
  Image mask(w, h);
  assert(mask.Ok());
  Byte* out = mask.GetData();
  if (!img.HasAlpha()) {
    // No alpha channel at all: every pixel is fully opaque, so the mask is all black
    memset(out, 0, 3 * w * h);
    return mask;
  }
  radius = max(0, radius);
  Byte* alpha = img.GetAlpha();
  for (int y = 0 ; y < h ; ++y) {
    for (int x = 0 ; x < w ; ++x) {
      bool white = false;
      // check this pixel and its neighbors (within radius) for alpha <= threshold
      for (int dy = -radius ; dy <= radius && !white ; ++dy) {
        int ny = y + dy;
        if (ny < 0 || ny >= h) continue;
        for (int dx = -radius ; dx <= radius && !white ; ++dx) {
          int nx = x + dx;
          if (nx < 0 || nx >= w) continue;
          if (alpha[ny * w + nx] <= threshold) {
            white = true;
          }
        }
      }
      Byte value = white ? 255 : 0;
      int i = (y * w + x) * 3;
      out[i] = out[i+1] = out[i+2] = value;
    }
  }
  return mask;
}

// ----------------------------------------------------------------------------- : Bleed edge

bool mirror_bleed_edge(Image& img, int bleed_x, int bleed_y) {
  int width = img.GetWidth(), height = img.GetHeight();
  bleed_x = max(0, bleed_x);
  bleed_y = max(0, bleed_y);
  if (3 * bleed_x >= width || 3 * bleed_y >= height) {
    queue_message(MESSAGE_ERROR, _("Image too small to add bleed edge"));
    return false;
  }
  if (!img.HasAlpha()) img.InitAlpha();
  Byte* pixels = img.GetData();
  Byte* alpha = img.GetAlpha();
  auto copy_pixel = [&](int dst, int src) {
    pixels[3 * dst + 0] = pixels[3 * src + 0];
    pixels[3 * dst + 1] = pixels[3 * src + 1];
    pixels[3 * dst + 2] = pixels[3 * src + 2];
    alpha[dst] = alpha[src];
  };
  // fill left and right borders
  for (int y = bleed_y; y < height - bleed_y; ++y) {
    int row = y * width;
    for (int x = 0; x < bleed_x; ++x) {
      copy_pixel(row + x,                   row + 2 * bleed_x - x);
      copy_pixel(row + width - bleed_x + x, row + width - bleed_x - 2 - x);
    }
  }
  // fill top and bottom borders (full width, so this also fills the corners)
  for (int y = 0; y < bleed_y; ++y) {
    for (int x = 0; x < width; ++x) {
      copy_pixel(x + y * width,                      x + (2 * bleed_y - y) * width);
      copy_pixel(x + (height - bleed_y + y) * width, x + (height - bleed_y - 2 - y) * width);
    }
  }
  return true;
}

// ----------------------------------------------------------------------------- : Blurring

Byte blur_pixel_alpha(Byte* in, int x, int y, int width, int height, int center_weight) {
  return (  center_weight * in[0]                + // center
         (x == 0          ? in[0] : in[-1])      + // left
         (y == 0          ? in[0] : in[-width])  + // up
         (x == width - 1  ? in[0] : in[1])       + // right
         (y == height - 1 ? in[0] : in[width])     // down
         ) / (4 + center_weight);
}

void blur_image_alpha(Image& img, int center_weight) {
  if (!img.HasAlpha()) return;
  int width = img.GetWidth(), height = img.GetHeight();
  Byte* data = img.GetAlpha();
  for (int y = 0 ; y < height ; ++y) {
    for (int x = 0 ; x < width ; ++x) {
      *data = blur_pixel_alpha(data, x, y, width, height, center_weight);
      ++data;
    }
  }
}

// ----------------------------------------------------------------------------- : Thickening

Byte thicken_pixel_alpha_7x7(const std::vector<unsigned char>& in, int i, int x, int y, int width, int height) {
  Byte result = in[i];
  Byte diag = in[i];
  Byte corner = in[i];
  if (x > 2) {
    if (y > 1)          corner = max(corner, in[i -3 -2*width]);
    if (y > 0)          diag   = max(diag  , in[i -3 -1*width]);
                        result = max(result, in[i -3         ]);
    if (y < height - 1) diag   = max(diag  , in[i -3 +1*width]);
    if (y < height - 2) corner = max(corner, in[i -3 +2*width]);
  }
  if (x > 1) {
    if (y > 2)          corner = max(corner, in[i -2 -3*width]);
    if (y > 1)          result = max(result, in[i -2 -2*width]);
    if (y > 0)          result = max(result, in[i -2 -1*width]);
                        result = max(result, in[i -2         ]);
    if (y < height - 1) result = max(result, in[i -2 +1*width]);
    if (y < height - 2) result = max(result, in[i -2 +2*width]);
    if (y < height - 3) corner = max(corner, in[i -2 +3*width]);
  }
  if (x > 0) {
    if (y > 2)          diag   = max(diag  , in[i -1 -3*width]);
    if (y > 1)          result = max(result, in[i -1 -2*width]);
    if (y > 0)          result = max(result, in[i -1 -1*width]);
                        result = max(result, in[i -1         ]);
    if (y < height - 1) result = max(result, in[i -1 +1*width]);
    if (y < height - 2) result = max(result, in[i -1 +2*width]);
    if (y < height - 3) diag   = max(diag  , in[i -1 +3*width]);
  }
  
    if (y > 2)          result = max(result, in[i    -3*width]);
    if (y > 1)          result = max(result, in[i    -2*width]);
    if (y > 0)          result = max(result, in[i    -1*width]);

    if (y < height - 1) result = max(result, in[i    +1*width]);
    if (y < height - 2) result = max(result, in[i    +2*width]);
    if (y < height - 3) result = max(result, in[i    +3*width]);

  if (x < width - 1) {
    if (y > 2)          diag   = max(diag  , in[i +1 -3*width]);
    if (y > 1)          result = max(result, in[i +1 -2*width]);
    if (y > 0)          result = max(result, in[i +1 -1*width]);
                        result = max(result, in[i +1         ]);
    if (y < height - 1) result = max(result, in[i +1 +1*width]);
    if (y < height - 2) result = max(result, in[i +1 +2*width]);
    if (y < height - 3) diag   = max(diag  , in[i +1 +3*width]);
  }
  if (x < width - 2) {
    if (y > 2)          corner = max(corner, in[i +2 -3*width]);
    if (y > 1)          result = max(result, in[i +2 -2*width]);
    if (y > 0)          result = max(result, in[i +2 -1*width]);
                        result = max(result, in[i +2         ]);
    if (y < height - 1) result = max(result, in[i +2 +1*width]);
    if (y < height - 2) result = max(result, in[i +2 +2*width]);
    if (y < height - 3) corner = max(corner, in[i +2 +3*width]);
  }
  if (x < width - 3) {
    if (y > 1)          corner = max(corner, in[i +3 -2*width]);
    if (y > 0)          diag   = max(diag  , in[i +3 -1*width]);
                        result = max(result, in[i +3         ]);
    if (y < height - 1) diag   = max(diag  , in[i +3 +1*width]);
    if (y < height - 2) corner = max(corner, in[i +3 +2*width]);
  }
  if (diag   > result) result = (Byte)((4 * (int)diag   +     (int)result) / 5);
  if (corner > result) result = (Byte)((2 * (int)corner + 3 * (int)result) / 5);
  return result;
}

Byte thicken_pixel_alpha_5x5(const std::vector<unsigned char>& in, int i, int x, int y, int width, int height) {
  Byte result = in[i];
  Byte diag = in[i];
  if (x > 1) {
    if (y > 0)          diag   = max(diag  , in[i -2 -1*width]);
                        result = max(result, in[i -2         ]);
    if (y < height - 1) diag   = max(diag  , in[i -2 +1*width]);
  }
  if (x > 0) {
    if (y > 1)          diag   = max(diag  , in[i -1 -2*width]);
    if (y > 0)          result = max(result, in[i -1 -1*width]);
                        result = max(result, in[i -1         ]);
    if (y < height - 1) result = max(result, in[i -1 +1*width]);
    if (y < height - 2) diag   = max(diag  , in[i -1 +2*width]);
  }

    if (y > 1)          result = max(result, in[i    -2*width]);
    if (y > 0)          result = max(result, in[i    -1*width]);

    if (y < height - 1) result = max(result, in[i    +1*width]);
    if (y < height - 2) result = max(result, in[i    +2*width]);

  if (x < width - 1) {
    if (y > 1)          diag   = max(diag  , in[i +1 -2*width]);
    if (y > 0)          result = max(result, in[i +1 -1*width]);
                        result = max(result, in[i +1         ]);
    if (y < height - 1) result = max(result, in[i +1 +1*width]);
    if (y < height - 2) diag   = max(diag  , in[i +1 +2*width]);
  }
  if (x < width - 2) {
    if (y > 0)          diag   = max(diag  , in[i +2 -1*width]);
                        result = max(result, in[i +2         ]);
    if (y < height - 1) diag   = max(diag  , in[i +2 +1*width]);
  }
  if (diag > result) result = (Byte)((3 * (int)diag + (int)result) / 4);
  return result;
}

Byte thicken_pixel_alpha_3x3(const std::vector<unsigned char>& in, int i, int x, int y, int width, int height) {
  Byte result = in[i];
  Byte diag = in[i];
  if (x > 0) {
    if (y > 0)          diag   = max(diag,   in[i -1 -width]);
                        result = max(result, in[i -1       ]);
    if (y < height - 1) diag   = max(diag,   in[i -1 +width]);
  }
    if (y > 0)          result = max(result, in[i    -width]);

    if (y < height - 1) result = max(result, in[i    +width]);
  if (x < width - 1) {
    if (y > 0)          diag   = max(diag,   in[i +1 -width]);
                        result = max(result, in[i +1       ]);
    if (y < height - 1) diag   = max(diag,   in[i +1 +width]);
  }
  if (diag > result) result = (Byte)((3*(int)diag + 2*(int)result) / 5);
  return result;
}

void thicken_image_alpha(Image& img, int radius) {
  if (!img.HasAlpha()) return;
  int width = img.GetWidth(), height = img.GetHeight();
  Byte* data = img.GetAlpha();
  std::vector<Byte> copy(data, data + width * height);
  int i = 0;
  for (int y = 0 ; y < height ; ++y) {
    for (int x = 0 ; x < width ; ++x) {
      *data = radius == 1 ? thicken_pixel_alpha_3x3(copy, i, x, y, width, height) :
              radius == 2 ? thicken_pixel_alpha_5x5(copy, i, x, y, width, height) :
                            thicken_pixel_alpha_7x7(copy, i, x, y, width, height) ;
      ++data;
      ++i;
    }
  }
}

// ----------------------------------------------------------------------------- : Resampled text

void downsample_to_alpha(Bitmap& bmp_in, Image& img_out) {
  Byte* temp = nullptr;
#if defined(__WXMSW__) && wxUSE_WXDIB
  wxDIB img_in(bmp_in);
  if (!img_in.IsOk()) return;
  // if text_scaling = 4, then the line always is dword aligned, so we need no adjusting
  // we created a bitmap with depth 24, so that is what we should have here
  if (img_in.GetDepth() != 24) throw InternalError(_("DIB has wrong bit depth"));
#else
  Image img_in = bmp_in.ConvertToImage();
#endif
  Byte* in  = img_in.GetData();
  Byte* out = img_in.GetData();
  // scale in the x direction, this overwrites parts of the input image
  if (img_in.GetWidth() == img_out.GetWidth() * text_scaling) {
    // no stretching
    int count = img_out.GetWidth() * img_in.GetHeight();
    for (int i = 0 ; i < count ; ++i) {
      int total = 0;
      for (int j = 0 ; j < text_scaling ; ++j) {
        total += in[3 * (j + text_scaling * i)];
      }
      out[i] = total / text_scaling;
    }
  } else {
    // resample to buffer
    temp = new Byte[img_out.GetWidth() * img_in.GetHeight()];
    out = temp;
    // custom stretch, see resample_image.cpp
    const int shift = 32-12-8; // => max size = 4096, max alpha = 255
    int w1 = img_in.GetWidth(), w2 = img_out.GetWidth(), h = img_in.GetHeight();
    int out_fact = (w2 << shift) / w1; // how much to output for 256 input = 1 pixel
    int out_rest = (w2 << shift) % w1;
    // make the image 'bolder' to compensate for compressing it
    int mul = 128 + min(256, 128*w1/(text_scaling*w2));
    for (int y = 0 ; y < h ; ++y) {
      int in_rem = out_fact + out_rest;
      for (int x = 0 ; x < w2 ; ++x) {
        int out_rem = 1 << shift;
        int tot = 0;
        while (out_rem >= in_rem) {
          // eat a whole input pixel
          tot += *in * in_rem;
          out_rem -= in_rem;
          in_rem = out_fact;
          in += 3;
        }
        if (out_rem > 0) {
          // eat a partial input pixel
          tot += *in * out_rem;
          in_rem -= out_rem;
        }
        // store
        *out = top(((tot >> shift) * mul) >> 8);
        out += 1;
      }
    }
    in = temp;
  }

  // now scale in the y direction, and write to the output alpha
  img_out.InitAlpha();
  int line_size_in = img_out.GetWidth();
#if defined(__WXMSW__) && wxUSE_WXDIB
  // DIBs are upside down
  out = img_out.GetAlpha() + (img_out.GetHeight() - 1) * line_size_in;
  int line_size_out = -line_size_in;
#else
  out = img_out.GetAlpha();
  int line_size_out = line_size_in;
#endif
  int h = img_out.GetHeight();
  if (img_in.GetHeight() == h * text_scaling) {
    // no stretching
    for (int y = 0 ; y < h ; ++y) {
      for (int x = 0 ; x < line_size_in ; ++x) {
        int total = 0;
        for (int j = 0 ; j < text_scaling ; ++j) {
          total += in[x + line_size_in * (j + text_scaling * y)];
        }
        out[x + line_size_out * y] = total / text_scaling;
      }
    }
  } else {
    const int shift = 32-12-8; // => max size = 4096, max alpha = 255
    int h1 = img_in.GetHeight(), w = img_out.GetWidth();
    int out_fact = (h << shift) / h1; // how much to output for 256 input = 1 pixel
    int out_rest = (h << shift) % h1;
    int mul = 128 + min(256, 128*h1/(text_scaling*h));
    for (int x = 0 ; x < w ; ++x) {
      int in_rem = out_fact + out_rest;
      for (int y = 0 ; y < h ; ++y) {
        int out_rem = 1 << shift;
        int tot = 0;
        while (out_rem >= in_rem) {
          // eat a whole input pixel
          tot += *in * in_rem;
          out_rem -= in_rem;
          in_rem = out_fact;
          in += line_size_in;
        }
        if (out_rem > 0) {
          // eat a partial input pixel
          tot += *in * out_rem;
          in_rem -= out_rem;
        }
        // store
        *out = top(((tot >> shift) * mul) >> 8);
        out += line_size_out;
      }
      in  = in  - h1 * line_size_in  + 1;
      out = out - h  * line_size_out + 1;
    }
  }

  delete[] temp;
}

Image make_stroke_image(Image& img, Color stroke_color, int stroke_radius, int blur_radius) {
  stroke_radius = max(0,min(100,stroke_radius));
  blur_radius   = max(0,min(100,blur_radius));
  if (!img.HasAlpha()) set_alpha(img, 1.0);
  int margin = blur_radius + stroke_radius;
  int s_width = img.GetWidth() + 2 * margin, s_height = img.GetHeight() + 2 * margin;
  int x_end = s_width - margin;
  int y_end = s_height - margin;
  wxImage s_img(s_width, s_height, false);
  s_img.InitAlpha();
  // convert to stroke color
  Byte* s_data = s_img.GetData();
  Byte* s_alpha = s_img.GetAlpha(), *alpha = img.GetAlpha();
  unsigned char r = stroke_color.Red();
  unsigned char g = stroke_color.Green();
  unsigned char b = stroke_color.Blue();
  unsigned char a = stroke_color.Alpha();
  for (int y = 0 ; y < s_height ; ++y) {
    for (int x = 0 ; x < s_width ; ++x) {
      s_data[0] = r;
      s_data[1] = g;
      s_data[2] = b;
      s_data += 3;
      if (margin <= x && x < x_end && margin <= y && y < y_end) {
        s_alpha[0] = alpha[0] * a / 255;
        alpha += 1;
      }
      else {
        s_alpha[0] = 0;
      }
      s_alpha += 1;
    }
  }
  // add stroke effect
  for (int i = 0 ; i < stroke_radius ; ++i) {
    thicken_image_alpha(s_img, 1);
  }
  // add blur
  for (int i = 0 ; i < blur_radius ; ++i) {
    blur_image_alpha(s_img, 3);
  }
  // transfer metadata
  if (img.HasOption(wxIMAGE_OPTION_PNG_DESCRIPTION)) {
    String metadata = transformAllEncodedRects(img.GetOption(wxIMAGE_OPTION_PNG_DESCRIPTION), RealRect::translate, margin, margin);
    s_img.SetOption(wxIMAGE_OPTION_PNG_DESCRIPTION, metadata);
  }
  return s_img;
}

// ----------------------------------------------------------------------------- : Coloring symbol images

RGB recolor(RGB x, RGB cr, RGB cg, RGB cb, RGB cw) {
  int lo = min(x.r,min(x.g,x.b));
  // amount of each
  int nr = x.r - lo;
  int ng = x.g - lo;
  int nb = x.b - lo;
  int nw = lo;
  // We should have that nr+ng+bw+nw < 255,
  //  otherwise the input is not a mixture of red/green/blue/white.
  // Just to be sure, divide by the sum instead of 255
  int total = max(255, nr+ng+nb+nw);
  
  return RGB(
      static_cast<Byte>( (nr * cr.r + ng * cg.r + nb * cb.r + nw * cw.r) / total ),
      static_cast<Byte>( (nr * cr.g + ng * cg.g + nb * cb.g + nw * cw.g) / total ),
      static_cast<Byte>( (nr * cr.b + ng * cg.b + nb * cb.b + nw * cw.b) / total )
    );
}

void recolor(Image& img, RGB cr, RGB cg, RGB cb, RGB cw) {
  RGB* data = (RGB*)img.GetData();
  int n = img.GetWidth() * img.GetHeight();
  for (int i = 0 ; i < n ; ++i) {
    data[i] = recolor(data[i], cr, cg, cb, cw);
  }
}

Byte to_grayscale(RGB x) {
  return (Byte)((6969 * x.r + 23434 * x.g + 2365 * x.b) / 32768); // from libpng
}

void recolor(Image& img, RGB cr) {
  RGB black(0,0,0), white(255,255,255);
  bool dark = to_grayscale(cr) < 100;
  recolor(img, cr, dark ? black : white, dark ? white : black, white);
}

