// The WCMG window: pick or detect the game, run the diagnosis on a worker
// thread, list the findings, show one in full, and save or zip the result.
// Plain Win32 with common controls 6; the system decides the look.
#include "win_common.hpp"
#include "scanners.hpp"
#include "wcmg_version.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <ctime>
#include <fstream>
#include <memory>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace wcmg::win {
namespace {

enum : UINT {
    WM_WCMG_PROGRESS = WM_APP + 1,  // lParam: std::string*
    WM_WCMG_DIAGNOSED,              // lParam: DiagDone*
    WM_WCMG_DETECTED,               // lParam: DetectDone*
    WM_WCMG_COLLECTED,              // lParam: ZipDone*
};

enum : int {
    IDC_LABEL = 100, IDC_PATH, IDC_DETECT, IDC_BROWSE, IDC_DIAGNOSE, IDC_STATUS,
    IDC_LIST, IDC_DETAIL, IDC_ZIP, IDC_SAVE, IDC_COPY, IDC_ABOUT,
};

struct DiagDone { std::shared_ptr<const Report> report; std::string error; };
struct DetectDone { std::vector<Paths> found; bool manual = false; };
struct ZipDone { fs::path path; std::string error; };

const wchar_t* kClass = L"WCMG.Main";
const wchar_t* kArtClass = L"WCMG.Art";
constexpr int kIconId = 1, kArtId = 2;  // match wcmg.rc
const wchar_t* kArtLine = L"Everyone is pointing at everyone. Click Diagnose.";
const wchar_t* kArtBusyLine = L"Checking the evidence. This can take a minute.";
const wchar_t* kTitle = L"Who Crashed My Game";

struct App {
    HINSTANCE inst = nullptr;
    HWND wnd = nullptr;
    HWND label = nullptr, path = nullptr, detect = nullptr, browse = nullptr, diagnose = nullptr, status = nullptr;
    HWND list = nullptr, detail = nullptr, zip = nullptr, save = nullptr, copy = nullptr, about = nullptr;
    HWND art = nullptr;              // empty-state panel, swaps with `detail` until a report exists
    HFONT font = nullptr;
    HICON icon_big = nullptr, icon_small = nullptr;
    IWICImagingFactory* wic = nullptr;
    IWICBitmap* meme = nullptr;      // decoded once, 32bpp premultiplied BGRA
    HBITMAP art_bmp = nullptr;       // `meme` scaled to the pane and flattened on the window colour
    int art_w = 0, art_h = 0;
    UINT dpi = 96;
    int text_h = 16;

    std::shared_ptr<const Report> report;
    std::vector<Paths> found;
    bool diagnosing = false, zipping = false, detecting = false;
    std::wstring start_path;  // from the command line
    bool auto_diagnose = false;

    int px(int v) const { return MulDiv(v, (int)dpi, 96); }
};

// Worker threads hand results back as heap objects; if the window is gone the
// message never arrives, so the sender frees it.
template <class T>
void post(HWND wnd, UINT msg, T* payload) {
    if (!PostMessageW(wnd, msg, 0, (LPARAM)payload)) delete payload;
}

std::wstring crlf(const std::string& s) {
    std::string out;
    out.reserve(s.size() + s.size() / 16);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r')) out += '\r';
        out += s[i];
    }
    return widen(out);
}

std::wstring window_text(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s((size_t)n + 1, L'\0');
    GetWindowTextW(h, s.data(), n + 1);
    s.resize((size_t)n);
    return s;
}

std::wstring tidy_input(std::wstring s) {
    while (!s.empty() && iswspace(s.back())) s.pop_back();
    size_t b = 0;
    while (b < s.size() && iswspace(s[b])) ++b;
    s.erase(0, b);
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') s = s.substr(1, s.size() - 2);
    return s;
}

std::wstring stamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[32];
    wcsftime(buf, 32, L"%Y%m%d-%H%M", &tm);
    return buf;
}

std::wstring known_folder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = p;
    CoTaskMemFree(p);
    return out;
}

std::wstring default_save_dir() {
    for (auto* id : {&FOLDERID_Desktop, &FOLDERID_Documents}) {
        auto d = known_folder(*id);
        std::error_code ec;
        if (!d.empty() && fs::is_directory(d, ec)) return d;
    }
    return L"";
}

fs::path exe_dir() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
    return fs::path(std::wstring(buf, n)).parent_path();
}

