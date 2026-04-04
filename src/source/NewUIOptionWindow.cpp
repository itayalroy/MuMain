// NewUIOptionWindow.cpp: implementation of the CNewUIOptionWindow class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NewUIOptionWindow.h"
#include "NewUISystem.h"
#include "GameConfig/GameConfig.h"
#include "RuntimeResolution.h"
#include "ZzzTexture.h"
#include "DSPlaySound.h"

#include <algorithm>
#include <cmath>

using namespace SEASON3B;

namespace
{
    constexpr int OptionWindowWidth = 190;
    constexpr int OptionWindowHeight = 349;
    constexpr int OptionWindowSideSegmentCount = 24;
    constexpr int CloseButtonYOffset = 309;
    constexpr int BorderlessLabelYOffset = 236;
    constexpr int ResolutionLabelYOffset = 258;
    constexpr int ResolutionBoxXOffset = 25;
    constexpr int ResolutionBoxYOffset = 270;
    constexpr int ResolutionBoxWidth = 141;
    constexpr int ResolutionBoxHeight = 29;
    constexpr int ResolutionTextYOffset = 278;
    constexpr int UiBaseWidth = 640;
    constexpr int UiBaseHeight = 480;
    constexpr int ResolutionPopupWidth = 220;
    constexpr int ResolutionPopupHeight = 219;
    constexpr int ResolutionPopupSideSegmentCount = 11;
    constexpr int ResolutionPopupListXOffset = 20;
    constexpr int ResolutionPopupListYOffset = 52;
    constexpr int ResolutionPopupListWidth = 180;
    constexpr int ResolutionPopupItemHeight = 18;
    constexpr int ResolutionPopupVisibleItems = 8;
    constexpr int ResolutionPopupHintYOffset = 193;

    std::wstring FormatResolutionLabel(unsigned int width, unsigned int height)
    {
        wchar_t label[32] = {};
        _snwprintf_s(label, _countof(label), _TRUNCATE, L"%ux%u", width, height);
        return label;
    }

    std::wstring FormatCurrentResolutionLabel(unsigned int width, unsigned int height, bool isSelectable)
    {
        wchar_t label[64] = {};
        _snwprintf_s(
            label,
            _countof(label),
            _TRUNCATE,
            isSelectable ? L"Current: %ux%u" : L"Current: %ux%u (unsupported)",
            width,
            height);
        return label;
    }

    POINT GetResolutionPopupPosition()
    {
        POINT popupPosition = {};
        popupPosition.x = (UiBaseWidth - ResolutionPopupWidth) / 2;
        popupPosition.y = (UiBaseHeight - ResolutionPopupHeight) / 2;
        return popupPosition;
    }

    void RenderFlatGrayPanel(int x, int y, int width, int height)
    {
        EnableAlphaTest();

        glColor4f(0.14f, 0.14f, 0.14f, 0.92f);
        RenderColor(static_cast<float>(x + 1), static_cast<float>(y + 1), static_cast<float>(width - 2), static_cast<float>(height - 2));

        glColor4f(0.38f, 0.38f, 0.38f, 1.0f);
        RenderColor(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), 1.0f);
        RenderColor(static_cast<float>(x), static_cast<float>(y + height - 1), static_cast<float>(width), 1.0f);
        RenderColor(static_cast<float>(x), static_cast<float>(y), 1.0f, static_cast<float>(height));
        RenderColor(static_cast<float>(x + width - 1), static_cast<float>(y), 1.0f, static_cast<float>(height));

        EndRenderColor();
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void RenderWindowFrame(float x, float y, float width, int sideSegments, int imgUp, int imgLeft, int imgRight, int imgDown)
    {
        RenderImage(imgUp, x, y, width, 64.f);
        y += 64.f;
        for (int i = 0; i < sideSegments; ++i)
        {
            RenderImage(imgLeft, x, y, 21.f, 10.f);
            RenderImage(imgRight, x + width - 21.f, y, 21.f, 10.f);
            y += 10.f;
        }
        RenderImage(imgDown, x, y, width, 45.f);
    }
}

