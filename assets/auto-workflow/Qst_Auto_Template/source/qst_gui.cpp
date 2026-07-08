// qst_gui.cpp — Qst Isosteric Heat of Adsorption GUI (Win32 + GDI+)
// Compile: cl /O2 /EHsc /std:c++17 /Fe:qst_calc_gui.exe qst_gui.cpp /link gdiplus.lib gdi32.lib user32.lib comctl32.lib comdlg32.lib

#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include "qst_core.h"

using namespace qst;
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// ─── Globals ──────────────────────────────────────────────────
HINSTANCE g_hInst;
HWND g_hWnd, g_hChart, g_hStatus, g_hLog;
HWND g_hEdPrefix, g_hEdPolyA, g_hEdPolyB;
HWND g_hEdQfrom, g_hEdQto, g_hEdQstep;
HWND g_hEdTemps[5], g_hEdPaths[5], g_hBtnBrowse[5], g_hCbDatasets;
std::wstring g_csvPaths[5];
double g_temperatures[5] = { 77, 87, 0, 0, 0 };
bool g_datasetEnabled[5] = { true, true, false, false, false };

int g_mOrder = 5, g_nOrder = 2;
double g_qFrom = 0.01, g_qTo = 10.0, g_qStep = 0.5;

std::wstring g_prefix = L"Qst_result";
std::vector<double> g_qstLoadings, g_qstValues, g_qstErrors;
FitResult g_fitResult;
std::vector<DataSet> g_datasets;   // copy of datasets used in fitting
std::atomic<bool> g_calculating{ false };
std::mutex g_resultMutex;
std::wstring g_logText;
HFONT g_hFont;

int g_activeChart = 0;
double g_dataNMin = 0, g_dataNMax = 0;  // global loading range detected from data

// ─── Helpers ──────────────────────────────────────────────────

void AL(const std::wstring& s) {
    std::lock_guard<std::mutex> lk(g_resultMutex);
    g_logText += s + L"\r\n";
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)(s + L"\r\n").c_str());
}

std::string ws2s(const std::wstring& w) {
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, NULL, NULL);
    while (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    while (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

std::wstring GetEditText(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len == 0) return L"";
    std::wstring s(len + 1, 0);
    GetWindowTextW(h, &s[0], len + 1);
    s.resize(len);
    return s;
}

void SetEditText(HWND h, const std::wstring& t) { SetWindowTextW(h, t.c_str()); }
int wtoi(const std::wstring& s) { return s.empty() ? 0 : _wtoi(s.c_str()); }
double wtod(const std::wstring& s) { return s.empty() ? 0.0 : _wtof(s.c_str()); }

// ─── File Dialog ──────────────────────────────────────────────

std::wstring BrowseCsv(HWND parent, int dsIdx) {
    OPENFILENAMEW ofn = {};
    wchar_t buf[MAX_PATH] = {};
    if (!g_csvPaths[dsIdx].empty()) wcscpy_s(buf, g_csvPaths[dsIdx].c_str());
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) return std::wstring(buf);
    return L"";
}

// ─── Auto-detect Qst range from loaded CSV data ───────────────

void AutoDetectQstRange() {
    double nMin = 1e100, nMax = -1e100;
    int found = 0;
    for (int d = 0; d < 5; ++d) {
        if (!g_datasetEnabled[d] || g_csvPaths[d].empty()) continue;
        std::string p = ws2s(g_csvPaths[d]);
        auto ds = read_isotherm_csv(p, 300.0);
        if (ds.N.empty()) continue;
        auto [mn, mx] = std::minmax_element(ds.N.begin(), ds.N.end());
        nMin = std::min(nMin, *mn);
        nMax = std::max(nMax, *mx);
        ++found;
    }
    if (found < 1) return;
    g_dataNMin = nMin;
    g_dataNMax = nMax;

    auto fmt = [](double v) -> std::wstring {
        wchar_t buf[32];
        if (std::abs(v) < 1e-3) swprintf_s(buf, L"%.4g", v);
        else if (std::abs(v) < 0.1) swprintf_s(buf, L"%.4lf", v);
        else if (std::abs(v) < 10) swprintf_s(buf, L"%.3lf", v);
        else if (std::abs(v) < 100) swprintf_s(buf, L"%.2lf", v);
        else swprintf_s(buf, L"%.1lf", v);
        return buf;
    };
    double pad = std::max(0.02, (nMax - nMin) * 0.05);
    SetEditText(g_hEdQfrom, fmt(std::max(0.0, nMin - pad)));
    SetEditText(g_hEdQto, fmt(nMax + pad));

    double span = nMax - nMin;
    double step = span / 24.0;
    double mag = std::pow(10.0, std::floor(std::log10(step)));
    step = std::round(step / mag) * mag;
    if (step <= 0) step = 0.01;
    wchar_t sbuf[32]; swprintf_s(sbuf, L"%.4g", step);
    SetEditText(g_hEdQstep, sbuf);

    std::wstring info = L"Loading range: " + fmt(nMin) + L" - " + fmt(nMax);
    SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)info.c_str());
}