std::wstring severity_label(Severity s) {
    switch (s) {
    case Severity::Critical: return L"Critical";
    case Severity::High: return L"High";
    case Severity::Medium: return L"Medium";
    case Severity::Low: return L"Low";
    case Severity::Info: return L"Info";
    }
    return L"Info";
}

std::string finding_text(const Finding& f) {
    std::string o = f.title + "\n\n";
    o += "Severity: " + narrow(severity_label(f.severity)) + "    Confidence: " + std::to_string(f.confidence) + "%";
    if (!f.blame.empty()) o += "    Blame: " + f.blame;
    o += "\n";
    if (!f.summary.empty()) o += "\n" + f.summary + "\n";
    if (!f.details.empty()) o += "\n" + f.details + "\n";
    if (!f.evidence.empty()) {
        o += "\nEvidence\n";
        for (auto& e : f.evidence) o += "  - " + e + "\n";
    }
    if (!f.advice.empty()) o += "\nWhat to do\n" + f.advice + "\n";
    if (!f.id.empty()) o += "\nRule: " + f.id + "\n";
    return o;
}

void set_status(App& a, const std::wstring& s) { SetWindowTextW(a.status, s.c_str()); }

void update_buttons(App& a) {
    bool busy = a.diagnosing || a.zipping;
    EnableWindow(a.path, !busy);
    EnableWindow(a.browse, !busy);
    EnableWindow(a.detect, !busy && !a.detecting);
    EnableWindow(a.diagnose, !busy);
    bool have = a.report != nullptr && !busy;
    EnableWindow(a.zip, have);
    EnableWindow(a.save, have);
    EnableWindow(a.copy, have);
}

// ---- fonts and layout ----

void make_font(App& a) {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof ncm;
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, a.dpi)) {
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
        ncm.lfMessageFont.lfHeight = MulDiv(ncm.lfMessageFont.lfHeight, (int)a.dpi, (int)GetDpiForSystem());
    }
    HFONT f = CreateFontIndirectW(&ncm.lfMessageFont);
    if (a.font) DeleteObject(a.font);
    a.font = f;
    HDC dc = GetDC(a.wnd);
    HGDIOBJ old = SelectObject(dc, a.font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(a.wnd, dc);
    a.text_h = tm.tmHeight;
    for (HWND h : {a.label, a.path, a.detect, a.browse, a.diagnose, a.status, a.list, a.detail, a.zip, a.save, a.copy, a.about})
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)a.font, TRUE);
    if (a.art) InvalidateRect(a.art, nullptr, TRUE);
    if (a.detail) SendMessageW(a.detail, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(a.px(6), a.px(6)));
}

int text_width(App& a, HWND h) {
    auto s = window_text(h);
    HDC dc = GetDC(a.wnd);
    HGDIOBJ old = SelectObject(dc, a.font);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, old);
    ReleaseDC(a.wnd, dc);
    return sz.cx;
}

int button_width(App& a, HWND h) { return std::max(a.px(88), text_width(a, h) + a.px(24)); }

void size_columns(App& a) {
    RECT rc;
    GetClientRect(a.list, &rc);
    int sev = a.px(78), conf = a.px(86), blame = a.px(150);
    int title = std::max(a.px(120), (int)rc.right - sev - conf - blame);
    ListView_SetColumnWidth(a.list, 0, sev);
    ListView_SetColumnWidth(a.list, 1, title);
    ListView_SetColumnWidth(a.list, 2, blame);
    ListView_SetColumnWidth(a.list, 3, conf);
}

