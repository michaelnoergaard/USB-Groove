// =============================================================================
// USBGroove.cpp — Linux System Tray App
//
// Automatically plays MP3 files from any inserted USB flash drive.
// Uses GStreamer for audio — no external player required.
// Runs as a Linux system tray application via libappindicator3.
//
// Dependencies (install via package manager):
//   libappindicator3-dev libnotify-dev libgstreamer1.0-dev libglib2.0-dev
//
// Build:
//   g++ -std=c++17 -O2 -Wall linux/USBGroove.cpp -o USBGroove-Linux \
//       $(pkg-config --cflags --libs appindicator3-0.1 glib-2.0 gio-2.0 \
//         gstreamer-1.0 libnotify) -lstdc++fs
// =============================================================================

#include <libappindicator/app-indicator.h>
#include <libnotify/notify.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static AppIndicator*    g_indicator    = nullptr;
static GMainLoop*       g_loop         = nullptr;
static GstElement*      g_pipeline     = nullptr;
static GVolumeMonitor*  g_volMonitor   = nullptr;

static std::vector<std::string> g_playlist;
static int              g_currentTrack = -1;
static bool             g_isPaused     = false;
static bool             g_shuffleOn    = false;
static bool             g_repeatAll    = false;
static std::string      g_currentMountPath;  // mount path of active USB
static GMount*          g_activeMount  = nullptr;  // GMount of active USB drive
static int              g_lockFd       = -1;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void BuildMenu();
static void PlayTrack(int index);
static void StopPlayback();
static void PauseResume();
static void NextTrack();
static void PrevTrack();
static std::string TrackTitle(int index);
static std::vector<std::string> ScanForMp3s(const std::string& root);
static void StartPlaylist(std::vector<std::string> tracks);
static void ShowNotification(const char* title, const char* body);
static void Log(const std::string& msg);
static bool IsAutorunEnabled();
static void ToggleAutorun();
static std::string GetExePath();
static void EjectUSB();
static bool AcquireLock();
static void ReleaseLock();

// ---------------------------------------------------------------------------
// GStreamer bus callback — handles end-of-stream and errors
// ---------------------------------------------------------------------------
static gboolean OnBusMessage(GstBus* /*bus*/, GstMessage* msg, gpointer /*data*/)
{
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
    {
        // Track finished — advance to next
        if (g_currentTrack >= 0)
        {
            int next = g_currentTrack + 1;
            if (next < static_cast<int>(g_playlist.size()))
            {
                PlayTrack(next);
            }
            else if (g_repeatAll)
            {
                PlayTrack(0);
                Log("Repeat All — restarting playlist.");
            }
            else
            {
                StopPlayback();
                ShowNotification("USB Groove", "Playlist finished.");
                Log("Playlist finished.");
            }
        }
        break;
    }
    case GST_MESSAGE_ERROR:
    {
        GError* err = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(msg, &err, &debug);

        std::string errMsg = "GStreamer error: ";
        if (err) errMsg += err->message;

        // Hint about missing MP3 codec
        if (err && strstr(err->message, "codec") != nullptr)
            errMsg += " (try: sudo apt install gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly)";

        Log(errMsg);

        if (err) g_error_free(err);
        if (debug) g_free(debug);

        // Skip bad file and try next
        if (g_currentTrack >= 0)
        {
            int next = g_currentTrack + 1;
            if (next < static_cast<int>(g_playlist.size()))
                PlayTrack(next);
            else
                StopPlayback();
        }
        break;
    }
    default:
        break;
    }

    return TRUE;
}

// ---------------------------------------------------------------------------
// Playback engine (GStreamer playbin)
// ---------------------------------------------------------------------------
static void PlayTrack(int index)
{
    if (index < 0 || index >= static_cast<int>(g_playlist.size())) return;

    // Stop current playback
    if (g_pipeline)
        gst_element_set_state(g_pipeline, GST_STATE_NULL);

    // Create pipeline if needed
    if (!g_pipeline)
    {
        g_pipeline = gst_element_factory_make("playbin", "player");
        if (!g_pipeline)
        {
            Log("Failed to create GStreamer playbin element.");
            return;
        }

        GstBus* bus = gst_element_get_bus(g_pipeline);
        gst_bus_add_watch(bus, OnBusMessage, nullptr);
        gst_object_unref(bus);
    }

    // playbin needs file:// URIs
    gchar* uri = gst_filename_to_uri(g_playlist[index].c_str(), nullptr);
    if (!uri)
    {
        Log("Failed to convert path to URI: " + g_playlist[index]);
        return;
    }

    g_object_set(g_pipeline, "uri", uri, nullptr);
    g_free(uri);

    gst_element_set_state(g_pipeline, GST_STATE_PLAYING);

    g_currentTrack = index;
    g_isPaused = false;

    BuildMenu();

    Log("Playing [" + std::to_string(index + 1) + "/" +
        std::to_string(g_playlist.size()) + "]: " + g_playlist[index]);
}

