//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/value/image.hpp>
#include <gui/image_slice_window.hpp>
#include <data/format/clipboard.hpp>
#include <data/action/value.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <wx/clipbrd.h>
#include <gui/util.hpp>

// ----------------------------------------------------------------------------- : ImageValueEditor

IMPLEMENT_VALUE_EDITOR(Image) {}

bool ImageValueEditor::onLeftDClick(const RealPoint&, wxMouseEvent&) {
  String directory = settings.default_image_dir;
  String filename = _("");
  CardP card = parent.getCard();
  String cardname = card ? card->identification() : _("clipboard"); // use card.uid instead of card->identification() ?
  if (ImageSliceWindow::previously_used_settings_path.find(cardname) != ImageSliceWindow::previously_used_settings_path.end()) {
    String filepath = ImageSliceWindow::previously_used_settings_path[cardname];
    size_t pos = filepath.rfind(wxFileName::GetPathSeparator());
    if (pos != String::npos) {
      directory = filepath.substr(0, pos+1);
      filename = filepath.substr(pos+1);
    }
  }
  wxFileDialog dlg(wxGetTopLevelParent(&editor()), _TITLE_("open image file"), directory, filename,
    _("All images|*.bmp;*.jpg;*.jpeg;*.png;*.webp;*.gif;*.tif;*.tiff|Windows bitmaps (*.bmp)|*.bmp|JPEG images (*.jpg;*.jpeg)|*.jpg;*.jpeg|PNG images (*.png)|*.png|WebP images (*.webp)|*.webp|GIF images (*.gif)|*.gif|TIFF images (*.tif;*.tiff)|*.tif;*.tiff"),
    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_OK) {
    filename = dlg.GetPath();
    settings.default_image_dir = wxPathOnly(filename);
    wxImage image;
    {
      wxLogNull noLog;
      image = wxImage(filename);
    }
    if (!image.Ok()) queue_message(MESSAGE_ERROR, _ERROR_("can't load image"));
    else sliceImage(image, filename, cardname);
  }
  return true;
}

void ImageValueEditor::sliceImage(const Image& image, const String& filename, const String& cardname) {
  if (!image.Ok()) return;
  // determine import scale based on the user's settings.
  double import_scale = 1.0;
  StyleSheetP stylesheet = editor().getCard()->stylesheet;
  if (!stylesheet) stylesheet = editor().getSet()->stylesheet;
  if (stylesheet) import_scale = settings.importScaleSettingsFor(*stylesheet);
  RealSize target_size = RealSize(style().getSize() * import_scale);
  target_size = RealSize((int)target_size.width, (int)target_size.height);
  // mask
  GeneratedImage::Options options((int)target_size.width, (int)target_size.height, &parent.getStylePackage(), &parent.getLocalPackage());
  AlphaMask mask;
  style().mask.getNoCache(options, mask);
  // slice
  ImageSliceWindow s(wxGetTopLevelParent(&editor()), image, filename, cardname, target_size, mask);
  // clicked ok?
  if (s.ShowModal() == wxID_OK) {
    // store the image into the set
    LocalFileName new_image_file = getLocalPackage().newFileName(field().name, _(".png")); // a new unique name in the package
    Image img = s.getImage();
    img.SaveFile(getLocalPackage().nameOut(new_image_file), wxBITMAP_TYPE_PNG); // always use PNG images, see #69. Disk space is cheap anyway.
    addAction(value_action(valueP(), new_image_file));
  }
}

// ----------------------------------------------------------------------------- : Clipboard

bool ImageValueEditor::canCopy() const {
  return !value().filename.empty();
}

bool ImageValueEditor::canPaste() const {
  return wxTheClipboard->IsSupported(wxDF_BITMAP) &&
        !wxTheClipboard->IsSupported(CardsDataObject::format); // we don't want to (accidentally) paste card images
}

bool ImageValueEditor::doCopy() {
  // load image
  auto image_file = getLocalPackage().openIn(value().filename);
  Image image;
  if (!image_load_file(image, *image_file)) return false;
  // set data
  if (!wxTheClipboard->Open()) return false;
  bool ok = wxTheClipboard->SetData(new wxBitmapDataObject(image));
  wxTheClipboard->Flush();
  wxTheClipboard->Close();
  return ok;
}

bool ImageValueEditor::doPaste() {
  // get bitmap
  if (!wxTheClipboard->Open()) return false;
  wxBitmapDataObject data;
  bool ok = wxTheClipboard->GetData(data);
  wxTheClipboard->Flush();
  wxTheClipboard->Close();
  if (!ok)  return false;
  // slice
  CardP card = parent.getCard();
  String cardname = card ? card->identification() : _("clipboard");
  sliceImage(data.GetBitmap().ConvertToImage(), _("clipboard"), cardname);
  return true;
}

bool ImageValueEditor::doDelete() {
  addAction(value_action(valueP(), LocalFileName()));
  return true;
}


bool ImageValueEditor::onChar(wxKeyEvent& ev) {
  if (ev.AltDown() || ev.ShiftDown() || ev.ControlDown()) return false;
  switch (ev.GetKeyCode()) {
    case WXK_DELETE:
      doDelete();
      return true;
    default:
      return false;
  }
}

// ----------------------------------------------------------------------------- : Resetting

void ImageValueEditor::doDefaultReset() {
  if (!valueP()) return;
  unique_ptr<ResetValueAction<ImageValue, false>> action = make_unique<ResetValueAction<ImageValue, false>>(valueP());
  addAction(std::move(action));
}