void layout(App& a) {
    RECT rc;
    GetClientRect(a.wnd, &rc);
    const int W = rc.right, H = rc.bottom;
    const int m = a.px(12), gap = a.px(8);
    const int bh = std::max(a.px(26), a.text_h + a.px(10));

    HDWP dwp = BeginDeferWindowPos(13);
    auto place = [&](HWND h, int x, int y, int w, int hh) {
        dwp = DeferWindowPos(dwp, h, nullptr, x, y, std::max(w, 0), std::max(hh, 0), SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // Top row: label, path box, three buttons on the right.
    int y = m;
    int wd = button_width(a, a.diagnose), wb = button_width(a, a.browse), wdt = button_width(a, a.detect);
    int x_right = W - m;
    int x_diag = x_right - wd, x_browse = x_diag - gap - wb, x_detect = x_browse - gap - wdt;
    int lw = text_width(a, a.label);
    place(a.label, m, y + (bh - a.text_h) / 2, lw, a.text_h + a.px(2));
    int px0 = m + lw + gap;
    // A drop-down combo sizes its edit part from the font; the height given here is the list's.
    place(a.path, px0, y + (bh - (a.text_h + a.px(8))) / 2, x_detect - gap - px0, a.px(260));
    place(a.detect, x_detect, y, wdt, bh);
    place(a.browse, x_browse, y, wb, bh);
    place(a.diagnose, x_diag, y, wd, bh);
    y += bh + gap / 2;

    place(a.status, m, y, W - 2 * m, a.text_h + a.px(4));
    y += a.text_h + a.px(4) + gap;

    // Bottom row.
    int yb = H - m - bh;
    int x = m;
    for (HWND h : {a.zip, a.save, a.copy}) {
        int w = button_width(a, h);
        place(h, x, yb, w, bh);
        x += w + gap;
    }
    int aw = text_width(a, a.about) + a.px(4);
    place(a.about, std::max(x, W - m - aw), yb + (bh - a.text_h) / 2, aw, a.text_h + a.px(2));

    // Panes: findings on the left, the selected one on the right.
    int top = y, bottom = yb - gap;
    int avail = W - 2 * m - gap;
    int left = avail * 52 / 100;
    place(a.list, m, top, left, bottom - top);
    place(a.detail, m + left + gap, top, avail - left, bottom - top);
    place(a.art, m + left + gap, top, avail - left, bottom - top);
    EndDeferWindowPos(dwp);
    size_columns(a);
}

// ---- content ----

void show_art(App& a, bool on) {
    ShowWindow(a.art, on ? SW_SHOW : SW_HIDE);
    ShowWindow(a.detail, on ? SW_HIDE : SW_SHOW);
    if (on) InvalidateRect(a.art, nullptr, TRUE);
}

void show_detail(App& a) {
    show_art(a, !a.report);
    if (!a.report) return;
    int sel = ListView_GetNextItem(a.list, -1, LVNI_SELECTED);
    std::string text;
    if (sel >= 0 && sel < (int)a.report->findings.size()) text = finding_text(a.report->findings[(size_t)sel]);
    else {
        try { text = report_text(*a.report); }
        catch (const std::exception& e) { text = std::string("The report could not be written: ") + e.what(); }
    }
    SetWindowTextW(a.detail, crlf(text).c_str());
}

void fill_findings(App& a) {
    SendMessageW(a.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(a.list);
    if (a.report) {
        int i = 0;
        for (auto& f : a.report->findings) {
            auto sev = severity_label(f.severity);
            LVITEMW it{};
            it.mask = LVIF_TEXT | LVIF_PARAM;
            it.iItem = i;
            it.pszText = sev.data();
            it.lParam = i;
            ListView_InsertItem(a.list, &it);
            auto title = widen(f.title), blame = widen(f.blame), conf = std::to_wstring(f.confidence) + L"%";
            ListView_SetItemText(a.list, i, 1, title.data());
            ListView_SetItemText(a.list, i, 2, blame.data());
            ListView_SetItemText(a.list, i, 3, conf.data());
            ++i;
        }
    }
    SendMessageW(a.list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(a.list, nullptr, TRUE);
    show_detail(a);
}

LRESULT list_custom_draw(App& a, NMLVCUSTOMDRAW* cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        size_t i = (size_t)cd->nmcd.lItemlParam;
        if (!a.report || i >= a.report->findings.size()) return CDRF_DODEFAULT;
        // Classic (unthemed) selection paints white on blue; leave that alone.
        bool selected = ListView_GetItemState(a.list, (int)cd->nmcd.dwItemSpec, LVIS_SELECTED) != 0;
        if (selected && !IsAppThemed()) return CDRF_DODEFAULT;
        switch (a.report->findings[i].severity) {
        case Severity::Critical: cd->clrText = RGB(176, 0, 0); break;
        case Severity::High: cd->clrText = RGB(192, 40, 24); break;
        case Severity::Medium: cd->clrText = RGB(150, 90, 0); break;
        default: return CDRF_DODEFAULT;
        }
        return CDRF_NEWFONT;
    }
    }
    return CDRF_DODEFAULT;
}

// ---- empty-state artwork ----

void load_meme(App& a) {
    if (a.meme || !a.wic) return;
    HRSRC res = FindResourceW(a.inst, MAKEINTRESOURCEW(kArtId), RT_RCDATA);
    HGLOBAL h = res ? LoadResource(a.inst, res) : nullptr;
    auto* bytes = h ? static_cast<const BYTE*>(LockResource(h)) : nullptr;
    if (!bytes) return;
    IStream* stream = SHCreateMemStream(bytes, SizeofResource(a.inst, res));
    if (!stream) return;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* conv = nullptr;
    if (SUCCEEDED(a.wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, frame, &conv)))
        a.wic->CreateBitmapFromSource(conv, WICBitmapCacheOnLoad, &a.meme);
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    stream->Release();
}

// Scales with WIC (much smoother than StretchBlt) and flattens the alpha onto
// the window colour so painting is a plain BitBlt.
void rebuild_art(App& a, int w, int h) {
    if (a.art_bmp && a.art_w == w && a.art_h == h) return;
    if (a.art_bmp) DeleteObject(a.art_bmp);
    a.art_bmp = nullptr;
    a.art_w = a.art_h = 0;
    if (!a.meme || w <= 0 || h <= 0) return;
    IWICBitmapScaler* scaler = nullptr;
    if (FAILED(a.wic->CreateBitmapScaler(&scaler))) return;
    if (SUCCEEDED(scaler->Initialize(a.meme, (UINT)w, (UINT)h, WICBitmapInterpolationModeFant))) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && SUCCEEDED(scaler->CopyPixels(nullptr, (UINT)w * 4, (UINT)w * 4 * (UINT)h, static_cast<BYTE*>(bits)))) {
            COLORREF bg = GetSysColor(COLOR_WINDOW);
            const unsigned bgc[3] = {GetBValue(bg), GetGValue(bg), GetRValue(bg)};
            auto* px = static_cast<BYTE*>(bits);
            for (size_t i = 0, n = (size_t)w * (size_t)h; i < n; ++i, px += 4) {
                unsigned inv = 255u - px[3];
                for (int c = 0; c < 3; ++c) px[c] = (BYTE)std::min(255u, px[c] + (bgc[c] * inv + 127) / 255);
                px[3] = 255;
            }
            a.art_bmp = bmp;
            a.art_w = w;
            a.art_h = h;
        } else if (bmp) DeleteObject(bmp);
    }
    scaler->Release();
}

