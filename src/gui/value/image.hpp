//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/value/editor.hpp>
#include <render/value/image.hpp>

// ----------------------------------------------------------------------------- : ImageValueEditor

/// An editor 'control' for editing ImageValues
class ImageValueEditor : public ImageValueViewer, public ValueEditor {
public:
  DECLARE_VALUE_EDITOR(Image);

  void draw(RotatedDC& dc) override;
  void determineSize(bool) override;
  void onValueChange() override;
  bool onLeftDClick(const RealPoint&, wxMouseEvent&) override;

  // --------------------------------------------------- : Clipboard
  
  bool canCopy() const override;
  bool canPaste() const override;
  bool doCopy() override;
  bool doPaste() override;
  bool doDelete() override;
  
  bool onChar(wxKeyEvent&) override;

  // --------------------------------------------------- : Resetting

  bool canDefaultReset() const override { return true; }
  void doDefaultReset() override;

private:
  // Open the image slice window showing the given image
  void sliceImage(const Image&, const String& filename, const String& cardname);

  // --------------------------------------------------- : Native look preview

  /// (Re)create preview_bitmap so that it fits in a w*h area, if it is not up to date yet
  void updatePreview(int w, int h);

  Bitmap preview_bitmap;        ///< Cached preview of the image, only used for the native look
  int    preview_w = 0;         ///< Size of the area preview_bitmap was made for
  int    preview_h = 0;
  bool   preview_valid = false; ///< Is the cache up to date? (also true if there is no image to show)
};