// ─── Log/CSV Writing ─────────────────────────────────────────

void WriteToFileRaw(const std::wstring& path, const std::string& content) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    DWORD written;
    WriteFile(h, bom, 3, &written, NULL);
    WriteFile(h, content.c_str(), (DWORD)content.size(), &written, NULL);
    CloseHandle(h);
}

// ─── Save Chart / Log ─────────────────────────────────────────

void SaveChartImage(HWND parent) {
    wchar_t buf[MAX_PATH] = L"Qst_chart";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = L"SVG Files (*.svg)\0*.svg\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"svg";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    std::string svgPath = ws2s(std::wstring(buf));
    if (g_activeChart == 0)
        svg_virial_fit(svgPath, g_datasets, g_fitResult, g_mOrder, g_nOrder);
    else
        svg_qst_curve(svgPath, g_qstLoadings, g_qstValues);
    AL(L"Chart saved: " + std::wstring(buf));
}

void SaveLog(HWND parent) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;
    std::string content;
    { std::lock_guard<std::mutex> lk(g_resultMutex); content = ws2s(g_logText); }
    WriteToFileRaw(std::wstring(buf), content);
    AL(L"Log saved: " + std::wstring(buf));
}

// ─── GDI+ Chart Drawing ───────────────────────────────────────