extern BOOL g_bUseWindowMode;
extern BOOL g_bUseBorderlessMode;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SEASON3B::CNewUIOptionWindow::CNewUIOptionWindow()
{
    m_pNewUIMng = NULL;
    m_Pos.x = 0;
    m_Pos.y = 0;

    m_bAutoAttack = true;
    m_bWhisperSound = false;
    m_bSlideHelp = true;
    m_iVolumeLevel = 0;
    m_iRenderLevel = 4;
    m_bRenderAllEffects = true;
    m_bBorderlessMode = false;
    m_iSelectedResolutionIndex = -1;
    m_iResolutionPopupScroll = 0;
    m_bResolutionPopupOpen = false;
}

SEASON3B::CNewUIOptionWindow::~CNewUIOptionWindow()
{
    Release();
}

bool SEASON3B::CNewUIOptionWindow::Create(CNewUIManager* pNewUIMng, int x, int y)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_OPTION, this);
    SetPos(x, y);
    LoadImages();
    SetButtonInfo();
    Show(false);
    return true;
}

void SEASON3B::CNewUIOptionWindow::SetButtonInfo()
{
    m_BtnClose.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnClose.ChangeButtonImgState(true, IMAGE_OPTION_BTN_CLOSE, true);
    m_BtnClose.ChangeButtonInfo(m_Pos.x + 68, m_Pos.y + CloseButtonYOffset, 54, 30);
    m_BtnClose.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnClose.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
}

