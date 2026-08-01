// discord_ui.cpp  -- in-game Discord info window
// Follows the exact same patterns as CUIBagWindow in storagebag.cpp:
//   - Inherits CWnd (NOT CUIWnd which requires a UOL)
//   - Opens via CWnd::CreateWnd
//   - Draws via Draw(const RECT*) override
//   - Mouse drag handled via OnMouseButton(msg, wParam, rx, ry) + OnMouseMove(rx, ry)
//   - Sprites loaded via get_rm()->GetObjectA + get_unknown  (same as LoadSprite in storagebag.cpp)

#include "discord_ui.h"
#include "wvs/wnd.h"
#include "wvs/wndman.h"
#include "wvs/util.h"
#include <windows.h>

// ---------------------------------------------------------------------------
// Window layout constants (must match your 300x400 image in UIWindow.img/DiscordUI/base)
// ---------------------------------------------------------------------------
static constexpr int kWndW   = 300;
static constexpr int kWndH   = 400;
static constexpr int kTitleH = 30;   // top 30 px = drag bar
// X button area: top-right corner of the image
static constexpr RECT kRcClose = { 268, 4, 296, 28 };

// ---------------------------------------------------------------------------
// Load a WZ canvas — mirrors LoadSprite() from storagebag.cpp exactly
// ---------------------------------------------------------------------------
static IWzCanvasPtr Discord_LoadSprite(const wchar_t* p)
{
    IWzCanvasPtr c;
    try {
        Ztl_variant_t v = get_rm()->GetObjectA(const_cast<wchar_t*>(p));
        IUnknownPtr pUnk = get_unknown(v);
        if (pUnk) pUnk->QueryInterface(__uuidof(IWzCanvas), reinterpret_cast<void**>(&c));
    } catch (...) {}
    return c;
}

// ---------------------------------------------------------------------------
// Opaque blit — mirrors BlitAt() in storagebag.cpp
// ---------------------------------------------------------------------------
static void Discord_BlitAt(IWzCanvasPtr dst, IWzCanvasPtr src, int x, int y)
{
    if (dst && src)
        try { dst->CopyEx(x, y, src, CANVAS_ALPHATYPE::CA_REMOVEALPHA, 0, 0, 0, 0, 0, 0); }
        catch (...) {}
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

    int m_screenX     = 0;
    int m_screenY     = 0;
    int m_bDragging   = 0;
    int m_nDragAnchorX = 0;
    int m_nDragAnchorY = 0;
    int m_nCloseHover  = 0;
    int m_nClosePressed = 0;
    RECT m_rcClose{};

    IWzCanvasPtr m_pBg;   // UI/UIWindow.img/DiscordUI/base

    // -----------------------------------------------------------------------
    CUIDiscordWindow(int nLeft, int nTop)
        : m_screenX(nLeft), m_screenY(nTop)
    {
        ms_pInstance = this;
        m_rcClose    = kRcClose;

        m_pBg = Discord_LoadSprite(L"UI/UIWindow.img/DiscordUI/base");

        // Same call pattern as CUIBagWindow constructor (storagebag.cpp line 1027)
        CWnd::CreateWnd(this, nLeft, nTop, kWndW, kWndH, 10, 1, nullptr, 0);
    }

    virtual ~CUIDiscordWindow() override
    {
        if (ms_pInstance == this) ms_pInstance = nullptr;
    }

    // -----------------------------------------------------------------------
    // Draw — mirrors CUIBagWindow::Draw
    // -----------------------------------------------------------------------
    virtual void Draw(const RECT* pRect) override
    {
        CWnd::Draw(pRect);
        IWzCanvasPtr pCanvas = GetCanvas();
        if (!pCanvas) return;

        // Draw full background image (your 300x400 art with X and title already painted on it)
        Discord_BlitAt(pCanvas, m_pBg, 0, 0);
    }

    // -----------------------------------------------------------------------
    // Mouse button — same signature as CUIBagWindow::OnMouseButton
    // -----------------------------------------------------------------------
    virtual void OnMouseButton(unsigned int msg, unsigned int wParam, int rx, int ry) override
    {
        POINT pt{ rx, ry };

        if (msg == WM_LBUTTONDOWN) {
            if (PtInRect(&m_rcClose, pt)) {
                m_nClosePressed = 1;
                InvalidateRect(nullptr);
                return;
            }
            // Title-bar drag — anchor = absolute cursor position (same pattern as storagebag.cpp line 1235)
            if (ry < kTitleH) {
                m_bDragging     = 1;
                m_nDragAnchorX  = m_screenX + rx;
                m_nDragAnchorY  = m_screenY + ry;
            }
        }
        else if (msg == WM_LBUTTONUP) {
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
    // Mouse move — window drag + close-button hover
    // Mirrors CUIBagWindow::OnMouseMove for the drag part
    // -----------------------------------------------------------------------
    virtual int OnMouseMove(int rx, int ry) override
    {
        if (m_bDragging) {
            int absCursorX = m_screenX + rx;
            int absCursorY = m_screenY + ry;
            int dx = absCursorX - m_nDragAnchorX;
            int dy = absCursorY - m_nDragAnchorY;
            int newX = m_screenX + dx;
            int newY = m_screenY + dy;
            // Clamp to screen edges
            int sw = get_screen_width(), sh = get_screen_height();
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX + kWndW > sw) newX = sw - kWndW;
            if (newY + kWndH > sh) newY = sh - kWndH;
            m_screenX       = newX;
            m_screenY       = newY;
            m_nDragAnchorX  = absCursorX;
            m_nDragAnchorY  = absCursorY;
            MoveWnd(newX, newY);
            return 1;
        }

        // Hover state on close button
        POINT pt{ rx, ry };
        int newHov = PtInRect(&m_rcClose, pt) ? 1 : 0;
        if (newHov != m_nCloseHover) {
            m_nCloseHover = newHov;
            InvalidateRect(nullptr);
        }
        return 1;
    }

    // -----------------------------------------------------------------------
    virtual void OnDestroy() override
    {
        m_pBg = nullptr;
        if (ms_pInstance == this) ms_pInstance = nullptr;
        CWnd::OnDestroy();
    }

    virtual void Update() override
    {
        // Keep window alive; InvalidateRect so it redraws every frame
        InvalidateRect(nullptr);
    }

    virtual int OnSetFocus(int /*bFocus*/) override { return 0; }

    virtual const CRTTI* GetRTTI() const override { return &ms_RTTI; }
    virtual int IsKindOf(const CRTTI* pRTTI) const override { return ms_RTTI.IsKindOf(pRTTI); }
};

// ---------------------------------------------------------------------------
// Public toggle (called from damageskin.cpp on opcode 0x3727)
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
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    new CUIDiscordWindow(x, y);
}
