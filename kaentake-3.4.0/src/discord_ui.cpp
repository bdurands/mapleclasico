// discord_ui.cpp  -- in-game Discord info window
// Follows the exact same patterns as CUIBagWindow in storagebag.cpp.
// Draws player info (name, level, job, map) + server invite link over the WZ background.

#include "discord_ui.h"
#include "discord.h"
#include "wvs/wnd.h"
#include "wvs/wndman.h"
#include "wvs/util.h"
#include "ztl/ztl.h"
#include <windows.h>
#include <string>
#include <cstdio>

// ---------------------------------------------------------------------------
// Window layout constants (matches your 300x400 UIWindow.img/DiscordUI/base image)
// ---------------------------------------------------------------------------
static constexpr int  kWndW   = 300;
static constexpr int  kWndH   = 400;
static constexpr int  kTitleH = 30;
static constexpr RECT kRcClose = { 268, 4, 296, 28 };

// Text positions inside the window (adjust to match your art layout)
static constexpr int kTextX       = 20;   // left margin for all text
static constexpr int kNameY       = 60;   // "Jugador: Nombre"
static constexpr int kLevelY      = 85;   // "Nivel: 10  Job: Beginners"
static constexpr int kMapY        = 110;  // "Mapa: Henesys"
static constexpr int kDividerY    = 145;  // separator line visual (just spacing)
static constexpr int kServerNameY = 170;  // "Servidor: EllinMS"
static constexpr int kInviteLabelY = 195; // "Unete al Discord:"
static constexpr int kInviteLinkY  = 220; // "discord.gg/n4myxhd3UC"

// Server invite link — hardcoded
static constexpr const char* kInviteLink   = "discord.gg/n4myxhd3UC";
static constexpr const char* kServerName   = "EllinMS";

// ---------------------------------------------------------------------------
// Font addresses — same as storagebag.cpp
// ---------------------------------------------------------------------------
static constexpr uintptr_t kAddr_SetFont = 0x0046341A;

// ---------------------------------------------------------------------------
// Load a WZ canvas — exact copy of LoadSprite() from storagebag.cpp
// ---------------------------------------------------------------------------
static IWzCanvasPtr Discord_LoadSprite(const wchar_t* p)
{
    IWzCanvasPtr c;
    try { c = get_unknown(get_rm()->GetObjectA(const_cast<wchar_t*>(p))); } catch (...) {}
    return c;
}

// Opaque blit — same as BlitAt() in storagebag.cpp
static void Discord_BlitAt(IWzCanvasPtr dst, IWzCanvasPtr src, int x, int y)
{
    if (dst && src)
        try { dst->CopyEx(x, y, src, CANVAS_ALPHATYPE::CA_REMOVEALPHA, 0, 0, 0, 0, 0, 0); }
        catch (...) {}
}

// Draw a line of text (char*) on the canvas using the basic font
static void Discord_DrawText(IWzCanvasPtr pCanvas, IWzFontPtr pFont, int x, int y, const char* text)
{
    if (!pCanvas || !pFont || !text) return;
    try {
        pCanvas->DrawTextA(x, y, Ztl_bstr_t(text), pFont, Ztl_variant_t(), Ztl_variant_t());
    } catch (...) {}
}

// ---------------------------------------------------------------------------
// CUIDiscordWindow
// ---------------------------------------------------------------------------
class CUIDiscordWindow : public CWnd
{
public:
    ZALLOC_GLOBAL
    inline static CUIDiscordWindow* ms_pInstance = nullptr;
    inline static CRTTI             ms_RTTI{ nullptr };

    int m_screenX      = 0;
    int m_screenY      = 0;
    int m_bDragging    = 0;
    int m_nDragAnchorX = 0;
    int m_nDragAnchorY = 0;
    int m_nCloseHover  = 0;
    int m_nClosePressed = 0;
    RECT m_rcClose{};

    IWzCanvasPtr m_pBg;     // UI/UIWindow.img/DiscordUI/base  (your 300x400 image)
    IWzFontPtr   m_pFont;   // basic game font for text

    // -----------------------------------------------------------------------
    CUIDiscordWindow(int nLeft, int nTop)
        : m_screenX(nLeft), m_screenY(nTop)
    {
        ms_pInstance = this;
        m_rcClose    = kRcClose;

        m_pBg   = Discord_LoadSprite(L"UI/UIWindow.img/DiscordUI/base");
        m_pFont = nullptr;
        try {
            PcCreateObject<IWzFontPtr>(L"Canvas#Font", m_pFont, nullptr);
            if (m_pFont) {
                HRESULT hr = reinterpret_cast<HRESULT(__thiscall*)(IWzFont*, Ztl_bstr_t, unsigned long,
                    unsigned long, const Ztl_variant_t&)>(kAddr_SetFont)(
                    m_pFont, L"Dotum", 11, 0xFF202020, Ztl_variant_t(L"")); // Dark grey text
                if (FAILED(hr)) m_pFont = nullptr;
            }
        } catch (...) { m_pFont = nullptr; }

        CWnd::CreateWnd(this, nLeft, nTop, kWndW, kWndH, 10, 1, nullptr, 0);
    }

    virtual ~CUIDiscordWindow() override
    {
        if (ms_pInstance == this) ms_pInstance = nullptr;
    }