void SEASON3B::CNewUIOptionWindow::Release()
{
    UnloadImages();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void SEASON3B::CNewUIOptionWindow::SetPos(int x, int y)
{
    m_Pos.x = x;
    m_Pos.y = y;
}

bool SEASON3B::CNewUIOptionWindow::UpdateMouseEvent()
{
    if (m_bResolutionPopupOpen)
    {
        const POINT popupPosition = GetResolutionPopupPosition();
        const int popupX = popupPosition.x;
        const int popupY = popupPosition.y;
        const int listX = popupX + ResolutionPopupListXOffset;
        const int listY = popupY + ResolutionPopupListYOffset;
        const int listHeight = GetResolutionPopupHeight();
        const bool isMouseOverPopup = CheckMouseIn(popupX, popupY, ResolutionPopupWidth, ResolutionPopupHeight);
        const bool isMouseOverList = listHeight > 0 && CheckMouseIn(listX, listY, ResolutionPopupListWidth, listHeight);

        if (isMouseOverPopup && MouseWheel != 0)
        {
            m_iResolutionPopupScroll += (MouseWheel < 0) ? 1 : -1;
            ClampResolutionPopupScroll();
            MouseWheel = 0;
            return false;
        }

        if (SEASON3B::IsRelease(VK_LBUTTON))
        {
            if (isMouseOverList)
            {
                const int relativeMouseY = MouseY - listY;
                const int clickedIndex = m_iResolutionPopupScroll + (relativeMouseY / ResolutionPopupItemHeight);
                const bool clickedSelectedResolution = clickedIndex == m_iSelectedResolutionIndex;
                if (ApplyResolutionSelection(clickedIndex) || clickedSelectedResolution)
                {
                    m_bResolutionPopupOpen = false;
                }

                return false;
            }

            if (!isMouseOverPopup)
            {
                m_bResolutionPopupOpen = false;
            }

            return false;
        }

        return false;
    }

    if (m_BtnClose.UpdateMouseEvent() == true)
    {
        g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
        return false;
    }

    if (!m_ResolutionOptions.empty())
    {
        if (SEASON3B::IsRelease(VK_LBUTTON) && CheckMouseIn(
            m_Pos.x + ResolutionBoxXOffset,
            m_Pos.y + ResolutionBoxYOffset,
            ResolutionBoxWidth,
            ResolutionBoxHeight))
        {
            ToggleResolutionPopup();
            return false;
        }
    }

    if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x + 150, m_Pos.y + 43, 15, 15))
    {
        m_bAutoAttack = !m_bAutoAttack;
    }
    if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x + 150, m_Pos.y + 65, 15, 15))
    {
        m_bWhisperSound = !m_bWhisperSound;
    }
    if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x + 150, m_Pos.y + 127, 15, 15))
    {
        m_bSlideHelp = !m_bSlideHelp;
    }

    if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x + 150, m_Pos.y + 210, 15, 15))
    {
        m_bRenderAllEffects = !m_bRenderAllEffects;
    }

    if (SEASON3B::IsPress(VK_LBUTTON) && CheckMouseIn(m_Pos.x + 150, m_Pos.y + 231, 15, 15))
    {
        if (RuntimeResolution::QueueBorderlessModeChange(!m_bBorderlessMode))
        {
            RebuildResolutionList();
            m_bResolutionPopupOpen = false;
        }

        return false;
    }

    if (CheckMouseIn(m_Pos.x + 33 - 8, m_Pos.y + 104, 124 + 8, 16))
    {
        int iOldValue = m_iVolumeLevel;
        if (MouseWheel > 0)
        {
            MouseWheel = 0;
            m_iVolumeLevel++;
            if (m_iVolumeLevel > 10)
            {
                m_iVolumeLevel = 10;
            }
        }
        else if (MouseWheel < 0)
        {
            MouseWheel = 0;
            m_iVolumeLevel--;
            if (m_iVolumeLevel < 0)
            {
                m_iVolumeLevel = 0;
            }
        }
        if (SEASON3B::IsRepeat(VK_LBUTTON))
        {
            int x = MouseX - (m_Pos.x + 33);
            if (x < 0)
            {
                m_iVolumeLevel = 0;
            }
            else
            {
                float fValue = (10.f * x) / 124.f;
                m_iVolumeLevel = (int)fValue + 1;
            }
        }

        if (iOldValue != m_iVolumeLevel)
        {
            SetEffectVolumeLevel(m_iVolumeLevel);
        }
    }
    if (CheckMouseIn(m_Pos.x + 25, m_Pos.y + 168, 141, 29))
    {
        if (SEASON3B::IsRepeat(VK_LBUTTON))
        {
            int x = MouseX - (m_Pos.x + 25);
            float fValue = (5.f * x) / 141.f;
            m_iRenderLevel = (int)fValue;
        }
    }

    if (CheckMouseIn(m_Pos.x, m_Pos.y, OptionWindowWidth, OptionWindowHeight) == true)
    {
        return false;
    }

    return true;
}

