/* Native Windows front end for the original C11 GymTrack.
 * Business operations are executed by tracker_bridge.c against unchanged sources.
 * Build with build.bat. No browser, service, framework or network is required. */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#define COBJMACROS
#include <d2d1.h>
#include <mmsystem.h>
#include <stdio.h>
#include "tracker_bridge.h"

enum
{
    NAV_HOME = 100,
    NAV_RECORDS,
    NAV_PR,
    NAV_PLAN,
    NAV_CAL,
    ACT_ADD = 120,
    ACT_EDIT,
    ACT_DELETE,
    ACT_SAVE,
    ACT_LOAD,
    FILTER_TEXT = 130,
    FILTER_MODE,
    SORT_MODE,
    ACT_FILTER,
    ACT_CLEAR,
    PR_TEXT = 140,
    PR_RUN,
    PLAN_RUN = 150,
    CAL_RUN = 160,
    CAL_UNIT,
    LOG_DATE = 200,
    LOG_NAME,
    LOG_WEIGHT,
    LOG_REPS,
    LOG_ADD,
    LOG_UPDATE,
    LOG_REMOVE,
    LOG_COMMIT,
    LOG_CANCEL,
    LOG_LIST,
    EDIT_COMMIT,
    LOG_DATE_FEEDBACK,
    PLAN_GROUP = 300,
    PLAN_OPTION = 320,
    CAL_FIELD = 400
};
static HINSTANCE instance;
static HWND mainWindow, recordList, filterText, filterMode, sortMode, report;
static HWND nav[5], tiles[4], toolbar[5], filterButton, clearButton;
static HWND prText, prButton, planGroups[4], planOptions[4][3], planButton;
static HWND calFields[4], calUnit, calButton;
static HWND motionButton, previewButton;
static WorkoutDB db;
static int view, dirty, scaleDpi = 96;
static HFONT font, boldFont, titleFont, smallFont, monoFont, brandFont;
static HBRUSH bgBrush, fieldBrush;
static const COLORREF BG = RGB(13, 16, 23), PANEL = RGB(22, 27, 38), FIELD = RGB(31, 38, 52);
static const COLORREF TEXT = RGB(234, 240, 252), MUTED = RGB(143, 157, 180),
                      BLUE = RGB(69, 143, 255);
static char statusText[256] = "Ready. Your workout history is stored on this computer.";
static const char *groups[] = {"Not selected", "Chest",   "Back", "Shoulders",
                               "Biceps",       "Triceps", "Legs", "Abs"};
/* Labels mirror generatePlan in original/features.c. Computation stays there. */
static const char *options[7][3][2] = {{{"Dumbbell flat press", "Barbell flat press"},
                                        {"Incline dumbbell press", "Incline barbell press"},
                                        {"Cable fly", "Pec Dec fly"}},
                                       {{"Lat pulldown", "Pull Ups"},
                                        {"Barbell row", "Dumbbell row"},
                                        {"Seated cable row", "Dumbbell Row"}},
                                       {{"Dumbbell overhead press", "Barbell overhead press"},
                                        {"Dumbbell lateral raise", "Cable lateral raise"},
                                        {"Reverse dumbbell fly", "Face Pulls"}},
                                       {{"Dumbbell curl", "Barbell curl"},
                                        {"Dumbbell hammer curl", "Cable hammer curl"},
                                        {"Incline Dumbbell Curl", "Bayesian Curl"}},
                                       {{"Tricep Extension", "Closegrip Benchpress"},
                                        {"Overhead cable extension", "Overhead dumbbell extension"},
                                        {"Single arm Tricep pushdown", "Dumbell Tricep kickbacks"}},
                                       {{"Barbell squat", "Leg Press"},
                                        {"Romanian deadlift", "Hamstring Curl"},
                                        {"Lunges", "Bulgarian Split Squats"}},
                                       {{"Body weight Crunches", "Cable crunches"},
                                        {"Leg Raises", "Hanging knee raise"},
                                        {"Dragon Flag", "Plank"}}};
