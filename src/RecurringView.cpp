#include "RecurringView.h"
#include "AppState.h"
#include "Drawing.h"
#include "InlineEdit.h"
#include "Layout.h"
#include "SyncPicker.h"
#include "ThemedDatePicker.h"
#include "UIEventHandler.h"
#include <commctrl.h>
#include <algorithm>
#include <cwctype>

namespace {
constexpr int ROW1_Y = 58;
constexpr int ROW2_Y = 96;
constexpr int ROW3_Y = 134;
constexpr int FORM_BOTTOM = 172; // Separator line below the form.
constexpr int LIST_TOP = 173;
constexpr int LABEL_X = 16;
constexpr int FIELD_X = 100;
constexpr int FIELD_H = 28;
constexpr int HEADER_H = 28;
constexpr int CARD_H = 44;
constexpr int CARD_GAP = 6;

HWND g_repeatBtn = nullptr;
HWND g_textEdit = nullptr;
HWND g_dateEdit = nullptr;
HWND g_countEdit = nullptr;
HWND g_unitBtn = nullptr;
HWND g_saveBtn = nullptr;
HWND g_cancelBtn = nullptr;
bool g_pageVisible = false;
bool g_dropDownOpen = false;
RepeatUnit g_unit = RepeatUnit::Day;
std::string g_editingUid;       // Rule loaded into the form; empty for a new one.
std::wstring g_takenOverText;   // Text moved here from the new-task field.

struct Row {
    std::string uid;
    RECT rect{};
    RECT deleteRect{};
};
std::vector<Row> g_rows;
int g_hoverRow = -1;
bool g_hoverDelete = false;

std::wstring Trimmed(HWND edit) {
    const int length = GetWindowTextLengthW(edit);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, &text[0], length + 1);
    text.resize(static_cast<size_t>(length));
    while (!text.empty() && std::iswspace(text.front())) text.erase(text.begin());
    while (!text.empty() && std::iswspace(text.back())) text.pop_back();
    return text;
}

const RecurringRule* FindRule(const std::string& uid) {
    for (const RecurringRule& rule : g_store.recurring_rules) {
        if (rule.uid == uid) return &rule;
    }
    return nullptr;
}

int ReadCount() {
    wchar_t text[8]{};
    GetWindowTextW(g_countEdit, text, 8);
    return (std::max)(0, (std::min)(Recurring::MAX_REPEAT_COUNT, _wtoi(text)));
}

std::string ReadDate() {
    return ThemedDatePicker::GetDate(g_dateEdit);
}

void SetDate(const std::string& date) {
    ThemedDatePicker::SetDate(g_dateEdit, date);
}

void SetUnit(RepeatUnit unit) {
    g_unit = unit;
    SetWindowTextW(g_unitBtn, (Recurring::UnitLabel(unit) + L"  ▾").c_str());
    InvalidateRect(g_unitBtn, nullptr, TRUE);
}

void InvalidateForm() {
    RECT client{};
    GetClientRect(g_hWnd, &client);
    RECT form = { 0, 48, client.right, FORM_BOTTOM };
    InvalidateRect(g_hWnd, &form, FALSE);
}

void Refresh() {
    UpdateControlsVisibility();
    RecalculateLayout();
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void ResetForm(const std::wstring& text, bool resetTargets) {
    g_editingUid.clear();
    SetWindowTextW(g_textEdit, text.c_str());
    SetDate(Recurring::Today());
    SetWindowTextW(g_countEdit, L"0");
    SetUnit(RepeatUnit::Day);
    SetWindowTextW(g_saveBtn, L"Feladat felvétele");
    if (resetTargets) SyncPicker::ResetSelection();
}

void LoadRule(const RecurringRule& rule) {
    g_editingUid = rule.uid;
    SetWindowTextW(g_textEdit, rule.text.c_str());
    SetDate(rule.next_due);
    SetWindowTextW(g_countEdit, std::to_wstring(rule.repeat_count).c_str());
    SetUnit(rule.repeat_unit);
    SetWindowTextW(g_saveBtn, L"Mentés");
    g_syncSelection.clear();
    for (int id : rule.targets) {
        if (g_store.FindSource(id)) g_syncSelection.push_back(id);
    }
    if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
    Refresh();
    SetFocus(g_textEdit);
    SendMessageW(g_textEdit, EM_SETSEL, 0, -1);
}

void InsertOnTop(const std::vector<int>& ids) {
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), *it) == g_sessionActiveTaskIds.end()) {
            g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin(), *it);
        }
    }
}