bool SEASON3B::CNewUIOptionWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION) == true)
    {
        if (SEASON3B::IsPress(VK_ESCAPE) == true)
        {
            if (m_bResolutionPopupOpen)
            {
                m_bResolutionPopupOpen = false;
                PlayBuffer(SOUND_CLICK01);
                return false;
            }

            g_pNewUISystem->Hide(SEASON3B::INTERFACE_OPTION);
            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    return true;
}

bool SEASON3B::CNewUIOptionWindow::Update()
{
    return true;
}

bool SEASON3B::CNewUIOptionWindow::Render()
{
    EnableAlphaTest();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RenderFrame();
    RenderContents();
    RenderButtons();
    if (m_bResolutionPopupOpen)
    {
        RenderResolutionPopup();
    }
    DisableAlphaBlend();
    return true;
}

float SEASON3B::CNewUIOptionWindow::GetLayerDepth()	//. 10.5f
{
    return 10.5f;
}

float SEASON3B::CNewUIOptionWindow::GetKeyEventOrder()	// 10.f;
{
    return 10.0f;
}

void SEASON3B::CNewUIOptionWindow::OpenningProcess()
{
    RebuildResolutionList();
    m_bResolutionPopupOpen = false;
}

void SEASON3B::CNewUIOptionWindow::ClosingProcess()
{
    m_bResolutionPopupOpen = false;
}

void SEASON3B::CNewUIOptionWindow::RebuildResolutionList()
{
    m_bBorderlessMode = g_bUseBorderlessMode == TRUE;
    BuildResolutionList();
    SyncResolutionSelection();
    ClampResolutionPopupScroll();
}

void SEASON3B::CNewUIOptionWindow::LoadImages()
{
    LoadBitmap(L"Interface\\newui_button_close.tga", IMAGE_OPTION_BTN_CLOSE, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_msgbox_back.jpg", IMAGE_OPTION_FRAME_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_item_back03.tga", IMAGE_OPTION_FRAME_DOWN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_top.tga", IMAGE_OPTION_FRAME_UP, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_back06(L).tga", IMAGE_OPTION_FRAME_LEFT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_back06(R).tga", IMAGE_OPTION_FRAME_RIGHT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_line.jpg", IMAGE_OPTION_LINE, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_point.tga", IMAGE_OPTION_POINT, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_check.tga", IMAGE_OPTION_BTN_CHECK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_effect03.tga", IMAGE_OPTION_EFFECT_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_effect04.tga", IMAGE_OPTION_EFFECT_COLOR, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_volume01.tga", IMAGE_OPTION_VOLUME_BACK, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_option_volume02.tga", IMAGE_OPTION_VOLUME_COLOR, GL_LINEAR);
}

void SEASON3B::CNewUIOptionWindow::UnloadImages()
{
    DeleteBitmap(IMAGE_OPTION_BTN_CLOSE);
    DeleteBitmap(IMAGE_OPTION_FRAME_BACK);
    DeleteBitmap(IMAGE_OPTION_FRAME_DOWN);
    DeleteBitmap(IMAGE_OPTION_FRAME_UP);
    DeleteBitmap(IMAGE_OPTION_FRAME_LEFT);
    DeleteBitmap(IMAGE_OPTION_FRAME_RIGHT);
    DeleteBitmap(IMAGE_OPTION_LINE);
    DeleteBitmap(IMAGE_OPTION_POINT);
    DeleteBitmap(IMAGE_OPTION_BTN_CHECK);
    DeleteBitmap(IMAGE_OPTION_EFFECT_BACK);
    DeleteBitmap(IMAGE_OPTION_EFFECT_COLOR);
    DeleteBitmap(IMAGE_OPTION_VOLUME_BACK);
    DeleteBitmap(IMAGE_OPTION_VOLUME_COLOR);
}

void SEASON3B::CNewUIOptionWindow::RenderFrame()
{
    float x, y;
    x = m_Pos.x;
    y = m_Pos.y;
    RenderImage(IMAGE_OPTION_FRAME_BACK, x, y, 190.f, static_cast<float>(OptionWindowHeight));
    RenderWindowFrame(x, y, 190.f, OptionWindowSideSegmentCount, IMAGE_OPTION_FRAME_UP, IMAGE_OPTION_FRAME_LEFT, IMAGE_OPTION_FRAME_RIGHT, IMAGE_OPTION_FRAME_DOWN);

    y = m_Pos.y + 60.f;
    RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
    y += 40.f;
    RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);

    y += 60.f;
    RenderImage(IMAGE_OPTION_LINE, x + 18, y, 154.f, 2.f);
}

void SEASON3B::CNewUIOptionWindow::RenderContents()
{
    float x, y;
    x = m_Pos.x + 20.f;
    y = m_Pos.y + 46.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 40.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);

    y += 60.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);
    y += 22.f;
    RenderImage(IMAGE_OPTION_POINT, x, y, 10.f, 10.f);

    g_pRenderText->SetFont(g_hFont);
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    g_pRenderText->SetBgColor(0);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 48, GlobalText[386]);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 70, GlobalText[387]);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 92, GlobalText[389]);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 132, GlobalText[919]);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 154, GlobalText[1840]);
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + 154+60, L"Render Full Effects");
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + BorderlessLabelYOffset, L"Borderless");
    RenderResolutionSelector();
}

