//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/gfx.hpp
 *
 *  Graphics/image processing functions.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/real_point.hpp>
#include <util/angle.hpp>
#include <gfx/color.hpp>

// ----------------------------------------------------------------------------- : Resampling

/// Resample (resize) an image, uses bilenear filtering
void resample(const Image& img_in, Image& img_out);
Image resample(const Image& img_in, int width, int height);
Image resample(const Image& img_in, double zoom);

/// Resamples an image, first clips the input image to a specified rectangle
/** The selected rectangle is resampled into the entire output image */
void resample_and_clip(const Image& img_in, Image& img_out, wxRect rect);

/// How to preserve the aspect ratio of an image when rescaling
enum PreserveAspect
{  ASPECT_STRETCH    ///< don't preserve
,  ASPECT_BORDER    ///< put borders around the image to make it the right shape
,  ASPECT_FIT      ///< generate a smaller image if needed
};

/// Resample an image, but preserve the aspect ratio by adding a transparent border around the output if needed.
void resample_preserve_aspect(const Image& img_in, Image& img_out);
Image resample_preserve_aspect(const Image& img_in, int width, int height);

/// Resample an image to create a sharp result by applying a sharpening filter
/** Amount must be between 0 and 100 */
void sharp_resample(const Image& img_in, Image& img_out, int amount);

/// Sharpening version of of resample_and_clip
void sharp_resample_and_clip(const Image& img_in, Image& img_out, wxRect rect, int amount);

/// Draw text by first drawing it using a larger font and then downsampling it
/** optionally rotated by an angle.
 *  pos     = the position to draw
 *  rect    = rectangle to draw in (a rectangle somewhere around pos)
 *  stretch = amount to stretch in the direction of the text after drawing
 */
void draw_resampled_text(DC& dc, const String& text, const RealPoint& pos, const RealRect& rect, Radians angle, Color color, int blur_radius = 0, Color stroke_color = Color(0,0,0), int stroke_radius = 0, double stretch = 1.0);

// scaling factor to use when drawing resampled text
extern const int text_scaling;

// Downsamples the red channel of the input image to the alpha channel of the output image
// img_in must be text_scaling times as large as img_out
void downsample_to_alpha(Bitmap& bmp_in, Image& img_out);

/// Resize an image, but keep a border of unchanged pixels along
/// each edge (9-slice scaling): the corners are copied verbatim, the top/bottom
/// edges are stretched only horizontally, the left/right edges are stretched only
/// vertically, and only the remaining center is stretched in both directions.
void resample_nine_slice(const Image& img_in, Image& img_out, int left, int right, int top, int bottom);

// ----------------------------------------------------------------------------- : Crop

Image crop(const Image& base_img, int width, int height, int offset_x, int offset_y, const Color& background_color);

// ----------------------------------------------------------------------------- : Image rotation

/// Rotates an image counter clockwise
Image rotate_image(const Image& image, Radians angle);

/// Flip an image horizontally
Image flip_image_horizontal(const Image& image);
/// Flip an image vertically
Image flip_image_vertical(const Image& image);

// ----------------------------------------------------------------------------- : Blending

/// Blends two images together using some linear gradient
/** The result is stored in img1
 *  The two coordinates give the two points between which the images are blended
 *  Coordinates are given in the range [0..1);
 */
void linear_blend(Image& img1, const Image& img2, double x1,double y1, double x2,double y2);

/// Blends two images together, using a third image as a mask
/** The result is stored in img1
 *  mask is used as a mask, white pixels are taken from img1, black pixels from img2
 *  color channels are blended separatly
 */
void mask_blend(Image& light, const Image& dark, const Image& mask);

// ----------------------------------------------------------------------------- : Effects

/// Saturate an image
void saturate(Image& image, double amount);

/// Brighten an image
void brighten(Image& image, double amount);

/// Invert the colors in an image
void invert(Image& img);

// Blur the alpha of a pixel
Byte blur_pixel_alpha(Byte* in, int x, int y, int width, int height, int center_weight = 2);
// Blur the alpha channel of an image
void blur_image_alpha(Image& img, int center_weight = 2);

// Thicken the alpha of a pixel
Byte thicken_pixel_alpha_3x3(const std::vector<Byte>& in, int i, int x, int y, int width, int height);
Byte thicken_pixel_alpha_5x5(const std::vector<Byte>& in, int i, int x, int y, int width, int height);
Byte thicken_pixel_alpha_7x7(const std::vector<Byte>& in, int i, int x, int y, int width, int height);
// Thicken the alpha channel of an image
void thicken_image_alpha(Image& img, int radius);

