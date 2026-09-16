//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <wx/wx.h>
#include <wx/wxprec.h>
#include <wx/webrequest.h>
#include <wx/process.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <unordered_map>
#include <atomic>

// ----------------------------------------------------------------------------- : Payload URL

static const wxString url = wxString("https://github.com/MagicSetEditorPacks/Installer-Pack/raw/refs/heads/main/magicseteditor.zip");

// ----------------------------------------------------------------------------- : Localized Text

const std::unordered_map<wxString, wxString> update_map = [] {
  std::unordered_map<wxString, wxString> map = {
      { _("en"),   _("Magic Set Editor will now update itself.") }
    , { _("chs"),  _("Magic Set Editor现在将自动更新。") }
    , { _("cht"),  _("Magic Set Editor現在將自動更新。") }
    , { _("da"),   _("Magic Set Editor opdaterer nu sig selv.") }
    , { _("de"),   _("Magic Set Editor wird nun aktualisiert.") }
    , { _("es"),   _("Magic Set Editor se actualizará automáticamente.") }
    , { _("fr"),   _("Magic Set Editor va maintenant se mettre à jour.") }
    , { _("it"),   _("Magic Set Editor si aggiornerà automaticamente.") }
    , { _("jp"),   _("Magic Set Editorが自動的に更新されます。") }
    , { _("ko"),   _("Magic Set Editor가 이제 자동으로 업데이트됩니다.") }
    , { _("pl"),   _("Magic Set Editor zaktualizuje się teraz samoczynnie.") }
    , { _("ptbr"), _("Magic Set Editor será atualizado automaticamente.") }
    , { _("ru"),   _("Сейчас произойдет обновление Magic Set Editor ") }
  };
  return map;
}();

const std::unordered_map<wxString, wxString> close_map = [] {
  std::unordered_map<wxString, wxString> map = {
      { _("en"),   _("Please close all other Magic Set Editor windows, then click OK.") }
    , { _("chs"),  _("请关闭所有其他Magic Set Editor窗口，然后单击“OK”。") }
    , { _("cht"),  _("請關閉所有其他Magic Set Editor窗口，然後按一下“OK”。") }
    , { _("da"),   _("Luk alle andre Magic Set Editor-vinduer, og klik derefter på OK.") }
    , { _("de"),   _("Bitte schließen Sie alle anderen Fenster des Magic Set Editor und klicken Sie anschließend auf OK.") }
    , { _("es"),   _("Cierre todas las demás ventanas de Magic Set Editor y haga clic en OK.") }
    , { _("fr"),   _("Veuillez fermer toutes les autres fenêtres de Magic Set Editor, puis cliquez sur OK.") }
    , { _("it"),   _("Chiudere tutte le altre finestre di Magic Set Editor, quindi fare clic su OK.") }
    , { _("jp"),   _("他のMagic Set Editorウィンドウをすべて閉じて、「OK」をクリックしてください。") }
    , { _("ko"),   _("다른 Magic Set Editor 창을 모두 닫은 다음 OK을 클릭하십시오.") }
    , { _("pl"),   _("Zamknij wszystkie pozostałe okna Magic Set Editor, a następnie kliknij OK.") }
    , { _("ptbr"), _("Feche todas as outras janelas do Magic Set Editor e clique em OK") }
    , { _("ru"),   _("Пожалуйста, закройте все остальные окна Magic Set Editor, затем нажмите ОК.") }
  };
  return map;
  }();

// ----------------------------------------------------------------------------- : Events

wxDEFINE_EVENT(wxEVT_EXTRACTION, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_FAIL,       wxThreadEvent);
wxDEFINE_EVENT(wxEVT_SUCCESS,    wxThreadEvent);

// ----------------------------------------------------------------------------- : Utility

// Retry a filesystem operation a few times with exponential backoff.
template <typename Op>
bool retry_io(Op op, int max_attempts = 6, int initial_delay_ms = 50) {
  int delay = initial_delay_ms;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    if (op()) return true;
    if (attempt == max_attempts) return false;
    wxMilliSleep(delay);
    delay *= 2;
  }
  return false;
}

// ----------------------------------------------------------------------------- : Classes

class MSEUpdater : public wxApp {
public:
  virtual bool OnInit();
};

IMPLEMENT_APP(MSEUpdater)

class Thread;

class Frame : public wxFrame {
public:
  Frame(wxString locale);

  void OnExtraction(wxThreadEvent&  event);
  void OnFail      (wxThreadEvent&  event);
  void OnSuccess   (wxThreadEvent&  event);
  void OnOK        (wxCommandEvent& event);

