// local includes
#include "session_listener.h"
#include "windows_utils.h"
#include "src/logging.h"

#include <atomic>

// prevent clang format from "optimizing" the header include order
// clang-format off
#include <dbt.h>
// clang-format on

namespace display_device {

  // Static member initialization
  std::mutex SessionEventListener::mutex_;
  SessionEventListener::UnlockCallback SessionEventListener::pending_task_;
  std::thread SessionEventListener::worker_thread_;
  std::queue<SessionEventListener::UnlockCallback> SessionEventListener::task_queue_;
  std::condition_variable SessionEventListener::cv_;
  bool SessionEventListener::worker_running_ = false;
  HWND SessionEventListener::hidden_window_ = nullptr;
  std::thread SessionEventListener::message_thread_;
  std::atomic<bool> SessionEventListener::thread_running_ { false };
  std::atomic<bool> SessionEventListener::initialized_ { false };
  std::atomic<bool> SessionEventListener::event_based_ { false };

  namespace {
    const wchar_t *WINDOW_CLASS_NAME = L"SunshineSessionListener";
    std::condition_variable init_cv_;
    std::mutex init_mutex_;
    bool init_complete_ = false;
    bool init_success_ = false;

    /**
     * @brief Registration handle for suspend/resume notifications.
     * @note Only touched by the message loop thread.
     */
    HPOWERNOTIFY power_notify_ = nullptr;

    /**
     * @brief Registration handle for monitor device interface notifications.
     * @note Only touched by the message loop thread.
     */
    HDEVNOTIFY device_notify_ = nullptr;

    /**
     * @brief GUID_DEVINTERFACE_MONITOR ({E6F07B5F-EE97-4A90-B076-33F57BF4EAA7}).
     * @note Declared locally because the constant is not exported by every toolchain we build with.
     */
    const GUID MONITOR_INTERFACE_GUID { 0xe6f07b5f, 0xee97, 0x4a90, { 0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7 } };
  }