// Create a stroke effect that goes around an image
Image make_stroke_image(Image& img, Color stroke_color, int stroke_radius, int blur_radius = 0);

// Create a mask that encodes which pixels are or neighbor transparent pixels
Image make_visibility_mask(Image& img, int threshold, int radius);

/// Extend an image by mirroring the pixels on its edges
bool mirror_bleed_edge(Image& img, int bleed_x, int bleed_y);

// ----------------------------------------------------------------------------- : Combining

/// Ways in which images can be combined, similair to what Photoshop supports
enum ImageCombine
{  COMBINE_DEFAULT  // normal combine, but with a low priority, i.e. "apply default instead of add" == "add"
                  // it is not representable in scripting/files, so should only be used internally
,  COMBINE_NORMAL
,  COMBINE_ADD
,  COMBINE_SUBTRACT
,  COMBINE_AVERAGE
,  COMBINE_STAMP
,  COMBINE_DIFFERENCE
,  COMBINE_NEGATION
,  COMBINE_MULTIPLY
,  COMBINE_DARKEN
,  COMBINE_LIGHTEN
,  COMBINE_COLOR_DODGE
,  COMBINE_COLOR_BURN
,  COMBINE_SCREEN
,  COMBINE_OVERLAY
,  COMBINE_HARD_LIGHT
,  COMBINE_SOFT_LIGHT
,  COMBINE_REFLECT
,  COMBINE_GLOW
,  COMBINE_FREEZE
,  COMBINE_HEAT
,  COMBINE_AND
,  COMBINE_OR
,  COMBINE_XOR
,  COMBINE_SHADOW
,  COMBINE_LIGHTMAP_ABSOLUTE
,  COMBINE_LIGHTMAP_RELATIVE
,  COMBINE_SYMMETRIC_OVERLAY
,  COMBINE_BRIGHTNESS_TO_ALPHA
,  COMBINE_DARKNESS_TO_ALPHA
,  COMBINE_GREATER_THAN_5
,  COMBINE_GREATER_THAN_10
,  COMBINE_GREATER_THAN_15
,  COMBINE_GREATER_THAN_20
,  COMBINE_GREATER_THAN_25
,  COMBINE_GREATER_THAN_30
,  COMBINE_GREATER_THAN_35
,  COMBINE_GREATER_THAN_40
,  COMBINE_GREATER_THAN_45
,  COMBINE_GREATER_THAN_50
,  COMBINE_GREATER_THAN_55
,  COMBINE_GREATER_THAN_60
,  COMBINE_GREATER_THAN_65
,  COMBINE_GREATER_THAN_70
,  COMBINE_GREATER_THAN_75
,  COMBINE_GREATER_THAN_80
,  COMBINE_GREATER_THAN_85
,  COMBINE_GREATER_THAN_90
,  COMBINE_GREATER_THAN_95
,  COMBINE_GREATER_THAN_100
,  COMBINE_GREATER_THAN_105
,  COMBINE_GREATER_THAN_110
,  COMBINE_GREATER_THAN_115
,  COMBINE_GREATER_THAN_120
,  COMBINE_GREATER_THAN_125
,  COMBINE_GREATER_THAN_130
,  COMBINE_GREATER_THAN_135
,  COMBINE_GREATER_THAN_140
,  COMBINE_GREATER_THAN_145
,  COMBINE_GREATER_THAN_150
,  COMBINE_GREATER_THAN_155
,  COMBINE_GREATER_THAN_160
,  COMBINE_GREATER_THAN_165
,  COMBINE_GREATER_THAN_170
,  COMBINE_GREATER_THAN_175
,  COMBINE_GREATER_THAN_180
,  COMBINE_GREATER_THAN_185
,  COMBINE_GREATER_THAN_190
,  COMBINE_GREATER_THAN_195
,  COMBINE_GREATER_THAN_200
,  COMBINE_GREATER_THAN_205
,  COMBINE_GREATER_THAN_210
,  COMBINE_GREATER_THAN_215
,  COMBINE_GREATER_THAN_220
,  COMBINE_GREATER_THAN_225
,  COMBINE_GREATER_THAN_230
,  COMBINE_GREATER_THAN_235
,  COMBINE_GREATER_THAN_240
,  COMBINE_GREATER_THAN_245
,  COMBINE_GREATER_THAN_250
,  COMBINE_SMALLER_THAN_5
,  COMBINE_SMALLER_THAN_10
,  COMBINE_SMALLER_THAN_15
,  COMBINE_SMALLER_THAN_20
,  COMBINE_SMALLER_THAN_25
,  COMBINE_SMALLER_THAN_30
,  COMBINE_SMALLER_THAN_35
,  COMBINE_SMALLER_THAN_40
,  COMBINE_SMALLER_THAN_45
,  COMBINE_SMALLER_THAN_50
,  COMBINE_SMALLER_THAN_55
,  COMBINE_SMALLER_THAN_60
,  COMBINE_SMALLER_THAN_65
,  COMBINE_SMALLER_THAN_70
,  COMBINE_SMALLER_THAN_75
,  COMBINE_SMALLER_THAN_80
,  COMBINE_SMALLER_THAN_85
,  COMBINE_SMALLER_THAN_90
,  COMBINE_SMALLER_THAN_95
,  COMBINE_SMALLER_THAN_100
,  COMBINE_SMALLER_THAN_105
,  COMBINE_SMALLER_THAN_110
,  COMBINE_SMALLER_THAN_115
,  COMBINE_SMALLER_THAN_120
,  COMBINE_SMALLER_THAN_125
,  COMBINE_SMALLER_THAN_130
,  COMBINE_SMALLER_THAN_135
,  COMBINE_SMALLER_THAN_140
,  COMBINE_SMALLER_THAN_145
,  COMBINE_SMALLER_THAN_150
,  COMBINE_SMALLER_THAN_155
,  COMBINE_SMALLER_THAN_160
,  COMBINE_SMALLER_THAN_165
,  COMBINE_SMALLER_THAN_170
,  COMBINE_SMALLER_THAN_175
,  COMBINE_SMALLER_THAN_180
,  COMBINE_SMALLER_THAN_185
,  COMBINE_SMALLER_THAN_190
,  COMBINE_SMALLER_THAN_195
,  COMBINE_SMALLER_THAN_200
,  COMBINE_SMALLER_THAN_205
,  COMBINE_SMALLER_THAN_210
,  COMBINE_SMALLER_THAN_215
,  COMBINE_SMALLER_THAN_220
,  COMBINE_SMALLER_THAN_225
,  COMBINE_SMALLER_THAN_230
,  COMBINE_SMALLER_THAN_235
,  COMBINE_SMALLER_THAN_240
,  COMBINE_SMALLER_THAN_245
,  COMBINE_SMALLER_THAN_250
};