  void DoFail      (const wxString& error);

  wxStaticText* status;
  wxStaticText* address;
  wxButton*     button;
  Thread*       thread;

  std::atomic<bool> OK{false};
  std::atomic<bool> thread_finished{false};
};

class Thread : public wxThread {
public:
  Thread(Frame* frame);

  void NotifyExtraction(const wxString& file);
  void NotifyFail      (const wxString& error);
  void NotifySuccess   (const wxString& app_folder);

  virtual wxThread::ExitCode Entry();

  Frame* frame;
};

// ----------------------------------------------------------------------------- : Methods

bool MSEUpdater::OnInit() {
  Appearance darkmode = Appearance::System;
  wxString locale = wxString("en");
  for (int i = 0; i < argc; ++i) {
    wxString arg = wxString(argv[i]);
    if (arg.StartsWith("darkmode") && arg.size() > 8) {
      darkmode = arg.GetChar(8) == '1' ? Appearance::Dark : Appearance::Light;
    }
    else if (arg.StartsWith("locale") && arg.size() > 6) {
      locale = arg.substr(6);
    }
  }
  SetAppearance(darkmode);
  Frame *frame = new Frame(locale);
  SetTopWindow(frame);
  frame->Show();
  return true;
}

Frame::Frame(wxString locale) : wxFrame(nullptr, wxID_ANY, wxString("Magic Set Editor Updater")) {
  // Set app icon
  wxLogNull noLog;
#if defined(__WXMSW__) && !defined(__GNUC__)
  SetIcon(wxIcon(_("updater")));
#else
  SetIcon(wxIcon(wxStandardPaths::Get().GetDataDir() + _("/src_updater/updater.ico"), wxBITMAP_TYPE_ICO));
#endif
  // Create controls
  wxString update_text = update_map.find(locale) != update_map.end() ? update_map.at(locale) : update_map.at("en");
  wxString close_text  =  close_map.find(locale) !=  close_map.end() ?  close_map.at(locale) :  close_map.at("en");
  status  = new wxStaticText(this, wxID_ANY, update_text, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxALIGN_CENTRE_VERTICAL | wxST_NO_AUTORESIZE);
  address = new wxStaticText(this, wxID_ANY, close_text,  wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxALIGN_CENTRE_VERTICAL | wxST_NO_AUTORESIZE);
  button  = new wxButton    (this, wxID_OK,  wxString("OK"));
  wxFont font = status->GetFont().Scale(1.2f);
  status ->SetFont(font);
  address->SetFont(font);
  button ->SetFont(font);
  wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(status,  1, wxEXPAND | wxALL,            12);
  sizer->Add(address, 1, wxEXPAND | (wxALL & ~wxTOP), 12);
  sizer->Add(button,  1, wxEXPAND,                    12);
  sizer->SetMinSize(wxSize(800, -1));
  SetSizerAndFit(sizer);

  // Bind events
  Bind(wxEVT_EXTRACTION, &Frame::OnExtraction, this);
  Bind(wxEVT_SUCCESS,    &Frame::OnSuccess,    this);
  Bind(wxEVT_FAIL,       &Frame::OnFail,       this);
  Bind(wxEVT_BUTTON,     &Frame::OnOK,         this, wxID_OK);

  // Run thread
  thread = new Thread(this);
  if (thread->Create() == wxTHREAD_NO_ERROR) {
    thread->Run();
  }
}

void Frame::OnExtraction(wxThreadEvent& event) {
  status->SetLabelText(wxString("Extracting File To:"));
  address->SetLabelText(event.GetString());
  Update();
}

void Frame::OnFail(wxThreadEvent& event) {
  DoFail(event.GetString());
}
void Frame::DoFail(const wxString& error) {
  status->SetForegroundColour(wxColor(*wxRED));
  status->SetLabelText(error);
  address->SetLabelText(wxString(""));
  Update();
  wxMilliSleep(3000);
  Destroy();
}

void Frame::OnSuccess(wxThreadEvent& event) {
  wxString path = event.GetString() + wxString("magicseteditor");
  path = path + wxString(".exe");
  long processID = wxFileExists(path) ? wxExecute(path, wxEXEC_ASYNC) : 0L;
  if (processID == 0L) {
    DoFail(wxString("Could not launch Magic Set Editor."));
  }
  else {
    status->SetLabelText(wxString("Launching Magic Set Editor."));
    address->SetLabelText(wxString(""));
    Update();
    wxMilliSleep(800);
    Destroy();
  }
}