static int S(int value) { return MulDiv(value, scaleDpi, 96); }
static void place(HWND w, int x, int y, int width, int height)
{
    MoveWindow(w, S(x), S(y), S(width), S(height), FALSE);
}
static void show(HWND w, int visible) {
    SetWindowPos(w,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|
        SWP_NOREDRAW|(visible?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
}
static void textAt(HDC dc, const char *text, int x, int y, int w, int h, HFONT f, COLORREF color,
                   UINT flags)
{
    RECT r = {S(x), S(y), S(x + w), S(y + h)};
    SelectObject(dc, f);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, text, -1, &r, flags | DT_NOPREFIX);
}
static void paintMain(HDC dc);
static void status(const char *s);
#include "gui_effects.h"
static void panel(HDC dc, int x, int y, int w, int h, COLORREF fill, COLORREF border, int radius)
{
    RECT r = {S(x), S(y), S(x + w), S(y + h)};
    fxGlass(dc, r, S(radius), fill, border, 224);
}
static HWND control(HWND parent, const char *cls, const char *label, DWORD style, int id)
{
    HWND w = CreateWindowExA(0, cls, label, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style, 0, 0, 1, 1, parent,
                             (HMENU)(INT_PTR)id, instance, NULL);
    SendMessage(w, WM_SETFONT, (WPARAM)font, TRUE);
    return w;
}
static HWND button(HWND parent, const char *label, int id)
{
    HWND w = control(parent, "BUTTON", label, WS_TABSTOP | BS_OWNERDRAW, id);
    fxAttachButton(w);
    return w;
}
static HWND edit(HWND parent, const char *value, int id, int max)
{
    HWND w = control(parent, "EDIT", value, WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, id);
    SendMessage(w, EM_SETLIMITTEXT, max, 0);
    return w;
}
static LRESULT CALLBACK comboProc(HWND w, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    (void)id;
    (void)data;
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT || m == WM_PRINTCLIENT)
    {
        PAINTSTRUCT ps;
        HDC dc = m == WM_PAINT ? BeginPaint(w, &ps) : (HDC)wp;
        RECT r;
        GetClientRect(w, &r);
        int width = r.right * 96 / scaleDpi, height = r.bottom * 96 / scaleDpi;
        FillRect(dc, &r, bgBrush);
        panel(dc, 0, 0, width, height, FIELD, GetFocus() == w ? BLUE : RGB(60, 71, 90), 8);
        char s[160] = "";
        int index = (int)SendMessage(w, CB_GETCURSEL, 0, 0);
        if (index >= 0)
            SendMessageA(w, CB_GETLBTEXT, index, (LPARAM)s);
        textAt(dc, s, 9, 0, width - 33, height, font, IsWindowEnabled(w) ? TEXT : MUTED,
               DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        HPEN p = CreatePen(PS_SOLID, S(1), MUTED);
        HGDIOBJ old = SelectObject(dc, p);
        MoveToEx(dc, S(width - 20), S(height / 2 - 2), NULL);
        LineTo(dc, S(width - 15), S(height / 2 + 3));
        LineTo(dc, S(width - 10), S(height / 2 - 2));
        SelectObject(dc, old);
        DeleteObject(p);
        if (m == WM_PAINT)
            EndPaint(w, &ps);
        return 0;
    }
    LRESULT result = DefSubclassProc(w, m, wp, lp);
    if (m == CB_SETCURSEL || m == WM_SETFOCUS || m == WM_KILLFOCUS || m == WM_ENABLE)
        InvalidateRect(w, NULL, TRUE);
    return result;
}
static HWND combo(HWND parent, int id)
{
    HWND w = control(
        parent, "COMBOBOX", "",
        WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, id);
    SetWindowSubclass(w, comboProc, 1, 0);
    return w;
}
static void addChoice(HWND w, const char *s) { SendMessageA(w, CB_ADDSTRING, 0, (LPARAM)s); }
static int choice(HWND w) { return (int)SendMessage(w, CB_GETCURSEL, 0, 0); }
static void selectChoice(HWND w, int i) { SendMessage(w, CB_SETCURSEL, i, 0); }
static void getText(HWND w, char *s, int n) { GetWindowTextA(w, s, n); }
static void status(const char *s)
{
    snprintf(statusText, sizeof statusText, "%s", s);
    InvalidateRect(mainWindow, NULL, FALSE);
}
static void warn(HWND w, const char *s)
{
    MessageBoxA(w, s, "GymTrack", MB_OK | MB_ICONWARNING);
}
static void setReport(const char *s)
{
    size_t n = strlen(s);
    char *p = malloc(n * 2 + 1);
    if (!p)
        return;
    size_t j = 0;
    for (size_t i = 0; i < n; i++)
    {
        if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r'))
            p[j++] = '\r';
        p[j++] = s[i];
    }
    p[j] = 0;
    SetWindowTextA(report, p);
    free(p);
}
static void drawSymbol(HDC dc, int x, int y, int kind, COLORREF color)
{
    HPEN p = CreatePen(PS_SOLID, S(2), color);
    HGDIOBJ old = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (kind == 0)
    {
        RoundRect(dc, S(x), S(y + 4), S(x + 23), S(y + 21), S(5), S(5));
        MoveToEx(dc, S(x + 11), S(y + 8), NULL);
        LineTo(dc, S(x + 11), S(y + 17));
        MoveToEx(dc, S(x + 7), S(y + 12), NULL);
        LineTo(dc, S(x + 16), S(y + 12));
    }
    else if (kind == 1)
    {
        for (int i = 0; i < 3; i++)
        {
            MoveToEx(dc, S(x + 3), S(y + 5 + i * 7), NULL);
            LineTo(dc, S(x + 23), S(y + 5 + i * 7));
        }
    }
    else if (kind == 2)
    {
        Ellipse(dc, S(x + 5), S(y + 1), S(x + 21), S(y + 17));
        MoveToEx(dc, S(x + 9), S(y + 16), NULL);
        LineTo(dc, S(x + 6), S(y + 25));
        LineTo(dc, S(x + 13), S(y + 21));
        LineTo(dc, S(x + 20), S(y + 25));
        LineTo(dc, S(x + 17), S(y + 16));
    }
    else if (kind == 3)
    {
        RoundRect(dc, S(x + 2), S(y + 3), S(x + 24), S(y + 25), S(4), S(4));
        MoveToEx(dc, S(x + 6), S(y + 9), NULL);
        LineTo(dc, S(x + 20), S(y + 9));
        MoveToEx(dc, S(x + 8), S(y + 15), NULL);
        LineTo(dc, S(x + 12), S(y + 20));
        LineTo(dc, S(x + 20), S(y + 13));
    }
    else
    {
        POINT a[5] = {{S(x + 13), S(y)},
                      {S(x + 4), S(y + 14)},
                      {S(x + 12), S(y + 14)},
                      {S(x + 10), S(y + 26)},
                      {S(x + 23), S(y + 10)}};
        Polyline(dc, a, 5);
    }
    SelectObject(dc, old);
    SelectObject(dc, ob);
    DeleteObject(p);
}
static void roundIcon(HDC dc, int x, int y, int diameter, int kind, double hover)
{
    RECT r = {S(x), S(y), S(x + diameter), S(y + diameter)};
    COLORREF tint =
        kind == 3 ? RGB(76, 53, 139) : (kind == 4 ? RGB(35, 82, 133) : RGB(30, 74, 135));
    fxGlass(dc, r, S(diameter), fxMix(tint, RGB(77, 136, 235), hover * .32),
            fxMix(RGB(79, 111, 171), RGB(153, 192, 255), hover * .65), 236);
    int saved = SaveDC(dc);
    double factor = diameter > 36 ? .86 : .68;
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM transform = {(FLOAT)factor,
                       0,
                       0,
                       (FLOAT)factor,
                       (FLOAT)S(x + (int)((diameter - 26 * factor) / 2)),
                       (FLOAT)S(y + (int)((diameter - 26 * factor) / 2))};
    SetWorldTransform(dc, &transform);
    drawSymbol(dc, 0, 0, kind, RGB(203, 226, 255));
    RestoreDC(dc, saved);
}
static void buttonBackdrop(HDC dc, HWND controlWindow)
{
    if (GetParent(controlWindow) == mainWindow)
    {
        POINT origin = {0, 0};
        MapWindowPoints(controlWindow, mainWindow, &origin, 1);
        RECT local,area;GetClientRect(controlWindow,&local);GetClientRect(mainWindow,&area);
        if(origin.x<S(237)) {
            HBRUSH b=CreateSolidBrush(RGB(23,31,47));FillRect(dc,&local,b);DeleteObject(b);
        } else if(fxAmbient && area.right>0 && area.bottom>0) {
            HDC source=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(source,fxAmbient);
            StretchBlt(dc,0,0,local.right,local.bottom,source,
                origin.x*256/area.right,origin.y*192/area.bottom,
                max(1,local.right*256/area.right),max(1,local.bottom*192/area.bottom),SRCCOPY);
            SelectObject(source,old);DeleteDC(source);
        } else FillRect(dc,&local,bgBrush);
    }
    else
    {
        RECT r;
        GetClientRect(controlWindow, &r);
        FillRect(dc, &r, bgBrush);
    }
}
static void drawButton(const DRAWITEMSTRUCT *d)
{
    if (d->CtlType == ODT_COMBOBOX)
    {
        FillRect(d->hDC, &d->rcItem, fieldBrush);
        char value[160] = "";
        if (d->itemID != (UINT)-1)
            SendMessageA(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)value);
        RECT r = d->rcItem;
        r.left += S(9);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, (d->itemState & ODS_DISABLED) ? MUTED : TEXT);
        SelectObject(d->hDC, font);
        DrawTextA(d->hDC, value, -1, &r,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (d->itemState & ODS_FOCUS)
            DrawFocusRect(d->hDC, &d->rcItem);
        return;
    }
    int id = (int)d->CtlID, w = (d->rcItem.right - d->rcItem.left) * 96 / scaleDpi,
        h = (d->rcItem.bottom - d->rcItem.top) * 96 / scaleDpi;
    HDC dc = CreateCompatibleDC(d->hDC);
    HBITMAP buffer = CreateCompatibleBitmap(d->hDC, d->rcItem.right, d->rcItem.bottom);
    HGDIOBJ previousBuffer = SelectObject(dc, buffer);
    RECT r = d->rcItem;
    buttonBackdrop(dc, d->hwndItem);
    double hover = fxButtonHover(d->hwndItem);
    int active = (id >= NAV_HOME && id <= NAV_CAL && id - NAV_HOME == view);
    int primary =
        id == ACT_ADD || id == LOG_COMMIT || id == EDIT_COMMIT || id == PLAN_RUN || id == CAL_RUN;
    COLORREF fill = active ? RGB(29, 55, 91) : (primary ? RGB(35, 106, 220) : PANEL);
    if (d->itemState & ODS_SELECTED)
        fill = RGB(35, 67, 109);
    if (d->itemState & ODS_DISABLED)
        fill = RGB(26, 29, 37);
    else
        fill = fxMix(fill, RGB(58, 83, 124), hover * .48);
    char label[160];
    GetWindowTextA(d->hwndItem, label, sizeof label);
    int tile = (id >= 500 && id < 504);
    int inset = (d->itemState & ODS_SELECTED) ? 2 : 0;
    panel(dc, 1 + inset, 1 + inset, w - 2 - inset * 2, h - 2 - inset * 2, fill,
          fxMix(active || primary ? RGB(65, 129, 228) : RGB(64, 76, 101), RGB(116, 163, 240),
                hover * .55),
          tile ? 30 : 22);
    COLORREF fg = (d->itemState & ODS_DISABLED) ? MUTED : TEXT;
    if (id >= NAV_HOME && id <= NAV_CAL)
    {
        roundIcon(dc, 8, 6, 32, id - NAV_HOME, active ? 1 : hover);
        textAt(dc, label, 48, 0, w - 56, h, boldFont, fg, DT_SINGLELINE | DT_VCENTER);
    }
    else if (tile)
    {
        int t = id - 500;
        roundIcon(dc, 18, 13, 42, t == 0 ? 0 : t + 1, hover);
        textAt(dc, label, 18, 64, w - 32, 24, boldFont, TEXT, DT_SINGLELINE);
        const char *sub[] = {"Record every set", "Track your best lifts", "Build your next session",
                             "Estimate your target"};
        textAt(dc, sub[t], 18, 88, w - 32, 22, smallFont, MUTED, DT_SINGLELINE);
    }
    else
        textAt(dc, label, 10, 0, w - 20, h, boldFont, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (d->itemState & ODS_FOCUS)
    {
        InflateRect(&r, -S(5), -S(5));
        DrawFocusRect(dc, &r);
    }
    BitBlt(d->hDC, 0, 0, d->rcItem.right, d->rcItem.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previousBuffer);
    DeleteObject(buffer);
    DeleteDC(dc);
}
static void addColumn(HWND list, int index, const char *name, int width)
{
    LVCOLUMNA c = {0};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = (char *)name;
    c.cx = S(width);
    c.iSubItem = index;
    ListView_InsertColumn(list, index, &c);
}
static LRESULT CALLBACK headerProc(HWND w, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id,
                                   DWORD_PTR data)
{
    (void)id;
    (void)data;
    if (m == WM_PAINT || m == WM_PRINTCLIENT)
    {
        PAINTSTRUCT ps;
        HDC dc = m == WM_PAINT ? BeginPaint(w, &ps) : (HDC)wp;
        RECT r;
        GetClientRect(w, &r);
        FillRect(dc, &r, fieldBrush);
        for (int i = 0; i < Header_GetItemCount(w); i++)
        {
            char s[80];
            HDITEMA item = {0};
            item.mask = HDI_TEXT;
            item.pszText = s;
            item.cchTextMax = sizeof s;
            Header_GetItem(w, i, &item);
            Header_GetItemRect(w, i, &r);
            r.left += S(7);
            SetTextColor(dc, MUTED);
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, smallFont);
            DrawTextA(dc, s, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (m == WM_PAINT)
            EndPaint(w, &ps);
        return 0;
    }
    return DefSubclassProc(w, m, wp, lp);
}
static HWND newList(HWND parent, int id)
{
    HWND list = control(parent, WC_LISTVIEWA, "",
                        WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, id);
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list, PANEL);
    ListView_SetTextBkColor(list, PANEL);
    ListView_SetTextColor(list, TEXT);
    SetWindowTheme(list, L"DarkMode_Explorer", NULL);
    SetWindowSubclass(ListView_GetHeader(list), headerProc, 1, 0);
    return list;
}
static void cell(HWND list, int row, int col, const char *s)
{
    ListView_SetItemText(list, row, col, (char *)s);
}
static int selectedRecord(void)
{
    int i = ListView_GetNextItem(recordList, -1, LVNI_SELECTED);
    if (i < 0)
        return 0;
    LVITEMA item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = i;
    ListView_GetItem(recordList, &item);
    return (int)item.lParam;
}
static WorkoutEntry *record(int id)
{
    for (size_t i = 0; i < db.count; i++)
        if (db.data[i].id == id)
            return &db.data[i];
    return NULL;
}
static void refreshRecords(void)
{
    int selected = selectedRecord();
    ListView_DeleteAllItems(recordList);
    char filter[128];
    getText(filterText, filter, sizeof filter);
    int mode = choice(filterMode), recordId = 0, filterOk = 1;
    if (mode == 1 && filter[0])
    {
        filterOk = ht_parse_int(filter, 1, INT_MAX - 1, &recordId);
    }
    size_t shown = 0;
    for (size_t i = 0; i < db.count; i++)
    {
        WorkoutEntry *e = &db.data[i];
        if (!filterOk)
            continue;
        if (filter[0] &&
            ((mode == 0 && strcmp(filter, e->exercise)) || (mode == 1 && e->id != recordId)))
            continue;
        char s[64];
        snprintf(s, sizeof s, "%d", e->id);
        LVITEMA it = {0};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = (int)shown;
        it.pszText = s;
        it.lParam = e->id;
        int row = ListView_InsertItem(recordList, &it);
        cell(recordList, row, 1, e->date);
        cell(recordList, row, 2, e->exercise);
        snprintf(s, sizeof s, "%d", e->sessionId);
        cell(recordList, row, 3, s);
        snprintf(s, sizeof s, "%d", e->setNumber);
        cell(recordList, row, 4, s);
        snprintf(s, sizeof s, "%.2f", e->weight100 / 100.0);
        cell(recordList, row, 5, s);
        double lb;
        convertWeight(e->weight100 / 100.0, 1, &lb);
        snprintf(s, sizeof s, "%.2f", lb);
        cell(recordList, row, 6, s);
        snprintf(s, sizeof s, "%d", e->reps);
        cell(recordList, row, 7, s);
        if (e->id == selected)
            ListView_SetItemState(recordList, row, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
        ++shown;
    }
    EnableWindow(toolbar[1], selectedRecord() != 0);
    EnableWindow(toolbar[2], selectedRecord() != 0);
    char s[256];
    snprintf(s, sizeof s, "%zu of %zu sets shown   |   %s   |   workouts.txt", shown, db.count,
             dirty ? "Unsaved changes" : "No pending changes");
    status(s);
}
static void layout(void)
{
    RECT r;
    GetClientRect(mainWindow, &r);
    int w = r.right * 96 / scaleDpi, h = r.bottom * 96 / scaleDpi;
    int x = 246, cw = w - x - 30;
    for (int i = 0; i < 5; i++)
    {
        place(nav[i], 20, 151 + i * 54, 196, 44);
        InvalidateRect(nav[i], NULL, TRUE);
    }
    place(toolbar[3], 20, h - 133, 196, 40);
    place(toolbar[4], 20, h - 83, 196, 40);
    place(motionButton,20,h-180,106,34);
    place(previewButton,132,h-180,84,34);
    for (int i = 0; i < 4; i++)
    {
        place(tiles[i], x + i * (cw + 12) / 4, 125, (cw - 36) / 4, 118);
        show(tiles[i], view == 0);
    }
    int records = view == 0 || view == 1;
    int top = view == 0 ? 282 : 142;
    show(recordList, records);
    show(filterText, records);
    show(filterMode, records);
    show(sortMode, records);
    show(filterButton, records);
    show(clearButton, records);
    for (int i = 0; i < 3; i++)
        show(toolbar[i], records);
    place(filterMode, x, top, 148, 220);
    place(filterText, x + 158, top, 210, 32);
    place(filterButton, x + 378, top, 76, 32);
    place(clearButton, x + 462, top, 68, 32);
    place(sortMode, w - 210, top, 180, 220);
    place(toolbar[0], x, top + 52, 134, 38);
    place(toolbar[1], x + 144, top + 52, 108, 38);
    place(toolbar[2], x + 262, top + 52, 108, 38);
    place(recordList, x, top + 108, cw, h - top - 160);
    int fixed = 70 + 110 + 66 + 48 + 84 + 84 + 65;
    ListView_SetColumnWidth(recordList, 2, S(cw - fixed - 26));
    show(report, view >= 2);
    show(prText, view == 2);
    show(prButton, view == 2);
    place(prText, x, 156, 330, 34);
    place(prButton, x + 342, 156, 145, 34);
    place(report, x, view == 2 ? 212 : (view == 3 ? 481 : 380), cw,
          h - (view == 2 ? 212 : (view == 3 ? 481 : 380)) - 52);
    RECT reportRect;
    GetClientRect(report, &reportRect);
    InflateRect(&reportRect, -S(14), -S(12));
    SendMessage(report, EM_SETRECT, 0, (LPARAM)&reportRect);
    for (int i = 0; i < 4; i++)
    {
        show(planGroups[i], view == 3);
        place(planGroups[i], x + 12, 174 + i * 62, 120, 250);
        for (int j = 0; j < 3; j++)
        {
            show(planOptions[i][j], view == 3);
            place(planOptions[i][j], x + 142 + j * (cw - 154) / 3, 174 + i * 62, (cw - 174) / 3,
                  200);
        }
    }
    show(planButton, view == 3);
    place(planButton, x, 432, 168, 36);
    show(calUnit, view == 4);
    place(calUnit, x + 20, 170, 160, 160);
    for (int i = 0; i < 4; i++)
    {
        show(calFields[i], view == 4);
        place(calFields[i], x + 20 + (i % 2) * (cw / 2), 234 + (i / 2) * 72, cw / 2 - 44, 34);
    }
    show(calButton, view == 4);
    place(calButton, w - 220, 165, 170, 36);
    RedrawWindow(mainWindow,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
static void refreshPR(void)
{
    char name[NAME_LEN];
    getText(prText, name, sizeof name);
    ht_run(HT_PR, &db, "", name);
    setReport(ht_output());
}
static void switchView(int next)
{
    if (next == view)
        return;
    BOOL before = fxBeforeChange();
    view = next;
    SetWindowTextA(report, "");
    if (view == 2)
        refreshPR();
    if (view == 3)
        setReport("Select one to four distinct muscle groups, choose the exercises, then generate "
                  "your plan.");
    if (view == 4)
        setReport("Enter your current weight, goal weight, maintenance calories and duration, then "
                  "calculate.");
    layout();
    refreshRecords();
    fxAfterChange(before);
}
static void paintMain(HDC dc)
{
    RECT r;
    GetClientRect(mainWindow, &r);
    fxDrawAmbient(dc, r.right, r.bottom);
    int w = r.right * 96 / scaleDpi, h = r.bottom * 96 / scaleDpi, x = 246, cw = w - x - 30;
    panel(dc, 10, 10, 216, h - 20, RGB(21, 28, 43), RGB(53, 69, 96), 36);
    HICON logo =
        (HICON)LoadImage(instance, MAKEINTRESOURCE(101), IMAGE_ICON, S(52), S(52), LR_DEFAULTCOLOR);
    if (logo)
    {
        DrawIconEx(dc, S(28), S(31), logo, S(52), S(52), 0, NULL, DI_NORMAL);
        DestroyIcon(logo);
    }
    textAt(dc, "GymTrack", 88, 43, 128, 32, brandFont, TEXT, DT_SINGLELINE | DT_VCENTER);
    textAt(dc, "YOUR TRAINING SPACE", 29, 117, 190, 20, smallFont, MUTED, DT_SINGLELINE);
    const char *titles[] = {"Your training, in focus.", "Workout records", "Personal records",
                            "Workout plan", "Calorie estimate"};
    const char *subs[] = {"Every set counts. Keep your progress close.",
                          "Find, review and refine every set you have logged.",
                          "Your heaviest loads and best reps at each weight.",
                          "Choose your muscle groups and exercise variations.",
                          "The same goal calculator as your C project."};
    textAt(dc, titles[view], x, 37, cw, 42, titleFont, TEXT, DT_SINGLELINE);
    textAt(dc, subs[view], x, 84, cw, 26, font, MUTED, DT_SINGLELINE);
    if (view < 2)
    {
        int top = view == 0 ? 282 : 142;
        if (view == 0)
            textAt(dc, "WORKOUT RECORDS", x, 257, cw, 20, smallFont, MUTED, DT_SINGLELINE);
        textAt(dc, "Select a row to edit or delete  |  Double-click to edit", x + 388, top + 61,
               cw - 390, 24, smallFont, MUTED, DT_SINGLELINE | DT_RIGHT);
        if (ListView_GetItemCount(recordList) == 0)
            textAt(dc,
                   db.count ? "No records match this filter."
                            : "Your first set starts here. Choose + Log sets.",
                   x + 20, top + 164, cw - 40, 30, font, MUTED, DT_CENTER | DT_SINGLELINE);
    }
    if (view == 2)
    {
        textAt(dc, "EXACT EXERCISE NAME  (blank shows all)", x, 128, cw, 22, smallFont, MUTED,
               DT_SINGLELINE);
    }
    if (view == 3)
    {
        panel(dc, x, 128, cw, 290, PANEL, RGB(44, 52, 70), 16);
        textAt(dc, "MUSCLE GROUP", x + 12, 143, 120, 22, smallFont, MUTED, DT_SINGLELINE);
        for (int j = 0; j < 3; j++)
        {
            char s[30];
            snprintf(s, sizeof s, "EXERCISE %d", j + 1);
            textAt(dc, s, x + 142 + j * (cw - 154) / 3, 143, 150, 22, smallFont, MUTED,
                   DT_SINGLELINE);
        }
    }
    if (view == 4)
    {
        panel(dc, x, 128, cw, 231, PANEL, RGB(44, 52, 70), 18);
        textAt(dc, "WEIGHT UNIT", x + 20, 145, 160, 20, smallFont, MUTED, DT_SINGLELINE);
        const char *labels[] = {"CURRENT WEIGHT", "GOAL WEIGHT", "MAINTENANCE KCAL / DAY",
                                "DURATION IN MONTHS"};
        for (int i = 0; i < 4; i++)
            textAt(dc, labels[i], x + 20 + (i % 2) * (cw / 2), 211 + (i / 2) * 72, cw / 2 - 44, 20,
                   smallFont, MUTED, DT_SINGLELINE);
    }
    textAt(dc, statusText, x, h - 35, cw, 22, smallFont, MUTED, DT_SINGLELINE | DT_END_ELLIPSIS);
}

typedef struct
{
    HWND window, date, name, weight, reps, list, dateFeedback;
    int editId, count, dateValid;
    double weights[100];
    int repetitions[100];
} LogDialog;
static LogDialog logDialog;
/* Validate the entire date field before a set can enter even the preview.
 * The original ValidDate function remains the authority for calendar rules. */
static int validateLogDate(void)
{
    char value[64];
    if (!IsWindow(logDialog.date))
        return 0;
    getText(logDialog.date, value, sizeof value);
    logDialog.dateValid = GetWindowTextLengthA(logDialog.date) == 10 && ValidDate(value);
    if (IsWindow(logDialog.dateFeedback))
        SetWindowTextA(logDialog.dateFeedback,
                       logDialog.dateValid
                           ? "Valid calendar date"
                           : "Invalid date. Use a real YYYY-MM-DD date from 1900 to 2099.");
    const int guardedButtons[] = {LOG_ADD, LOG_UPDATE, LOG_COMMIT, EDIT_COMMIT};
    for (size_t i = 0; i < sizeof guardedButtons / sizeof guardedButtons[0]; ++i)
    {
        HWND b = GetDlgItem(logDialog.window, guardedButtons[i]);
        if (b)
            EnableWindow(b, logDialog.dateValid);
    }
    InvalidateRect(logDialog.date, NULL, TRUE);
    return logDialog.dateValid;
}
static void logRefresh(void)
{
    ListView_DeleteAllItems(logDialog.list);
    for (int i = 0; i < logDialog.count; i++)
    {
        char s[64];
        snprintf(s, sizeof s, "%d", i + 1);
        LVITEMA it = {0};
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = s;
        ListView_InsertItem(logDialog.list, &it);
        snprintf(s, sizeof s, "%.2f", logDialog.weights[i]);
        cell(logDialog.list, i, 1, s);
        snprintf(s, sizeof s, "%d", logDialog.repetitions[i]);
        cell(logDialog.list, i, 2, s);
    }
    InvalidateRect(logDialog.window, NULL, TRUE);
}
static int readSet(HWND parent, double *kg, int *reps)
{
    char s[128];
    getText(logDialog.weight, s, sizeof s);
    if (!ht_parse_double(s, 0, 1000, kg))
    {
        warn(parent, "Enter a finite weight from 0 to 1000 kg.");
        SetFocus(logDialog.weight);
        return 0;
    }
    getText(logDialog.reps, s, sizeof s);
    if (!ht_parse_int(s, 1, 1000, reps))
    {
        warn(parent, "Enter a whole repetition count from 1 to 1000.");
        SetFocus(logDialog.reps);
        return 0;
    }
    return 1;
}
static void closeLog(HWND w)
{
    DestroyWindow(w);
    EnableWindow(mainWindow, TRUE);
    SetActiveWindow(mainWindow);
}
static LRESULT CALLBACK LogProc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m)
    {
    case WM_CREATE:
    {
        logDialog.window = w;
        SYSTEMTIME st;
        GetLocalTime(&st);
        char date[24];
        snprintf(date, sizeof date, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
        /* Keep extra pasted characters visible so validation can reject them. */
        logDialog.date = edit(w, date, LOG_DATE, 63);
        logDialog.name = edit(w, "", LOG_NAME, 47);
        logDialog.weight = edit(w, "", LOG_WEIGHT, 40);
        logDialog.reps = edit(w, "", LOG_REPS, 12);
        place(logDialog.date, 28, 111, 160, 34);
        place(logDialog.name, 205, 111, 367, 34);
        place(logDialog.weight, 28, 197, 180, 34);
        place(logDialog.reps, 224, 197, 150, 34);
        logDialog.dateFeedback = control(w, "STATIC", "", SS_LEFT, LOG_DATE_FEEDBACK);
        SendMessage(logDialog.dateFeedback, WM_SETFONT, (WPARAM)smallFont, TRUE);
        place(logDialog.dateFeedback, 28, 150, 544, 20);
        if (logDialog.editId)
        {
            WorkoutEntry *e = record(logDialog.editId);
            char s[64];
            if (e)
            {
                SetWindowTextA(logDialog.date, e->date);
                SetWindowTextA(logDialog.name, e->exercise);
                snprintf(s, sizeof s, "%.2f", e->weight100 / 100.0);
                SetWindowTextA(logDialog.weight, s);
                snprintf(s, sizeof s, "%d", e->reps);
                SetWindowTextA(logDialog.reps, s);
            }
            EnableWindow(logDialog.date, FALSE);
            HWND b = button(w, "Save changes", EDIT_COMMIT);
            place(b, 388, 197, 184, 36);
        }
        else
        {
            HWND b = button(w, "Add set", LOG_ADD);
            place(b, 390, 197, 182, 36);
            logDialog.list = newList(w, LOG_LIST);
            place(logDialog.list, 28, 270, 544, 225);
            addColumn(logDialog.list, 0, "Set", 120);
            addColumn(logDialog.list, 1, "Weight (kg)", 210);
            addColumn(logDialog.list, 2, "Repetitions", 190);
            b = button(w, "Update selected", LOG_UPDATE);
            place(b, 28, 509, 168, 34);
            b = button(w, "Remove selected", LOG_REMOVE);
            place(b, 208, 509, 168, 34);
            b = button(w, "Log session", LOG_COMMIT);
            place(b, 388, 566, 184, 38);
        }
        HWND b = button(w, "Cancel", LOG_CANCEL);
        place(b, 28, logDialog.editId ? 304 : 566, 128, 38);
        validateLogDate();
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        FillRect(dc, &r, bgBrush);
        textAt(dc, logDialog.editId ? "Edit this set" : "Log a workout", 28, 23, 540, 40, titleFont,
               TEXT, DT_SINGLELINE);
        textAt(dc,
               logDialog.editId
                   ? "Date, session and set number stay fixed."
                   : "One exercise. One session. A separate weight and reps for every set.",
               28, 66, 544, 24, smallFont, MUTED, DT_SINGLELINE);
        textAt(dc, "DATE  YYYY-MM-DD", 28, 90, 175, 20, smallFont, MUTED, DT_SINGLELINE);
        textAt(dc, "EXERCISE", 205, 90, 367, 20, smallFont, MUTED, DT_SINGLELINE);
        textAt(dc, "WEIGHT IN KG", 28, 174, 180, 20, smallFont, MUTED, DT_SINGLELINE);
        textAt(dc, "REPETITIONS", 224, 174, 150, 20, smallFont, MUTED, DT_SINGLELINE);
        if (!logDialog.editId)
        {
            char s[80];
            snprintf(s, sizeof s, "SESSION PREVIEW  /  %d OF 100 SETS", logDialog.count);
            textAt(dc, s, 28, 244, 540, 20, smallFont, MUTED, DT_SINGLELINE);
        }
        else
            textAt(dc, "Exercise names: 1-47 printable characters, at least one letter, no |.", 28,
                   258, 544, 28, smallFont, MUTED, DT_WORDBREAK);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_DRAWITEM:
        drawButton((const DRAWITEMSTRUCT *)lp);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = S(24);
        return TRUE;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(
            (HDC)wp,
            (HWND)lp == logDialog.dateFeedback
                ? (logDialog.dateValid ? RGB(107, 207, 170) : RGB(255, 135, 147))
                : ((HWND)lp == logDialog.date && !logDialog.dateValid ? RGB(255, 135, 147) : TEXT));
        SetBkColor((HDC)wp, (HWND)lp == logDialog.dateFeedback ? BG : FIELD);
        return (LRESULT)((HWND)lp == logDialog.dateFeedback ? bgBrush : fieldBrush);
    case WM_NOTIFY:
    {
        NMHDR *n = (NMHDR *)lp;
        if (n->idFrom == LOG_LIST && n->code == LVN_ITEMCHANGED)
        {
            int i = ListView_GetNextItem(logDialog.list, -1, LVNI_SELECTED);
            if (i >= 0 && i < logDialog.count)
            {
                char s[64];
                snprintf(s, sizeof s, "%.2f", logDialog.weights[i]);
                SetWindowTextA(logDialog.weight, s);
                snprintf(s, sizeof s, "%d", logDialog.repetitions[i]);
                SetWindowTextA(logDialog.reps, s);
            }
        }
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wp);
        if (id == LOG_DATE && HIWORD(wp) == EN_CHANGE)
        {
            validateLogDate();
            return 0;
        }
        if (id == LOG_CANCEL)
        {
            closeLog(w);
            return 0;
        }
        if (id == LOG_ADD || id == LOG_UPDATE)
        {
            if (!validateLogDate())
            {
                SetFocus(logDialog.date);
                return 0;
            }
            double kg;
            int reps;
            if (!readSet(w, &kg, &reps))
                return 0;
            int i = id == LOG_ADD ? logDialog.count
                                  : ListView_GetNextItem(logDialog.list, -1, LVNI_SELECTED);
            if (i < 0)
            {
                warn(w, "Select a staged set to update.");
                return 0;
            }
            if (i >= 100)
            {
                warn(w, "A session can contain at most 100 sets.");
                return 0;
            }
            logDialog.weights[i] = (int)(kg * 100.0 + 0.5) / 100.0;
            logDialog.repetitions[i] = reps;
            if (id == LOG_ADD)
                logDialog.count++;
            logRefresh();
            SetFocus(logDialog.weight);
            SendMessage(logDialog.weight, EM_SETSEL, 0, -1);
            return 0;
        }
        if (id == LOG_REMOVE)
        {
            int i = ListView_GetNextItem(logDialog.list, -1, LVNI_SELECTED);
            if (i < 0)
            {
                warn(w, "Select a staged set first.");
                return 0;
            }
            for (int j = i; j + 1 < logDialog.count; j++)
            {
                logDialog.weights[j] = logDialog.weights[j + 1];
                logDialog.repetitions[j] = logDialog.repetitions[j + 1];
            }
            logDialog.count--;
            logRefresh();
            return 0;
        }
        if (id == LOG_COMMIT || id == EDIT_COMMIT)
        {
            if (!validateLogDate())
            {
                SetFocus(logDialog.date);
                return 0;
            }
            char name[NAME_LEN], date[11];
            getText(logDialog.name, name, sizeof name);
            getText(logDialog.date, date, sizeof date);
            WorkoutEntry check = {0};
            if (logDialog.editId)
            {
                WorkoutEntry *e = record(logDialog.editId);
                if (!e)
                    return 0;
                check = *e;
            }
            else
            {
                check.id = db.nextId;
                check.sessionId = db.nextSession;
                check.setNumber = 1;
                check.reps = 1;
                memcpy(check.date, date, sizeof date);
            }
            snprintf(check.exercise, sizeof check.exercise, "%s", name);
            if (!ValidDate(check.date))
            {
                warn(w, "Use a valid YYYY-MM-DD date between 1900 and 2099.");
                return 0;
            }
            if (!validEntry(&db, &check, logDialog.editId))
            {
                warn(w, "Check the exercise name and available IDs. Names need a letter, only "
                        "printable ASCII, and no |. An edit must not duplicate a "
                        "session/exercise/set.");
                return 0;
            }
            size_t oldCount = db.count;
            int ok;
            if (logDialog.editId)
            {
                double kg;
                int reps;
                if (!readSet(w, &kg, &reps))
                    return 0;
                ok = ht_edit(&db, logDialog.editId, name, kg, reps);
            }
            else
            {
                if (logDialog.count == 0)
                {
                    warn(w, "Add at least one set to the session preview first.");
                    return 0;
                }
                ok = ht_add(&db, date, name, logDialog.count, logDialog.weights,
                            logDialog.repetitions);
            }
            if (ok || db.count != oldCount)
                dirty = 1;
            if (!ok)
            {
                warn(w, ht_output());
                refreshRecords();
                if (db.count != oldCount)
                    closeLog(w);
                return 0;
            }
            sortEntries(&db, choice(sortMode) == 1);
            refreshRecords();
            closeLog(w);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        closeLog(w);
        return 0;
    }
    return DefWindowProcA(w, m, wp, lp);
}
static void openLog(int id)
{
    memset(&logDialog, 0, sizeof logDialog);
    logDialog.editId = id;
    RECT r;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
    int height = id ? 398 : 650;
    HWND w = CreateWindowExA(WS_EX_DLGMODALFRAME, "HTLogWindow",
                             id ? "Edit workout set" : "Log workout sets", WS_CAPTION | WS_SYSMENU,
                             r.left + (r.right - r.left - S(616)) / 2,
                             r.top + (r.bottom - r.top - S(height)) / 2, S(616), S(height),
                             mainWindow, NULL, instance, NULL);
    if (!w)
        return;
    EnableWindow(mainWindow, FALSE);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(w, 20, &dark, sizeof dark);
    ShowWindow(w, SW_SHOW);
    SetFocus(logDialog.editId ? logDialog.name : logDialog.date);
}
static void updatePlanRow(int i)
{
    int g = choice(planGroups[i]) - 1;
    for (int j = 0; j < 3; j++)
    {
        SendMessage(planOptions[i][j], CB_RESETCONTENT, 0, 0);
        if (g >= 0)
        {
            addChoice(planOptions[i][j], options[g][j][0]);
            addChoice(planOptions[i][j], options[g][j][1]);
            selectChoice(planOptions[i][j], 0);
        }
        EnableWindow(planOptions[i][j], g >= 0);
    }
}
static int save(void)
{
    if (ht_run(HT_SAVE, &db, "", DATA_FILE))
    {
        dirty = 0;
        refreshRecords();
        return 1;
    }
    warn(mainWindow,
         "Save failed. Your records are still in memory. Check write access and free space, then "
         "try again. The previous save may be available as workouts.txt.bak.");
    return 0;
}
static void action(int id)
{
    if(id==171){fxUserOverride=!fxMotion;fxMotion=fxUserOverride;
        SetWindowTextA(motionButton,fxMotion?"Motion: On":"Motion: Off");
        if(!fxMotion)fxStopTransition();else fxAfterChange(fxBeforeChange());return;}
    if(id==172){fxUserOverride=1;fxMotion=TRUE;SetWindowTextA(motionButton,"Motion: On");
        fxAfterChange(fxBeforeChange());fxDuration=1200;fxPreview=fxActive;return;}
    if (id >= NAV_HOME && id <= NAV_CAL)
    {
        switchView(id - NAV_HOME);
        return;
    }
    if (id >= 500 && id < 504)
    {
        if (id == 500)
            openLog(0);
        else
            switchView(id - 499);
        return;
    }
    if (id == ACT_ADD)
    {
        openLog(0);
        return;
    }
    if (id == ACT_EDIT)
    {
        int n = selectedRecord();
        if (n)
            openLog(n);
        return;
    }
    if (id == ACT_DELETE)
    {
        int n = selectedRecord();
        if (!n)
            return;
        char msg[160];
        snprintf(msg, sizeof msg,
                 "Delete record %d? This removes one set. Its ID will not be reused.", n);
        if (MessageBoxA(mainWindow, msg, "Delete workout set",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)
        {
            char answers[40];
            snprintf(answers, sizeof answers, "%d\n1\n", n);
            if (ht_run(HT_DELETE, &db, answers, NULL))
            {
                dirty = 1;
                sortEntries(&db, choice(sortMode) == 1);
                refreshRecords();
            }
            else
                warn(mainWindow, ht_output());
        }
        return;
    }
    if (id == ACT_SAVE)
    {
        save();
        return;
    }
    if (id == ACT_LOAD)
    {
        if (MessageBoxA(mainWindow,
                        "Replace the records in memory with workouts.txt? Any unsaved changes will "
                        "be lost.",
                        "Reload saved data", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            return;
        DWORD a = GetFileAttributesA(DATA_FILE);
        if (a == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            status("No workouts.txt file found. Current records are unchanged.");
            return;
        }
        if (ht_run(HT_LOAD, &db, "", DATA_FILE))
        {
            dirty = 0;
            sortEntries(&db, choice(sortMode) == 1);
            refreshRecords();
            if (view == 2)
                refreshPR();
        }
        else
            warn(mainWindow, ht_output());
        return;
    }
    if (id == ACT_CLEAR)
    {
        SetWindowTextA(filterText, "");
        refreshRecords();
        return;
    }
    if (id == ACT_FILTER)
    {
        char s[128];
        getText(filterText, s, sizeof s);
        int n;
        if (choice(filterMode) == 1 && s[0] && !ht_parse_int(s, 1, INT_MAX - 1, &n))
        {
            warn(mainWindow, "Enter a record ID from 1 to 2147483646.");
            return;
        }
        refreshRecords();
        return;
    }
    if (id == PR_RUN)
    {
        refreshPR();
        return;
    }
    if (id == PLAN_RUN)
    {
        int count = 0, g[4], o[4][3];
        for (int i = 0; i < 4; i++)
        {
            int v = choice(planGroups[i]);
            if (v > 0)
            {
                for (int k = 0; k < count; k++)
                    if (g[k] == v)
                    {
                        warn(mainWindow, "Choose each muscle group only once.");
                        return;
                    }
                g[count] = v;
                for (int j = 0; j < 3; j++)
                    o[count][j] = choice(planOptions[i][j]) + 1;
                count++;
            }
        }
        if (count == 0)
        {
            warn(mainWindow, "Choose at least one muscle group.");
            return;
        }
        char answers[128];
        size_t used = (size_t)snprintf(answers, sizeof answers, "%d\n", count);
        for (int i = 0; i < count; i++)
            used += (size_t)snprintf(answers + used, sizeof answers - used, "%d\n", g[i]);
        for (int i = 0; i < count; i++)
            for (int j = 0; j < 3; j++)
                used += (size_t)snprintf(answers + used, sizeof answers - used, "%d\n", o[i][j]);
        if (!ht_run(HT_PLAN, &db, answers, NULL))
        {
            warn(mainWindow, ht_output());
            return;
        }
        const char *s = strstr(ht_output(), "Workout plan:");
        setReport(s ? s : ht_output());
        return;
    }
    if (id == CAL_RUN)
    {
        char values[4][80], answers[400];
        double num[4];
        double factor = choice(calUnit) == 1 ? KG_TO_LB : 1;
        double low[] = {10 * factor, 10 * factor, 1, 0.01},
               high[] = {300 * factor, 400 * factor, 10000, 120};
        for (int i = 0; i < 4; i++)
        {
            getText(calFields[i], values[i], sizeof values[i]);
            if (!ht_parse_double(values[i], low[i], high[i], &num[i]))
            {
                char s[180];
                snprintf(s, sizeof s, "Field %d needs a finite number from %.2f to %.2f.", i + 1,
                         low[i], high[i]);
                warn(mainWindow, s);
                SetFocus(calFields[i]);
                return;
            }
        }
        snprintf(answers, sizeof answers, "%d\n%s\n%s\n%s\n%s\n", choice(calUnit) + 1, values[0],
                 values[1], values[2], values[3]);
        ht_run(HT_CALORIES, &db, answers, NULL);
        const char *s = strstr(ht_output(), "---");
        if (!s)
            s = strstr(ht_output(), "The calculated");
        setReport(s ? s : ht_output());
        return;
    }
}
static void createMainControls(void)
{
    motionButton=button(mainWindow,fxMotion?"Motion: On":"Motion: Off",171);
    previewButton=button(mainWindow,"Preview",172);
    const char *names[] = {"Overview", "Workout records", "Personal records", "Workout plan",
                           "Calories"};
    for (int i = 0; i < 5; i++)
        nav[i] = button(mainWindow, names[i], NAV_HOME + i);
    const char *tileNames[] = {"Log workout", "Personal records", "Workout plan",
                               "Calorie estimate"};
    for (int i = 0; i < 4; i++)
        tiles[i] = button(mainWindow, tileNames[i], 500 + i);
    const char *actions[] = {"+ Log sets", "Edit set", "Delete set", "Save data", "Load data"};
    int ids[] = {ACT_ADD, ACT_EDIT, ACT_DELETE, ACT_SAVE, ACT_LOAD};
    for (int i = 0; i < 5; i++)
        toolbar[i] = button(mainWindow, actions[i], ids[i]);
    filterMode = combo(mainWindow, FILTER_MODE);
    addChoice(filterMode, "Exact exercise");
    addChoice(filterMode, "Record ID");
    selectChoice(filterMode, 0);
    filterText = edit(mainWindow, "", FILTER_TEXT, 47);
    filterButton = button(mainWindow, "Search", ACT_FILTER);
    clearButton = button(mainWindow, "Clear", ACT_CLEAR);
    sortMode = combo(mainWindow, SORT_MODE);
    addChoice(sortMode, "Sort: Record ID");
    addChoice(sortMode, "Sort: Date");
    selectChoice(sortMode, 0);
    recordList = newList(mainWindow, 170);
    const char *cols[] = {"ID", "Date", "Exercise", "Session", "Set", "kg", "lb", "Reps"};
    int widths[] = {70, 110, 240, 66, 48, 84, 84, 65};
    for (int i = 0; i < 8; i++)
        addColumn(recordList, i, cols[i], widths[i]);
    report = control(
        mainWindow, "EDIT", "",
        WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_HSCROLL, 180);
    SendMessage(report, WM_SETFONT, (WPARAM)monoFont, TRUE);
    SendMessage(report, EM_SETLIMITTEXT, 0x7ffffffe, 0);
    prText = edit(mainWindow, "", PR_TEXT, 47);
    prButton = button(mainWindow, "Show records", PR_RUN);
    for (int i = 0; i < 4; i++)
    {
        planGroups[i] = combo(mainWindow, PLAN_GROUP + i);
        for (int j = 0; j < 8; j++)
            addChoice(planGroups[i], groups[j]);
        selectChoice(planGroups[i], 0);
        for (int j = 0; j < 3; j++)
            planOptions[i][j] = combo(mainWindow, PLAN_OPTION + i * 3 + j);
        updatePlanRow(i);
    }
    planButton = button(mainWindow, "Generate plan", PLAN_RUN);
    calUnit = combo(mainWindow, CAL_UNIT);
    addChoice(calUnit, "Kilograms (kg)");
    addChoice(calUnit, "Pounds (lb)");
    selectChoice(calUnit, 0);
    for (int i = 0; i < 4; i++)
        calFields[i] = edit(mainWindow, "", CAL_FIELD + i, 60);
    calButton = button(mainWindow, "Calculate", CAL_RUN);
}
static LRESULT CALLBACK MainProc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m)
    {
    case WM_CREATE:
        mainWindow = w;
        createMainControls();
        sortEntries(&db, 0);
        layout();
        refreshRecords();
        return 0;
    case WM_SIZE:
        fxStopTransition();
        layout();
        return 0;
    case WM_SETTINGCHANGE:
        fxReadPreferences();
        if (!fxMotion)
            fxStopTransition();
        return 0;
    case WM_PRINTCLIENT:
        paintMain((HDC)wp);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = S(1100);
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = S(640);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC target = BeginPaint(w, &ps);
        RECT r;
        GetClientRect(w, &r);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, r.right, r.bottom);
        HGDIOBJ old = SelectObject(dc, bitmap);
        paintMain(dc);
        BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DRAWITEM:
        drawButton((const DRAWITEMSTRUCT *)lp);
        return TRUE;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = S(24);
        return TRUE;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, TEXT);
        SetBkColor((HDC)wp, FIELD);
        return (LRESULT)fieldBrush;
    case WM_NOTIFY:
    {
        NMHDR *n = (NMHDR *)lp;
        if (n->hwndFrom == recordList)
        {
            if (n->code == LVN_ITEMCHANGED)
            {
                EnableWindow(toolbar[1], selectedRecord() != 0);
                EnableWindow(toolbar[2], selectedRecord() != 0);
            }
            if (n->code == NM_DBLCLK)
                action(ACT_EDIT);
            if (n->code == LVN_COLUMNCLICK)
            {
                int column = ((NMLISTVIEW *)lp)->iSubItem;
                if (column == 0 || column == 1)
                {
                    selectChoice(sortMode, column == 1);
                    sortEntries(&db, column == 1);
                    dirty = 1;
                    refreshRecords();
                }
            }
        }
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == SORT_MODE && code == CBN_SELCHANGE)
        {
            sortEntries(&db, choice(sortMode) == 1);
            dirty = 1;
            refreshRecords();
            return 0;
        }
        if (id >= PLAN_GROUP && id < PLAN_GROUP + 4 && code == CBN_SELCHANGE)
        {
            updatePlanRow(id - PLAN_GROUP);
            return 0;
        }
        if (code == BN_CLICKED)
            action(id);
        return 0;
    }
    case WM_CLOSE:
        if (save())
            DestroyWindow(w);
        else if (MessageBoxA(w, "Exit without saving? Unsaved changes will be lost.", "Save failed",
                             MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)
            DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        fxRelease();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(w, m, wp, lp);
}
#ifdef HT_GUI_TEST
#include "../tests/gui_checks.inc"
#endif
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int showCmd)
{
    (void)prev;
    (void)cmd;
    instance = inst;
    fxReadPreferences();
    /* Keep data beside the executable, independent of how VS Code starts it. */
    char location[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, location, sizeof location);
    if (!length || length >= sizeof location)
    {
        MessageBoxA(NULL, "Move the project to a shorter folder path.", "GymTrack",
                    MB_ICONERROR);
        return 1;
    }
    char *slash = strrchr(location, '\\');
    if (slash)
    {
        *slash = 0;
        if (!SetCurrentDirectoryA(location))
        {
            MessageBoxA(NULL, "Cannot open the application folder.", "GymTrack",
                        MB_ICONERROR);
            return 1;
        }
    }
    SetProcessDPIAware();
    HDC dc = GetDC(NULL);
    scaleDpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    font = CreateFontA(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                       CLEARTYPE_QUALITY, 0, "Segoe UI");
    boldFont = CreateFontA(-S(14), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, "Segoe UI");
    titleFont = CreateFontA(-S(30), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, "Segoe UI");
    smallFont = CreateFontA(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, "Segoe UI");
    brandFont = CreateFontA(-S(23), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, "Segoe UI");
    monoFont = CreateFontA(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, "Consolas");
    bgBrush = CreateSolidBrush(BG);
    fieldBrush = CreateSolidBrush(FIELD);
    INITCOMMONCONTROLSEX ic = {sizeof ic, ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&ic);
    db.nextId = db.nextSession = 1;
    if (!reserveEntries(&db, 8) || !ht_run(HT_LOAD, &db, "", DATA_FILE))
    {
        MessageBoxA(NULL,
                    "Startup failed. The existing workouts.txt file was not overwritten. Check its "
                    "HT1 format or restore a valid backup.\n\nMove an invalid file aside before "
                    "starting with an empty journal.",
                    "GymTrack", MB_OK | MB_ICONERROR);
        free(db.data);
        return 1;
    }
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof wc;
    wc.hInstance = inst;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = "HTMainWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(101));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = bgBrush;
    RegisterClassExA(&wc);
    wc.lpfnWndProc = LogProc;
    wc.lpszClassName = "HTLogWindow";
    RegisterClassExA(&wc);
    RECT area;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &area, 0);
    int width = S(1230), height = S(820);
    if (width > area.right - area.left)
        width = area.right - area.left;
    if (height > area.bottom - area.top)
        height = area.bottom - area.top;
    HWND w = CreateWindowExA(
        0, "HTMainWindow", "GymTrack 1.4", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        area.left + (area.right - area.left - width) / 2,
        area.top + (area.bottom - area.top - height) / 2, width, height, NULL, NULL, inst, NULL);
    if (!w)
    {
        free(db.data);
        return 1;
    }
    BOOL dark = TRUE;
    DwmSetWindowAttribute(w, 20, &dark, sizeof dark);
    ShowWindow(w, showCmd);
    UpdateWindow(w);
#ifdef HT_GUI_TEST
    int testResult = ht_gui_checks();
    DestroyWindow(w);
    PostQuitMessage(testResult);
#endif
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        HWND modal = IsWindow(logDialog.window) ? logDialog.window : NULL;
        HWND target = modal ? modal : w;
        if (msg.message == WM_KEYDOWN)
        {
            if (msg.wParam == VK_ESCAPE && modal)
            {
                closeLog(modal);
                continue;
            }
            if (!modal && (GetKeyState(VK_CONTROL) & 0x8000))
            {
                if (msg.wParam == 'S')
                {
                    save();
                    continue;
                }
                if (msg.wParam == 'N')
                {
                    openLog(0);
                    continue;
                }
                if (msg.wParam == 'F')
                {
                    switchView(1);
                    SetFocus(filterText);
                    continue;
                }
            }
            if (!modal && msg.wParam == VK_RETURN && GetFocus() == filterText)
            {
                action(ACT_FILTER);
                continue;
            }
        }
        if (!IsDialogMessage(target, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    free(db.data);
    ht_release();
    DeleteObject(font);
    DeleteObject(boldFont);
    DeleteObject(titleFont);
    DeleteObject(smallFont);
    DeleteObject(monoFont);
    DeleteObject(brandFont);
    DeleteObject(bgBrush);
    DeleteObject(fieldBrush);
    return (int)msg.wParam;
}
