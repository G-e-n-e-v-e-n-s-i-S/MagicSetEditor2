//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/welcome_window.hpp>
#include <gui/util.hpp>
#include <gui/new_window.hpp>
#include <gui/set/window.hpp>
#include <gui/update_checker.hpp>
#include <gui/packages_window.hpp>
#include <util/window_id.hpp>
#include <data/settings.hpp>
#include <data/format/formats.hpp>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <util/io/package_manager.hpp>
#include <data/locale.hpp>

// 2007-02-06: New HoverButton, hopefully this on works on GTK
#define USE_HOVERBUTTON

// ----------------------------------------------------------------------------- : WelcomeWindow

bool __compare_package_name(const PackagedP& a, const PackagedP& b) {
    return a->name() < b->name();
}
WelcomeWindow::WelcomeWindow()
  : wxFrame(nullptr, wxID_ANY, _TITLE_("magic set editor"), wxDefaultPosition, wxSize(520,380), wxDEFAULT_DIALOG_STYLE | wxTAB_TRAVERSAL | wxCLIP_CHILDREN )
  , logo (load_resource_image(settings.darkModePrefix() + _("about")))
{
  SetIcon(load_resource_icon(_("app")));

  SetBackgroundStyle(wxBG_STYLE_PAINT);

  // init controls
  wxControl* new_set   = new HoverButtonExt(this, ID_FILE_NEW,           load_resource_image(_("welcome_new")),     _BUTTON_("new set"),       _HELP_("new set"));
  wxControl* open_set  = new HoverButtonExt(this, ID_FILE_OPEN,          load_resource_image(_("welcome_open")),    _BUTTON_("open set"),      _HELP_("open set"));
  #if !USE_OLD_STYLE_UPDATE_CHECKER
  wxControl* updates   = new HoverButtonExt(this, ID_FILE_CHECK_UPDATES, load_resource_image(_("welcome_updates")), _BUTTON_("check updates"), _HELP_("check updates"));
  #endif
  wxControl* open_last = nullptr;
  if (!settings.recent_sets.empty()) {
    const String& filename = settings.recent_sets.front();
    if (wxFileName::FileExists(filename) || wxFileName::DirExists(filename + _("/"))) {
      wxFileName n(filename);
      open_last = new HoverButtonExt(this, ID_FILE_RECENT, load_resource_image(_("welcome_last")), _BUTTON_("last opened set"), _HELP_1_("last opened set", n.GetName()));
    }
  }

  wxSizer* s1 = new wxBoxSizer(wxHORIZONTAL);
  s1->AddSpacer(25);
    wxSizer* s2 = new wxBoxSizer(wxVERTICAL);
    s2->AddSpacer(100);
    s2->Add(new_set,   0, wxALL, 2);
    s2->Add(open_set,  0, wxALL, 2);
    #if !USE_OLD_STYLE_UPDATE_CHECKER
    s2->Add(updates,   0, wxALL, 2);
    #endif
    if (open_last) s2->Add(open_last, 0, wxALL, 2);
    s2->AddStretchSpacer();

    //
    select_language = new wxComboBox(this, ID_SELECT_LANGUAGE, _(""), wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    // set values
    vector<PackagedP> locales;
    package_manager.findMatching(_("*.mse-locale"), locales);
    sort(locales.begin(), locales.end(), __compare_package_name);
    int n = 0;
    FOR_EACH(package, locales) {
      select_language->Append(package->full_name, package.get());
        if (settings.locale == package->name()) {
          select_language->SetSelection(n);
        }
        n++;
    }
    wxSizer* s3 = new wxBoxSizer(wxHORIZONTAL);
    s3->AddSpacer(280);
    s3->Add(select_language, 0, wxALL, 1);
    s2->AddSpacer(30);
    s2->Add(s3);
  s1->Add(s2);
  SetSizer(s1);
}

void WelcomeWindow::onPaint(wxPaintEvent&) {
  wxBufferedPaintDC dc(this);
  draw(dc);
}

void WelcomeWindow::draw(DC& dc) {
  wxSize ws = GetClientSize();
  // draw background
  dc.SetPen  (*wxTRANSPARENT_PEN);
  dc.SetBrush(settings.darkModeColor());
  dc.DrawRectangle(0, 0, ws.GetWidth(), ws.GetHeight());
  // draw logo
  dc.DrawBitmap(logo,  (ws.GetWidth() -  logo.GetWidth()) / 2, 5);
  #if USE_BETA_LOGO
    dc.DrawBitmap(logo2,  ws.GetWidth() - logo2.GetWidth(),      ws.GetHeight() - logo2.GetHeight());
  #endif
  // draw version number
  dc.SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, _("Arial")));
  dc.SetTextForeground(Color(0,126,176));
  int tw,th;
  String version_string = _("version ") + app_version.toString() + version_suffix;
  dc.GetTextExtent(version_string,&tw,&th);
  dc.DrawText(version_string, 4, ws.GetHeight()-th-4);
}