void SEASON3B::CNewUIOptionWindow::RenderButtons()
{
    m_BtnClose.Render();

    if (m_bAutoAttack)
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 43, 15, 15, 0, 0);
    }
    else
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 43, 15, 15, 0, 15.f);
    }

    if (m_bWhisperSound)
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 65, 15, 15, 0, 0);
    }
    else
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 65, 15, 15, 0, 15.f);
    }

    if (m_bSlideHelp)
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 127, 15, 15, 0, 0);
    }
    else
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 127, 15, 15, 0, 15.f);
    }

    RenderImage(IMAGE_OPTION_VOLUME_BACK, m_Pos.x + 33, m_Pos.y + 104, 124.f, 16.f);
    if (m_iVolumeLevel > 0)
    {
        RenderImage(IMAGE_OPTION_VOLUME_COLOR, m_Pos.x + 33, m_Pos.y + 104, 124.f * 0.1f * (m_iVolumeLevel), 16.f);
    }

    RenderImage(IMAGE_OPTION_EFFECT_BACK, m_Pos.x + 25, m_Pos.y + 168, 141.f, 29.f);
    if (m_iRenderLevel >= 0)
    {
        RenderImage(IMAGE_OPTION_EFFECT_COLOR, m_Pos.x + 25, m_Pos.y + 168, 141.f * 0.2f * (m_iRenderLevel + 1), 29.f);
    }

    if (m_bRenderAllEffects)
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 210, 15, 15, 0, 0);
    }
    else
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 210, 15, 15, 0, 15.f);
    }

    if (m_bBorderlessMode)
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 231, 15, 15, 0, 0);
    }
    else
    {
        RenderImage(IMAGE_OPTION_BTN_CHECK, m_Pos.x + 150, m_Pos.y + 231, 15, 15, 0, 15.f);
    }
}

void SEASON3B::CNewUIOptionWindow::SetAutoAttack(bool bAuto)
{
    m_bAutoAttack = bAuto;
}

bool SEASON3B::CNewUIOptionWindow::IsAutoAttack()
{
    return m_bAutoAttack;
}

void SEASON3B::CNewUIOptionWindow::SetWhisperSound(bool bSound)
{
    m_bWhisperSound = bSound;
}

bool SEASON3B::CNewUIOptionWindow::IsWhisperSound()
{
    return m_bWhisperSound;
}

void SEASON3B::CNewUIOptionWindow::SetSlideHelp(bool bHelp)
{
    m_bSlideHelp = bHelp;
}

bool SEASON3B::CNewUIOptionWindow::IsSlideHelp()
{
    return m_bSlideHelp;
}

void SEASON3B::CNewUIOptionWindow::SetVolumeLevel(int iVolume)
{
    m_iVolumeLevel = iVolume;
}

