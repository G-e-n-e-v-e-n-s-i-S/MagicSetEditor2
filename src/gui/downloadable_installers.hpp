//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/installer.hpp>
#include <util/io/package_manager.hpp>
#include <gui/packages_window.hpp>
#include <wx/wfstream.h>
#include <wx/webrequest.h>
#include <wx/dialup.h>
#include <wx/datetime.h>

class DownloadableInstallerList;

// ----------------------------------------------------------------------------- : DownloadableInstallers

/// The global installer downloader
extern DownloadableInstallerList downloadable_installers;

/// Handle downloading of installers
class DownloadableInstallerList {
public:
  inline DownloadableInstallerList() : download_status(NOT_DOWNLOADED), check_status(NOT_CHECKED), shown_dialog(false) {}

  /// Check for updates if the settings say so
  inline void check_updates() {
    wxDialUpManager* manager = wxDialUpManager::Create();
    bool connected = manager->IsOk() && manager->IsOnline();
    delete manager;
    if (!connected) return;
    if (settings.check_updates_when == CHECK_NEVER) return;
    int interval_days = (settings.check_updates_when == CHECK_7_DAYS) ? 7 : 30;
    if (days_since_last_check() >= interval_days) {
      check_updates_now();
    }
  }
  /// Check for updates
  /// If async==true then checking is done in another thread
  inline void check_updates_now(bool async = true) {
    if (async) {
      CheckUpdateThread* thread = new CheckUpdateThread;
      thread->Create();
      thread->Run();
    } else {
      CheckUpdateThread::Work();
    }
  }

  /// Start downloading the list of updates, return true if we are done
  inline bool download() {
    if (download_status == DONE) return true;
    if (download_status == NOT_DOWNLOADED) {
      download_status = DOWNLOADING;
      DownloadThread* thread = new DownloadThread();
      thread->Create();
      thread->Run();
    }
    return false;
  }

  /// Show a dialog to inform the user that updates are available (if there are any)
  /// Call check_updates first. Call this function from an onIdle loop
  inline void show_update_dialog(Window* set_window) {
    if (shown_dialog || check_status != FOUND) return; // we already have the latest version, or this has already been displayed.
    shown_dialog = true;
    wxMessageDialog dial = wxMessageDialog(set_window, _LABEL_("updates found"), _TITLE_("updates available"), wxYES_NO);
    if (dial.ShowModal() == wxID_YES) {
      (new PackagesWindow(set_window))->Show();
    }
  }

  // Have we shown the update dialog?
  bool shown_dialog;

  vector<DownloadableInstallerP> installers;

  enum DownloadStatus { NOT_DOWNLOADED, DOWNLOADING, DONE } download_status;
  enum CheckStatus    { NOT_CHECKED, CHECKING, FAILED, FOUND, NOT_FOUND } check_status;
private:
  wxMutex lock;

  /// Today's date, encoded as an integer YYYYMMDD
  static inline int today_as_int() {
    wxDateTime now = wxDateTime::Now();
    return now.GetYear() * 10000 + (static_cast<int>(now.GetMonth()) + 1) * 100 + now.GetDay();
  }

  /// Number of days since settings.check_updates_last_check (a large number if never checked)
  static inline int days_since_last_check() {
    int last = settings.check_updates_last_check;
    if (last <= 0) return 1 << 30; // never checked before, so check now
    int y = last / 10000;
    int m = (last / 100) % 100;
    int d = last % 100;
    wxDateTime last_check_date(static_cast<wxDateTime::wxDateTime_t>(d), static_cast<wxDateTime::Month>(m - 1), y);
    wxTimeSpan elapsed = wxDateTime::Now() - last_check_date;
    return (int)elapsed.GetDays();
  }

  struct DownloadThread : public wxThread {
    inline ExitCode Entry() override {
      try {
        // fetch list
        wxWebRequestSync request = wxWebSessionSync::GetDefault().CreateRequest(settings.installer_list_url);
        auto const result = request.Execute();
        if (!result) {
          wxMutexLocker l(downloadable_installers.lock);
          downloadable_installers.download_status = DONE;
          downloadable_installers.check_status = FAILED;
          return 0;
        }
        wxInputStream* is = request.GetResponse().GetStream();
        // Read installer list
        Reader reader(*is, nullptr, _("installers"), true);
        vector<DownloadableInstallerP> installers;
        reader.handle(_("installers"),installers);
        // done
        wxMutexLocker l(downloadable_installers.lock);
        swap(installers, downloadable_installers.installers);
        downloadable_installers.download_status = DONE;
        return 0;
      } catch (...) {
        // ignore all errors, we don't want problems if update checking fails
        downloadable_installers.download_status = DONE;
        downloadable_installers.check_status = FAILED;
        return 0;
      }
    }
  };

  struct CheckUpdateThread : public wxThread {
  public:
    inline void* Entry() override {
#ifndef __APPLE__
      Work();
#endif
      return 0;
    }

    inline static void Work() {
      if (downloadable_installers.check_status > NOT_CHECKED) return; // don't check multiple times simultaneously
      downloadable_installers.check_status = CHECKING;
      try {
        while (!downloadable_installers.download()) {
          wxMilliSleep(30);
        }
        if (downloadable_installers.check_status == FAILED) return;
        settings.check_updates_last_check = today_as_int();
        InstallablePackages installable_packages;
        FOR_EACH(inst, downloadable_installers.installers) {
          merge(installable_packages, inst);
        }
        FOR_EACH(p, installable_packages) {
          if (p->description->name == mse_package && app_version < p->description->version) {
            downloadable_installers.check_status = FOUND;
            return;
          }
          Version v;
          if (package_manager.installedVersion(p->description->name, v) && v < p->description->version) {
            if (
                 (settings.check_updates_what == CHECK_EVERYTHING)
              || (settings.check_updates_what == CHECK_GAMES && (  p->description->name.EndsWith("mse-updater")
                                                                || p->description->name.EndsWith("mse-locale")
                                                                || p->description->name.EndsWith("mse-game")
                                                                || p->description->name.EndsWith("mse-include")
                                                                || p->description->name.EndsWith("mse-style")
                                                                || p->description->name.EndsWith("mse-symbol-font")))
              || (settings.check_updates_what == CHECK_APP   && (  p->description->name.EndsWith("mse-updater")
                                                                || p->description->name.EndsWith("mse-locale")))
            ) {
              downloadable_installers.check_status = FOUND;
              return;
            }
          }
        }
        downloadable_installers.check_status = NOT_FOUND;
      } catch (...) {
        // ignore all errors, we don't want problems if update checking fails
        downloadable_installers.check_status = FAILED;
      }
    }
  };
};

