#pragma once

// NoveBrowse - Windows Taskbar Badge (Overlay Icon) Utility
// This header-only utility creates a small overlay icon containing
// a number and applies it to a target window's taskbar button using
// ITaskbarList3::SetOverlayIcon.
//
// Usage example (Win32/Main thread):
//   HWND hwnd = ...; // top-level browser window handle
//   novebrowse::TaskbarBadge::SetOverlayNumber(hwnd, 3);
//   // To clear: novebrowse::TaskbarBadge::ClearOverlay(hwnd);

#if defined(_WIN32)

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

namespace novebrowse {

class TaskbarBadge {
 public:
  // Applies an overlay number (1-99) to the taskbar icon for the given window.
  // If number <= 0, the overlay is cleared.
  static bool SetOverlayNumber(HWND window_handle, int number) {
    if (window_handle == nullptr) {
      return false;
    }
    if (number <= 0) {
      return ClearOverlay(window_handle);
    }
    if (number > 99) {
      number = 99;
    }

    HICON number_icon = CreateNumberOverlayIcon(number);
    if (number_icon == nullptr) {
      return false;
    }

    bool result = ApplyOverlayIcon(window_handle, number_icon);
    DestroyIcon(number_icon);
    return result;
  }

  // Clears any existing overlay icon on the taskbar button for the window.
  static bool ClearOverlay(HWND window_handle) {
    if (window_handle == nullptr) {
      return false;
    }
    CComPtr<ITaskbarList3> taskbar;
    if (!CreateTaskbarInstance(&taskbar)) {
      return false;
    }
    HRESULT hr = taskbar->SetOverlayIcon(window_handle, nullptr, L"");
    return SUCCEEDED(hr);
  }

 private:
  // Creates a 32x32 ARGB icon with a colored circle and white number text.
  static HICON CreateNumberOverlayIcon(int number) {
    const int kIconSize = 32;  // Windows scales overlay appropriately.

    BITMAPV5HEADER header = {};
    header.bV5Size = sizeof(BITMAPV5HEADER);
    header.bV5Width = kIconSize;
    header.bV5Height = -kIconSize;  // Negative for top-down DIB.
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;

    HDC screen_dc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color_bitmap = CreateDIBSection(screen_dc,
                                            reinterpret_cast<const BITMAPINFO*>(&header),
                                            DIB_RGB_COLORS,
                                            &bits,
                                            nullptr,
                                            0);
    if (screen_dc) ReleaseDC(nullptr, screen_dc);
    if (!color_bitmap || !bits) {
      if (color_bitmap) DeleteObject(color_bitmap);
      return nullptr;
    }

    // Prepare a memory DC to draw the badge.
    HDC mem_dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old_bitmap = SelectObject(mem_dc, color_bitmap);

    // Transparent background.
    RECT rect = {0, 0, kIconSize, kIconSize};
    HBRUSH transparent_brush = CreateSolidBrush(RGB(0, 0, 0));
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    // Manually clear pixels to transparent.
    ZeroMemory(bits, kIconSize * kIconSize * 4);

    // Draw a filled red circle as the badge background.
    HBRUSH circle_brush = CreateSolidBrush(RGB(220, 0, 0));
    HBRUSH old_brush = (HBRUSH)SelectObject(mem_dc, circle_brush);
    HPEN null_pen = (HPEN)SelectObject(mem_dc, GetStockObject(NULL_PEN));
    const int inset = 2;
    Ellipse(mem_dc, inset, inset, kIconSize - inset, kIconSize - inset);
    SelectObject(mem_dc, old_brush);
    SelectObject(mem_dc, null_pen);
    DeleteObject(circle_brush);

    // Draw white number text centered.
    wchar_t text[4] = {0};
    if (number < 10) {
      text[0] = L'0' + number;
    } else {
      text[0] = L'0' + (number / 10);
      text[1] = L'0' + (number % 10);
    }

    HFONT font = CreateFontW(
        20, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(mem_dc, font);
    SetTextColor(mem_dc, RGB(255, 255, 255));
    SetBkMode(mem_dc, TRANSPARENT);

    DrawTextW(mem_dc, text, lstrlenW(text), &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(mem_dc, old_font);
    DeleteObject(font);

    // Create mask bitmap (unused for alpha icon, but required by API).
    HBITMAP mask_bitmap = CreateBitmap(kIconSize, kIconSize, 1, 1, nullptr);

    ICONINFO icon_info = {};
    icon_info.fIcon = TRUE;
    icon_info.hbmColor = color_bitmap;
    icon_info.hbmMask = mask_bitmap;

    HICON hicon = CreateIconIndirect(&icon_info);

    // Cleanup GDI objects retained in DC selection stack.
    SelectObject(mem_dc, old_bitmap);
    DeleteDC(mem_dc);
    DeleteObject(color_bitmap);
    DeleteObject(mask_bitmap);

    return hicon;
  }

  static bool ApplyOverlayIcon(HWND window_handle, HICON icon) {
    CComPtr<ITaskbarList3> taskbar;
    if (!CreateTaskbarInstance(&taskbar)) {
      return false;
    }
    HRESULT hr = taskbar->SetOverlayIcon(window_handle, icon, L"NoveBrowse Badge");
    return SUCCEEDED(hr);
  }

  static bool CreateTaskbarInstance(ITaskbarList3** out_taskbar) {
    if (out_taskbar == nullptr) {
      return false;
    }
    *out_taskbar = nullptr;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool need_uninit = SUCCEEDED(hr);

    ITaskbarList3* taskbar = nullptr;
    hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&taskbar));
    if (FAILED(hr) || taskbar == nullptr) {
      if (need_uninit) CoUninitialize();
      return false;
    }
    taskbar->HrInit();

    // The returned pointer is managed by caller. We do not uninitialize COM
    // here because the taskbar object may rely on the apartment; the caller
    // process should keep COM initialized for the lifetime of usage.
    *out_taskbar = taskbar;
    return true;
  }
};

}  // namespace novebrowse

#endif  // defined(_WIN32)