void WelcomeWindow::onSelectLanguage(wxCommandEvent&) {
    // locale
    int n = select_language->GetSelection();
    if (n == wxNOT_FOUND) return;
    Packaged* p = (Packaged*)select_language->GetClientData(n);
    settings.locale = p->name();
    the_locale = Locale::byName(settings.locale);

    //close app
    (new WelcomeWindow())->Show();
    Close();
}

void WelcomeWindow::onOpenSet(wxCommandEvent&) {
  wxFileDialog* dlg = new wxFileDialog(this, _TITLE_("open set"), settings.default_set_dir, wxEmptyString, import_formats(), wxFD_OPEN);
  if (dlg->ShowModal() == wxID_OK) {
    settings.default_set_dir = dlg->GetDirectory();
    wxBusyCursor wait;
    try {
      close(import_set(dlg->GetPath()));
    } catch (Error& e) {
      handle_error(_("Error loading set: ") + e.what());
    }
  }
}

void WelcomeWindow::onNewSet(wxCommandEvent&) {
  close(new_set_window(this));
}

void WelcomeWindow::onOpenLast(wxCommandEvent&) {
  wxBusyCursor wait;
  assert(!settings.recent_sets.empty());
  try {
    close( open_package<Set>(settings.recent_sets.front()) );
  } catch (PackageNotFoundError& e) {
    handle_error(_("Cannot find set ") + e.what() + _(" to open."));
    // remove this package from the recent sets, so we don't get this error again
    settings.recent_sets.erase(settings.recent_sets.begin());
  }
}

void WelcomeWindow::onCheckUpdates(wxCommandEvent&) {
  Show(false); // hide, so the PackagesWindow will not use this window as its parent
  (new PackagesWindow(nullptr))->Show();
  Close();
}

void WelcomeWindow::close(const SetP& set) {
  if (!set) return;
  (new SetWindow(nullptr, set))->Show();
  Close();
}


BEGIN_EVENT_TABLE(WelcomeWindow, wxFrame)
  EVT_BUTTON         (ID_FILE_NEW,           WelcomeWindow::onNewSet)
  EVT_BUTTON         (ID_FILE_OPEN,          WelcomeWindow::onOpenSet)
  EVT_BUTTON         (ID_FILE_RECENT,        WelcomeWindow::onOpenLast)
  EVT_COMBOBOX       (ID_SELECT_LANGUAGE,     WelcomeWindow::onSelectLanguage)
  EVT_BUTTON         (ID_FILE_CHECK_UPDATES, WelcomeWindow::onCheckUpdates)
  EVT_PAINT          (                       WelcomeWindow::onPaint)
//  EVT_IDLE           (                     WelcomeWindow::onIdle)
END_EVENT_TABLE  ()


// ----------------------------------------------------------------------------- : Hover button with label

HoverButtonExt::HoverButtonExt(Window* parent, int id, const wxImage& icon, const String& label, const String& sub_label)
  : HoverButton(parent, id, _("btn"))
  , icon(icon)
  , label(label), sub_label(sub_label)
  , font_large(14, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, _("Arial"))
  , font_small(8,  wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, _("Arial"))
{}

void HoverButtonExt::draw(DC& dc) {
  // draw button
  HoverButton::draw(dc);
  int d = drawDelta();
  // icon
  if (icon.Ok()) dc.DrawBitmap(icon, d+7, d+7);
  // text
  dc.SetTextForeground(*wxBLACK);
  dc.SetFont(font_large);
  dc.DrawText(label, d+44, d+7);
  dc.SetFont(font_small);
  dc.DrawText(sub_label, d+45, d+28);
}