void Save() {
    const std::wstring text = Trimmed(g_textEdit);
    if (text.empty()) {
        if (g_store.sounds_enabled) MessageBeep(MB_ICONWARNING);
        SetFocus(g_textEdit);
        return;
    }
    RecurringRule rule;
    rule.uid = g_editingUid;
    rule.text = text;
    rule.start_date = ReadDate();
    rule.repeat_count = ReadCount();
    rule.repeat_unit = g_unit;
    for (int id : g_syncSelection) {
        if (g_store.FindSource(id)) rule.targets.push_back(id);
    }
    const bool plainTaskNow = rule.repeat_count == 0 && rule.start_date <= Recurring::Today();
    InsertOnTop(g_store.SaveRecurringRule(rule));

    if (!g_takenOverText.empty() && Trimmed(g_hEdit) == g_takenOverText) SetWindowTextW(g_hEdit, L"");
    g_takenOverText.clear();
    ResetForm(L"", true);
    if (plainTaskNow) {
        // Nothing left to schedule: show the new task where it landed.
        g_viewMode = ViewMode::ActiveTasks;
        g_scrollY = 0;
        Refresh();
        SetFocus(g_hEdit);
        return;
    }
    Refresh();
    SetFocus(g_textEdit);
}

void DeleteRule(HWND owner, const std::string& uid) {
    const RecurringRule* rule = FindRule(uid);
    if (!rule) return;
    const std::wstring question = L"Biztos törli ezt az ütemezést?\n" + rule->text +
        L"\n\nA már létrehozott feladatok megmaradnak.";
    if (ShowCenteredMessageBox(owner, question, L"Ütemezés törlése", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if (uid == g_editingUid) ResetForm(L"", true);
    g_store.DeleteRecurringRule(uid);
    Refresh();
}

void ShowUnitMenu(HWND owner) {
    RECT button{};
    GetWindowRect(g_unitBtn, &button);
    HMENU menu = CreatePopupMenu();
    const RepeatUnit units[] = { RepeatUnit::Day, RepeatUnit::Week, RepeatUnit::Month };
    for (RepeatUnit unit : units) {
        AppendMenuW(menu, MF_STRING | (unit == g_unit ? MF_CHECKED : MF_UNCHECKED),
            static_cast<UINT>(unit) + 1, Recurring::UnitLabel(unit).c_str());
    }
    const bool wasInContextMenu = g_inContextMenu;
    g_inContextMenu = true;
    const UINT picked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
        button.left, button.bottom + 2, 0, owner, nullptr);
    g_inContextMenu = wasInContextMenu;
    DestroyMenu(menu);
    if (picked >= 1 && picked <= 3) {
        SetUnit(static_cast<RepeatUnit>(picked - 1));
        InvalidateForm();
    }
    SetFocus(g_unitBtn);
}

HWND AdjacentControl(HWND current, bool reverse) {
    const HWND order[] = { g_textEdit, g_dateEdit, g_countEdit, g_unitBtn, g_hSyncToggleBtn, g_cancelBtn, g_saveBtn };
    std::vector<HWND> stops;
    for (HWND control : order) {
        if (control && IsWindowVisible(control) && IsWindowEnabled(control)) stops.push_back(control);
    }
    auto it = std::find(stops.begin(), stops.end(), current);
    if (it == stops.end()) return stops.empty() ? nullptr : stops.front();
    const size_t index = static_cast<size_t>(it - stops.begin());
    return stops[reverse ? (index + stops.size() - 1) % stops.size() : (index + 1) % stops.size()];
}

LRESULT CALLBACK FormSubclassProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR /*refData*/) {
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, FormSubclassProc, subclassId);
    // The cloud button is shared with the active view, which keeps its own behavior.
    if (!g_pageVisible) return DefSubclassProc(control, message, wParam, lParam);

    if (message == WM_KEYDOWN) {
        if (wParam == VK_TAB) {
            HWND next = AdjacentControl(control, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            if (next) {
                SetFocus(next);
                if (next == g_textEdit || next == g_countEdit) SendMessageW(next, EM_SETSEL, 0, -1);
            }
            return 0;
        }
        if (wParam == VK_RETURN) {
            const bool isButton = control == g_unitBtn || control == g_cancelBtn || control == g_hSyncToggleBtn;
            if (isButton) SendMessageW(control, BM_CLICK, 0, 0);
            else Save();
            return 0;
        }
        if ((wParam == VK_UP || wParam == VK_DOWN) && (control == g_unitBtn || control == g_countEdit)) {
            const int delta = wParam == VK_UP ? 1 : -1;
            if (control == g_unitBtn) {
                SetUnit(static_cast<RepeatUnit>((static_cast<int>(g_unit) + 3 + delta) % 3));
            } else {
                const int count = (std::max)(0, (std::min)(Recurring::MAX_REPEAT_COUNT, ReadCount() + delta));
                SetWindowTextW(g_countEdit, std::to_wstring(count).c_str());
                SendMessageW(g_countEdit, EM_SETSEL, 0, -1);
            }
            InvalidateForm();
            return 0;
        }
    }
    if (message == WM_CHAR && (wParam == L'\t' || wParam == L'\r')) return 0;
    return DefSubclassProc(control, message, wParam, lParam);
}