void paint_art(App& a, HWND wnd, HDC dc) {
    RECT rc;
    GetClientRect(wnd, &rc);
    FillRect(dc, &rc, GetSysColorBrush(COLOR_WINDOW));
    load_meme(a);
    const int m = a.px(16), gap = a.px(12);
    int line_h = a.text_h + a.px(4);
    int aw = rc.right - 2 * m, ah = rc.bottom - 2 * m - gap - line_h;
    int w = 0, h = 0;
    if (a.meme && aw > 0 && ah > 0) {
        UINT iw = 0, ih = 0;
        a.meme->GetSize(&iw, &ih);
        w = aw;
        h = MulDiv(w, (int)ih, (int)iw);
        if (h > ah) { h = ah; w = MulDiv(h, (int)iw, (int)ih); }
        rebuild_art(a, w, h);
    }
    int block = (a.art_bmp ? h + gap : 0) + line_h;
    int y = std::max(m, ((int)rc.bottom - block) / 2);
    if (a.art_bmp) {
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, a.art_bmp);
        BitBlt(dc, (rc.right - w) / 2, y, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        y += h + gap;
    }
    RECT tr{m, y, rc.right - m, y + line_h};
    HGDIOBJ oldf = SelectObject(dc, a.font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    DrawTextW(dc, a.diagnosing ? kArtBusyLine : kArtLine, -1, &tr, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldf);
}

LRESULT CALLBACK art_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* a = reinterpret_cast<App*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        a = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(wnd, GWLP_USERDATA, (LONG_PTR)a);
    }
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(wnd, nullptr, FALSE); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(wnd, &ps);
        if (a) paint_art(*a, wnd, dc);
        EndPaint(wnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(wnd, msg, wp, lp);
}

