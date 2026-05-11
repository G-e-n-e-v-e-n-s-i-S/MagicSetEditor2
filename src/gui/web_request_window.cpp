//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/web_request_window.hpp>
#include <util/window_id.hpp>
#include <wx/mstream.h>

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
  wxWebResponse response = evt.GetResponse();
  if (!response.IsOk()) {
    onFail(_ERROR_("web request corrupted"));
    return;
  }
  wxInputStream* stream = response.GetStream();
  if (!stream || !stream->IsOk()) {
    onFail(_ERROR_("web request corrupted"));
    return;
  }

  content_type = response.GetContentType();
  if (content_type.StartsWith("image/")) {
    image_out = Image(*stream);
    if (!image_out.IsOk()) {
      onFail(_ERROR_("web request corrupted"));
      return;
    }
  }
  else if (content_type.StartsWith("text/") || content_type.Contains("json")) {
    wxMemoryOutputStream mem;
    char buffer[8192];
    while (true) {
      stream->Read(buffer, sizeof(buffer));
      size_t read = stream->LastRead();
      if (read > 0) mem.Write(buffer, read);
      if (stream->Eof()) break;
      if (stream->GetLastError() != wxSTREAM_NO_ERROR) {
        onFail(_ERROR_("web request corrupted"));
        return;
      }
    }
    text_out.resize(mem.GetSize());
    mem.CopyTo(text_out.data(), mem.GetSize());
  }
  else {
    onFail(_ERROR_("web request unsupported format"));
    return;
  }
  EndModal(wxID_OK);
}

void WebRequestWindow::onFail(const String& message) {
  content_type.Clear();
  info->SetLabel(_ERROR_("web request failed"));
  address->SetLabel(message);
}

void WebRequestWindow::onCancel(wxCommandEvent&) {
  EndModal(wxID_CANCEL);
}

BEGIN_EVENT_TABLE(WebRequestWindow, wxDialog)
EVT_BUTTON       (wxID_CANCEL, WebRequestWindow::onCancel)
END_EVENT_TABLE  ()