HWND CreateButton(HWND owner, HINSTANCE instance, const wchar_t* label, int id) {
    HWND button = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 10, FIELD_H, owner, (HMENU)(INT_PTR)id, instance, nullptr);
    SendMessageW(button, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    return button;
}

void AddTooltip(HWND tooltip, HWND owner, HWND control, const wchar_t* text) {
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    tool.hwnd = owner;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
}

// "Hetente · első: ma", "Megjelenik: 2026.10.20.", ...
std::wstring Summary() {
    const std::string date = ReadDate();
    const bool today = date == Recurring::Today();
    const int count = ReadCount();
    if (count == 0) {
        return today ? L"Azonnal felkerül a feladatok közé." : L"Egyszer jelenik meg: " + Recurring::FormatDate(date);
    }
    return Recurring::DescribeRepeat(count, g_unit) + L", első: " +
        (today ? L"ma (azonnal felkerül)" : Recurring::FormatDate(date));
}

std::wstring TargetNames(const RecurringRule& rule) {
    std::wstring names;
    for (int id : rule.targets) {
        const SyncSource* source = g_store.FindSource(id);
        if (!source) continue;
        if (!names.empty()) names += L", ";
        names += source->name;
    }
    if (!names.empty()) return names;
    return rule.targets.empty() ? L"Helyi" : L"Helyi (a célfájl már nincs beállítva)";
}

void FillRoundRect(HDC hdc, const RECT& rc, COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}
}