    // -----------------------------------------------------------------------
    // Draw — background image + player info text + invite link
    // -----------------------------------------------------------------------
    virtual void Draw(const RECT* pRect) override
    {
        CWnd::Draw(pRect);
        IWzCanvasPtr pCanvas = GetCanvas();
        if (!pCanvas) return;

        // 1) Background image (your 300x400 Discord-themed art)
        Discord_BlitAt(pCanvas, m_pBg, 0, 0);

        // 2) Try to get font (retry each frame in case it failed at construct time)
        if (!m_pFont) {
            try {
                PcCreateObject<IWzFontPtr>(L"Canvas#Font", m_pFont, nullptr);
                if (m_pFont) {
                    HRESULT hr = reinterpret_cast<HRESULT(__thiscall*)(IWzFont*, Ztl_bstr_t, unsigned long,
                        unsigned long, const Ztl_variant_t&)>(kAddr_SetFont)(
                        m_pFont, L"Dotum", 11, 0xFF202020, Ztl_variant_t(L""));
                    if (FAILED(hr)) m_pFont = nullptr;
                }
            } catch (...) { m_pFont = nullptr; }
        }
        IWzFontPtr pf = m_pFont;

        // 3) Player info text
        char bufName[128], bufLevel[128], bufMap[128];

        if (!g_discord_playerName.empty()) {
            _snprintf_s(bufName,  sizeof(bufName),  _TRUNCATE, "Jugador: %s",
                        g_discord_playerName.c_str());
            _snprintf_s(bufLevel, sizeof(bufLevel), _TRUNCATE, "Nivel: %d  |  %s",
                        g_discord_level, g_discord_jobName.c_str());
            _snprintf_s(bufMap,   sizeof(bufMap),   _TRUNCATE, "Mapa: %s",
                        g_discord_mapName.c_str());
        } else {
            _snprintf_s(bufName,  sizeof(bufName),  _TRUNCATE, "Jugador: --");
            _snprintf_s(bufLevel, sizeof(bufLevel), _TRUNCATE, "Nivel: --");
            _snprintf_s(bufMap,   sizeof(bufMap),   _TRUNCATE, "Mapa: --");
        }

        if (pf) {
            Discord_DrawText(pCanvas, pf, kTextX, kNameY,        bufName);
            Discord_DrawText(pCanvas, pf, kTextX, kLevelY,       bufLevel);
            Discord_DrawText(pCanvas, pf, kTextX, kMapY,         bufMap);

            // 4) Server info + invite link
            Discord_DrawText(pCanvas, pf, kTextX, kServerNameY,   "Servidor: EllinMS");
            Discord_DrawText(pCanvas, pf, kTextX, kInviteLabelY,  "Unete al Discord:");
            Discord_DrawText(pCanvas, pf, kTextX, kInviteLinkY,   kInviteLink);
        }
    }

    // -----------------------------------------------------------------------
    virtual void OnMouseButton(unsigned int msg, unsigned int wParam, int rx, int ry) override
    {
        POINT pt{ rx, ry };
        if (msg == WM_LBUTTONDOWN) {
            if (PtInRect(&m_rcClose, pt)) {
                m_nClosePressed = 1; InvalidateRect(nullptr); return;
            }
            if (ry < kTitleH) {
                m_bDragging     = 1;
                m_nDragAnchorX  = m_screenX + rx;
                m_nDragAnchorY  = m_screenY + ry;
            }
        } else if (msg == WM_LBUTTONUP) {
            m_bDragging = 0;
            if (m_nClosePressed) {
                m_nClosePressed = 0;
                if (PtInRect(&m_rcClose, pt)) { Destroy(); return; }
                InvalidateRect(nullptr);
            }
        }
        CWnd::OnMouseButton(msg, wParam, rx, ry);
    }

    // -----------------------------------------------------------------------
    virtual int OnMouseMove(int rx, int ry) override
    {
        if (m_bDragging) {
            int absCursorX = m_screenX + rx;
            int absCursorY = m_screenY + ry;
            int newX = m_screenX + (absCursorX - m_nDragAnchorX);
            int newY = m_screenY + (absCursorY - m_nDragAnchorY);
            int sw = get_screen_width(), sh = get_screen_height();
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + kWndW > sw) newX = sw - kWndW;
            if (newY + kWndH > sh) newY = sh - kWndH;
            m_screenX = newX; m_screenY = newY;
            m_nDragAnchorX = absCursorX; m_nDragAnchorY = absCursorY;
            MoveWnd(newX, newY);
            return 1;
        }
        POINT pt{ rx, ry };
        int newHov = PtInRect(&m_rcClose, pt) ? 1 : 0;
        if (newHov != m_nCloseHover) { m_nCloseHover = newHov; InvalidateRect(nullptr); }
        return 1;
    }

    // -----------------------------------------------------------------------
    virtual void OnDestroy() override
    {
        m_pBg   = nullptr;
        m_pFont = nullptr;
        if (ms_pInstance == this) ms_pInstance = nullptr;
        CWnd::OnDestroy();
    }

    virtual void Update() override { InvalidateRect(nullptr); }
    virtual int  OnSetFocus(int /*bFocus*/) override { return 0; }
    virtual const CRTTI* GetRTTI() const override { return &ms_RTTI; }
    virtual int IsKindOf(const CRTTI* pRTTI) const override { return ms_RTTI.IsKindOf(pRTTI); }
};

// ---------------------------------------------------------------------------
void DiscordUI_Toggle()
{
    if (CUIDiscordWindow::ms_pInstance) {
        CUIDiscordWindow::ms_pInstance->Destroy();
        return;
    }
    int sw = get_screen_width(), sh = get_screen_height();
    int x = (sw - kWndW) / 2;
    int y = (sh - kWndH) / 2;
    if (x < 0) x = 0; if (y < 0) y = 0;
    new CUIDiscordWindow(x, y);
}