String combine_to_string(const ImageCombine&);

/// Combine image b onto image a using some combining function.
/// The results are stored in the image A.
/// This image gets the alpha channel from B, it should then be
/// drawn onto the area where A originated.
void combine_image(Image& a, const Image& b, ImageCombine combine);

/// Draw an image to a DC using a combining function
void draw_combine_image(DC& dc, UInt x, UInt y, const Image& img, ImageCombine combine);

// ----------------------------------------------------------------------------- : Masks

/// Use the red channel of img_alpha as alpha channel for img
void set_alpha(Image& img, const Image& img_alpha);
/// Use the given bytes as alpha channel for img
void set_alpha(Image& img, Byte* alphas, const wxSize& alphas_size);
/// Set the transparency of img
void set_alpha(Image& img, double alpha);

/// An alpha mask is an alpha channel that can be copied to another image
/** It is created by treating black in the source image as transparent and white (red) as opaque
 */
class AlphaMask : public IntrusivePtrBase<AlphaMask> {
public:
  AlphaMask();
  AlphaMask(const Image& mask);
  ~AlphaMask();
  
  /// Load an alpha mask
  void load(const Image& image);
  /// Unload the mask
  void clear();
  
  /// Apply the alpha mask to an image
  void setAlpha(Image& i) const;
  /// Apply the alpha mask to a bitmap
  void setAlpha(Bitmap& b) const;
  
  /// Is the given location opaque (not fully transparent)? when the mask were stretched to size
  bool isOpaque(const RealPoint& p, const RealSize& size) const;
  bool isOpaque(int x, int y) const;
  
  /// Determine a convex hull polygon *around* the mask
  void convexHull(vector<wxPoint>& points) const;
  
  /// Make an image of the given color using this mask
  Image colorImage(const Color& color) const;
  
  /// Returns the start of a row, when the mask were stretched to size
  /** This is: the x coordinate of the first non-transparent pixel */
  double rowLeft (double y, const RealSize& size) const;
  /// Returns the end of a row, when the mask were stretched to size
  double rowRight(double y, const RealSize& size) const;
  
  /// Does this mask have the given size?
  inline bool hasSize(const wxSize& compare_size) const { return size == compare_size; }
  /// Is the mask loaded?
  inline bool isLoaded() const { return alpha; }
  
private:
  wxSize size; ///< Size of the mask
  Byte* alpha; ///< Data of alpha mask
  mutable int *lefts, *rights; ///< Row sizes
  
  /// Compute lefts and rights from alpha
  void loadRowSizes() const;
};

