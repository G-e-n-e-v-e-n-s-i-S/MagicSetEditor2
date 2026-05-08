//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <wx/gauge.h>
#include <wx/webrequest.h>

// ----------------------------------------------------------------------------- : WebRequestWindow

/// A window for displaying the progression of a WebRequest and returning the WebResponse
class WebRequestWindow : public wxDialog {
public:
  WebRequestWindow(const String& url, bool sizer=true);

  String          content_type;
  std::string     text_out;
  Image           image_out;

protected:
  DECLARE_EVENT_TABLE();

  wxStaticText*   info, *address;
  wxGauge*        gauge;
  wxWebRequest    request;

  void onUpdate(wxWebRequestEvent& evt);

  void onComplete(wxWebRequestEvent& evt);

  void onFail(const String& message);

  void onCancel(wxCommandEvent&);

};