  LRESULT CALLBACK
  SessionEventListener::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // Hand the deferred task (if any) over to the worker thread. Every event that can make
    // the console desktop usable again funnels through here so that a task deferred while
    // the machine was locked or asleep is retried instead of being dropped.
    const auto dispatch_pending_task = [](const char *trigger) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!pending_task_) {
        return;
      }

      BOOST_LOG(info) << "[SessionListener] Running the deferred task after " << trigger;
      task_queue_.push(std::move(pending_task_));
      pending_task_ = nullptr;
      cv_.notify_one();
    };

    if (message == WM_WTSSESSION_CHANGE) {
      switch (wparam) {
        case WTS_SESSION_UNLOCK:
          BOOST_LOG(info) << "[SessionListener] Session unlock event detected";
          dispatch_pending_task("a session unlock");
          break;
        case WTS_SESSION_LOCK:
          BOOST_LOG(info) << "[SessionListener] Session lock event detected";
          break;
        case WTS_CONSOLE_CONNECT:
          // The console session is attached to this machine again, so anything that was
          // deferred while it was not can be retried now.
          BOOST_LOG(info) << "[SessionListener] Console connect event detected";
          dispatch_pending_task("a console connect");
          break;
        case WTS_SESSION_LOGON:
          BOOST_LOG(info) << "[SessionListener] Session logon event detected";
          dispatch_pending_task("a session logon");
          break;
        case WTS_CONSOLE_DISCONNECT:
          BOOST_LOG(info) << "[SessionListener] Console disconnect event detected";
          break;
        default:
          break;
      }
    }
    else if (message == WM_POWERBROADCAST) {
      switch (wparam) {
        case PBT_APMSUSPEND:
          BOOST_LOG(info) << "[SessionListener] System is suspending";
          break;
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
          // Windows resumes straight into the lock screen, which is exactly when display
          // work gets refused. Retry it now instead of leaving the host stranded on the
          // streaming topology.
          BOOST_LOG(info) << "[SessionListener] System resumed from suspend";
          dispatch_pending_task("a system resume");
          break;
        default:
          break;
      }
      return TRUE;
    }
    else if (message == WM_DISPLAYCHANGE) {
      BOOST_LOG(info) << "[SessionListener] Display change event detected";
      dispatch_pending_task("a display change");
    }
    else if (message == WM_DEVICECHANGE) {
      if (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE) {
        BOOST_LOG(info) << "[SessionListener] Monitor device change event detected";
        dispatch_pending_task("a monitor device change");
      }
    }
    else if (message == WM_DESTROY) {
      PostQuitMessage(0);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  void
  SessionEventListener::message_loop() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
      DWORD last_error = GetLastError();
      if (last_error != ERROR_CLASS_ALREADY_EXISTS) {
        BOOST_LOG(error) << "[SessionListener] Failed to register window class: " << last_error;
        {
          std::lock_guard<std::mutex> lock(init_mutex_);
          init_complete_ = true;
          init_success_ = false;
        }
        init_cv_.notify_one();
        return;
      }
    }

    hidden_window_ = CreateWindowExW(
      0, WINDOW_CLASS_NAME, L"SunshineSessionListenerWindow",
      0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (!hidden_window_) {
      BOOST_LOG(error) << "[SessionListener] Failed to create hidden window: " << GetLastError();
      {
        std::lock_guard<std::mutex> lock(init_mutex_);
        init_complete_ = true;
        init_success_ = false;
      }
      init_cv_.notify_one();
      return;
    }

    if (!WTSRegisterSessionNotification(hidden_window_, NOTIFY_FOR_THIS_SESSION)) {
      BOOST_LOG(warning) << "[SessionListener] Failed to register for session notifications: " << GetLastError();
      DestroyWindow(hidden_window_);
      hidden_window_ = nullptr;
      {
        std::lock_guard<std::mutex> lock(init_mutex_);
        init_complete_ = true;
        init_success_ = false;
      }
      init_cv_.notify_one();
      return;
    }

    // A message-only window is not a top-level window, so it never receives the broadcast
    // WM_POWERBROADCAST / WM_DEVICECHANGE messages. Subscribe explicitly instead. Neither
    // registration is fatal: the session notifications above keep working without them.
    power_notify_ = RegisterSuspendResumeNotification(hidden_window_, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!power_notify_) {
      BOOST_LOG(warning) << "[SessionListener] Failed to register for suspend/resume notifications: " << GetLastError();
    }

    DEV_BROADCAST_DEVICEINTERFACE_W monitor_filter {};
    monitor_filter.dbcc_size = sizeof(monitor_filter);
    monitor_filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    monitor_filter.dbcc_classguid = MONITOR_INTERFACE_GUID;

    device_notify_ = RegisterDeviceNotificationW(hidden_window_, &monitor_filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!device_notify_) {
      BOOST_LOG(warning) << "[SessionListener] Failed to register for monitor device notifications: " << GetLastError();
    }

    BOOST_LOG(info) << "[SessionListener] Session event listener initialized";

    {
      std::lock_guard<std::mutex> lock(init_mutex_);
      init_complete_ = true;
      init_success_ = true;
    }
    init_cv_.notify_one();

    MSG msg;
    while (thread_running_ && GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    if (device_notify_) {
      UnregisterDeviceNotification(device_notify_);
      device_notify_ = nullptr;
    }
    if (power_notify_) {
      UnregisterSuspendResumeNotification(power_notify_);
      power_notify_ = nullptr;
    }
    if (hidden_window_) {
      WTSUnRegisterSessionNotification(hidden_window_);
      DestroyWindow(hidden_window_);
      hidden_window_ = nullptr;
    }
    UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(nullptr));
  }

  void
  SessionEventListener::worker_loop() {
    BOOST_LOG(info) << "[SessionListener] Worker thread started";
    
    while (true) {
      UnlockCallback task;
      
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [] { 
          return !task_queue_.empty() || !worker_running_; 
        });
        
        if (!worker_running_ && task_queue_.empty()) {
          break;
        }
        
        if (task_queue_.empty()) {
          continue;
        }
        
        task = std::move(task_queue_.front());
        task_queue_.pop();
      }
      
      // 在锁外执行任务，避免死锁
      try {
        task();
      }
      catch (const std::exception& e) {
        BOOST_LOG(error) << "[SessionListener] Task threw an exception: " << e.what();
      }
    }
    
    BOOST_LOG(info) << "[SessionListener] Worker thread exited";
  }

  bool
  SessionEventListener::init() {
    if (initialized_) {
      return event_based_;
    }

    {
      std::lock_guard<std::mutex> lock(init_mutex_);
      init_complete_ = false;
      init_success_ = false;
    }

    // 启动worker线程
    {
      std::lock_guard<std::mutex> lock(mutex_);
      worker_running_ = true;
    }
    worker_thread_ = std::thread(worker_loop);

    // 启动消息线程
    thread_running_ = true;
    message_thread_ = std::thread(message_loop);

    // 等待初始化完成
    {
      std::unique_lock<std::mutex> lock(init_mutex_);
      init_cv_.wait(lock, [] { return init_complete_; });
    }

    initialized_ = true;
    event_based_ = init_success_;

    if (!event_based_) {
      BOOST_LOG(warning) << "[SessionListener] Failed to initialize the event listener";
      thread_running_ = false;
      if (message_thread_.joinable()) {
        message_thread_.join();
      }
    }

    return event_based_;
  }

  void
  SessionEventListener::deinit() {
    if (!initialized_) {
      return;
    }

    BOOST_LOG(info) << "[SessionListener] Cleanup started";

    thread_running_ = false;
    if (hidden_window_) {
      PostMessage(hidden_window_, WM_QUIT, 0, 0);
    }
    if (message_thread_.joinable()) {
      message_thread_.join();
    }

    // 停止worker线程
    {
      std::lock_guard<std::mutex> lock(mutex_);
      worker_running_ = false;
      cv_.notify_one();
    }

    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }

    // 清理状态
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending_task_ = nullptr;
      while (!task_queue_.empty()) {
        task_queue_.pop();
      }
    }

    initialized_ = false;
    event_based_ = false;
    BOOST_LOG(info) << "[SessionListener] Cleanup complete";
  }

  bool
  SessionEventListener::is_event_based() {
    return event_based_;
  }

  void
  SessionEventListener::add_unlock_task(UnlockCallback task) {
    if (!task) {
      return;
    }
    
    const bool is_locked = w_utils::is_user_session_locked();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_locked) {
      // 未锁定：直接提交到队列执行
      BOOST_LOG(info) << "[SessionListener] Session is not locked, running the task immediately";
      task_queue_.push(std::move(task));
      cv_.notify_one();
    }
    else {
      // 锁定中：保存任务等待解锁
      BOOST_LOG(info) << "[SessionListener] Task queued until the session unlocks";
      pending_task_ = std::move(task);
    }
  }

  void
  SessionEventListener::clear_unlock_task() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_task_ = nullptr;
  }

}  // namespace display_device