void DrawChartVirialFit(Graphics& g, RECT rc) {
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (g_datasets.empty() || g_fitResult.params.empty()) return;

    // Padding: left=75, top=30, right=30, bottom=55
    int ml = 75, mt = 30, mr = 30, mb = 55;
    int pw = W - ml - mr;
    int ph = H - mt - mb;

    // Compute data ranges
    double N_min = 1e100, N_max = -1e100, L_min = 1e100, L_max = -1e100;
    for (auto& ds : g_datasets) {
        for (size_t i = 0; i < ds.N.size(); ++i) {
            N_min = std::min(N_min, ds.N[i]);
            N_max = std::max(N_max, ds.N[i]);
            L_min = std::min(L_min, ds.lnP[i]);
            L_max = std::max(L_max, ds.lnP[i]);
        }
    }
    double pad = 0.05;
    double Nr = N_max - N_min, Lr = L_max - L_min;
    N_min -= Nr * pad; N_max += Nr * pad;
    L_min -= Lr * pad; L_max += Lr * pad;

    auto tx = [&](double N) { return (REAL)(ml + (N - N_min) / (N_max - N_min) * pw); };
    auto ty = [&](double L) { return (REAL)(H - mb - (L - L_min) / (L_max - L_min) * ph); };

    // Background
    SolidBrush bg(Color(255, 255, 255, 255));
    g.FillRectangle(&bg, 0, 0, W, H);

    // Grid lines
    Pen gridPen(Color(230, 230, 230), 0.5f);
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);

    Font font(L"Microsoft YaHei", 10, FontStyleRegular, UnitPixel);
    Font fontSm(L"Microsoft YaHei", 9, FontStyleRegular, UnitPixel);
    SolidBrush textBr(Color(80, 80, 80));
    SolidBrush axisBr(Color(40, 40, 40));

    for (int i = 0; i <= 4; ++i) {
        REAL x = tx(N_min + i * (N_max - N_min) / 4.0);
        g.DrawLine(&gridPen, x, (REAL)mt, x, (REAL)(H - mb));
        std::wstringstream ss; ss << std::fixed << std::setprecision(2) << (N_min + i * (N_max - N_min) / 4.0);
        PointF pt(x, (REAL)(H - mb + 14));
        g.DrawString(ss.str().c_str(), -1, &fontSm, pt, &sf, &textBr);
    }
    for (int i = 0; i <= 4; ++i) {
        REAL y = ty(L_min + i * (L_max - L_min) / 4.0);
        g.DrawLine(&gridPen, (REAL)ml, y, (REAL)(W - mr), y);
        std::wstringstream ss; ss << std::fixed << std::setprecision(1) << (L_min + i * (L_max - L_min) / 4.0);
        PointF pt((REAL)(ml - 8), y);
        sf.SetAlignment(StringAlignmentFar);
        g.DrawString(ss.str().c_str(), -1, &fontSm, pt, &sf, &textBr);
        sf.SetAlignment(StringAlignmentCenter);
    }

    // Axes
    Pen axisPen(Color(60, 60, 60), 1.2f);
    g.DrawLine(&axisPen, (REAL)ml, (REAL)mt, (REAL)ml, (REAL)(H - mb));
    g.DrawLine(&axisPen, (REAL)ml, (REAL)(H - mb), (REAL)(W - mr), (REAL)(H - mb));

    // Axis labels
    Font fontLabel(L"Microsoft YaHei", 11, FontStyleRegular, UnitPixel);
    PointF ptXL((REAL)(W / 2), (REAL)(H - 8));
    g.DrawString(L"Loading (mmol/g)", -1, &fontLabel, ptXL, &sf, &axisBr);
    sf.SetFormatFlags(StringFormatFlagsDirectionVertical);
    PointF ptYL((REAL)12, (REAL)(H / 2));
    g.DrawString(L"ln(P / kPa)", -1, &fontLabel, ptYL, &sf, &axisBr);
    sf.SetFormatFlags(0);

    // Colors
    Color colors[] = {
        Color(31, 119, 180), Color(214, 39, 40), Color(44, 160, 44),
        Color(255, 127, 14), Color(148, 103, 189)
    };

    // Fitted curves first, then hollow experimental points on top.
    for (int di = 0; di < (int)g_datasets.size(); ++di) {
        Pen curvePen(colors[di % 5], 2.4f);
        int npts = 200;
        PointF prev;
        bool first = true;
        for (int i = 0; i <= npts; ++i) {
            double Nv = N_min + i * (N_max - N_min) / npts;
            double Lv = virial_lnP(Nv, g_datasets[di].T, g_fitResult.params.data(), g_mOrder, g_nOrder);
            PointF pt(tx(Nv), ty(Lv));
            if (!first) g.DrawLine(&curvePen, prev, pt);
            prev = pt;
            first = false;
        }
    }

    // Data points
    for (int di = 0; di < (int)g_datasets.size(); ++di) {
        Pen ptPen(colors[di % 5], 1.8f);
        SolidBrush whiteBr(Color(255, 255, 255, 255));
        for (size_t i = 0; i < g_datasets[di].N.size(); ++i) {
            REAL cx = tx(g_datasets[di].N[i]);
            REAL cy = ty(g_datasets[di].lnP[i]);
            g.FillEllipse(&whiteBr, cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
            g.DrawEllipse(&ptPen, cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        }
    }

    // Legend
    int lx = W - 150, ly = mt + 5;
    for (int di = 0; di < (int)g_datasets.size(); ++di) {
        Pen lPen(colors[di % 5], 2.4f);
        SolidBrush whiteBr(Color(255, 255, 255, 255));
        REAL y0 = (REAL)(ly + di * 20 + 5);
        g.DrawLine(&lPen, (REAL)lx, y0, (REAL)(lx + 18), y0);
        g.FillEllipse(&whiteBr, (REAL)(lx + 5), y0 - 4.0f, 8.0f, 8.0f);
        g.DrawEllipse(&lPen, (REAL)(lx + 5), y0 - 4.0f, 8.0f, 8.0f);
        std::wstringstream ss; ss << (int)g_datasets[di].T << L" K Exp + Fit";
        PointF ptL((REAL)(lx + 28), (REAL)(ly + di * 20));
        sf.SetAlignment(StringAlignmentNear);
        g.DrawString(ss.str().c_str(), -1, &fontSm, ptL, &sf, &axisBr);
    }

    // R^2
    sf.SetAlignment(StringAlignmentFar);
    std::wstringstream r2s;
    r2s << L"R\u00B2 = " << std::fixed << std::setprecision(6) << g_fitResult.r_squared;
    PointF ptR2((REAL)(W - 10), (REAL)(H - mb - 5));
    g.DrawString(r2s.str().c_str(), -1, &fontLabel, ptR2, &sf, &axisBr);
    sf.SetAlignment(StringAlignmentCenter);
}

void DrawChartQstCurve(Graphics& g, RECT rc) {
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (g_qstLoadings.empty() || g_qstValues.empty()) return;

    int ml = 75, mt = 30, mr = 30, mb = 55;
    int pw = W - ml - mr;
    int ph = H - mt - mb;

    double N_min = g_qstLoadings.front(), N_max = g_qstLoadings.back();
    double Q_min = 1e100, Q_max = -1e100;
    for (auto q : g_qstValues) { Q_min = std::min(Q_min, q); Q_max = std::max(Q_max, q); }
    double QR = Q_max - Q_min; if (QR < 0.1) QR = 1.0;
    Q_min -= QR * 0.15; Q_max += QR * 0.15;
    double NR = N_max - N_min; if (NR < 1e-9) NR = 1.0;

    auto tx = [&](double N) { return (REAL)(ml + (N - N_min) / NR * pw); };
    auto ty = [&](double Q) { return (REAL)(H - mb - (Q - Q_min) / (Q_max - Q_min) * ph); };

    SolidBrush bg(Color(255, 255, 255, 255));
    g.FillRectangle(&bg, 0, 0, W, H);

    Pen gridPen(Color(230, 230, 230), 0.5f);
    Font font(L"Microsoft YaHei", 10, FontStyleRegular, UnitPixel);
    Font fontSm(L"Microsoft YaHei", 9, FontStyleRegular, UnitPixel);
    SolidBrush textBr(Color(80, 80, 80));
    SolidBrush axisBr(Color(40, 40, 40));
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter);

    for (int i = 0; i <= 4; ++i) {
        REAL x = tx(N_min + i * NR / 4.0);
        g.DrawLine(&gridPen, x, (REAL)mt, x, (REAL)(H - mb));
        std::wstringstream ss; ss << std::fixed << std::setprecision(2) << (N_min + i * NR / 4.0);
        PointF pt(x, (REAL)(H - mb + 14));
        g.DrawString(ss.str().c_str(), -1, &fontSm, pt, &sf, &textBr);
    }
    for (int i = 0; i <= 4; ++i) {
        REAL y = ty(Q_min + i * (Q_max - Q_min) / 4.0);
        g.DrawLine(&gridPen, (REAL)ml, y, (REAL)(W - mr), y);
        std::wstringstream ss; ss << std::fixed << std::setprecision(1) << (Q_min + i * (Q_max - Q_min) / 4.0);
        PointF pt((REAL)(ml - 8), y);
        sf.SetAlignment(StringAlignmentFar);
        g.DrawString(ss.str().c_str(), -1, &fontSm, pt, &sf, &textBr);
        sf.SetAlignment(StringAlignmentCenter);
    }

    Pen axisPen(Color(60, 60, 60), 1.2f);
    g.DrawLine(&axisPen, (REAL)ml, (REAL)mt, (REAL)ml, (REAL)(H - mb));
    g.DrawLine(&axisPen, (REAL)ml, (REAL)(H - mb), (REAL)(W - mr), (REAL)(H - mb));

    // Axis labels
    Font fontLabel(L"Microsoft YaHei", 11, FontStyleRegular, UnitPixel);
    PointF ptXL((REAL)(W / 2), (REAL)(H - 8));
    g.DrawString(L"Loading (mmol/g)", -1, &fontLabel, ptXL, &sf, &axisBr);
    sf.SetFormatFlags(StringFormatFlagsDirectionVertical);
    PointF ptYL((REAL)12, (REAL)(H / 2));
    g.DrawString(L"Qst (kJ/mol)", -1, &fontLabel, ptYL, &sf, &axisBr);
    sf.SetFormatFlags(0);

    // Area fill
    SolidBrush areaBr(Color(40, 214, 39, 40));
    PointF* areaPts = new PointF[(int)g_qstLoadings.size() + 2];
    int ai = 0;
    areaPts[ai++] = PointF(tx(g_qstLoadings[0]), ty(Q_min));
    for (size_t i = 0; i < g_qstLoadings.size(); ++i)
        areaPts[ai++] = PointF(tx(g_qstLoadings[i]), ty(g_qstValues[i]));
    areaPts[ai++] = PointF(tx(g_qstLoadings.back()), ty(Q_min));
    g.FillPolygon(&areaBr, areaPts, ai, FillModeWinding);
    delete[] areaPts;

    // Qst curve line
    Pen curvePen(Color(214, 39, 40), 2.0f);
    PointF prev(tx(g_qstLoadings[0]), ty(g_qstValues[0]));
    for (size_t i = 1; i < g_qstLoadings.size(); ++i) {
        PointF curr(tx(g_qstLoadings[i]), ty(g_qstValues[i]));
        g.DrawLine(&curvePen, prev, curr);
        prev = curr;
    }

    // Title
    sf.SetAlignment(StringAlignmentFar);
    std::wstring title = L"Qst = -R * sum a_i N^i";
    PointF ptTitle((REAL)(W - 10), (REAL)(mt + 2));
    g.DrawString(title.c_str(), -1, &fontLabel, ptTitle, &sf, &axisBr);
    sf.SetAlignment(StringAlignmentCenter);
}