static void StopPlayback()
{
    if (g_pipeline)
    {
        gst_element_set_state(g_pipeline, GST_STATE_NULL);
        gst_object_unref(g_pipeline);
        g_pipeline = nullptr;
    }

    g_playlist.clear();
    g_currentTrack = -1;
    g_isPaused = false;
    g_currentMountPath.clear();

    BuildMenu();
    Log("Playback stopped.");
}

static void PauseResume()
{
    if (g_currentTrack < 0 || !g_pipeline) return;

    if (!g_isPaused)
    {
        gst_element_set_state(g_pipeline, GST_STATE_PAUSED);
        g_isPaused = true;
        Log("Paused.");
    }
    else
    {
        gst_element_set_state(g_pipeline, GST_STATE_PLAYING);
        g_isPaused = false;
        Log("Resumed.");
    }

    BuildMenu();
}

static void NextTrack()
{
    if (g_playlist.empty()) return;
    int next = g_currentTrack + 1;
    if (next < static_cast<int>(g_playlist.size()))
        PlayTrack(next);
}

static void PrevTrack()
{
    if (g_playlist.empty()) return;
    int prev = g_currentTrack - 1;
    if (prev >= 0)
        PlayTrack(prev);
}

static void StartPlaylist(std::vector<std::string> tracks)
{
    StopPlayback();
    g_playlist = std::move(tracks);

    if (g_shuffleOn)
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(g_playlist.begin(), g_playlist.end(), rng);
    }

    PlayTrack(0);
}

// ---------------------------------------------------------------------------
// MP3 file scanner
// ---------------------------------------------------------------------------
static std::vector<std::string> ScanForMp3s(const std::string& root)
{
    std::vector<std::string> results;

    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied))
        {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            // Case-insensitive .mp3 check
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp3")
                results.push_back(entry.path().string());
        }
    }
    catch (const fs::filesystem_error& e)
    {
        Log(std::string("Scan error: ") + e.what());
    }

    std::sort(results.begin(), results.end());
    return results;
}

// ---------------------------------------------------------------------------
// USB detection via GIO VolumeMonitor
// ---------------------------------------------------------------------------
static void OnMountAdded(GVolumeMonitor* /*monitor*/, GMount* mount, gpointer /*data*/)
{
    // Check if this mount is removable (USB drives are)
    if (!g_mount_can_unmount(mount)) return;

    GFile* root = g_mount_get_root(mount);
    if (!root) return;

    gchar* path = g_file_get_path(root);
    g_object_unref(root);

    if (!path) return;

    std::string mountPath(path);
    g_free(path);

    gchar* name = g_mount_get_name(mount);
    std::string mountName(name ? name : "USB Drive");
    if (name) g_free(name);

    Log("USB drive detected: " + mountPath + " (" + mountName + ")");

    auto mp3s = ScanForMp3s(mountPath);
    if (mp3s.empty())
    {
        std::string msg = "No MP3 files found on " + mountName;
        ShowNotification("USB Groove", msg.c_str());
        Log(msg);
        return;
    }

    std::string msg = "Found " + std::to_string(mp3s.size()) +
                      " MP3 file(s) on " + mountName + " — starting playback.";
    ShowNotification("USB Groove", msg.c_str());
    Log(msg);

    // Store the active mount (ref it so it stays valid)
    if (g_activeMount)
        g_object_unref(g_activeMount);
    g_activeMount = G_MOUNT(g_object_ref(mount));

    g_currentMountPath = mountPath;
    StartPlaylist(std::move(mp3s));
}