void RecurringView::CreateControls(HWND owner, HINSTANCE instance) {
    g_repeatBtn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        540, 56, 31, FIELD_H, owner, (HMENU)IDC_RECURRING_BTN, instance, nullptr);
    g_textEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 10, FIELD_H, owner, (HMENU)IDC_RECURRING_TEXT_EDIT, instance, nullptr);
    SendMessageW(g_textEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessageW(g_textEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"A feladat szövege");
    g_dateEdit = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"", WS_CHILD | WS_TABSTOP | DTS_SHORTDATEFORMAT,
        0, 0, 10, FIELD_H, owner, (HMENU)IDC_RECURRING_DATE, instance, nullptr);
    SendMessageW(g_dateEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    g_countEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_CENTER,
        0, 0, 10, FIELD_H, owner, (HMENU)IDC_RECURRING_COUNT_EDIT, instance, nullptr);
    SendMessageW(g_countEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessageW(g_countEdit, EM_SETLIMITTEXT, 3, 0);
    g_unitBtn = CreateButton(owner, instance, L"", IDC_RECURRING_UNIT_BTN);
    g_saveBtn = CreateButton(owner, instance, L"Feladat felvétele", IDC_RECURRING_SAVE_BTN);
    g_cancelBtn = CreateButton(owner, instance, L"Mégse", IDC_RECURRING_CANCEL_BTN);

    for (HWND control : { g_textEdit, g_dateEdit, g_countEdit, g_unitBtn, g_saveBtn, g_cancelBtn, g_hSyncToggleBtn }) {
        if (control) SetWindowSubclass(control, FormSubclassProc, 0, 0);
    }
    ThemedDatePicker::Attach(g_dateEdit); // Added last, so it sees keys before the form's Tab/Enter handling.
    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, owner, nullptr, instance, nullptr);
    if (tooltip) {
        SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 320);
        AddTooltip(tooltip, owner, g_repeatBtn, L"Ismétlődő vagy későbbi feladat (Ctrl+Enter)\nA beírt szöveget is átviszi.");
        AddTooltip(tooltip, owner, g_countEdit,
            L"Hány naponta / hetente / havonta jöjjön elő újra (fel-le nyíllal is állítható).\n0: nem ismétlődik, csak egyszer jelenik meg.");
        AddTooltip(tooltip, owner, g_dateEdit, L"Ezen a napon jelenik meg először (a munkaidő kezdetekor). Ma: azonnal felkerül.\nKattintás: naptár; fel/le: ±1 nap, PgUp/PgDn: ±1 hónap, Home: ma.");
    }
    ResetForm(L"", false);
}

void RecurringView::Open(HWND owner) {
    CommitInlineEdit();
    g_takenOverText = Trimmed(g_hEdit);
    g_viewMode = ViewMode::Recurring;
    g_scrollY = 0;
    g_selectedIndex = -1;
    ResetForm(g_takenOverText, false); // Keeps the cloud selection made for the typed text.
    UpdateControlsVisibility();
    ::RecalculateLayout();
    InvalidateRect(owner, nullptr, TRUE);
    SetFocus(g_takenOverText.empty() ? g_textEdit : g_dateEdit);
}

void RecurringView::UpdateVisibility(bool pageVisible) {
    if (!g_textEdit) return;
    const bool wasVisible = g_pageVisible;
    g_pageVisible = pageVisible && !g_isMiniMode;
    // Leaving the page drops an unfinished edit, so its targets do not leak into the next new task.
    if (wasVisible && !g_pageVisible && !g_editingUid.empty()) ResetForm(L"", true);

    ShowWindow(g_repeatBtn, !g_isMiniMode && g_viewMode == ViewMode::ActiveTasks ? SW_SHOW : SW_HIDE);
    const int show = g_pageVisible ? SW_SHOW : SW_HIDE;
    for (HWND control : { g_textEdit, g_dateEdit, g_countEdit, g_unitBtn, g_saveBtn }) ShowWindow(control, show);
    ShowWindow(g_cancelBtn, g_pageVisible && !g_editingUid.empty() ? SW_SHOW : SW_HIDE);
    if (!g_pageVisible) return;

    RECT client{};
    GetClientRect(g_hWnd, &client);
    const int right = client.right - 14;
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(g_textEdit, nullptr, FIELD_X, ROW1_Y, right - FIELD_X, FIELD_H, flags);
    SetWindowPos(g_dateEdit, nullptr, FIELD_X, ROW2_Y, 130, FIELD_H, flags);
    SetWindowPos(g_countEdit, nullptr, 330, ROW2_Y, 48, FIELD_H, flags);
    SetWindowPos(g_unitBtn, nullptr, 384, ROW2_Y, 84, FIELD_H, flags);
    const int saveWidth = 150;
    SetWindowPos(g_saveBtn, nullptr, right - saveWidth, ROW3_Y, saveWidth, FIELD_H, flags);
    SetWindowPos(g_cancelBtn, nullptr, right - saveWidth - 6 - 80, ROW3_Y, 80, FIELD_H, flags);
    if (SyncPicker::HasSources()) SetWindowPos(g_hSyncToggleBtn, nullptr, FIELD_X, ROW3_Y, 31, FIELD_H, flags);
}

