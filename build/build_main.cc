#if defined(_WIN32)

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include "novebrowse/windows/taskbar_badge.h"

namespace {

std::wstring GetModuleDirectory() {
  wchar_t buffer[MAX_PATH] = {0};
  DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len == 0 || len == MAX_PATH) return L"";
  std::wstring path(buffer, len);
  size_t pos = path.find_last_of(L"/\\");
  if (pos == std::wstring::npos) return L"";
  return path.substr(0, pos);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  wchar_t sep = L'\\';
  if (a.back() == sep) return a + b;
  return a + sep + b;
}

bool FileExists(const std::wstring& path) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Find the first top-level visible window belonging to a given process id.
HWND FindMainWindowForProcess(DWORD process_id, DWORD timeout_ms) {
  const DWORD start = GetTickCount();
  HWND result = nullptr;
  while (GetTickCount() - start < timeout_ms) {
    result = nullptr;
    EnumWindows(
        [](HWND hwnd, LPARAM lparam) -> BOOL {
          DWORD pid = 0;
          GetWindowThreadProcessId(hwnd, &pid);
          if (pid == 0) return TRUE;
          if (pid != static_cast<DWORD>(lparam)) return TRUE;
          if (!IsWindowVisible(hwnd)) return TRUE;
          HWND root = GetAncestor(hwnd, GA_ROOT);
          if (root != hwnd) return TRUE;
          // Heuristic: skip tool windows without taskbar presence
          LONG ex_style = GetWindowLongW(hwnd, GWL_EXSTYLE);
          if (ex_style & WS_EX_TOOLWINDOW) return TRUE;
          SetLastError(0);
          SetWindowLongPtrW(hwnd, GWLP_USERDATA, GetWindowLongPtrW(hwnd, GWLP_USERDATA));
          // Store as found
          *((HWND*)&result) = hwnd;
          return FALSE;  // stop enumeration
        },
        static_cast<LPARAM>(process_id));
    if (result) return result;
    Sleep(100);
  }
  return nullptr;
}

std::optional<int> ParseBadgeNumberFromArgs(std::vector<std::wstring>* in_out_args) {
  std::optional<int> number;
  auto& args = *in_out_args;
  auto it = std::remove_if(args.begin(), args.end(), [&](const std::wstring& a) {
    const std::wstring key_eq = L"--window-badge=";
    const std::wstring key = L"--window-badge";
    if (a.rfind(key_eq, 0) == 0) {
      try {
        int n = std::stoi(a.substr(key_eq.size()));
        if (n > 0) number = n;
      } catch (...) {}
      return true;  // remove from forwarded args
    }
    if (a == key) {
      // Next argument may hold the value
      // We handle it in a second pass by scanning positions
    }
    return false;
  });
  args.erase(it, args.end());

  // Handle "--window-badge 5" form
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == L"--window-badge") {
      try {
        int n = std::stoi(args[i + 1]);
        if (n > 0) number = n;
      } catch (...) {}
      // Remove both tokens
      args.erase(args.begin() + i, args.begin() + i + 2);
      break;
    }
  }
  return number;
}

std::wstring Quote(const std::wstring& s) {
  std::wstring q = L"\"";
  q += s;
  q += L"\"";
  return q;
}

int DoRun(HINSTANCE instance) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::wstring> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  LocalFree(argv);

  std::optional<int> badge = ParseBadgeNumberFromArgs(&args);

  // Determine chrome.exe path
  std::wstring chrome_path;
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == L"--chrome-path") {
      chrome_path = args[i + 1];
      args.erase(args.begin() + i, args.begin() + i + 2);
      break;
    }
  }

  if (chrome_path.empty()) {
    std::wstring dir = GetModuleDirectory();
    if (!dir.empty()) {
      std::wstring candidate = JoinPath(dir, L"chrome.exe");
      if (FileExists(candidate)) chrome_path = candidate;
    }
  }
  if (chrome_path.empty()) {
    // Fallback to typical Chromium out path relative to working dir
    std::wstring cwd(MAX_PATH, L'\0');
    DWORD len = GetCurrentDirectoryW(MAX_PATH, cwd.data());
    cwd.resize(len);
    std::wstring candidate = JoinPath(JoinPath(cwd, L"out"), L"Default\\chrome.exe");
    if (FileExists(candidate)) chrome_path = candidate;
  }
  if (chrome_path.empty()) {
    MessageBoxW(nullptr, L"未找到 chrome.exe，请通过 --chrome-path 指定路径，或将启动器放到 chrome.exe 同目录。",
                L"NoveBrowse 启动器", MB_ICONERROR | MB_OK);
    return 2;
  }

  // Build command line for chrome
  std::wstring cmd = Quote(chrome_path);
  for (const auto& a : args) {
    cmd += L" ";
    // Simple quoting for arguments containing spaces
    if (a.find(L' ') != std::wstring::npos || a.find(L'\"') != std::wstring::npos) {
      cmd += Quote(a);
    } else {
      cmd += a;
    }
  }

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(
          chrome_path.c_str(),
          cmd.data(),  // mutable buffer OK
          nullptr, nullptr, FALSE,
          CREATE_UNICODE_ENVIRONMENT,
          nullptr, nullptr, &si, &pi)) {
    MessageBoxW(nullptr, L"启动 chrome.exe 失败。", L"NoveBrowse 启动器", MB_ICONERROR | MB_OK);
    return 3;
  }

  // Wait for main window and apply badge
  if (badge.has_value()) {
    HWND hwnd = FindMainWindowForProcess(pi.dwProcessId, /*timeout_ms=*/30000);
    if (hwnd != nullptr) {
      novebrowse::TaskbarBadge::SetOverlayNumber(hwnd, *badge);
    }
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return 0;
}

}  // namespace

int wmain() {
  return DoRun(GetModuleHandleW(nullptr));
}

#else
int main() { return 0; }
#endif