// ─── Calculation Thread ───────────────────────────────────────

void CalcThread() {
    g_calculating = true;
    { std::lock_guard<std::mutex> lk(g_resultMutex); g_logText.clear(); }
    SendMessageW(g_hLog, WM_SETTEXT, 0, (LPARAM)L"");
    SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)L"Calculating...");
    EnableWindow(GetDlgItem(g_hWnd, 1000), FALSE);

    g_datasets.clear();
    g_qstLoadings.clear();
    g_qstValues.clear();
    g_qstErrors.clear();
    g_fitResult = FitResult();
    InvalidateRect(g_hChart, NULL, TRUE);

    AL(L"=== Qst Calculation Started ===");

    // Collect datasets
    for (int d = 0; d < 5; ++d) {
        if (!g_datasetEnabled[d] || g_temperatures[d] <= 0) continue;
        std::string p = ws2s(g_csvPaths[d]);
        if (p.empty()) continue;
        AL(L"Dataset " + std::to_wstring(d + 1) + L": T=" + std::to_wstring((int)g_temperatures[d]) + L" K  " + s2ws(p));
        auto ds = read_isotherm_csv(p, g_temperatures[d]);
        if (ds.N.empty()) { AL(L"  WARNING: empty/invalid, skipped"); continue; }
        g_datasets.push_back(ds);
        AL(L"  " + std::to_wstring(ds.N.size()) + L" points");
    }

    if (g_datasets.size() < 2) {
        AL(L"ERROR: Need at least 2 valid datasets!");
        g_calculating = false;
        SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)L"Error: need >=2 valid datasets");
        EnableWindow(GetDlgItem(g_hWnd, 1000), TRUE);
        return;
    }

    AL(L"Auto fitting virial orders up to: a=" + std::to_wstring(g_mOrder) + L" b=" + std::to_wstring(g_nOrder));

    auto t0 = std::chrono::steady_clock::now();
    AutoFitResult autoFit = auto_virial_fit(g_datasets, g_mOrder, g_nOrder);
    g_mOrder = autoFit.selected_m;
    g_nOrder = autoFit.selected_n;
    g_fitResult = autoFit.result;
    SetEditText(g_hEdPolyA, std::to_wstring(g_mOrder));
    SetEditText(g_hEdPolyB, std::to_wstring(g_nOrder));
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    AL(L"--- Candidate Orders ---");
    for (auto& c : autoFit.candidates) {
        std::wstringstream cs;
        cs << L"  m=" << c.m_order << L" n=" << c.n_order
           << L"  R\262=" << std::fixed << std::setprecision(6) << c.result.r_squared
           << L"  chi2=" << std::scientific << c.result.chi_squared
           << L"  BIC=" << c.bic
           << L"  penalty=" << c.penalty
           << L"  score=" << c.score
           << L"  unstable=" << (c.unstable_qst ? L"yes" : L"no");
        AL(cs.str());
    }
    for (const auto& d : autoFit.diagnostics) AL(L"DIAG: " + s2ws(d));

    if (autoFit.range.valid) {
        g_qFrom = autoFit.range.from;
        g_qTo = autoFit.range.to;
        g_qStep = autoFit.range.step;
        wchar_t buf[64];
        swprintf_s(buf, 64, L"%.6g", g_qFrom); SetEditText(g_hEdQfrom, buf);
        swprintf_s(buf, 64, L"%.6g", g_qTo); SetEditText(g_hEdQto, buf);
        swprintf_s(buf, 64, L"%.6g", g_qStep); SetEditText(g_hEdQstep, buf);
    }

    {
        std::wstringstream ss;
        ss << L"Selected virial: m=" << g_mOrder << L" n=" << g_nOrder
           << L"; done in " << ms << L" ms, iter=" << g_fitResult.iterations
           << L", R\262 = " << std::fixed << std::setprecision(6) << g_fitResult.r_squared
           << L", conv=" << (g_fitResult.converged ? L"yes" : L"no");
        AL(ss.str());
    }

    AL(L"--- Parameters ---");
    for (int i = 0; i <= g_mOrder; ++i) {
        wchar_t nb[64]; swprintf_s(nb, 64, L"a_%d = %+.6e", i, g_fitResult.params[i]);
        AL(nb);
    }
    for (int i = 0; i <= g_nOrder; ++i) {
        wchar_t nb[64]; swprintf_s(nb, 64, L"b_%d = %+.6e", i, g_fitResult.params[g_mOrder + 1 + i]);
        AL(nb);
    }
    if (!g_fitResult.param_errors.empty()) {
        AL(L"--- Std Errors ---");
        for (int i = 0; i <= g_mOrder; ++i) {
            wchar_t nb[64]; swprintf_s(nb, 64, L"a_%d = %.3e", i, g_fitResult.param_errors[i]);
            AL(nb);
        }
        for (int i = 0; i <= g_nOrder; ++i) {
            wchar_t nb[64]; swprintf_s(nb, 64, L"b_%d = %.3e", i, g_fitResult.param_errors[g_mOrder + 1 + i]);
            AL(nb);
        }
    }

    // Qst curve
    for (double N = g_qFrom; N <= g_qTo + 1e-9; N += g_qStep) {
        g_qstLoadings.push_back(N);
        g_qstValues.push_back(qst_at_loading(N, g_fitResult.params.data(), g_mOrder));
        g_qstErrors.push_back(qst_error_at_loading(N, g_fitResult, g_mOrder));
    }

    // Save SVG
    std::string prefix = final_output_prefix(ws2s(g_prefix));
    std::string fitPath = prefix + "_Virial_Fit.svg";
    svg_virial_fit(fitPath, g_datasets, g_fitResult, g_mOrder, g_nOrder);
    std::string qstPath = prefix + "_Qst.svg";
    svg_qst_curve(qstPath, g_qstLoadings, g_qstValues);
    AL(L"SVG: " + s2ws(fitPath) + L", " + s2ws(qstPath));

    // Save CSV
    std::string csvPath = prefix + "_Qst.csv";
    write_qst_csv(csvPath, g_qstLoadings, g_qstValues, g_qstErrors);
    AL(L"CSV: " + s2ws(csvPath));

    // Qst table
    {
        std::wstringstream ss;
        ss << L"\n=== Qst vs Loading ===";
        for (size_t i = 0; i < g_qstLoadings.size(); ++i)
            ss << L"\nN=" << std::fixed << std::setprecision(6) << g_qstLoadings[i]
               << L"  Qst=" << std::setprecision(4) << g_qstValues[i]
               << L" +/- " << g_qstErrors[i] << L" kJ/mol";
        AL(ss.str());
    }

    // Auto-save log
    {
        std::string logContent;
        { std::lock_guard<std::mutex> lk(g_resultMutex); logContent = ws2s(g_logText); }
        WriteToFileRaw(s2ws(prefix + "_log.txt"), logContent);
    }

    AL(L"=== Done ===");
    g_calculating = false;
    InvalidateRect(g_hChart, NULL, TRUE);
    SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)L"Ready");
    EnableWindow(GetDlgItem(g_hWnd, 1000), TRUE);
}