void Frame::OnOK(wxCommandEvent& event) {
  if (!OK) {
    button->Enable(false);
    button->SetLabel(wxString("Cancel"));
    status->SetLabelText(wxString("Downloading New Version From:"));
    address->SetLabelText(url);
    Update();
    wxMilliSleep(200);
    button->Enable(true);
    wxMilliSleep(100);
    OK = true;
  }
  else {
    if (!thread_finished) {
      thread->Kill();
    }
    Destroy();
  }
}

Thread::Thread(Frame* frame) : wxThread(wxTHREAD_DETACHED), frame(frame) {}

void Thread::NotifyExtraction(const wxString& file) {
  wxThreadEvent* event = new wxThreadEvent(wxEVT_EXTRACTION);
  event->SetString(file);
  wxQueueEvent(frame, event);
}

void Thread::NotifyFail(const wxString& error) {
  frame->thread_finished = true;
  wxThreadEvent* event = new wxThreadEvent(wxEVT_FAIL);
  event->SetString(error);
  wxQueueEvent(frame, event);
}

void Thread::NotifySuccess(const wxString& app_folder) {
  frame->thread_finished = true;
  wxThreadEvent* event = new wxThreadEvent(wxEVT_SUCCESS);
  event->SetString(app_folder);
  wxQueueEvent(frame, event);
}

wxThread::ExitCode Thread::Entry() {
  try {
    // Get app folder
    wxFileName updater_folder = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    updater_folder.RemoveLastDir();
    if (!updater_folder.GetPath().EndsWith(wxString("data"))) {
      NotifyFail("Updater package is not in the 'data' folder.");
      return (ExitCode)-1;
    }
    updater_folder.RemoveLastDir();
    wxString app_folder = updater_folder.GetPath() + wxFileName::GetPathSeparator();

    // Fetch zip from url
    wxWebRequestSync request = wxWebSessionSync::GetDefault().CreateRequest(url);
    auto const result = request.Execute();
    if (!result) {
      NotifyFail("Download Failed.");
      return (ExitCode)-1;
    }
    wxInputStream* is = request.GetResponse().GetStream();
    if (!is->IsOk()) {
      NotifyFail("Download Corrupted.");
      return (ExitCode)-1;
    }

    // Stall until the user gives the go ahead
    while (!frame->OK) {
      wxMilliSleep(50);
    }

    // Extract files from zip
    wxZipInputStream zis(is);
    if (!zis.IsOk()) {
      NotifyFail("Zip Archive Corrupted.");
      return (ExitCode)-1;
    }
    std::unique_ptr<wxZipEntry> entry;
    while (entry.reset(zis.GetNextEntry()), entry.get() != nullptr) {
      wxString target_path = app_folder + entry->GetName();
      int nPermBits = entry->GetMode();
      if (entry->IsDir()) {
        if (!wxDirExists(target_path) &&
            !retry_io([&]{ return wxFileName::Mkdir(target_path, nPermBits, wxPATH_MKDIR_FULL); })) {
          NotifyFail("Could Not Create Folder. Magic Set Editor may still be open.");
          return (ExitCode)-1;
        }
        continue;
      }

      NotifyExtraction(target_path);
      wxFileName fn;
      fn.Assign(target_path);
      if (!wxDirExists(fn.GetPath()) &&
          !retry_io([&]{ return wxFileName::Mkdir(fn.GetPath(), nPermBits, wxPATH_MKDIR_FULL); })) {
        NotifyFail("Could Not Create Folder. Magic Set Editor may still be open.");
        return (ExitCode)-1;
      }
      if (!zis.CanRead()) {
        NotifyFail("Extraction Failed.");
        return (ExitCode)-1;
      }

      std::unique_ptr<wxFileOutputStream> fos;
      retry_io([&]{
        fos = std::make_unique<wxFileOutputStream>(target_path);
        return fos->IsOk();
      });
      if (!fos->IsOk()) {
        NotifyFail("Writing Failed. Magic Set Editor may still be open.");
        return (ExitCode)-1;
      }

      zis.Read(*fos);
      if (!fos->IsOk() || (!zis.Eof() && zis.GetLastError() != wxSTREAM_NO_ERROR)) {
        fos.reset();
        retry_io([&]{ return wxRemove(target_path.fn_str()) == 0; });
        NotifyFail("Writing Failed. Magic Set Editor may still be open.");
        return (ExitCode)-1;
      }
      fos->Close();
      zis.CloseEntry();
    }

    NotifySuccess(app_folder);
    wxMilliSleep(200);
    return (ExitCode)0;
  }
  catch (...) {
    NotifyFail("Something Went Wrong.");
    return (ExitCode)-1;
  }
}