int RecurringView::ListTop() {
    return LIST_TOP;
}

void RecurringView::RecalculateLayout(int clientWidth) {
    std::vector<const RecurringRule*> sorted;
    for (const RecurringRule& rule : g_store.recurring_rules) sorted.push_back(&rule);
    std::sort(sorted.begin(), sorted.end(), [](const RecurringRule* a, const RecurringRule* b) {
        return a->next_due != b->next_due ? a->next_due < b->next_due : a->text < b->text;
    });
    g_rows.clear();
    int y = LIST_TOP + 6 + HEADER_H + 6;
    for (const RecurringRule* rule : sorted) {
        Row row;
        row.uid = rule->uid;
        row.rect = { 10, y, clientWidth - 10, y + CARD_H };
        row.deleteRect = { clientWidth - 42, y + 10, clientWidth - 18, y + 34 };
        g_rows.push_back(row);
        y += CARD_H + CARD_GAP;
    }
    if (g_hoverRow >= static_cast<int>(g_rows.size())) g_hoverRow = -1;
    g_totalContentHeight = y - LIST_TOP;
}

void RecurringView::PaintForm(HDC hdc, int clientWidth) {
    const ThemeColors th = GetThemeColors(g_themeMode);
    RECT band = { 0, 48, clientWidth, FORM_BOTTOM };
    HBRUSH bandBrush = CreateSolidBrush(th.bgSubBar);
    FillRect(hdc, &band, bandBrush);
    DeleteObject(bandBrush);
    HPEN separator = CreatePen(PS_SOLID, 1, th.borderSep);
    HGDIOBJ oldPen = SelectObject(hdc, separator);
    MoveToEx(hdc, 0, FORM_BOTTOM, nullptr);
    LineTo(hdc, clientWidth, FORM_BOTTOM);
    SelectObject(hdc, oldPen);
    DeleteObject(separator);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_hFontTimerLabel);
    SetTextColor(hdc, th.textSecondary);
    const bool hasSources = SyncPicker::HasSources();
    struct Label { const wchar_t* text; int x; int y; int right; };
    const Label labels[] = {
        { L"Feladat", LABEL_X, ROW1_Y, FIELD_X - 8 },
        { L"Érvényes", LABEL_X, ROW2_Y, FIELD_X - 8 },
        { L"Ismétlés", 248, ROW2_Y, 324 },
        { hasSources ? L"Hova" : L"", LABEL_X, ROW3_Y, FIELD_X - 8 },
    };
    for (const Label& label : labels) {
        RECT rc = { label.x, label.y, label.right, label.y + FIELD_H };
        DrawTextW(hdc, label.text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, g_hFontSmall);
    RECT hint = { 476, ROW2_Y, clientWidth - 14, ROW2_Y + FIELD_H };
    DrawTextW(hdc, L"0 = nem ismétlődik", -1, &hint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT save{};
    GetWindowRect(IsWindowVisible(g_cancelBtn) ? g_cancelBtn : g_saveBtn, &save);
    MapWindowPoints(nullptr, g_hWnd, reinterpret_cast<POINT*>(&save), 2);
    RECT summary = { hasSources ? FIELD_X + 31 + 10 : FIELD_X, ROW3_Y, save.left - 10, ROW3_Y + FIELD_H };
    SetTextColor(hdc, th.textPrimary);
    const std::wstring summaryText = Summary();
    DrawTextW(hdc, summaryText.c_str(), -1, &summary, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void RecurringView::PaintList(HDC hdc, int clientWidth, int listTop, int /*listBottom*/) {
    const ThemeColors th = GetThemeColors(g_themeMode);
    const bool pink = g_themeMode == ThemeMode::Pink;
    SetBkMode(hdc, TRANSPARENT);

    RECT header = { 10, listTop + 6 - g_scrollY, clientWidth - 10, listTop + 6 + HEADER_H - g_scrollY };
    HBRUSH headerBrush = CreateSolidBrush(th.headerSectionBg);
    FillRect(hdc, &header, headerBrush);
    DeleteObject(headerBrush);
    SelectObject(hdc, g_hFontHeader);
    SetTextColor(hdc, th.headerSectionText);
    RECT headerText = { header.left + 8, header.top, header.right, header.bottom };
    const std::wstring headerLabel = L"Ütemezések (" + std::to_wstring(g_rows.size()) + L")";
    DrawTextW(hdc, headerLabel.c_str(), -1, &headerText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (g_rows.empty()) {
        SelectObject(hdc, g_hFontNormal);
        SetTextColor(hdc, th.textEmpty);
        RECT empty = { 20, header.bottom + 30, clientWidth - 20, header.bottom + 110 };
        DrawTextW(hdc, L"Még nincs ismétlődő vagy későbbi feladat.\nA fenti űrlapon vehetsz fel egyet.", -1, &empty,
            DT_CENTER | DT_WORDBREAK);
        return;
    }

    for (size_t i = 0; i < g_rows.size(); ++i) {
        const RecurringRule* rule = FindRule(g_rows[i].uid);
        if (!rule) continue;
        RECT card = g_rows[i].rect;
        OffsetRect(&card, 0, -g_scrollY);
        const bool hovered = static_cast<int>(i) == g_hoverRow;
        const bool editing = rule->uid == g_editingUid;
        const COLORREF cardBg = pink ? (editing ? RGB(226, 36, 130) : RGB(183, 0, 99))
            : (editing ? th.bgCardSelected : (hovered ? th.bgCardHover : th.bgCard));
        const COLORREF cardBorder = editing ? (pink ? RGB(255, 255, 255) : th.borderCardSelected)
            : (hovered ? th.borderCardHover : th.borderCard);
        FillRoundRect(hdc, card, cardBg, cardBorder, 8);

        RECT text = { card.left + 12, card.top + 4, card.right - 50, card.top + 24 };
        SelectObject(hdc, g_hFontNormal);
        SetTextColor(hdc, pink ? RGB(255, 255, 255) : th.textPrimary);
        DrawTextW(hdc, rule->text.c_str(), -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        std::wstring meta = rule->repeat_count > 0
            ? L"↻ " + Recurring::DescribeRepeat(rule->repeat_count, rule->repeat_unit) + L" · következő: "
            : L"Egyszeri · megjelenik: ";
        meta += Recurring::FormatDate(rule->next_due) + L" · " + TargetNames(*rule);
        RECT metaRect = { text.left, card.top + 24, text.right, card.bottom - 3 };
        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, pink ? RGB(255, 230, 242) : th.textSecondary);
        DrawTextW(hdc, meta.c_str(), -1, &metaRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        RECT del = g_rows[i].deleteRect;
        OffsetRect(&del, 0, -g_scrollY);
        const bool deleteHovered = hovered && g_hoverDelete;
        if (deleteHovered) FillRoundRect(hdc, del, th.bgCardSelected, th.borderCardHover, 4);
        SelectObject(hdc, g_hFontNormal);
        SetTextColor(hdc, deleteHovered ? RGB(220, 38, 38) : (pink ? RGB(255, 255, 255) : th.textSecondary));
        DrawTextW(hdc, L"✕", -1, &del, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

bool RecurringView::HandleCommand(HWND owner, WPARAM wParam) {
    const int id = LOWORD(wParam);
    const int notification = HIWORD(wParam);
    if (id == IDC_RECURRING_COUNT_EDIT && notification == EN_CHANGE) {
        InvalidateForm();
        return true;
    }
    if (notification != BN_CLICKED) return false;
    if (id == IDC_RECURRING_BTN) {
        Open(owner);
    } else if (id == IDC_RECURRING_SAVE_BTN) {
        Save();
    } else if (id == IDC_RECURRING_CANCEL_BTN) {
        ResetForm(L"", true);
        Refresh();
        SetFocus(g_textEdit);
    } else if (id == IDC_RECURRING_UNIT_BTN) {
        ShowUnitMenu(owner);
    } else {
        return false;
    }
    return true;
}

bool RecurringView::HandleNotify(LPARAM lParam) {
    const auto* header = reinterpret_cast<const NMHDR*>(lParam);
    if (!header || header->hwndFrom != g_dateEdit) return false;
    if (header->code == DTN_DROPDOWN) {
        g_dropDownOpen = true;
        g_inContextMenu = true; // The calendar pop-up must not count as focus loss.
        ThemedDatePicker::StyleDropDown(g_dateEdit);
    } else if (header->code == DTN_CLOSEUP) {
        g_dropDownOpen = false;
        g_inContextMenu = false;
        SetFocus(g_dateEdit);
    } else if (header->code == DTN_DATETIMECHANGE) {
        InvalidateForm();
    } else {
        return false;
    }
    return true;
}

void RecurringView::HandleClick(HWND owner, int x, int rawY) {
    if (rawY < LIST_TOP) return;
    const POINT point = { x, rawY + g_scrollY };
    for (const Row& row : g_rows) {
        if (PtInRect(&row.deleteRect, point)) {
            DeleteRule(owner, row.uid);
            return;
        }
        if (PtInRect(&row.rect, point)) {
            if (const RecurringRule* rule = FindRule(row.uid)) LoadRule(*rule);
            return;
        }
    }
}

void RecurringView::HandleMouseMove(HWND owner, int x, int rawY) {
    int hoverRow = -1;
    bool hoverDelete = false;
    if (x >= 0 && rawY >= LIST_TOP) {
        const POINT point = { x, rawY + g_scrollY };
        for (size_t i = 0; i < g_rows.size(); ++i) {
            if (!PtInRect(&g_rows[i].rect, point)) continue;
            hoverRow = static_cast<int>(i);
            hoverDelete = PtInRect(&g_rows[i].deleteRect, point) != FALSE;
            break;
        }
    }
    if (hoverRow == g_hoverRow && hoverDelete == g_hoverDelete) return;
    g_hoverRow = hoverRow;
    g_hoverDelete = hoverDelete;
    InvalidateRect(owner, nullptr, FALSE);
}

bool RecurringView::HandleDrawItem(const DRAWITEMSTRUCT* dis) {
    if (dis->CtlID != IDC_RECURRING_BTN) return false;
    const ThemeColors th = GetThemeColors(g_themeMode);
    const bool pink = g_themeMode == ThemeMode::Pink;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool dark = g_themeMode == ThemeMode::Dark;
    const COLORREF restingBorder = g_themeMode == ThemeMode::Light ? RGB(148, 163, 184) : th.borderSep;
    HBRUSH surface = CreateSolidBrush(th.bgSubBar);
    FillRect(dis->hDC, &dis->rcItem, surface);
    DeleteObject(surface);
    // Dark theme: the same gray as the "+ Hozzáad" button next to it.
    const COLORREF background = pink ? RGB(183, 0, 99) : (dark ? RGB(60, 60, 60) : (pressed ? th.bgCardHover : th.bgCard));
    const COLORREF border = pink ? th.borderCard
        : (pressed ? th.borderCardHover : (dark ? RGB(82, 82, 82) : restingBorder));
    FillRoundRect(dis->hDC, dis->rcItem, background, border, 6);
    RECT icon = dis->rcItem;
    if (pressed) OffsetRect(&icon, 1, 1);
    DrawRepeatIcon(dis->hDC, icon, pink ? RGB(255, 255, 255) : (pressed ? th.textPrimary : th.textSecondary));
    if (dis->itemState & ODS_FOCUS) {
        RECT focus = dis->rcItem;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(dis->hDC, &focus);
    }
    return true;
}

void RecurringView::FocusForm() {
    if (g_pageVisible) SetFocus(g_textEdit);
}

bool RecurringView::IsDropDownOpen() {
    return g_dropDownOpen;
}

void RecurringView::TakeNewTasks(HWND owner) {
    if (g_store.newRecurringTaskIds.empty()) return;
    InsertOnTop(g_store.newRecurringTaskIds);
    g_store.newRecurringTaskIds.clear();
    if (!IsWindowVisible(owner)) return;
    if (g_isMiniMode) RecalculateMiniLayout();
    else ::RecalculateLayout();
    InvalidateRect(owner, nullptr, FALSE);
}