static void OnMountRemoved(GVolumeMonitor* /*monitor*/, GMount* mount, gpointer /*data*/)
{
    GFile* root = g_mount_get_root(mount);
    if (!root) return;

    gchar* path = g_file_get_path(root);
    g_object_unref(root);

    if (!path) return;

    std::string mountPath(path);
    g_free(path);

    Log("USB drive removed: " + mountPath);

    // Stop playback if we were playing from this drive
    if (g_currentTrack >= 0 && !g_currentMountPath.empty() &&
        mountPath == g_currentMountPath)
    {
        StopPlayback();
        ShowNotification("USB Groove", "USB drive removed — playback stopped.");
    }

    // Clear active mount reference
    if (g_activeMount)
    {
        g_object_unref(g_activeMount);
        g_activeMount = nullptr;
        BuildMenu();
    }
}

// ---------------------------------------------------------------------------
// Notifications (libnotify)
// ---------------------------------------------------------------------------
static void ShowNotification(const char* title, const char* body)
{
    NotifyNotification* n = notify_notification_new(title, body, "media-playback-start");
    notify_notification_set_timeout(n, 4000);
    notify_notification_show(n, nullptr);
    g_object_unref(n);
}

// ---------------------------------------------------------------------------
// Autorun (XDG autostart)
// ---------------------------------------------------------------------------
static std::string GetAutorunPath()
{
    const char* configHome = g_get_user_config_dir();
    return std::string(configHome) + "/autostart/usb-groove.desktop";
}