void load_icons(App& a) {
    HICON big = (HICON)LoadImageW(a.inst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON, GetSystemMetricsForDpi(SM_CXICON, a.dpi),
                                  GetSystemMetricsForDpi(SM_CYICON, a.dpi), LR_DEFAULTCOLOR);
    HICON little = (HICON)LoadImageW(a.inst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON, GetSystemMetricsForDpi(SM_CXSMICON, a.dpi),
                                    GetSystemMetricsForDpi(SM_CYSMICON, a.dpi), LR_DEFAULTCOLOR);
    if (big) { SendMessageW(a.wnd, WM_SETICON, ICON_BIG, (LPARAM)big); if (a.icon_big) DestroyIcon(a.icon_big); a.icon_big = big; }
    if (little) { SendMessageW(a.wnd, WM_SETICON, ICON_SMALL, (LPARAM)little); if (a.icon_small) DestroyIcon(a.icon_small); a.icon_small = little; }
}

// ---- actions ----

void start_detect(App& a, bool manual) {
    if (a.detecting) return;
    a.detecting = true;
    update_buttons(a);
    if (!a.diagnosing) set_status(a, L"Looking for the game...");
    HWND wnd = a.wnd;
    std::thread([wnd, manual] {
        auto* done = new DetectDone;
        done->manual = manual;
        try { done->found = discover_installs(); } catch (...) {}
        post(wnd, WM_WCMG_DETECTED, done);
    }).detach();
}

void on_detected(App& a, std::unique_ptr<DetectDone> d) {
    a.detecting = false;
    a.found = d->found;
    // Resetting a drop-down combo also clears its edit text; keep what the user typed.
    auto typed = window_text(a.path);
    bool keep_text = !d->manual && (!a.start_path.empty() || !tidy_input(typed).empty());
    SendMessageW(a.path, CB_RESETCONTENT, 0, 0);
    for (auto& p : a.found) SendMessageW(a.path, CB_ADDSTRING, 0, (LPARAM)p.game_root.wstring().c_str());
    if (keep_text || a.found.empty()) SetWindowTextW(a.path, typed.c_str());
    else SendMessageW(a.path, CB_SETCURSEL, 0, 0);
    update_buttons(a);
    if (a.diagnosing) return;  // its progress owns the status line

    if (a.found.empty()) {
        set_status(a, L"No install found. Use Browse to pick the game folder.");
        if (d->manual)
            MessageBoxW(a.wnd,
                        L"WCMG could not find Mortal Shell II through Steam, Epic or the usual folders.\n\n"
                        L"Use Browse to pick the game folder. On Steam it is called Sparta, inside steamapps\\common.",
                        kTitle, MB_OK | MB_ICONINFORMATION);
    } else if (a.found.size() == 1) {
        set_status(a, L"Found the game (" + widen(a.found[0].how_found) + L"). Click Diagnose to check it.");
    } else {
        set_status(a, L"Found " + std::to_wstring(a.found.size()) + L" installs. Pick one from the list next to the path, then click Diagnose.");
        if (d->manual) SendMessageW(a.path, CB_SHOWDROPDOWN, TRUE, 0);
    }
}