// ─── Window Procedure ─────────────────────────────────────────

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hwnd;
        InitCommonControls();

        g_hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, FF_DONTCARE, L"Microsoft YaHei");

        int y = 8, hRow = 26;

        // Row 0: controls
        CreateWindowW(L"STATIC", L"Data:", WS_CHILD | WS_VISIBLE, 10, y + 4, 40, 20, hwnd, NULL, g_hInst, NULL);
        g_hCbDatasets = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                       50, y, 52, 200, hwnd, (HMENU)301, g_hInst, NULL);
        for (int i = 2; i <= 5; ++i) {
            wchar_t buf[4]; _itow_s(i, buf, 10);
            SendMessageW(g_hCbDatasets, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessageW(g_hCbDatasets, CB_SETCURSEL, 0, 0);

        CreateWindowW(L"STATIC", L"a:", WS_CHILD | WS_VISIBLE, 115, y + 4, 18, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdPolyA = CreateWindowW(L"EDIT", L"5", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                    132, y + 2, 30, hRow - 4, hwnd, (HMENU)310, g_hInst, NULL);
        CreateWindowW(L"STATIC", L"b:", WS_CHILD | WS_VISIBLE, 167, y + 4, 18, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdPolyB = CreateWindowW(L"EDIT", L"2", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                    184, y + 2, 30, hRow - 4, hwnd, (HMENU)311, g_hInst, NULL);

        CreateWindowW(L"STATIC", L"Prefix:", WS_CHILD | WS_VISIBLE, 235, y + 4, 45, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdPrefix = CreateWindowW(L"EDIT", L"Qst_result", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                     280, y + 2, 120, hRow - 4, hwnd, (HMENU)320, g_hInst, NULL);

        CreateWindowW(L"STATIC", L"Qst:", WS_CHILD | WS_VISIBLE, 425, y + 4, 30, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdQfrom = CreateWindowW(L"EDIT", L"0.01", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    455, y + 2, 50, hRow - 4, hwnd, (HMENU)330, g_hInst, NULL);
        CreateWindowW(L"STATIC", L"-", WS_CHILD | WS_VISIBLE, 506, y + 4, 10, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdQto = CreateWindowW(L"EDIT", L"10", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                  518, y + 2, 50, hRow - 4, hwnd, (HMENU)331, g_hInst, NULL);
        CreateWindowW(L"STATIC", L"x", WS_CHILD | WS_VISIBLE, 570, y + 4, 12, 20, hwnd, NULL, g_hInst, NULL);
        g_hEdQstep = CreateWindowW(L"EDIT", L"0.5", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    582, y + 2, 42, hRow - 4, hwnd, (HMENU)332, g_hInst, NULL);

y += hRow + 4;

        // Dataset rows
        for (int d = 0; d < 5; ++d) {
            std::wstring cbName = L"T" + std::to_wstring(d + 1);
            HWND cb = CreateWindowW(L"BUTTON", cbName.c_str(),
                                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     10, y + 2, 35, 22, hwnd, (HMENU)(IntToPtr(400 + d)), g_hInst, NULL);
            if (d < 2) SendMessageW(cb, BM_SETCHECK, BST_CHECKED, 0);

            std::wstring tStr = std::to_wstring((int)g_temperatures[d]);
            g_hEdTemps[d] = CreateWindowW(L"EDIT", tStr.c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER, 50, y + 2, 52, hRow - 4, hwnd, (HMENU)(IntToPtr(410 + d)), g_hInst, NULL);
            CreateWindowW(L"STATIC", L"K", WS_CHILD | WS_VISIBLE, 103, y + 4, 14, 20, hwnd, NULL, g_hInst, NULL);

            g_hEdPaths[d] = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                                           118, y + 2, 530, hRow - 4, hwnd, (HMENU)(IntToPtr(420 + d)), g_hInst, NULL);
            g_hBtnBrowse[d] = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE,
                                             653, y + 2, 28, hRow - 4, hwnd, (HMENU)(IntToPtr(430 + d)), g_hInst, NULL);
            y += hRow;
        }

        y += 8;

        HWND btnCalc = CreateWindowW(L"BUTTON", L"Calculate Qst", WS_CHILD | WS_VISIBLE,
                                      10, y, 120, 32, hwnd, (HMENU)1000, g_hInst, NULL);
        HWND btnChart = CreateWindowW(L"BUTTON", L"Save Chart", WS_CHILD | WS_VISIBLE,
                                       140, y, 90, 32, hwnd, (HMENU)1001, g_hInst, NULL);
        HWND btnLog = CreateWindowW(L"BUTTON", L"Save Log", WS_CHILD | WS_VISIBLE,
                                     238, y, 90, 32, hwnd, (HMENU)1002, g_hInst, NULL);

        CreateWindowW(L"STATIC", L"View:", WS_CHILD | WS_VISIBLE, 360, y + 8, 35, 20, hwnd, NULL, g_hInst, NULL);
        HWND cbChart = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                      395, y + 4, 240, 200, hwnd, (HMENU)2000, g_hInst, NULL);
        SendMessageW(cbChart, CB_ADDSTRING, 0, (LPARAM)L"Virial Fit -- ln(P) vs Loading");
        SendMessageW(cbChart, CB_ADDSTRING, 0, (LPARAM)L"Qst vs Loading");
        SendMessageW(cbChart, CB_SETCURSEL, 0, 0);

        y += 38;

        g_hChart = CreateWindowW(L"STATIC", NULL,
                                  WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
                                  10, y, 880, 300, hwnd, (HMENU)2001, g_hInst, NULL);
        y += 308;

        g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                  10, y, 880, 130, hwnd, (HMENU)2002, g_hInst, NULL);
        SendMessageW(g_hLog, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        y += 140;

        g_hStatus = CreateWindowW(L"STATIC", L"Ready -- Load 2+ CSV files and click Calculate Qst",
                                   WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                                   10, y, 880, 20, hwnd, (HMENU)2003, g_hInst, NULL);

        for (int cid : {310, 311, 320, 330, 331, 332, 1000, 1001, 1002}) {
            HWND h = GetDlgItem(hwnd, cid);
            if (h) SendMessageW(h, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)lp;
        if (di->CtlID == 2001) {
            RECT rc = di->rcItem;
            Graphics g(di->hDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            bool hasData = !g_datasets.empty() && !g_fitResult.params.empty();

            if (g_activeChart == 0) {
                if (hasData) DrawChartVirialFit(g, rc);
                else {
                    SolidBrush bg(Color(255, 255, 255, 255));
                    g.FillRectangle(&bg, (INT)rc.left, (INT)rc.top,
                                    (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));
                    Font f(L"Microsoft YaHei", 11, FontStyleRegular, UnitPixel);
                    SolidBrush br(Color(150, 150, 150));
                    g.DrawString(L"No chart yet. Load 2+ CSV files and click Calculate Qst.", -1,
                                 &f, PointF(15, 15), &br);
                }
            } else {
                if (!g_qstLoadings.empty()) DrawChartQstCurve(g, rc);
                else {
                    SolidBrush bg(Color(255, 255, 255, 255));
                    g.FillRectangle(&bg, (INT)rc.left, (INT)rc.top,
                                    (INT)(rc.right - rc.left), (INT)(rc.bottom - rc.top));
                    Font f(L"Microsoft YaHei", 11, FontStyleRegular, UnitPixel);
                    SolidBrush br(Color(150, 150, 150));
                    g.DrawString(L"No Qst data. Run calculation first.", -1, &f, PointF(15, 15), &br);
                }
            }

            Pen border(Color(180, 180, 180), 1.0f);
            g.DrawRectangle(&border, 0, 0, rc.right - rc.left - 1, rc.bottom - rc.top - 1);
        }
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);

        if (id >= 400 && id < 405) {
            g_datasetEnabled[id - 400] = (SendMessageW(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED);
            EnableWindow(g_hEdTemps[id - 400], g_datasetEnabled[id - 400]);
            EnableWindow(g_hEdPaths[id - 400], g_datasetEnabled[id - 400]);
            EnableWindow(g_hBtnBrowse[id - 400], g_datasetEnabled[id - 400]);
        }
        else if (id >= 430 && id < 435) {
            int d = id - 430;
            auto path = BrowseCsv(hwnd, d);
            if (!path.empty()) { g_csvPaths[d] = path; SetEditText(g_hEdPaths[d], path); AutoDetectQstRange(); }
        }
        else if (id == 301 && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hCbDatasets, CB_GETCURSEL, 0, 0);
            int count = sel + 2;
            for (int d = 0; d < 5; ++d) {
                bool vis = d < count;
                ShowWindow(GetDlgItem(hwnd, 400 + d), vis ? SW_SHOW : SW_HIDE);
                ShowWindow(g_hEdTemps[d], vis ? SW_SHOW : SW_HIDE);
                ShowWindow(g_hEdPaths[d], vis ? SW_SHOW : SW_HIDE);
                ShowWindow(g_hBtnBrowse[d], vis ? SW_SHOW : SW_HIDE);
                g_datasetEnabled[d] = vis;
                if (vis && d >= 2) SendMessageW(GetDlgItem(hwnd, 400 + d), BM_SETCHECK, BST_CHECKED, 0);
                if (!vis) {
                    SendMessageW(GetDlgItem(hwnd, 400 + d), BM_SETCHECK, BST_UNCHECKED, 0);
                    g_csvPaths[d].clear(); SetEditText(g_hEdPaths[d], L"");
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (id == 2000 && code == CBN_SELCHANGE) {
            g_activeChart = (int)SendMessageW((HWND)lp, CB_GETCURSEL, 0, 0);
            InvalidateRect(g_hChart, NULL, TRUE);
        }
        else if (id == 1000) {
            if (g_calculating) break;
            g_prefix = GetEditText(g_hEdPrefix); if (g_prefix.empty()) g_prefix = L"Qst_result";
            g_mOrder = std::clamp(wtoi(GetEditText(g_hEdPolyA)), 2, 8);
            g_nOrder = std::clamp(wtoi(GetEditText(g_hEdPolyB)), 1, 4);
            g_qFrom = wtod(GetEditText(g_hEdQfrom));
            g_qTo = wtod(GetEditText(g_hEdQto));
            g_qStep = wtod(GetEditText(g_hEdQstep)); if (g_qStep <= 0) g_qStep = 0.5;
            for (int d = 0; d < 5; ++d) {
                auto t = GetEditText(g_hEdTemps[d]); if (!t.empty()) g_temperatures[d] = wtod(t);
                g_csvPaths[d] = GetEditText(g_hEdPaths[d]);
            }
            std::thread(CalcThread).detach();
        }
        else if (id == 1001) SaveChartImage(hwnd);
        else if (id == 1002) SaveLog(hwnd);
        break;
    }

    case WM_CTLCOLORSTATIC: {
        if ((HWND)lp == g_hStatus) {
            SetBkColor((HDC)wp, GetSysColor(COLOR_BTNFACE));
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }

    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        int logH = (std::max)(80, h - 460);
        if (g_hChart && g_hLog && g_hStatus) {
            MoveWindow(g_hChart, 10, 230, w - 20, (std::max)(100, h - 230 - logH - 22), TRUE);
            MoveWindow(g_hLog, 10, h - logH - 22, w - 20, logH, TRUE);
            MoveWindow(g_hStatus, 10, h - 24, w - 20, 18, TRUE);
            InvalidateRect(g_hChart, NULL, TRUE);
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        ((MINMAXINFO*)lp)->ptMinTrackSize.x = 750;
        ((MINMAXINFO*)lp)->ptMinTrackSize.y = 550;
        return 0;
    }

    case WM_DESTROY:
        if (g_hFont) DeleteObject(g_hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void SetOutputDirectoryToExeDir() {
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return;
    *slash = L'\0';
    SetCurrentDirectoryW(path);
    set_output_base_dir(std::filesystem::path(path));
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    SetOutputDirectoryToExeDir();
    g_hInst = hInst;
    GdiplusStartupInput gdiSI;
    ULONG_PTR gdiToken;
    GdiplusStartup(&gdiToken, &gdiSI, NULL);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"QstCalcWindow";
    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(0, L"QstCalcWindow",
                              L"Qst Calculator -- Isosteric Heat of Adsorption",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 920, 700,
                              NULL, NULL, hInst, NULL);
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    GdiplusShutdown(gdiToken);
    return (int)msg.wParam;
}