static std::string GetExePath()
{
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
    {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "USBGroove-Linux";
}

static bool IsAutorunEnabled()
{
    return fs::exists(GetAutorunPath());
}

static void ToggleAutorun()
{
    std::string path = GetAutorunPath();

    if (IsAutorunEnabled())
    {
        std::remove(path.c_str());
        Log("Start on login OFF.");
        ShowNotification("USB Groove", "Start on login disabled.");
    }
    else
    {
        // Ensure autostart directory exists
        fs::create_directories(fs::path(path).parent_path());

        std::ofstream ofs(path);
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=USB Groove\n"
            << "Comment=Auto-plays MP3 files from USB drives\n"
            << "Exec=" << GetExePath() << "\n"
            << "Icon=media-playback-start\n"
            << "Terminal=false\n"
            << "X-GNOME-Autostart-enabled=true\n"
            << "Categories=Audio;Music;Player;\n";
        ofs.close();

        Log("Start on login ON.");
        ShowNotification("USB Groove", "Start on login enabled.");
    }
}

// ---------------------------------------------------------------------------
// Context menu (GTK)
// ---------------------------------------------------------------------------
static void OnPlayPause(GtkMenuItem* /*item*/, gpointer /*data*/)   { PauseResume(); }
static void OnNext(GtkMenuItem* /*item*/, gpointer /*data*/)        { NextTrack(); }
static void OnPrev(GtkMenuItem* /*item*/, gpointer /*data*/)        { PrevTrack(); }
static void OnStop(GtkMenuItem* /*item*/, gpointer /*data*/)        { StopPlayback(); }

// ---------------------------------------------------------------------------
// USB Eject
// ---------------------------------------------------------------------------
static void OnEjectFinished(GObject* source, GAsyncResult* result, gpointer /*data*/)
{
    GMount* mount = G_MOUNT(source);
    GError* error = nullptr;

    if (g_mount_eject_with_operation_finish(mount, result, &error))
    {
        ShowNotification("USB Groove", "USB drive safely ejected.");
        Log("USB drive ejected successfully.");
    }
    else
    {
        std::string msg = "Failed to eject USB drive";
        if (error)
        {
            msg += ": ";
            msg += error->message;
            g_error_free(error);
        }
        ShowNotification("USB Groove", msg.c_str());
        Log(msg);
    }

    if (g_activeMount)
    {
        g_object_unref(g_activeMount);
        g_activeMount = nullptr;
    }
    g_currentMountPath.clear();
    BuildMenu();
}

static void EjectUSB()
{
    if (!g_activeMount) return;

    StopPlayback();

    g_mount_eject_with_operation(
        g_activeMount,
        G_MOUNT_UNMOUNT_NONE,
        nullptr,       // no GMountOperation needed for USB
        nullptr,       // no GCancellable
        OnEjectFinished,
        nullptr);

    Log("Ejecting USB drive...");
}

static void OnEject(GtkMenuItem* /*item*/, gpointer /*data*/) { EjectUSB(); }

static void OnShuffle(GtkCheckMenuItem* item, gpointer /*data*/)
{
    g_shuffleOn = gtk_check_menu_item_get_active(item);
    Log(g_shuffleOn ? "Shuffle ON." : "Shuffle OFF.");
}

static void OnRepeat(GtkCheckMenuItem* item, gpointer /*data*/)
{
    g_repeatAll = gtk_check_menu_item_get_active(item);
    Log(g_repeatAll ? "Repeat All ON." : "Repeat All OFF.");
}

static void OnAutorun(GtkCheckMenuItem* /*item*/, gpointer /*data*/)
{
    ToggleAutorun();
}

static void OnPlaylistTrack(GtkMenuItem* /*item*/, gpointer data)
{
    int index = GPOINTER_TO_INT(data);
    PlayTrack(index);
}

static void OnAbout(GtkMenuItem* /*item*/, gpointer /*data*/)
{
    GtkWidget* dialog = gtk_message_dialog_new(
        nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "USB Groove — Linux\n\n"
        "Plays MP3 files automatically from any USB drive.\n"
        "No media player required — built-in GStreamer audio engine.\n\n"
        "Right-click tray icon for controls.");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void OnQuit(GtkMenuItem* /*item*/, gpointer /*data*/)
{
    StopPlayback();
    if (g_loop) g_main_loop_quit(g_loop);
}

static void BuildMenu()
{
    GtkWidget* menu = gtk_menu_new();
    const bool active = (g_currentTrack >= 0 && !g_playlist.empty());

    // "Now Playing" header
    if (active)
    {
        std::string header = (g_isPaused ? "Paused: " : "Playing: ") +
                             TrackTitle(g_currentTrack) +
                             "  [" + std::to_string(g_currentTrack + 1) +
                             "/" + std::to_string(g_playlist.size()) + "]";
        GtkWidget* headerItem = gtk_menu_item_new_with_label(header.c_str());
        gtk_widget_set_sensitive(headerItem, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), headerItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    }

    // Play/Pause
    GtkWidget* playPause = gtk_menu_item_new_with_label(
        g_isPaused ? "Resume" : "Pause");
    g_signal_connect(playPause, "activate", G_CALLBACK(OnPlayPause), nullptr);
    gtk_widget_set_sensitive(playPause, active);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), playPause);

    // Previous
    GtkWidget* prev = gtk_menu_item_new_with_label("Previous Track");
    g_signal_connect(prev, "activate", G_CALLBACK(OnPrev), nullptr);
    gtk_widget_set_sensitive(prev, active);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), prev);

    // Next
    GtkWidget* next = gtk_menu_item_new_with_label("Next Track");
    g_signal_connect(next, "activate", G_CALLBACK(OnNext), nullptr);
    gtk_widget_set_sensitive(next, active);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), next);

    // Stop
    GtkWidget* stop = gtk_menu_item_new_with_label("Stop");
    g_signal_connect(stop, "activate", G_CALLBACK(OnStop), nullptr);
    gtk_widget_set_sensitive(stop, active);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop);

    // Eject USB
    GtkWidget* eject = gtk_menu_item_new_with_label("Eject USB");
    g_signal_connect(eject, "activate", G_CALLBACK(OnEject), nullptr);
    gtk_widget_set_sensitive(eject, g_activeMount != nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), eject);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // Shuffle (checkbox)
    GtkWidget* shuffle = gtk_check_menu_item_new_with_label("Shuffle");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(shuffle), g_shuffleOn);
    g_signal_connect(shuffle, "toggled", G_CALLBACK(OnShuffle), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), shuffle);

    // Repeat All (checkbox)
    GtkWidget* repeat = gtk_check_menu_item_new_with_label("Repeat All");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(repeat), g_repeatAll);
    g_signal_connect(repeat, "toggled", G_CALLBACK(OnRepeat), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), repeat);

    // Start on login (checkbox)
    GtkWidget* autorun = gtk_check_menu_item_new_with_label("Start on Login");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(autorun), IsAutorunEnabled());
    g_signal_connect(autorun, "toggled", G_CALLBACK(OnAutorun), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), autorun);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // Playlist submenu
    if (!g_playlist.empty())
    {
        GtkWidget* playlistItem = gtk_menu_item_new_with_label("Playlist");
        GtkWidget* playlistMenu = gtk_menu_new();

        int limit = static_cast<int>(g_playlist.size());
        if (limit > 50) limit = 50;

        for (int i = 0; i < limit; ++i)
        {
            std::string label = std::to_string(i + 1) + ". " + TrackTitle(i);
            GtkWidget* item;
            if (i == g_currentTrack)
            {
                item = gtk_check_menu_item_new_with_label(label.c_str());
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
            }
            else
            {
                item = gtk_menu_item_new_with_label(label.c_str());
            }
            g_signal_connect(item, "activate", G_CALLBACK(OnPlaylistTrack),
                             GINT_TO_POINTER(i));
            gtk_menu_shell_append(GTK_MENU_SHELL(playlistMenu), item);
        }

        if (static_cast<int>(g_playlist.size()) > 50)
        {
            std::string more = "... and " +
                               std::to_string(g_playlist.size() - 50) + " more";
            GtkWidget* moreItem = gtk_menu_item_new_with_label(more.c_str());
            gtk_widget_set_sensitive(moreItem, FALSE);
            gtk_menu_shell_append(GTK_MENU_SHELL(playlistMenu), moreItem);
        }

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(playlistItem), playlistMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), playlistItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    }

    // About
    GtkWidget* about = gtk_menu_item_new_with_label("About");
    g_signal_connect(about, "activate", G_CALLBACK(OnAbout), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), about);

    // Quit
    GtkWidget* quit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit, "activate", G_CALLBACK(OnQuit), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(g_indicator, GTK_MENU(menu));
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static std::string TrackTitle(int index)
{
    if (index < 0 || index >= static_cast<int>(g_playlist.size()))
        return "(none)";

    fs::path p(g_playlist[index]);
    std::string name = p.stem().string();

    if (name.size() > 60)
        name = name.substr(0, 57) + "...";

    return name;
}