void start_diagnosis(App& a) {
    if (a.diagnosing || a.zipping) return;
    auto text = tidy_input(window_text(a.path));
    if (text.empty()) {
        MessageBoxW(a.wnd, L"Pick the game folder first, with Detect or Browse.", kTitle, MB_OK | MB_ICONINFORMATION);
        return;
    }
    fs::path pick(text);
    std::optional<Paths> paths;
    std::error_code ec;
    for (auto& f : a.found)
        if (fs::equivalent(f.game_root, pick, ec)) { paths = f; break; }
    if (!paths) paths = paths_from_user_pick(pick);
    if (!paths) {
        MessageBoxW(a.wnd,
                    (L"Mortal Shell II was not found in\n" + text +
                     L"\n\nPick the game's install folder (on Steam it is called Sparta), the MortalShell2 folder inside it, "
                     L"or MortalShell2-Win64-Shipping.exe.").c_str(),
                    kTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    std::string why;
    if (!validate_paths(*paths, &why)) {
        MessageBoxW(a.wnd, widen(why).c_str(), kTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    std::optional<fs::path> knowledge;
    if (auto k = exe_dir() / L"known.json"; fs::is_regular_file(k, ec)) knowledge = k;

    a.diagnosing = true;
    a.report.reset();
    fill_findings(a);  // also brings the artwork back while the new report is built
    SetWindowTextW(a.detail, L"");
    update_buttons(a);
    set_status(a, L"Diagnosing " + paths->game_root.wstring() + L"...");

    HWND wnd = a.wnd;
    std::thread([wnd, p = *paths, knowledge] {
        auto* done = new DiagDone;
        try {
            done->report = std::make_shared<const Report>(
                diagnose(p, knowledge, [wnd](const std::string& s) { post(wnd, WM_WCMG_PROGRESS, new std::string(s)); }));
        } catch (const std::exception& e) {
            done->error = e.what();
        } catch (...) {
            done->error = "unknown error";
        }
        post(wnd, WM_WCMG_DIAGNOSED, done);
    }).detach();
}

void on_diagnosed(App& a, std::unique_ptr<DiagDone> d) {
    a.diagnosing = false;
    InvalidateRect(a.art, nullptr, FALSE);
    if (!d->report) {
        update_buttons(a);
        set_status(a, L"Ready");
        MessageBoxW(a.wnd, (L"The diagnosis stopped: " + widen(d->error)).c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    a.report = d->report;
    fill_findings(a);
    update_buttons(a);
    size_t n = a.report->findings.size();
    if (n == 0) set_status(a, L"Ready. No findings; the report on the right says what was checked.");
    else set_status(a, L"Ready. " + std::to_wstring(n) + (n == 1 ? L" finding" : L" findings") + L", most likely cause first. Select one to read it.");
    if (n) SetFocus(a.list);
}

std::optional<std::wstring> ask_save_path(App& a, const std::wstring& name, const wchar_t* filter, const wchar_t* ext) {
    wchar_t file[MAX_PATH * 4] = L"";
    wcsncpy_s(file, name.c_str(), _TRUNCATE);
    auto dir = default_save_dir();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = a.wnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = (DWORD)std::size(file);
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return std::wstring(file);
}

void start_zip(App& a) {
    if (!a.report || a.zipping || a.diagnosing) return;
    auto out = ask_save_path(a, L"WCMG-" + stamp() + L".zip", L"Zip archive (*.zip)\0*.zip\0All files\0*.*\0", L"zip");
    if (!out) return;
    a.zipping = true;
    update_buttons(a);
    set_status(a, L"Writing the support bundle...");
    HWND wnd = a.wnd;
    std::thread([wnd, report = a.report, target = fs::path(*out)] {
        auto* done = new ZipDone;
        try {
            done->path = collect_bundle(*report, target, [wnd](const std::string& s) { post(wnd, WM_WCMG_PROGRESS, new std::string(s)); });
        } catch (const std::exception& e) {
            done->error = e.what();
        } catch (...) {
            done->error = "unknown error";
        }
        post(wnd, WM_WCMG_COLLECTED, done);
    }).detach();
}

void on_collected(App& a, std::unique_ptr<ZipDone> d) {
    a.zipping = false;
    update_buttons(a);
    if (!d->error.empty()) {
        set_status(a, L"Ready");
        MessageBoxW(a.wnd, (L"The support bundle could not be written.\n\n" + widen(d->error)).c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    auto p = d->path.wstring();
    set_status(a, L"Ready. Support bundle saved to " + p);
    if (MessageBoxW(a.wnd, (L"The support bundle is ready:\n\n" + p + L"\n\nOpen the folder that holds it?").c_str(), kTitle,
                    MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        auto args = L"/select,\"" + p + L"\"";
        if ((INT_PTR)ShellExecuteW(a.wnd, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL) <= 32)
            ShellExecuteW(a.wnd, L"open", d->path.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

std::string markdown_or_note(App& a) {
    try { return report_markdown(*a.report); }
    catch (const std::exception& e) { return std::string("The report could not be written: ") + e.what(); }
}

void save_report(App& a) {
    if (!a.report) return;
    auto out = ask_save_path(a, L"WCMG-report-" + stamp() + L".md", L"Markdown (*.md)\0*.md\0Text (*.txt)\0*.txt\0All files\0*.*\0", L"md");
    if (!out) return;
    auto md = markdown_or_note(a);
    std::ofstream f(fs::path(*out), std::ios::binary | std::ios::trunc);
    f.write(md.data(), (std::streamsize)md.size());
    f.close();
    if (!f) {
        MessageBoxW(a.wnd, (L"Could not write " + *out).c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    set_status(a, L"Report saved to " + *out);
}

void copy_report(App& a) {
    if (!a.report) return;
    auto text = crlf(markdown_or_note(a));
    if (!OpenClipboard(a.wnd)) {
        MessageBoxW(a.wnd, L"The clipboard is busy. Try again in a moment.", kTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* p = GlobalLock(mem)) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(mem);
            if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        } else GlobalFree(mem);
    }
    CloseClipboard();
    set_status(a, L"Report copied to the clipboard. Paste it into your bug report.");
}

void browse(App& a) {
    auto cur = tidy_input(window_text(a.path));
    std::error_code ec;
    while (!cur.empty() && !fs::is_directory(cur, ec)) {
        auto parent = fs::path(cur).parent_path().wstring();
        if (parent == cur) break;
        cur = parent;
    }
    if (auto picked = pick_folder(a.wnd, cur)) {
        SetWindowTextW(a.path, picked->c_str());
        if (auto p = paths_from_user_pick(*picked))
            set_status(a, L"Game folder set to " + p->game_root.wstring() + L". Click Diagnose to check it.");
        else
            set_status(a, L"That folder does not look like Mortal Shell II. Pick the Sparta folder or the exe.");
    }
}

// ---- window ----

HWND child(App& a, const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, a.wnd, (HMENU)(INT_PTR)id, a.inst, nullptr);
}

void create_children(App& a) {
    a.label = child(a, WC_STATICW, L"Game folder", SS_LEFT | SS_NOPREFIX, IDC_LABEL);
    a.path = child(a, WC_COMBOBOXW, L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, IDC_PATH);
    a.detect = child(a, WC_BUTTONW, L"Detect", BS_PUSHBUTTON | WS_TABSTOP, IDC_DETECT);
    a.browse = child(a, WC_BUTTONW, L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, IDC_BROWSE);
    a.diagnose = child(a, WC_BUTTONW, L"Diagnose", BS_DEFPUSHBUTTON | WS_TABSTOP, IDC_DIAGNOSE);
    a.status = child(a, WC_STATICW, L"Ready", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, IDC_STATUS);
    a.list = child(a, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, IDC_LIST, WS_EX_CLIENTEDGE);
    a.detail = child(a, WC_EDITW, L"",
                     ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, IDC_DETAIL, WS_EX_CLIENTEDGE);
    a.zip = child(a, WC_BUTTONW, L"Collect ZIP...", BS_PUSHBUTTON | WS_TABSTOP, IDC_ZIP);
    a.save = child(a, WC_BUTTONW, L"Save report...", BS_PUSHBUTTON | WS_TABSTOP, IDC_SAVE);
    a.copy = child(a, WC_BUTTONW, L"Copy report", BS_PUSHBUTTON | WS_TABSTOP, IDC_COPY);
    a.about = child(a, WC_STATICW, widen("WCMG " WCMG_VERSION " by eins0fx").c_str(), SS_RIGHT | SS_NOPREFIX, IDC_ABOUT);

    a.art = CreateWindowExW(WS_EX_CLIENTEDGE, kArtClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, a.wnd, nullptr, a.inst, &a);
    ShowWindow(a.detail, SW_HIDE);
    SendMessageW(a.detail, EM_SETLIMITTEXT, 0, 0);  // 0 lifts the 32K default
    SetWindowTheme(a.list, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(a.list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    const wchar_t* cols[] = {L"Severity", L"Title", L"Blame", L"Confidence"};
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = const_cast<wchar_t*>(cols[i]);
        c.cx = a.px(80);
        c.iSubItem = i;
        ListView_InsertColumn(a.list, i, &c);
    }
    COMBOBOXINFO cbi{sizeof cbi};
    if (GetComboBoxInfo(a.path, &cbi) && cbi.hwndItem) SendMessageW(cbi.hwndItem, EM_SETCUEBANNER, TRUE, (LPARAM)L"The Sparta folder, or anything inside it");
    make_font(a);
    update_buttons(a);
}

LRESULT CALLBACK wnd_proc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* a = reinterpret_cast<App*>(GetWindowLongPtrW(wnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        a = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        a->wnd = wnd;
        SetWindowLongPtrW(wnd, GWLP_USERDATA, (LONG_PTR)a);
    }
    if (!a) return DefWindowProcW(wnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        a->dpi = GetDpiForWindow(wnd);
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&a->wic));
        load_icons(*a);
        create_children(*a);
        layout(*a);
        if (!a->start_path.empty()) {
            SetWindowTextW(a->path, a->start_path.c_str());
            a->auto_diagnose = true;
        }
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) layout(*a);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        mm->ptMinTrackSize.x = a->px(900);
        mm->ptMinTrackSize.y = a->px(600);
        return 0;
    }
    case WM_DPICHANGED: {
        a->dpi = HIWORD(wp);
        make_font(*a);
        load_icons(*a);
        auto* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(wnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout(*a);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        // The read-only detail pane reads better on the window colour than on grey.
        if ((HWND)lp == a->detail) {
            HDC dc = (HDC)wp;
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(dc, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        switch (id) {
        case IDOK: // Enter anywhere outside the panes
        case IDC_DIAGNOSE: if (code == BN_CLICKED || id == IDOK) start_diagnosis(*a); return 0;
        case IDC_DETECT: start_detect(*a, true); return 0;
        case IDC_BROWSE: browse(*a); return 0;
        case IDC_ZIP: start_zip(*a); return 0;
        case IDC_SAVE: save_report(*a); return 0;
        case IDC_COPY: copy_report(*a); return 0;
        case IDC_PATH:
            if (code == CBN_SELCHANGE) {
                int i = (int)SendMessageW(a->path, CB_GETCURSEL, 0, 0);
                if (i >= 0 && i < (int)a->found.size())
                    set_status(*a, L"Selected " + a->found[(size_t)i].game_root.wstring() + L" (" + widen(a->found[(size_t)i].how_found) + L").");
            }
            return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        auto* h = reinterpret_cast<NMHDR*>(lp);
        if (h->hwndFrom == a->list) {
            if (h->code == NM_CUSTOMDRAW) return list_custom_draw(*a, reinterpret_cast<NMLVCUSTOMDRAW*>(lp));
            if (h->code == LVN_ITEMCHANGED) {
                auto* nm = reinterpret_cast<NMLISTVIEW*>(lp);
                if (nm->uChanged & LVIF_STATE && ((nm->uOldState ^ nm->uNewState) & LVIS_SELECTED)) show_detail(*a);
            }
        }
        break;
    }
    case WM_WCMG_PROGRESS: {
        std::unique_ptr<std::string> s(reinterpret_cast<std::string*>(lp));
        if (a->diagnosing || a->zipping) set_status(*a, widen(*s) + L"...");
        return 0;
    }
    case WM_WCMG_DIAGNOSED: on_diagnosed(*a, std::unique_ptr<DiagDone>(reinterpret_cast<DiagDone*>(lp))); return 0;
    case WM_WCMG_DETECTED: on_detected(*a, std::unique_ptr<DetectDone>(reinterpret_cast<DetectDone*>(lp))); return 0;
    case WM_WCMG_COLLECTED: on_collected(*a, std::unique_ptr<ZipDone>(reinterpret_cast<ZipDone*>(lp))); return 0;
    case WM_CLOSE:
        if (a->zipping &&
            MessageBoxW(wnd, L"The support bundle is still being written. Close anyway?", kTitle, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(wnd);
        return 0;
    case WM_SYSCOLORCHANGE:
        rebuild_art(*a, 0, 0);  // the flattened background colour changed
        InvalidateRect(a->art, nullptr, TRUE);
        break;
    case WM_DESTROY:
        if (a->font) DeleteObject(a->font);
        a->font = nullptr;
        rebuild_art(*a, 0, 0);
        if (a->meme) a->meme->Release();
        if (a->wic) a->wic->Release();
        a->meme = nullptr;
        a->wic = nullptr;
        if (a->icon_big) DestroyIcon(a->icon_big);
        if (a->icon_small) DestroyIcon(a->icon_small);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wp, lp);
}
}  // namespace

int run_gui(HINSTANCE inst, int show, const std::wstring& initial_path) {
    App app;
    app.inst = inst;
    app.start_path = tidy_input(initial_path);

    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClass;
    wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(kIconId), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    RegisterClassExW(&wc);

    WNDCLASSEXW art{sizeof art};
    art.lpfnWndProc = art_proc;
    art.hInstance = inst;
    art.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    art.lpszClassName = kArtClass;
    RegisterClassExW(&art);

    UINT sys_dpi = GetDpiForSystem();
    HWND wnd = CreateWindowExW(0, kClass, kTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                               MulDiv(1100, (int)sys_dpi, 96), MulDiv(720, (int)sys_dpi, 96), nullptr, nullptr, inst, &app);
    if (!wnd) return 1;
    ShowWindow(wnd, show);
    UpdateWindow(wnd);

    start_detect(app, false);
    if (app.auto_diagnose) start_diagnosis(app);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(wnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
}  // namespace wcmg::win