int SEASON3B::CNewUIOptionWindow::GetVolumeLevel()
{
    return m_iVolumeLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderLevel(int iRender)
{
    m_iRenderLevel = iRender;
}

int SEASON3B::CNewUIOptionWindow::GetRenderLevel()
{
    return m_iRenderLevel;
}

void SEASON3B::CNewUIOptionWindow::SetRenderAllEffects(bool bRenderAllEffects)
{
    m_bRenderAllEffects = bRenderAllEffects;
}

bool SEASON3B::CNewUIOptionWindow::GetRenderAllEffects()
{
    return m_bRenderAllEffects;
}

void SEASON3B::CNewUIOptionWindow::RenderResolutionSelector()
{
    g_pRenderText->RenderText(m_Pos.x + 40, m_Pos.y + ResolutionLabelYOffset, L"Resolution");
    RenderFlatGrayPanel(m_Pos.x + ResolutionBoxXOffset, m_Pos.y + ResolutionBoxYOffset, ResolutionBoxWidth, ResolutionBoxHeight);

    const bool hasSelection = m_iSelectedResolutionIndex >= 0
        && m_iSelectedResolutionIndex < static_cast<int>(m_ResolutionOptions.size());

    const std::wstring selectionLabel = hasSelection ? m_ResolutionOptions[m_iSelectedResolutionIndex].Label : L"N/A";
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    g_pRenderText->RenderText(m_Pos.x + ResolutionBoxXOffset + 4, m_Pos.y + ResolutionTextYOffset, selectionLabel.c_str(), ResolutionBoxWidth - 22, 0, RT3_SORT_CENTER);

    g_pRenderText->RenderText(
        m_Pos.x + ResolutionBoxXOffset + ResolutionBoxWidth - 16,
        m_Pos.y + ResolutionTextYOffset,
        L"v");
}

void SEASON3B::CNewUIOptionWindow::RenderResolutionPopup()
{
    const int visibleResolutionCount = GetVisibleResolutionCount();
    if (visibleResolutionCount <= 0)
    {
        return;
    }

    const POINT popupPosition = GetResolutionPopupPosition();
    const float popupX = static_cast<float>(popupPosition.x);
    const float popupY = static_cast<float>(popupPosition.y);

    RenderImage(IMAGE_OPTION_FRAME_BACK, popupX, popupY, static_cast<float>(ResolutionPopupWidth), static_cast<float>(ResolutionPopupHeight));
    RenderWindowFrame(popupX, popupY, static_cast<float>(ResolutionPopupWidth), ResolutionPopupSideSegmentCount, IMAGE_OPTION_FRAME_UP, IMAGE_OPTION_FRAME_LEFT, IMAGE_OPTION_FRAME_RIGHT, IMAGE_OPTION_FRAME_DOWN);

    RenderImage(IMAGE_OPTION_LINE, popupX + 18.f, popupY + 40.f, static_cast<float>(ResolutionPopupWidth - 36), 2.f);
    RenderImage(IMAGE_OPTION_LINE, popupX + 18.f, popupY + 182.f, static_cast<float>(ResolutionPopupWidth - 36), 2.f);

    const int listX = popupPosition.x + ResolutionPopupListXOffset;
    const int listY = popupPosition.y + ResolutionPopupListYOffset;
    const int listHeight = GetResolutionPopupHeight();
    RenderFlatGrayPanel(listX, listY, ResolutionPopupListWidth, listHeight);

    g_pRenderText->SetBgColor(0);
    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetTextColor(255, 255, 255, 255);
    g_pRenderText->RenderText(popupPosition.x, popupPosition.y + 16, L"Choose Resolution", ResolutionPopupWidth, 0, RT3_SORT_CENTER);

    g_pRenderText->SetFont(g_hFont);
    for (int visibleIndex = 0; visibleIndex < visibleResolutionCount; ++visibleIndex)
    {
        const int resolutionIndex = m_iResolutionPopupScroll + visibleIndex;
        if (resolutionIndex < 0 || resolutionIndex >= static_cast<int>(m_ResolutionOptions.size()))
        {
            continue;
        }

        const ResolutionOption& option = m_ResolutionOptions[resolutionIndex];

        const int itemY = listY + (visibleIndex * ResolutionPopupItemHeight);
        if (resolutionIndex == m_iSelectedResolutionIndex)
        {
            glColor4f(0.72f, 0.72f, 0.72f, 0.45f);
            RenderColor(static_cast<float>(listX + 2), static_cast<float>(itemY + 1), static_cast<float>(ResolutionPopupListWidth - 4), static_cast<float>(ResolutionPopupItemHeight - 2));
            EndRenderColor();
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (!option.IsSelectable)
        {
            g_pRenderText->SetTextColor(185, 185, 185, 255);
        }
        else if (option.IsCurrentEntry)
        {
            g_pRenderText->SetTextColor(255, 226, 170, 255);
        }
        else
        {
            g_pRenderText->SetTextColor(255, 255, 255, 255);
        }

        g_pRenderText->RenderText(
            listX + 4,
            itemY + 3,
            option.Label.c_str(),
            ResolutionPopupListWidth - 8,
            0,
            RT3_SORT_CENTER);
    }

    if (m_iResolutionPopupScroll > 0)
    {
        g_pRenderText->SetTextColor(200, 200, 200, 255);
        g_pRenderText->RenderText(listX, listY - 14, L"^", ResolutionPopupListWidth, 0, RT3_SORT_CENTER);
    }

    if (m_iResolutionPopupScroll + visibleResolutionCount < static_cast<int>(m_ResolutionOptions.size()))
    {
        g_pRenderText->SetTextColor(200, 200, 200, 255);
        g_pRenderText->RenderText(listX, listY + listHeight + 2, L"v", ResolutionPopupListWidth, 0, RT3_SORT_CENTER);
    }

    g_pRenderText->SetTextColor(180, 180, 180, 255);
    g_pRenderText->RenderText(
        popupPosition.x,
        popupPosition.y + ResolutionPopupHintYOffset,
        L"Click to apply, ESC to close",
        ResolutionPopupWidth,
        0,
        RT3_SORT_CENTER);
}

void SEASON3B::CNewUIOptionWindow::BuildResolutionList()
{
    m_ResolutionOptions.clear();
    m_iResolutionPopupScroll = 0;

    const unsigned int currentWidth = static_cast<unsigned int>(GameConfig::GetInstance().GetWindowWidth());
    const unsigned int currentHeight = static_cast<unsigned int>(GameConfig::GetInstance().GetWindowHeight());

    ResolutionOption currentOption = {};
    currentOption.Width = currentWidth;
    currentOption.Height = currentHeight;
    currentOption.IsCurrentEntry = true;
    currentOption.IsSelectable = RuntimeResolution::CanSelectForCurrentMode(currentWidth, currentHeight);
    currentOption.Label = FormatCurrentResolutionLabel(currentWidth, currentHeight, currentOption.IsSelectable);
    m_ResolutionOptions.push_back(currentOption);

    std::vector<ResolutionOption> selectableOptions;

    auto addResolution = [&selectableOptions, currentWidth, currentHeight](unsigned int width, unsigned int height)
    {
        if (!RuntimeResolution::CanSelectForCurrentMode(width, height))
        {
            return;
        }

        if (width == currentWidth && height == currentHeight)
        {
            return;
        }

        const bool alreadyAdded = std::find_if(
            selectableOptions.begin(),
            selectableOptions.end(),
            [width, height](const ResolutionOption& option)
            {
                return option.Width == width && option.Height == height;
            }) != selectableOptions.end();

        if (alreadyAdded)
        {
            return;
        }

        ResolutionOption option = {};
        option.Width = width;
        option.Height = height;
        option.Label = FormatResolutionLabel(width, height);
        option.IsCurrentEntry = false;
        option.IsSelectable = true;
        selectableOptions.push_back(option);
    };

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    for (DWORD index = 0; ::EnumDisplaySettingsW(nullptr, index, &mode) != FALSE; ++index)
    {
        addResolution(static_cast<unsigned int>(mode.dmPelsWidth), static_cast<unsigned int>(mode.dmPelsHeight));
        mode.dmSize = sizeof(mode);
    }

    std::sort(selectableOptions.begin(), selectableOptions.end(),
        [](const ResolutionOption& left, const ResolutionOption& right)
        {
            const unsigned long long leftArea = static_cast<unsigned long long>(left.Width) * static_cast<unsigned long long>(left.Height);
            const unsigned long long rightArea = static_cast<unsigned long long>(right.Width) * static_cast<unsigned long long>(right.Height);
            if (leftArea != rightArea)
            {
                return leftArea < rightArea;
            }

            if (left.Width != right.Width)
            {
                return left.Width < right.Width;
            }

            return left.Height < right.Height;
        });

    m_ResolutionOptions.insert(m_ResolutionOptions.end(), selectableOptions.begin(), selectableOptions.end());
}

void SEASON3B::CNewUIOptionWindow::SyncResolutionSelection()
{
    const RuntimeResolutionPendingChange pendingChange = RuntimeResolution::GetPendingChange();
    if (pendingChange.HasPendingChange)
    {
        m_iSelectedResolutionIndex = FindResolutionIndex(pendingChange.RequestedWidth, pendingChange.RequestedHeight);
    }
    else
    {
        m_iSelectedResolutionIndex = FindResolutionIndex(
            static_cast<unsigned int>(GameConfig::GetInstance().GetWindowWidth()),
            static_cast<unsigned int>(GameConfig::GetInstance().GetWindowHeight()));
    }

    if (m_iSelectedResolutionIndex < 0 && !m_ResolutionOptions.empty())
    {
        m_iSelectedResolutionIndex = 0;
    }

    EnsureResolutionSelectionVisible();
}

bool SEASON3B::CNewUIOptionWindow::ApplyResolutionSelection(int index)
{
    if (index < 0 || index >= static_cast<int>(m_ResolutionOptions.size()))
    {
        return false;
    }

    const ResolutionOption& resolution = m_ResolutionOptions[index];
    if (!resolution.IsSelectable)
    {
        return false;
    }

    if (index == m_iSelectedResolutionIndex)
    {
        return false;
    }

    const int previousIndex = m_iSelectedResolutionIndex;
    m_iSelectedResolutionIndex = index;

    if (!RuntimeResolution::QueueResolutionChange(resolution.Width, resolution.Height))
    {
        m_iSelectedResolutionIndex = previousIndex;
        return false;
    }

    EnsureResolutionSelectionVisible();
    return true;
}

int SEASON3B::CNewUIOptionWindow::FindResolutionIndex(unsigned int width, unsigned int height) const
{
    for (size_t index = 0; index < m_ResolutionOptions.size(); ++index)
    {
        const ResolutionOption& option = m_ResolutionOptions[index];
        if (option.Width == width && option.Height == height)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int SEASON3B::CNewUIOptionWindow::GetVisibleResolutionCount() const
{
    return std::min<int>(static_cast<int>(m_ResolutionOptions.size()), ResolutionPopupVisibleItems);
}

int SEASON3B::CNewUIOptionWindow::GetResolutionPopupHeight() const
{
    return GetVisibleResolutionCount() * ResolutionPopupItemHeight;
}

void SEASON3B::CNewUIOptionWindow::ClampResolutionPopupScroll()
{
    const int visibleResolutionCount = GetVisibleResolutionCount();
    const int maxScroll = std::max(0, static_cast<int>(m_ResolutionOptions.size()) - visibleResolutionCount);

    if (m_iResolutionPopupScroll < 0)
    {
        m_iResolutionPopupScroll = 0;
    }
    else if (m_iResolutionPopupScroll > maxScroll)
    {
        m_iResolutionPopupScroll = maxScroll;
    }
}

void SEASON3B::CNewUIOptionWindow::EnsureResolutionSelectionVisible()
{
    const int visibleResolutionCount = GetVisibleResolutionCount();
    if (visibleResolutionCount <= 0 || m_iSelectedResolutionIndex < 0)
    {
        m_iResolutionPopupScroll = 0;
        return;
    }

    if (m_iSelectedResolutionIndex < m_iResolutionPopupScroll)
    {
        m_iResolutionPopupScroll = m_iSelectedResolutionIndex;
    }
    else if (m_iSelectedResolutionIndex >= m_iResolutionPopupScroll + visibleResolutionCount)
    {
        m_iResolutionPopupScroll = m_iSelectedResolutionIndex - visibleResolutionCount + 1;
    }

    ClampResolutionPopupScroll();
}

void SEASON3B::CNewUIOptionWindow::ToggleResolutionPopup()
{
    m_bResolutionPopupOpen = !m_bResolutionPopupOpen;
    if (m_bResolutionPopupOpen)
    {
        EnsureResolutionSelectionVisible();
    }
}