static void Log(const std::string& msg)
{
    std::string logPath = "/tmp/USBGroove.log";
    std::ofstream ofs(logPath, std::ios::app);
    if (!ofs) return;

    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char ts[32];
    strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", &tm_buf);

    ofs << ts << msg << "\n";
}

// ---------------------------------------------------------------------------
// Single-instance guard (flock)
// ---------------------------------------------------------------------------
static bool AcquireLock()
{
    std::string lockPath = "/tmp/usb-groove-" + std::to_string(getuid()) + ".lock";
    g_lockFd = open(lockPath.c_str(), O_CREAT | O_RDWR, 0600);
    if (g_lockFd < 0) return false;

    if (flock(g_lockFd, LOCK_EX | LOCK_NB) != 0)
    {
        close(g_lockFd);
        g_lockFd = -1;
        return false;  // another instance is running
    }

    return true;
}

static void ReleaseLock()
{
    if (g_lockFd >= 0)
    {
        flock(g_lockFd, LOCK_UN);
        close(g_lockFd);
        g_lockFd = -1;
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Single-instance guard
    if (!AcquireLock())
        return 0;  // another instance is running, exit silently

    // Initialize GTK, GStreamer, and libnotify
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    notify_init("USB Groove");

    Log("USB Groove (Linux) started. Waiting for USB drives...");

    // System tray indicator
    g_indicator = app_indicator_new(
        "usb-groove",
        "media-playback-start",    // stock icon — works on all desktops
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    app_indicator_set_status(g_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(g_indicator, "USB Groove");

    BuildMenu();

    // USB volume monitoring via GIO
    g_volMonitor = g_volume_monitor_get();
    g_signal_connect(g_volMonitor, "mount-added",
                     G_CALLBACK(OnMountAdded), nullptr);
    g_signal_connect(g_volMonitor, "mount-removed",
                     G_CALLBACK(OnMountRemoved), nullptr);

    Log("Volume monitoring active.");

    // Run main loop
    g_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(g_loop);

    // Cleanup
    StopPlayback();
    if (g_activeMount)
    {
        g_object_unref(g_activeMount);
        g_activeMount = nullptr;
    }
    notify_uninit();
    g_main_loop_unref(g_loop);
    g_object_unref(g_volMonitor);
    ReleaseLock();

    return 0;
}
