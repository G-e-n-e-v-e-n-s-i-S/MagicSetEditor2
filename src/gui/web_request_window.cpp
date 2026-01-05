//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/web_request_window.hpp>
#include <util/window_id.hpp>

// ----------------------------------------------------------------------------- : WebRequestWindow

WebRequestWindow::WebRequestWindow(const String& url, bool sizer)
  : wxDialog(NULL, wxID_ANY, _TITLE_("web request"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxDIALOG_NO_PARENT)
{
  // init controls
  info = new wxStaticText(this, -1, _LABEL_("web request address"));
  address = new wxStaticText(this, -1, url);
  gauge = new wxGauge(this, wxID_ANY, 0);
  // init sizers
  if (sizer) {
    wxSizer* s = new wxBoxSizer(wxVERTICAL);
      s->Add(info, 0, wxALL, 8);
      s->Add(address, 0, (wxALL & ~wxTOP), 4);
      s->Add(gauge, 0, wxEXPAND | wxALL, 8);
      s->Add(CreateButtonSizer(wxCANCEL), 1, wxEXPAND, 8);
    s->SetSizeHints(this);
    SetSizer(s);
  }

  // create web request
  request = wxWebSession::GetDefault().CreateRequest(this, url);
  if (!request.IsOk() ) {
    onFail(_ERROR_("web request cant create"));
    return;
  }

  // bind request events
  Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
    switch (evt.GetState())
    {
    // request completed
    case wxWebRequest::State_Completed:
    {
      onComplete(evt);
      break;
    }
    // request failed
    case wxWebRequest::State_Failed:
      onFail(evt.GetErrorDescription());
      break;
    }
  });
  Bind(wxEVT_WEBREQUEST_DATA, [this](wxWebRequestEvent& evt) {
    // request updated
    onUpdate(evt);
  });

  // start request
  wxMilliSleep(70); // make sure we don't DDoS anyone
  request.Start();
}

void WebRequestWindow::onUpdate(wxWebRequestEvent& evt) {
  off_t expected_bytes = request.GetBytesExpectedToReceive();
  off_t bytes = request.GetBytesReceived();
  if (expected_bytes > 0) {
    gauge->SetRange(expected_bytes);
    gauge->SetValue(bytes);
  }
  else {
    gauge->Pulse();
  }
}

void WebRequestWindow::onComplete(wxWebRequestEvent& evt) {
  out = evt.GetResponse();
  if (out.IsOk()) {
    EndModal(wxID_OK);
  }
  else {
    onFail(_ERROR_("web request corrupted"));
  }
}

void WebRequestWindow::onFail(const String& message) {
  info->SetLabel(_ERROR_("web request failed"));
  address->SetLabel(message);
}

void WebRequestWindow::onCancel(wxCommandEvent&) {
  EndModal(wxID_CANCEL);
}

BEGIN_EVENT_TABLE(WebRequestWindow, wxDialog)
EVT_BUTTON       (wxID_CANCEL, WebRequestWindow::onCancel)
END_EVENT_TABLE  ()
