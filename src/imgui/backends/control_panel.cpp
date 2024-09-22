﻿/**
 * This file is part of Special K.
 *
 * Special K is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Special K is distributed in the hope that it will be useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Special K.
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#define _CRT_SECURE_NO_WARNINGS

#include <imgui/imgui.h>
#include <imgui/backends/imgui_gl3.h>
#include <imgui/backends/imgui_d3d9.h>
#include <imgui/backends/imgui_d3d11.h>
#include <d3d9.h>

#include <SpecialK/config.h>
#include <SpecialK/DLL_VERSION.H>
#include <SpecialK/command.h>
#include <SpecialK/console.h>

#include <SpecialK/window.h>
#include <SpecialK/steam_api.h>
#include <SpecialK/log.h>

#include <SpecialK/render_backend.h>
#include <SpecialK/dxgi_backend.h>
#include <SpecialK/sound.h>

#include <SpecialK/diagnostics/debug_utils.h>
#include <SpecialK/input/xinput_hotplug.h>

#include <SpecialK/nvapi.h>


#include <windows.h>
#include <cstdio>
#include "resource.h"

__declspec (dllexport)
void
SK_ImGui_Toggle (void);

typedef BOOL (WINAPI *SetCursorPos_pfn)
(
  _In_ int X,
  _In_ int Y
);

extern SetCursorPos_pfn  SetCursorPos_Original;

extern uint32_t __stdcall SK_Steam_PiratesAhoy (void);
extern uint32_t __stdcall SK_SteamAPI_AppID    (void);

extern const wchar_t* __stdcall SK_GetBackend (void);
extern       bool     __stdcall SK_IsInjected (bool set = false);

extern bool     __stdcall SK_FAR_IsPlugIn      (void);
extern void     __stdcall SK_FAR_ControlPanel  (void);

       bool               SK_DXGI_SlowStateCache = false;

extern GetCursorInfo_pfn GetCursorInfo_Original;
       bool              cursor_vis      = false;

extern HWND              SK_FindRootWindow (DWORD proc_id);

void
LoadFileInResource ( int          name,
                     int          type,
                     DWORD&       size,
                     const char*& data )
{
  HMODULE handle =
    GetModuleHandle (nullptr);

  HRSRC rc =
   FindResource ( handle,
                    MAKEINTRESOURCE (name),
                    MAKEINTRESOURCE (type) );

  HGLOBAL rcData = 
    LoadResource (handle, rc);

  if (rcData != nullptr) {

    size =
      SizeofResource (handle, rc);

    data =
      static_cast <const char *>(
        LockResource (rcData)
      );
   }
}

extern void SK_Steam_SetNotifyCorner (void);


struct show_eula_s {
  bool show;
  bool never_show_again;
} extern eula;

extern void __stdcall SK_ImGui_DrawEULA (LPVOID reserved);
extern bool           SK_ImGui_Visible;

extern void
__stdcall
SK_SteamAPI_SetOverlayState (bool active);

extern bool
__stdcall
SK_SteamAPI_GetOverlayState (bool real);

extern void ImGui_ToggleCursor (void);

#include <map>
#include <unordered_set>
#include <d3d11.h>

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#include <ImGui/imgui_internal.h>

// Special K Extensions to ImGui
namespace SK_ImGui
{
  bool
  VerticalToggleButton (const char *text, bool *v)
  {
    using namespace ImGui;

          ImFont        *font      = GImGui->Font;
    const ImFont::Glyph *glyph;
          char           c;
          bool           ret;
          ImGuiContext&  g         = *GImGui;
    const ImGuiStyle&    style     = g.Style;
          float          pad       = style.FramePadding.x;
          ImVec4         color;
    const char*          hash_mark = strstr (text, "##");
          ImVec2         text_size = CalcTextSize     (text, hash_mark);
          ImGuiWindow*   window    = GetCurrentWindow ();
          ImVec2         pos       = ImVec2 ( window->DC.CursorPos.x + pad,
                                              window->DC.CursorPos.y + text_size.x + pad );
    
    const ImU32 text_color =
      ImGui::ColorConvertFloat4ToU32 (style.Colors [ImGuiCol_Text]);

    color = style.Colors [ImGuiCol_Button];

    if (*v)
      color = style.Colors [ImGuiCol_ButtonActive];

    ImGui::PushStyleColor (ImGuiCol_Button, color);
    ImGui::PushID         (text);

    ret = ImGui::Button ( "", ImVec2 (text_size.y + pad * 2,
                                      text_size.x + pad * 2) );
    ImGui::PopStyleColor ();

    while ((c = *text++))
    {
      glyph = font->FindGlyph (c);

      if (! glyph)
        continue;
    
      window->DrawList->PrimReserve (6, 4);
      window->DrawList->PrimQuadUV  (
              ImVec2   ( pos.x + glyph->Y0 , pos.y - glyph->X0 ),
              ImVec2   ( pos.x + glyph->Y0 , pos.y - glyph->X1 ),
              ImVec2   ( pos.x + glyph->Y1 , pos.y - glyph->X1 ),
              ImVec2   ( pos.x + glyph->Y1 , pos.y - glyph->X0 ),
    
                ImVec2 (         glyph->U0,         glyph->V0 ),
                ImVec2 (         glyph->U1,         glyph->V0 ),
                ImVec2 (         glyph->U1,         glyph->V1 ),
                ImVec2 (         glyph->U0,         glyph->V1 ),

                  text_color );

      pos.y -= glyph->XAdvance;

      // Do not print embedded hash codes
      if (hash_mark != nullptr && text >= hash_mark)
        break;
    }

    ImGui::PopID ();

    if (ret)
      *v ^= 1;

    return ret;
  }

  // Should return true when clicked, this is not consistent with
  //   the rest of the ImGui API.
  bool BatteryMeter (void)
  {
    if (! SK::SteamAPI::AppID ())
      return false;

    ISteamUtils* utils =
      SK_SteamAPI_Utils ();

    if (utils)
    {
      uint8_t battery_level =
        utils->GetCurrentBatteryPower ();
      
      if (battery_level != 255) // 255 = Running on AC
      {
        float battery_ratio = (float)battery_level/100.0f;

        static char szBatteryLevel [128] = { '\0' };
        snprintf (szBatteryLevel, 127, "%lu%% Battery Charge Remaining", battery_level);

        ImGui::PushStyleColor (ImGuiCol_PlotHistogram,  ImColor::HSV (battery_ratio * 0.278f, 0.88f, 0.666f));
        ImGui::PushStyleColor (ImGuiCol_Text,           ImColor (255, 255, 255));
        ImGui::PushStyleColor (ImGuiCol_FrameBg,        ImColor ( 0.3f,  0.3f,  0.3f, 0.7f));
        ImGui::PushStyleColor (ImGuiCol_FrameBgHovered, ImColor ( 0.6f,  0.6f,  0.6f, 0.8f));
        ImGui::PushStyleColor (ImGuiCol_FrameBgActive,  ImColor ( 0.9f,  0.9f,  0.9f, 0.9f));

        ImGui::ProgressBar ( battery_ratio,
                               ImVec2 (-1, 0),
                                 szBatteryLevel );

        ImGui::PopStyleColor  (5);

        return true;
      }
    }

    return false;
  }
} // namespace SK_ImGui

void
SK_ImGui_UpdateCursor (void)
{
  ImGui::GetIO ().MouseDownDuration [0] = 0.0f;
  
  POINT orig_pos;
  GetCursorPos_Original (&orig_pos);
  SetCursorPos_Original (0, 0);
  
  SK_ImGui_Cursor.update ();
  
  SetCursorPos_Original (orig_pos.x, orig_pos.y);
}

ImVec2 SK_ImGui_LastWindowCenter (-1.0f, -1.0f);
void   SK_ImGui_CenterCursorOnWindow (void);

const char*
SK_ImGui_ControlPanelTitle (void)
{
  static char szTitle [512] = { '\0' };

  static bool init          =    false,
              steam         =   (SK::SteamAPI::AppID () != 0x0);

  if (! init)
  {
    // TEMP HACK
    static HMODULE hModTBFix = GetModuleHandle (L"tbfix.dll");

    std::wstring title = L"";

    extern std::wstring __stdcall SK_GetPluginName (void);
    extern bool         __stdcall SK_HasPlugin     (void);

    if (SK_HasPlugin () && hModTBFix == nullptr)
      title += SK_GetPluginName ();
    else
      title += L"Special K (v " SK_VERSION_STR_W L")";

    title += L" Control Panel";

    if (steam)
    {
      std::string appname = SK::SteamAPI::AppName ();

      if (appname.length ())
        title += L"      -      ";

      snprintf (szTitle, 511, "%ws%s", title.c_str (), appname.c_str ());
    }

    else
      snprintf (szTitle, 511, "%ws", title.c_str ());

    init = true;
  }

  return szTitle;
}

#include <TlHelp32.h>

static SK_WASAPI_SessionManager sessions;
static SK_WASAPI_AudioSession*  audio_session;

bool
SK_ImGui_SelectAudioSessionDlg (void)
{
         bool  changed   = false;
  const  float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;
  static bool  was_open  = false;

  int            sel_idx = -1;


  if (ImGui::BeginPopupModal ("Audio Session Selector", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders))
  {
    int count = 0;

    SK_WASAPI_AudioSession** pSessions =
      sessions.getActive (&count);

    float max_width = ImGui::CalcTextSize ("Audio Session Selector").x;

    for (int i = 0; i < count; i++)
    {
      ImVec2 size = ImGui::CalcTextSize (pSessions [i]->getName ());

      if (size.x > max_width) max_width = size.x;
    }

    ImGui::PushItemWidth (max_width * 2.5f);

    if (ImGui::ListBoxHeader ("##empty", count, std::min (count + 3, 10)))
    {
      ImGui::PushStyleColor (ImGuiCol_ChildWindowBg, ImColor (0, 0, 0, 0));
      ImGui::BeginChild ("SessionSelectHeader");
      ImGui::Columns    (2);
      ImGui::Text       ("Task");
      ImGui::NextColumn ();
      ImGui::Text       ("Volume / Mute");
      ImGui::NextColumn ();
      ImGui::Columns    (1);

      ImGui::Separator  ();

      ImGui::BeginChild ("SessionSelectData");
      ImGui::Columns    (2);

      for (int i = 0; i < count; i++)
      {
        SK_WASAPI_AudioSession* pSession =
          pSessions [i];

        bool selected     = false;
        bool drawing_self = (pSession->getProcessId () == GetCurrentProcessId ());

        CComPtr <ISimpleAudioVolume> volume_ctl =
          pSession->getSimpleAudioVolume ();

        BOOL mute;
        if (volume_ctl != nullptr && SUCCEEDED (volume_ctl->GetMute (&mute)))
        {
          if (drawing_self)
          {
            ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
            ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
            ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
          }
          if (ImGui::Selectable (pSession->getName (), drawing_self || pSession == audio_session))
            sel_idx = i;

          if (drawing_self)
            ImGui::PopStyleColor (3);

          ImGui::NextColumn ();

          float volume = 0.0f;
          volume_ctl->GetMasterVolume (&volume);

          char szLabel [32] = { '\0' };
          snprintf (szLabel, 31, "###VoumeSlider%lu", i);

          ImGui::PushStyleColor (ImGuiCol_Text,           mute ? ImColor (0.5f, 0.5f, 0.5f) : ImColor (1.0f, 1.0f, 1.0f));
          ImGui::PushStyleColor (ImGuiCol_FrameBg,        ImColor::HSV ( 0.4f * volume, 0.6f, mute ? 0.2f : 0.4f));
          ImGui::PushStyleColor (ImGuiCol_FrameBgHovered, ImColor::HSV ( 0.4f * volume, 0.7f, mute ? 0.2f : 0.4f));
          ImGui::PushStyleColor (ImGuiCol_FrameBgActive,  ImColor::HSV ( 0.4f * volume, 0.8f, mute ? 0.2f : 0.4f));
          ImGui::PushStyleColor (ImGuiCol_SliderGrab,     ImColor::HSV ( 0.4f * volume, 0.9f,        0.6f * (mute ? 0.5f : 1.0f)));

          volume *= 100.0f;

          if (ImGui::SliderFloat (szLabel, &volume, 0.0f, 100.0f, "Volume: %03.1f%%")) {
            volume /= 100.0f;
            volume_ctl->SetMasterVolume (volume, nullptr);
          }

          ImGui::PopStyleColor (5);

          ImGui::SameLine ();

          snprintf (szLabel, 31, "###VoumeCheckbox%lu", i);
          ImGui::PushItemWidth (10.0f);
          if (ImGui::Checkbox (szLabel, (bool *)&mute))
            volume_ctl->SetMute (mute, nullptr);
          ImGui::PopItemWidth  ();

          ImGui::NextColumn ();
        }
      }

      ImGui::Columns  (1);
      ImGui::EndChild ( );
      ImGui::EndChild ( );
      ImGui::PopStyleColor ();


      ImGui::ListBoxFooter ();

      if (sel_idx != -1)
      {
        audio_session = pSessions [sel_idx];

        changed  = true;
      }

      if (changed)
      {
        was_open = false;
        ImGui::CloseCurrentPopup ();
      }
    }

    ImGui::PopItemWidth ();
    ImGui::EndPopup     ();
  }

  return changed;
}

void
SK_ImGui_AdjustCursor (void)
{
  CreateThread ( nullptr, 0,
    [](LPVOID user)->
    DWORD
    {
      ClipCursor_Original (nullptr);
        SK_AdjustWindow   ();        // Restore game's clip cursor behavior

      CloseHandle (GetCurrentThread ());

      return 0;
    }, nullptr, 0x00, nullptr );
}

bool reset_frame_history = true;
bool was_reset           = false;

__declspec (dllexport)
bool
SK_ImGui_ControlPanel (void)
{
  ImGuiIO& io =
    ImGui::GetIO ();

  static HMODULE hModTBFix = GetModuleHandle (L"tbfix.dll");
  static HMODULE hModTZFix = GetModuleHandle (L"tzfix.dll");

  //
  // Framerate history
  //
  static float values [120]  = { 0 };
  static int   values_offset =   0;

  if (! reset_frame_history) {
    values [values_offset] = 1000.0f * ImGui::GetIO ().DeltaTime;
    values_offset = (values_offset + 1) % IM_ARRAYSIZE (values);
  }

  reset_frame_history = false;


  static float last_width  = -1;
  static float last_height = -1;

  bool recenter = ( last_width  != io.DisplaySize.x ||
                    last_height != io.DisplaySize.y );

  if (recenter)
  {
    SK_ImGui_LastWindowCenter.x = -1.0f;
    SK_ImGui_LastWindowCenter.y = -1.0f;

    ImGui::SetNextWindowPosCenter       (ImGuiSetCond_Always);
    last_width = io.DisplaySize.x; last_height = io.DisplaySize.y;
  }


  ImGui::SetNextWindowSizeConstraints (ImVec2 (250, 200), ImVec2 ( 0.9f * io.DisplaySize.x,
                                                                   0.9f * io.DisplaySize.y ) );

  
  const char* szTitle   = SK_ImGui_ControlPanelTitle ();
  static int  title_len = int ((float)ImGui::CalcTextSize (szTitle).x * 1.075f);
  bool        open      = true;


  ImGuiStyle& style =
    ImGui::GetStyle ();

  style.WindowTitleAlign = ImVec2 (0.5f, 0.5f);

  style.WindowMinSize.x = title_len * 1.075f;
  style.WindowMinSize.y = 200;

  extern bool nav_usable;

  if (nav_usable)
    ImGui::PushStyleColor (ImGuiCol_Text, ImColor::HSV ((float)(timeGetTime () % 2800) / 2800.0f,  (0.5f + (sin ((float)(timeGetTime () % 500) / 500.0f)) * 0.5f) / 2.0f, 1.0f));
  else
    ImGui::PushStyleColor (ImGuiCol_Text, ImColor (255, 255, 255));
  ImGui::Begin          (szTitle, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders);
  ImGui::PopStyleColor  ();
          char szAPIName [32] = { '\0' };
    snprintf ( szAPIName, 32, "%ws    "
#ifndef _WIN64
                                   "[ 32-bit ]",
#else
                                   "[ 64-bit ]",
#endif
                             SK_GetCurrentRenderBackend ().name );

    ImGui::MenuItem ("Active Render API        ", szAPIName);

    ImGui::Separator ();

    char szResolution [64] = { '\0' };
    snprintf ( szResolution, 63, "   %lux%lu", 
                                   (UINT)io.DisplayFramebufferScale.x, (UINT)io.DisplayFramebufferScale.y );

    bool override = false;

    if (ImGui::MenuItem (" Framebuffer Resolution", szResolution))
    {
      config.window.res.override.x = (UINT)io.DisplayFramebufferScale.x;
      config.window.res.override.y = (UINT)io.DisplayFramebufferScale.y;

      override = true;
    }

    RECT client;
    GetClientRect (game_window.hWnd, &client);

    snprintf ( szResolution, 63, "   %lux%lu", 
                                   client.right - client.left,
                                     client.bottom - client.top );

    if (ImGui::MenuItem (" Window Resolution     ", szResolution))
    {
      config.window.res.override.x = client.right  - client.left;
      config.window.res.override.y = client.bottom - client.top;

      override = true;
    }

    HDC hDC = GetWindowDC (game_window.hWnd);

    int res_x = GetDeviceCaps (hDC, HORZRES);
    int res_y = GetDeviceCaps (hDC, VERTRES);

    ReleaseDC (game_window.hWnd, hDC);

    if ( client.right - client.left   != res_x || client.bottom - client.top   != res_y ||
         io.DisplayFramebufferScale.x != res_x || io.DisplayFramebufferScale.y != res_y )
    {
      snprintf ( szResolution, 63, "   %lux%lu", 
                                     res_x,
                                       res_y );
    
      if (ImGui::MenuItem (" Device Resolution    ", szResolution))
      {
        config.window.res.override.x = res_x;
        config.window.res.override.y = res_y;

        override = true;
      }
    }

    if (! config.window.res.override.isZero ())
    {
      snprintf ( szResolution, 63, "   %lux%lu", 
                                     config.window.res.override.x,
                                       config.window.res.override.y );

      bool selected = true;
      if (ImGui::MenuItem (" Override Resolution   ", szResolution, &selected))
      {
        config.window.res.override.x = 0;
        config.window.res.override.y = 0;

        override = true;
      }
    }


    if (sk::NVAPI::nv_hardware)
    {
      SK_GetCurrentRenderBackend ().gsync_state.update ();

      char szGSyncStatus [128] = { '\0' };
      if (SK_GetCurrentRenderBackend ().gsync_state.capable)
      {
        strcat (szGSyncStatus, "Supported + ");
        if (SK_GetCurrentRenderBackend ().gsync_state.active)
          strcat (szGSyncStatus, "Active");
        else
          strcat (szGSyncStatus, "Inactive");
      }
      else
        strcat (szGSyncStatus, "Unsupported");

      ImGui::MenuItem (" G-Sync Status   ", szGSyncStatus);
    }


    if (override)
      SK_ImGui_AdjustCursor ();


    ImGui::Columns   ( 1 );
    ImGui::Separator (   );

    static bool has_own_scale = (hModTZFix || hModTBFix);
    static bool first_frame   = true;

    // Initialize the dialog using the user's scale preference
    if (first_frame)
    {
      // Range-restrict for sanity!
      config.imgui.scale = std::max (1.0f, std::min (5.0f, config.imgui.scale));

      io.FontGlobalScale = config.imgui.scale;
      first_frame        = false;
    }

    if ((! has_own_scale) && ImGui::CollapsingHeader ("UI Scale"))
    {
      ImGui::TreePush    ("");

      if (ImGui::SliderFloat ("Scale (only 1.0 is officially supported)", &config.imgui.scale, 1.0f, 3.0f))
      {
        // ImGui does not perform strict parameter validation, and values out of range for this can be catastrophic.
        config.imgui.scale = std::max (1.0f, std::min (5.0f, config.imgui.scale));
        io.FontGlobalScale = config.imgui.scale;
      }

      ImGui::TreePop     ();
    }


    // STUPID DESIGN
    if (SK_FAR_IsPlugIn ())
    {
      SK_FAR_ControlPanel ();
    }

    const  float font_size           =              ImGui::GetFont ()->FontSize                        * io.FontGlobalScale;
    const  float font_size_multiline = font_size + ImGui::GetStyle ().ItemSpacing.y + ImGui::GetStyle ().ItemInnerSpacing.y;

    static bool has_own_limiter = (hModTZFix || hModTBFix);


    if (! has_own_limiter)
    {
      ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.666f);

      if ( ImGui::CollapsingHeader ("Framerate Limiter", ImGuiTreeNodeFlags_CollapsingHeader | 
                                                         ImGuiTreeNodeFlags_DefaultOpen ) )
      {
        float sum = 0.0f;

        float min = FLT_MAX;
        float max = 0.0f;

        for (float val : values) {
          sum += val;

          if (val > max)
            max = val;

          if (val < min)
            min = val;
        }

        static char szAvg [512];

        extern float target_fps;

        float target_frametime = ( target_fps == 0.0f ) ?
                                    ( 1000.0f / 60.0f ) :
                                      ( 1000.0f / target_fps );

        sprintf_s
              ( szAvg,
                  512,
                    u8"Avg milliseconds per-frame: %6.3f  (Target: %6.3f)\n"
                    u8"    Extreme frametimes:      %6.3f min, %6.3f max\n\n\n\n"
                    u8"Variation:  %8.5f ms        %.1f FPS  ±  %3.1f frames",
                      sum / 120.0f, target_frametime,
                        min, max, max - min,
                          1000.0f / (sum / 120.0f), (max - min) / (1000.0f / (sum / 120.0f)) );

        ImGui::PushStyleColor ( ImGuiCol_PlotLines, 
                                  ImColor::HSV ( 0.31f - 0.31f *
                           std::min ( 1.0f, (max - min) / (2.0f * target_frametime) ),
                                                   0.73f,
                                                     0.93f ) );

        ImGui::PlotLines ( "",
                             values,
                               IM_ARRAYSIZE (values),
                                 values_offset,
                                   szAvg,
                                     0.0f,
                                       2.0f * target_frametime,
                                         ImVec2 (
                                           std::max (500.0f, ImGui::GetContentRegionAvailWidth ()), font_size * 7) );

        ImGui::PopStyleColor ();

        bool changed = ImGui::SliderFloat ( "Special K Framerate Tolerance", &config.render.framerate.limiter_tolerance, 0.005f, 0.75);
        
        if (ImGui::IsItemHovered ()) {
          ImGui::BeginTooltip ();
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor (0.95f, 0.75f, 0.25f, 1.0f));
          ImGui::Text           ("Controls Framerate Smoothness\n\n");
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor (0.75f, 0.75f, 0.75f, 1.0f));
          ImGui::Text           ("  Lower = Smoother, but setting ");
          ImGui::SameLine       ();
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor (0.95f, 1.0f, 0.65f, 1.0f));
          ImGui::Text           ("too low");
          ImGui::SameLine       ();
          ImGui::PushStyleColor (ImGuiCol_Text, ImColor(0.75f, 0.75f, 0.75f, 1.0f));
          ImGui::Text           (" will cause framerate instability...");
          ImGui::PopStyleColor  (4);
          ImGui::EndTooltip     ();
        }

        // Don't apply this number if it's < 10, that does very undesirable things
        float target_orig = target_fps;

        if (ImGui::InputFloat ("Framerate Limit", &target_fps))
        {
          if (target_fps > 9 || target_fps == 0)
            SK_GetCommandProcessor ()->ProcessCommandFormatted ("TargetFPS %f", target_fps);
          else
            target_fps = target_orig;
        }

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("0.0 will disable Special K's framerate limiter");
      }

      ImGui::PopItemWidth ();
    }

    SK_RenderAPI api = SK_GetCurrentRenderBackend ().api;

    if ( api == SK_RenderAPI::D3D11 &&
         ImGui::CollapsingHeader ("Direct3D 11 Settings" ) )
    {
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
      ImGui::TreePush       ("");

      ////ImGui::Checkbox ("Overlay Compatibility Mode", &SK_DXGI_SlowStateCache);

      ////if (ImGui::IsItemHovered ())
        ////ImGui::SetTooltip ("Increased compatibility with video capture software");

      if (ImGui::CollapsingHeader ("Texture Management"))
      {
        ImGui::TreePush ("");
        ImGui::Checkbox ("Enable Texture Caching", &config.textures.d3d11.cache); 

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Reduce driver memory management overhead in games that stream textures.");

        ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.85f, 0.1f, 0.9f));
        ImGui::SameLine (); ImGui::BulletText ("Requires restart");
        ImGui::PopStyleColor  ();

        if (config.textures.d3d11.cache)
        {
          ImGui::SameLine ();
          ImGui::Spacing  ();
          ImGui::SameLine ();

          ImGui::Checkbox ("Ignore Textures Without Mipmaps", &config.textures.cache.ignore_nonmipped);

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Important Compatibility Setting for Some Games (e.g. The Witcher 3)");
        }

        ImGui::TreePop  ();

        if (config.textures.d3d11.cache)
        {
          ImGui::PushStyleColor (ImGuiCol_Border, ImColor (245,245,245));
          ImGui::Separator (   );
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 1.0f, 1.0f, 1.0f));
          ImGui::Columns   ( 3 );
            ImGui::Text    ( "          Size" );                                                                     ImGui::NextColumn ();
            ImGui::Text    ( "      Activity" );                                                                 ImGui::NextColumn ();
            ImGui::Text    ( "       Savings" );
          ImGui::Columns   ( 1 );
          ImGui::PopStyleColor
                           (   );

          ImGui::PushStyleColor
                           ( ImGuiCol_Text, ImVec4 (0.75f, 0.75f, 0.75f, 1.0f) );

          ImGui::Separator      ( );
          ImGui::PopStyleColor  (1);

          ImGui::PushStyleColor(ImGuiCol_Border, ImColor(0.5f, 0.5f, 0.5f, 0.666f));
          ImGui::Columns   ( 3 );
            ImGui::Text    ( "%#7zu      MiB",
                                                           SK_D3D11_Textures.AggregateSize_2D >> 20ULL );  ImGui::NextColumn (); 
             ImGui::TextColored
                           (ImVec4 (0.3f, 1.0f, 0.3f, 1.0f),
                             "%#5lu      Hits",            SK_D3D11_Textures.RedundantLoads_2D         );  ImGui::NextColumn ();
             if (InterlockedAdd64 (&SK_D3D11_Textures.Budget, 0) != 0)
               ImGui::Text ( "Budget:  %#7zu MiB  ",        SK_D3D11_Textures.Budget / 1048576UL );
          ImGui::Columns   ( 1 );

          ImGui::Separator (   );

          ImGui::Columns   ( 3 );
            ImGui::Text    ( "%#7zu Textures",             SK_D3D11_Textures.TexRefs_2D.size () );        ImGui::NextColumn ();
            ImGui::TextColored
                           ( ImVec4 (1.0f, 0.3f, 0.3f, 1.60f),
                             "%#5lu   Misses",             SK_D3D11_Textures.CacheMisses_2D          );   ImGui::NextColumn ();
           ImGui::Text   ( "Time:        %#7.01lf ms  ", SK_D3D11_Textures.RedundantTime_2D          );
          ImGui::Columns   ( 1 );

          ImGui::Separator (   );

          ImGui::Columns   ( 3 );  
            ImGui::Text    ( "%#6lu   Evictions",            SK_D3D11_Textures.Evicted_2D         );        ImGui::NextColumn ();
            ImGui::TextColored (ImColor::HSV (std::min ( 0.4f * (float)SK_D3D11_Textures.RedundantLoads_2D / 
                                                                (float)SK_D3D11_Textures.CacheMisses_2D, 0.4f ), 0.95f, 0.8f),
                             " %.2f  Hit/Miss",                  (double)SK_D3D11_Textures.RedundantLoads_2D / 
                                                                (double)SK_D3D11_Textures.CacheMisses_2D  ); ImGui::NextColumn ();
            ImGui::Text    ( "Driver I/O: %#7zu MiB  ",    SK_D3D11_Textures.RedundantData_2D >> 20ULL );

          ImGui::Columns   ( 1 );

          ImGui::Separator (   );

          ImGui::PopStyleColor ( );

          int size = config.textures.cache.max_size;

          ImGui::TreePush  ( "" );
          if (ImGui::SliderInt ( "Cache Size (GPU-shared memory)", &size, 256, 8192, "%.0f MiB"))
            config.textures.cache.max_size = size;
            SK_GetCommandProcessor ()->ProcessCommandFormatted ("TexCache.MaxSize %d ", config.textures.cache.max_size);
          ImGui::TreePop   (    );

          ImGui::PopStyleColor ( );
        }
      }

      static bool enable_resolution_limits = ! ( config.render.dxgi.res.min.isZero () && 
                                                 config.render.dxgi.res.max.isZero () );

      bool res_limits =
        ImGui::CollapsingHeader ("Resolution Limiting", enable_resolution_limits ? ImGuiTreeNodeFlags_DefaultOpen : 0x00);

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip ();
        ImGui::Text         ("Restrict the lowest/highest resolutions reported to a game");
        ImGui::Separator    ();
        ImGui::BulletText   ("Useful for games that compute aspect ratio based on the highest reported resolution.");
        ImGui::EndTooltip   ();
      }

      if (res_limits)
      {
        ImGui::TreePush  ("");
        ImGui::InputInt2 ("Minimum Resolution", (int *)&config.render.dxgi.res.min.x);
        ImGui::InputInt2 ("Maximum Resolution", (int *)&config.render.dxgi.res.max.x);
        ImGui::TreePop   ();
       }

      ImGui::TreePop       ( );
      ImGui::PopStyleColor (3);
    }

    if ( ImGui::CollapsingHeader ("Compatibility Settings") )
    {
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.02f, 0.68f, 0.90f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.07f, 0.72f, 0.90f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.14f, 0.78f, 0.87f, 0.80f));
      ImGui::TreePush       ("");

      if (ImGui::CollapsingHeader ("Third-Party Software"))
      {
        ImGui::TreePush ("");
        ImGui::Checkbox ("Disable GeForce Experience and NVIDIA Shield", &config.compatibility.disable_nv_bloat);
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("May improve software compatibility, but disables ShadowPlay, couch co-op and various Shield-related functionality.");
        ImGui::TreePop  ();
      }

      if (ImGui::CollapsingHeader ("Render Backends", SK_IsInjected () ? ImGuiTreeNodeFlags_DefaultOpen : 0))
      {
        ImGui::TreePush ("");
        auto EnableActiveAPI = [](SK_RenderAPI api) ->
        void
        {
          switch (api)
          {
            case SK_RenderAPI::D3D9Ex:
              config.apis.d3d9ex.hook     = true;
            case SK_RenderAPI::D3D9:
              config.apis.d3d9.hook       = true;
              break;

            case SK_RenderAPI::D3D12:
              config.apis.dxgi.d3d12.hook = true;
            case SK_RenderAPI::D3D11:
              config.apis.dxgi.d3d11.hook = true;
              break;

            case SK_RenderAPI::OpenGL:
              config.apis.OpenGL.hook     = true;
              break;

            case SK_RenderAPI::Vulkan:
              config.apis.Vulkan.hook     = true;
              break;
          }
        };

        ImGui::PushStyleVar                                                           (ImGuiStyleVar_ChildWindowRounding, 10.0f);
        ImGui::BeginChild ("", ImVec2 (font_size * 39, font_size_multiline * 4), true/*, ImGuiWindowFlags_ChildWindowAutoFitY*/);

        ImGui::Columns    ( 2 );

        ImGui::Checkbox ("Direct3D 9", &config.apis.d3d9.hook);

        if (config.apis.d3d9.hook)
          ImGui::Checkbox ("Direct3D 9Ex", &config.apis.d3d9ex.hook);
        else {
          ImGui::TextDisabled ("   Direct3D 9Ex");
          config.apis.d3d9ex.hook = false;
        }

        ImGui::NextColumn (  );

        ImGui::Checkbox ("Direct3D 11", &config.apis.dxgi.d3d11.hook);

        if (config.apis.dxgi.d3d11.hook)
          ImGui::Checkbox ("Direct3D 12", &config.apis.dxgi.d3d12.hook);
        else {
          ImGui::TextDisabled ("   Direct3D 12");
          config.apis.dxgi.d3d12.hook = false;
        }

        ImGui::Columns    ( 1 );
        ImGui::Separator  (   );
        ImGui::Columns    ( 2 );
                          
        ImGui::Checkbox   ("OpenGL ", &config.apis.OpenGL.hook); ImGui::SameLine ();
        ImGui::Checkbox   ("Vulkan ", &config.apis.Vulkan.hook);

        ImGui::NextColumn (  );

        if (ImGui::Button (" Disable All But the Active API "))
        {
          config.apis.d3d9ex.hook     = false; config.apis.d3d9.hook       = false;
          config.apis.dxgi.d3d11.hook = false; config.apis.dxgi.d3d12.hook = false;
          config.apis.OpenGL.hook     = false; config.apis.Vulkan.hook     = false;
        }

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Application start time and negative interactions with third-party software can be reduced by turning off APIs that are not needed...");

        ImGui::Columns    ( 1 );

        ImGui::EndChild   (  );
        ImGui::PopStyleVar ( );

        EnableActiveAPI   (api);
        ImGui::TreePop    ();
      }

      if (ImGui::CollapsingHeader ("Hardware Monitoring"))
      {
        ImGui::TreePush ("");
        ImGui::Checkbox ("NvAPI  ", &config.apis.NvAPI.enable);
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("NVIDIA's hardware monitoring API, needed for the GPU stats on the OSD. Turn off only if your driver is buggy.");

        ImGui::SameLine ();
        ImGui::Checkbox ("ADL   ",   &config.apis.ADL.enable);
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("AMD's hardware monitoring API, needed for the GPU stats on the OSD. Turn off only if your driver is buggy.");
        ImGui::TreePop  ();
      }

      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.40f, 0.40f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.45f, 0.45f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.53f, 0.53f, 0.80f));

      if (ImGui::CollapsingHeader ("Debugging"))
      {
        ImGui::TreePush  ("");
        ImGui::BeginGroup ( );
        ImGui::Checkbox  ("Enable Crash Handler",           &config.system.handle_crashes);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Play Metal Gear Solid Alert Sound and Log Crashes in logs/crash.log");

        ImGui::Checkbox  ("ReHook LoadLibrary",             &config.compatibility.rehook_loadlibrary);

        if (ImGui::IsItemHovered ()) {
          ImGui::BeginTooltip ();
          ImGui::Text         ("Keep LoadLibrary Hook at Front of Hook Chain");
          ImGui::Separator    ();
          ImGui::BulletText   ("Improves Debug Log Accuracy");
          ImGui::BulletText   ("Third-Party Software May Deadlock Game at Startup if Enabled");
          ImGui::EndTooltip   ();
        }

        ImGui::SliderInt ("Log Level",                      &config.system.log_level, 0, 5);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Controls Debug Log Verbosity; Higher = Bigger/Slower Logs");

        ImGui::Checkbox  ("Log Game Output",                &config.system.game_output);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Log any Game Text Output to logs/game_output.log");

        if (ImGui::Checkbox  ("Print Debug Output to Console",  &config.system.display_debug_out))
        {
          if (config.system.display_debug_out) {
            if (! SK::Diagnostics::Debugger::CloseConsole ()) config.system.display_debug_out = true;
          }
          else {
            SK::Diagnostics::Debugger::SpawnConsole ();
          }
        }

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Spawns Debug Console at Startup for Debug Text from Third-Party Software");

        ImGui::Checkbox  ("Trace LoadLibrary",              &config.system.trace_load_library);

        if (ImGui::IsItemHovered ()) {
          ImGui::BeginTooltip ();
          ImGui::Text         ("Monitor DLL Load Activity");
          ImGui::Separator    ();
          ImGui::BulletText   ("Required for Render API Auto-Detection in Global Injector");
          ImGui::EndTooltip   ();
        }

        ImGui::Checkbox  ("Strict DLL Loader Compliance",   &config.system.strict_compliance);

        if (ImGui::IsItemHovered ()) {
          ImGui::BeginTooltip (  );
          ImGui::Text         ("Prevent Loading DLLs Simultaneousely Across Multiple Threads");
          ImGui::Separator    (  );
          ImGui::BulletText   ("Eliminates Race Conditions During DLL Startup");
          ImGui::BulletText   ("Unsafe for a LOT of Improperly Designed Third-Party Software");
          ImGui::TreePush     ("");
          ImGui::Text         ("\nPROPER DLL DESIGN:  Never Call LoadLibrary (...) from DllMain (...)'s Thread !!!");
          ImGui::Text         (  "                    Never Wait on a Synchronization Object from DllMain (...) !!");
          ImGui::TreePop      (  );
          ImGui::EndTooltip   (  );
        }

        ImGui::Checkbox  ("Resolve Module Symbol Names",     &config.system.resolve_symbol_names);

        if (ImGui::IsItemHovered ()) {
          ImGui::BeginTooltip (  );
          ImGui::Text         ("Resolve Module Symbol Names During DLL Startup");
          ImGui::Separator    (  );
          ImGui::BulletText   ("Improves startup time at the cost of the inability to automatically blacklist modules.");
          ImGui::BulletText   ("When disabled, drastically reduces DLL startup time");
          ImGui::EndTooltip   (  );
        }

        ImGui::EndGroup    ( );
        ImGui::SameLine    ( );
        ImGui::BeginGroup  ( );
        auto DescribeRect = [](LPRECT rect, const char* szType, const char* szName) ->
          void
          {
            ImGui::Text (szType);
            ImGui::NextColumn ();
            ImGui::Text (szName);
            ImGui::NextColumn ();
            ImGui::Text ( "| (%4li,%4li) / %4lix%li |  ",
                              rect->left, rect->top,
                                rect->right-rect->left, rect->bottom - rect->top );
            ImGui::NextColumn ();
          };

        ImGui::Columns (3);

        DescribeRect (&game_window.actual.window, "Window", "Actual" );
        DescribeRect (&game_window.actual.client, "Client", "Actual" );

        ImGui::Columns   (1);
        ImGui::Separator ( );
        ImGui::Columns   (3);

        DescribeRect (&game_window.game.window,   "Window", "Game"   );
        DescribeRect (&game_window.game.client,   "Client", "Game"   );

        ImGui::Columns   (1);
        ImGui::Separator ( );
        ImGui::Columns   (3);

        RECT client,window;
        GetClientRect (game_window.hWnd, &client);
        GetWindowRect (game_window.hWnd, &window);

        DescribeRect (&window,   "Window", "GetWindowRect"   );
        DescribeRect (&client,   "Client", "GetClientRect"   );
                           
        ImGui::Columns     (1);
        ImGui::Separator   ( );
        ImGui::EndGroup    ( );

        ImGui::Text ( "ImGui Cursor State: %lu (%lu,%lu) { %lu, %lu }",
                        SK_ImGui_Cursor.visible, SK_ImGui_Cursor.pos.x,
                                                 SK_ImGui_Cursor.pos.y,
                          SK_ImGui_Cursor.orig_pos.x, SK_ImGui_Cursor.orig_pos.y );
        ImGui::SameLine ();
        ImGui::Text (" {%s :: Last Update: %lu}", SK_ImGui_Cursor.idle ? "Idle" : "Not Idle", SK_ImGui_Cursor.last_move);
        ImGui::TreePop     ( );
      }

      ImGui::PopStyleColor (3);

      ImGui::TreePop       ( );
      ImGui::PopStyleColor (3);
    }

    if ( ImGui::CollapsingHeader ("Input Management") )
    {
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
      ImGui::TreePush       ("");

      if (ImGui::CollapsingHeader ("Mouse Cursor"))
      {
        ImGui::TreePush ("");
        ImGui::BeginGroup ();
        ImGui::Checkbox   ( "Hide When Not Moved", &config.input.cursor.manage        );

        if (config.input.cursor.manage) {
          ImGui::TreePush ("");
          ImGui::Checkbox ( "or Key Pressed",
                                                   &config.input.cursor.keys_activate );
          ImGui::TreePop  ();
        }
 
        ImGui::EndGroup   ();
        ImGui::SameLine   ();

        float seconds = 
          (float)config.input.cursor.timeout  / 1000.0f;

        float val = config.input.cursor.manage ? 1.0f : 0.0f;

        ImGui::PushStyleColor (ImGuiCol_FrameBg,        ImColor ( 0.3f,  0.3f,  0.3f,  val));
        ImGui::PushStyleColor (ImGuiCol_FrameBgHovered, ImColor ( 0.6f,  0.6f,  0.6f,  val));
        ImGui::PushStyleColor (ImGuiCol_FrameBgActive,  ImColor ( 0.9f,  0.9f,  0.9f,  val));
        ImGui::PushStyleColor (ImGuiCol_SliderGrab,     ImColor ( 1.0f,  1.0f,  1.0f, 1.0f));

        if ( ImGui::SliderFloat ( "Seconds Before Hiding",
                                    &seconds, 0.0f, 30.0f ) )
        {
          config.input.cursor.timeout = static_cast <LONG> (( seconds * 1000.0f ));
        }

        ImGui::PopStyleColor (4);

        if (! cursor_vis)
        {
          if (ImGui::Button (" Force Mouse Cursor Visible ")) {
            while (ShowCursor (TRUE) < 0)
              ;

            cursor_vis = true;
          }
        }

        else
        {
          if (ImGui::Button (" Force Mouse Cursor Hidden "))
          {
            while (ShowCursor (FALSE) >= -1)
              ;

            cursor_vis = false;
          }
        }

        ImGui::TreePop ();
      }

      if (ImGui::CollapsingHeader ("Gamepad"))
      {
        ImGui::TreePush      ("");

        ImGui::Checkbox       ("Rehook XInput", &config.input.gamepad.rehook_xinput); ImGui::SameLine ();

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Re-installs input hooks if third-party hooks are detected.");
            ImGui::Separator   ();
            ImGui::BulletText  ("This may improve compatibility with x360ce, but will require a game restart.");
          ImGui::EndTooltip    ();
        }

        ImGui::Checkbox       ("Disable PS4 HID Input", &config.input.gamepad.disable_ps4_hid);

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Prevents double input processing in games that support XInput and native PS4.");
            ImGui::Separator   ();
            ImGui::BulletText  ("This option requires restarting the game.");
          ImGui::EndTooltip    ();
        }

        ImGui::Separator ();

        static bool connected [4];
        connected [0] = SK_XInput_PollController (0);
        connected [1] = SK_XInput_PollController (1);
        connected [2] = SK_XInput_PollController (2);
        connected [3] = SK_XInput_PollController (3);

        if ( connected [0] || connected [1] ||
             connected [2] || connected [3] )
        {
          ImGui::Text("UI Controlled By:  "); ImGui::SameLine();

          if (connected [0]) {
            ImGui::RadioButton ("XInput Controller 0###XInputSlot", &config.input.gamepad.xinput.ui_slot, 0);
            if (connected [1] || connected [2] || connected [3]) ImGui::SameLine ();
          }

          if (connected [1]) {
            ImGui::RadioButton ("XInput Controller 1###XInputSlot", &config.input.gamepad.xinput.ui_slot, 1);
            if (connected [2] || connected [3]) ImGui::SameLine ();
          }

          if (connected [2]) {
            ImGui::RadioButton ("XInput Controller 2###XInputSlot", &config.input.gamepad.xinput.ui_slot, 2);
            if (connected [3]) ImGui::SameLine ();
          }

          if (connected [3])
            ImGui::RadioButton ("XInput Controller 3###XInputSlot", &config.input.gamepad.xinput.ui_slot, 3);
        }

        ImGui::Text ("XInput Placeholders");

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Substitute Real Controllers With Virtual Ones Until Connected.");
            ImGui::Separator   ();
            ImGui::BulletText  ("Useful for stupid games like God Eater 2 that do not support hot-plugging in a sane way.");
            ImGui::BulletText  ("Also reduces performance problems games cause themselves by trying to poll controllers that are not connected.");
          ImGui::EndTooltip    ();
        }

        ImGui::SameLine();

        auto XInputPlaceholderCheckbox = [](const char* szName, DWORD dwIndex) ->
             void
             {
               ImGui::Checkbox (szName, &config.input.gamepad.xinput.placehold [dwIndex]);

               SK_XInput_PacketJournal journal =
                 SK_XInput_GetPacketJournal (dwIndex);

               if (ImGui::IsItemHovered ())
               {
                 ImGui::BeginTooltip ( );
                  ImGui::TextColored (ImColor (255, 255, 255), "Hardware Packet Sequencing" );
                  ImGui::TextColored (ImColor (160, 160, 160), "(Last: %lu | Now: %lu)",
                                         journal.sequence.last, journal.sequence.current );
                  ImGui::Separator   ( );
                  ImGui::Columns     (2, nullptr, 0);
                  ImGui::TextColored (ImColor (255, 165, 0), "Virtual Packets..."); ImGui::NextColumn ();
                  ImGui::Text        ("%+07lu", journal.packet_count.virt);        ImGui::NextColumn ();
                  ImGui::TextColored (ImColor (127, 255, 0), "Real Packets...");    ImGui::NextColumn ();
                  ImGui::Text        ("%+07lu", journal.packet_count.real);
                  ImGui::Columns     (1);
                 ImGui::EndTooltip   ( );
               }
             };

        XInputPlaceholderCheckbox ("Slot 0", 0); ImGui::SameLine ();
        XInputPlaceholderCheckbox ("Slot 1", 1); ImGui::SameLine ();
        XInputPlaceholderCheckbox ("Slot 2", 2); ImGui::SameLine ();
        XInputPlaceholderCheckbox ("Slot 3", 3);

        ImGui::TreePop ();
      }

      if (ImGui::CollapsingHeader ("Low-Level Mouse Settings", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::TreePush      ("");

        ImGui::BeginGroup    ();
        ImGui::Text          ("Mouse Input API");
        ImGui::TreePush      ("");
        int  input_backend = 1;
        bool changed       = false;

        changed |=
          ImGui::RadioButton ("Win32",     &input_backend, 0); ImGui::SameLine ();
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Temporarily Disabled (intended for compatibility only)");
        changed |=
          ImGui::RadioButton ("Raw Input", &input_backend, 1);
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("More Reliable (currently the only supported input API)");

        ImGui::TreePop       ();
        ImGui::EndGroup      ();

        ImGui::SameLine ();

        ImGui::BeginGroup    ();
        ImGui::Text          ("Mouse Input Capture");
        ImGui::TreePush      ("");

        if (ImGui::Checkbox ("Block Mouse", &config.input.ui.capture_mouse))
        {
          SK_ImGui_UpdateCursor ();
          //SK_ImGui_AdjustCursor ();
        }

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Prevent Game from Detecting Mouse Input while this UI is Visible");
            ImGui::Separator   ();
            ImGui::BulletText  ("May help with mouselook in some games");
            //ImGui::BulletText  ("Implicitly enabled if running at a non-native Fullscreen resolution");
          ImGui::EndTooltip    ();
        }

        ImGui::SameLine ();

        if (ImGui::Checkbox ("Use Hardware Cursor", &config.input.ui.use_hw_cursor))
        {
          SK_ImGui_UpdateCursor ();
        }

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Redistribute Input Latency -- (Trade Cursor Lag for UI Lag)");
            ImGui::Separator   ();
            ImGui::BulletText  ("You will experience several frames of lag while dragging UI windows around.");
            ImGui::BulletText  ("Most Games use Hardware Cursors; turning this on will reduce visible cursor trails.");
            ImGui::BulletText  ("Automatically switches to Software when the game is not using Hardware.");
          ImGui::EndTooltip    ();
        }

        ImGui::SameLine ();

        ImGui::Checkbox ("No Warp (cursor visible)",              &SK_ImGui_Cursor.prefs.no_warp.visible);

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Do Not Alllow Game to Move Cursor to Center of Screen");
            ImGui::Separator   ();
            ImGui::BulletText  ("Any time the cursor is visible");
            ImGui::BulletText  ("Fixes buggy games like Mass Effect Andromeda");
          ImGui::EndTooltip    ();
        }

        ImGui::Checkbox ("Block Input When No Cursor is Visible", &config.input.ui.capture_hidden);  ImGui::SameLine ();

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Generally prevents mouselook if you move your cursor away from the config UI");

        ImGui::Checkbox ("No Warp (UI open)",                     &SK_ImGui_Cursor.prefs.no_warp.ui_open);

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip  ();
            ImGui::TextColored (ImVec4 (1.f, 1.f, 1.f, 1.f), "Do Not Alllow Game to Move Cursor to Center of Screen");
            ImGui::Separator   ();
            ImGui::BulletText  ("Any time the UI is visible");
            ImGui::BulletText  ("May be needed if Mouselook is fighting you tooth and nail.");
          ImGui::EndTooltip    ();
        }

        ImGui::TreePop        ();
        ImGui::EndGroup       ();

#if 0
        extern bool SK_DInput8_BlockWindowsKey (bool block);
        extern bool SK_DInput8_HasKeyboard     (void);

        if (SK_DInput8_HasKeyboard ())
        {
          if (ImGui::Checkbox ("Block Windows Key", &config.input.keyboard.block_windows_key))
          {
            config.input.keyboard.block_windows_key = SK_DInput8_BlockWindowsKey (config.input.keyboard.block_windows_key);
          }
        }
#endif

        ImGui::TreePop        ();
      }

      ImGui::TreePop       ( );
      ImGui::PopStyleColor (3);
    }

    if ( ImGui::CollapsingHeader ("Window Management") )
    {
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.02f, 0.68f, 0.90f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.07f, 0.72f, 0.90f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.14f, 0.78f, 0.87f, 0.80f));
      ImGui::TreePush       ("");

      //
      // If we did this from the render thread, we would deadlock most games
      //
      auto DeferCommand = [=] (const char* szCommand) ->
        void
          {
            CreateThread ( nullptr,
                             0x00,
                          [ ](LPVOID user) ->
              DWORD
                {
                  SK_GetCommandProcessor ()->ProcessCommandLine (
                     (const char*)user
                  );

                  CloseHandle (GetCurrentThread ());

                  return 0;
                },

              (LPVOID)szCommand,
            0x00,
          nullptr
        );
      };

      if (ImGui::CollapsingHeader ("Style and Position", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::TreePush ("");

        bool borderless        = config.window.borderless;
        bool center            = config.window.center;
        bool fullscreen        = config.window.fullscreen;

        if ( ImGui::Checkbox ( "Borderless  ", &borderless ) )
        {
            //config.window.fullscreen = false;

          if (! config.window.fullscreen)
            DeferCommand ("Window.Borderless toggle");
        }

        if (ImGui::IsItemHovered ()) 
        {
          if (! config.window.fullscreen)
          {
            if (borderless)
              ImGui::SetTooltip ("Add/Restore Window Borders");
            else
              ImGui::SetTooltip ("Remove Window Borders");
          }

          else
            ImGui::SetTooltip ("Cannot be Changed while Fullscreen is Checked");
        }

        if (borderless)
        {
          ImGui::SameLine ();

          if ( ImGui::Checkbox ( "Fullscreen (Borderless Upscale)", &fullscreen ) )
          {
            config.window.fullscreen = fullscreen;
            SK_ImGui_AdjustCursor ();
          }

          if (ImGui::IsItemHovered ())
          {
            ImGui::BeginTooltip ();
            ImGui::Text         ("Stretch Borderless Window to Fill Monitor");
            ImGui::Separator    ();
            ImGui::BulletText   ("Framebuffer Resolution Unchanged (GPU-Side Upscaling)");
            ImGui::BulletText   ("Upscaling to Match Desktop Resolution Adds AT LEAST 1 Frame of Input Latency!");
            ImGui::EndTooltip   ();
          }
        }

        if (! config.window.fullscreen)
        {
          ImGui::InputInt2 ("Override Resolution", (int *)&config.window.res.override.x);

          if (ImGui::IsItemHovered ())
          {
            ImGui::BeginTooltip ();
            ImGui::Text ("Set if Game's Window Resolution is Reported Wrong");
            ImGui::Separator ();
            ImGui::BulletText ("0x0 = Disable");
            ImGui::BulletText ("Applied the Next Time a Style/Position Setting is Changed");
            ImGui::EndTooltip ();
          }
        }

        if (! (config.window.borderless && config.window.fullscreen))
        {
          if ( ImGui::Checkbox ( "Center", &center ) ) {
            config.window.center = center;
            SK_ImGui_AdjustCursor ();
            //DeferCommand ("Window.Center toggle");
          }

          if (ImGui::IsItemHovered ()) {
            ImGui::BeginTooltip ();
            ImGui::Text         ("Keep the Render Window Centered at All Times");
            ImGui::Separator    ();
            ImGui::BulletText   ("The Drag-Lock feature cannot be used while Window Centering is turned on.");
            ImGui::EndTooltip   ();
          }

          if (! config.window.center)
          {
            ImGui::TreePush    ("");
            ImGui::TextColored (ImVec4 (1.0f, 1.0f, 0.0f, 1.0f), "\nPress Ctrl + Shift + ScrollLock to Toggle Drag-Lock Mode");
            ImGui::BulletText  ("Useful for Positioning Borderless Windows.");
            ImGui::Text        ("");
            ImGui::TreePop     ();
          }

          bool pixel_perfect = ( config.window.offset.x.percent == 0.0 &&
                                 config.window.offset.y.percent == 0.0 );

          if (ImGui::Checkbox ("Pixel-Perfect Placement", &pixel_perfect))
          {
            if (pixel_perfect) {
              config.window.offset.x.absolute = 0;
              config.window.offset.y.absolute = 0;
              config.window.offset.x.percent  = 0.0f;
              config.window.offset.y.percent  = 0.0f;
            }

            else {
              config.window.offset.x.percent  = 0.000001f;
              config.window.offset.y.percent  = 0.000001f;
              config.window.offset.x.absolute = 0;
              config.window.offset.y.absolute = 0;
            }
          }

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Pixel-Perfect Placement Will Behave Inconsistently If Desktop Resolution Changes");

          if (! config.window.center)
          {
            ImGui::SameLine ();

            ImGui::Checkbox   ("Remember Dragged Position", &config.window.persistent_drag);

            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Store the location of windows moved using Drag-Lock and apply at game startup");
          }

          bool moved = false;

          HMONITOR hMonitor =
            MonitorFromWindow ( game_window.hWnd,
                                  MONITOR_DEFAULTTONEAREST );

          MONITORINFO mi  = { 0 };
          mi.cbSize       = sizeof (mi);
          GetMonitorInfo (hMonitor, &mi);

          if (pixel_perfect)
          {
            int x_pos = std::abs (config.window.offset.x.absolute);
            int y_pos = std::abs (config.window.offset.y.absolute);

            bool right_align  = config.window.offset.x.absolute < 0;
            bool bottom_align = config.window.offset.y.absolute < 0;

            int extent_x = (mi.rcMonitor.right  - mi.rcMonitor.left) / 2 + 1;
            int extent_y = (mi.rcMonitor.bottom - mi.rcMonitor.top)  / 2 + 1;

            if (config.window.center) {
              extent_x /= 2;
              extent_y /= 2;
            }

            // Do NOT Apply Immediately or the Window Will Oscillate While
            //   Adjusting the Slider
            static bool queue_move = false;

            moved  = ImGui::SliderInt ("X Offset##WindowPix",       &x_pos, 0, extent_x, "%.0f pixels"); ImGui::SameLine ();
            moved |= ImGui::Checkbox  ("Right-aligned##WindowPix",  &right_align);
            moved |= ImGui::SliderInt ("Y Offset##WindowPix",       &y_pos, 0, extent_y, "%.0f pixels"); ImGui::SameLine ();
            moved |= ImGui::Checkbox  ("Bottom-aligned##WindowPix", &bottom_align);

            if (moved)
              queue_move = true;

            // We need to set pixel offset to 1 to do what the user expects
            //   these values to do... 0 = NO OFFSET, but the slider may move
            //     from right-to-left skipping 1.
            static bool reset_x_to_zero = false;
            static bool reset_y_to_zero = false;

            if (moved)
            {
              config.window.offset.x.absolute = x_pos * (right_align  ? -1 : 1);
              config.window.offset.y.absolute = y_pos * (bottom_align ? -1 : 1);

              if (right_align && config.window.offset.x.absolute >= 0)
                config.window.offset.x.absolute = -1;

              if (bottom_align && config.window.offset.y.absolute >= 0)
                config.window.offset.y.absolute = -1;

              if (config.window.offset.x.absolute == 0) {
                config.window.offset.x.absolute = 1;
                reset_x_to_zero = true;
              }

              if (config.window.offset.y.absolute == 0) {
                config.window.offset.y.absolute = 1;
                reset_y_to_zero = true;
              }
            }

            if (queue_move && (! ImGui::IsMouseDown (0)))
            {
              queue_move = false;

              SK_ImGui_AdjustCursor ();

              if (reset_x_to_zero) config.window.offset.x.absolute = 0;
              if (reset_y_to_zero) config.window.offset.y.absolute = 0;

              if (reset_x_to_zero || reset_y_to_zero)
                SK_ImGui_AdjustCursor ();

              reset_x_to_zero = false; reset_y_to_zero = false;
            }
          }

          else
          {
            float x_pos = std::abs (config.window.offset.x.percent);
            float y_pos = std::abs (config.window.offset.y.percent);

            x_pos *= 100.0f;
            y_pos *= 100.0f;

            bool right_align  = config.window.offset.x.percent < 0.0f;
            bool bottom_align = config.window.offset.y.percent < 0.0f;

            float extent_x = 50.05f;
            float extent_y = 50.05f;

            if (config.window.center) {
              extent_x /= 2.0f;
              extent_y /= 2.0f;
            }

            // Do NOT Apply Immediately or the Window Will Oscillate While
            //   Adjusting the Slider
            static bool queue_move = false;

            moved  = ImGui::SliderFloat ("X Offset##WindowRel",       &x_pos, 0, extent_x, "%.3f %%"); ImGui::SameLine ();
            moved |= ImGui::Checkbox    ("Right-aligned##WindowRel",  &right_align);
            moved |= ImGui::SliderFloat ("Y Offset##WindowRel",       &y_pos, 0, extent_y, "%.3f %%"); ImGui::SameLine ();
            moved |= ImGui::Checkbox    ("Bottom-aligned##WindowRel", &bottom_align);

            // We need to set pixel offset to 1 to do what the user expects
            //   these values to do... 0 = NO OFFSET, but the slider may move
            //     from right-to-left skipping 1.
            static bool reset_x_to_zero = false;
            static bool reset_y_to_zero = false;

            if (moved)
              queue_move = true;

            if (moved)
            {
              x_pos /= 100.0f;
              y_pos /= 100.0f;

              config.window.offset.x.percent = x_pos * (right_align  ? -1.0f : 1.0f);
              config.window.offset.y.percent = y_pos * (bottom_align ? -1.0f : 1.0f);

              if (right_align && config.window.offset.x.percent >= 0.0f)
                config.window.offset.x.percent = -0.01f;

              if (bottom_align && config.window.offset.y.percent >= 0.0f)
                config.window.offset.y.percent = -0.01f;

              if ( config.window.offset.x.percent <  0.000001f &&
                   config.window.offset.x.percent > -0.000001f )
              {
                config.window.offset.x.absolute = 1;
                reset_x_to_zero = true;
              }

              if ( config.window.offset.y.percent <  0.000001f &&
                   config.window.offset.y.percent > -0.000001f )
              {
                config.window.offset.y.absolute = 1;
                reset_y_to_zero = true;
              }
            }

            if (queue_move && (! ImGui::IsMouseDown (0)))
            {
              queue_move = false;

              SK_ImGui_AdjustCursor ();

              if (reset_x_to_zero) config.window.offset.x.absolute = 0;
              if (reset_y_to_zero) config.window.offset.y.absolute = 0;

              if (reset_x_to_zero || reset_y_to_zero)
                SK_ImGui_AdjustCursor ();

              reset_x_to_zero = false; reset_y_to_zero = false;
            }
          }
        }

        ImGui::TreePop ();
      }

      if (ImGui::CollapsingHeader ("Input/Output Behavior", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::TreePush ("");

        bool confine           = config.window.confine_cursor;
        bool unconfine         = config.window.unconfine_cursor;
        bool background_render = config.window.background_render;
        bool background_mute   = config.window.background_mute;

        ImGui::Text     ("Background Behavior");
        ImGui::TreePush ("");

        if ( ImGui::Checkbox ( "Mute Game ", &background_mute ) )
          DeferCommand ("Window.BackgroundMute toggle");
        
        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Mute the Game when Another Window has Input Focus");

        ImGui::SameLine ();

        if ( ImGui::Checkbox ( "Continue Rendering", &background_render ) )
          DeferCommand ("Window.BackgroundRender toggle");

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip ();
          ImGui::Text         ("Block Application Switch Notifications to the Game");
          ImGui::Separator    ();
          ImGui::BulletText   ("Most Games will Continue Rendering");
          ImGui::BulletText   ("Disables a Game's Built-in Mute-on-Alt+Tab Functionality");
          ImGui::BulletText   ("Keyboard/Mouse Input is Blocked, but not Gamepad Input");
          ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.8f, 0.1f, 1.0f));
          ImGui::BulletText   ("DO NOT USE THIS IN FULLSCREEN EXCLUSIVE MODE");
          ImGui::PopStyleColor ();
          ImGui::SameLine     ();
          ImGui::Text         (", Alt + Tab will not work.");
          ImGui::EndTooltip   ();
        }

        ImGui::TreePop ();

        ImGui::Text     ("Cursor Boundaries");
        ImGui::TreePush ("");
        
        int  ovr     = 0;
        bool changed = false;

        if (config.window.confine_cursor)
          ovr = 1;
        if (config.window.unconfine_cursor)
          ovr = 2;

        changed |= ImGui::RadioButton ("Normal Game Behavior", &ovr, 0); ImGui::SameLine ();
        changed |= ImGui::RadioButton ("Keep Inside Window",   &ovr, 1); ImGui::SameLine ();

        if (ImGui::IsItemHovered ())
        {
          ImGui::BeginTooltip ();
          ImGui::Text         ("Prevents Mouse Cursor from Leaving the Game Window");
          ImGui::Separator    ();
          ImGui::BulletText   ("This window-lock will be disengaged when you press Alt + Tab");
          ImGui::EndTooltip   ();
        }

        changed |= ImGui::RadioButton ("Unrestrict Cursor",    &ovr, 2);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Prevent Game from Restricting Cursor to Window");

        if (changed)
        {
          switch (ovr)
          {
            case 0:
              config.window.confine_cursor   = 0;
              config.window.unconfine_cursor = 0;
              break;
            case 1:
              config.window.confine_cursor   = 1;
              config.window.unconfine_cursor = 0;
              break;
            case 2:
              config.window.confine_cursor   = 0;
              config.window.unconfine_cursor = 1;
              break;
          }

          SK_ImGui_AdjustCursor ();
        }

        ImGui::TreePop ();
        ImGui::TreePop ();
      }

      ImGui::TreePop       ( );
      ImGui::PopStyleColor (3);
    }

    if (ImGui::CollapsingHeader ("Volume Management"))
    {
      std::string app_name;

      sessions.Activate ();

      CComPtr <IAudioMeterInformation> pMeterInfo =
        sessions.getMeterInfo ();

      if (pMeterInfo == nullptr)
        audio_session = nullptr;

      if (audio_session != nullptr)
        app_name = audio_session->getName ();

      app_name += "###AudioSessionAppName";

      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.02f, 0.90f, 0.68f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.07f, 0.90f, 0.72f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.14f, 0.87f, 0.78f, 0.80f));

      ImGui::Columns (2, "Amazon Sep", false);

      bool selected = true;
      if (ImGui::Selectable (app_name.c_str (), &selected))
        ImGui::OpenPopup ("Audio Session Selector");

      ImGui::PopStyleColor (3);

      if (ImGui::IsItemHovered ())
        ImGui::SetTooltip ("Click Here to Manage Another Application.");

      ImGui::SetColumnOffset (1, 450);
      ImGui::NextColumn      (      );

      ImGui::BeginGroup ();
      {
        ImGui::PushItemWidth (-1);
        if (ImGui::Button (u8"  <<  ")) {
          keybd_event_Original (VK_MEDIA_PREV_TRACK, 0x22, 1, 0);
          keybd_event_Original (VK_MEDIA_PREV_TRACK, 0x22, 3, 0);
        }

        ImGui::SameLine ();

        if (ImGui::Button (u8"  Play / Pause  ")) {
          keybd_event_Original (VK_MEDIA_PLAY_PAUSE, 0x22, 1, 0);
          keybd_event_Original (VK_MEDIA_PLAY_PAUSE, 0x22, 3, 0); 
        }

        ImGui::SameLine ();

        if (ImGui::Button (u8"  >>  ")) {
          keybd_event_Original (VK_MEDIA_NEXT_TRACK, 0x22, 1, 0);
          keybd_event_Original (VK_MEDIA_NEXT_TRACK, 0x22, 3, 0);
        }
        ImGui::PopItemWidth ();
      }
      ImGui::EndGroup   ();
      ImGui::Columns    (1);


      bool session_changed = SK_ImGui_SelectAudioSessionDlg ();

      ImGui::TreePush ("");

      if (audio_session == nullptr)
      {
        int count;

        SK_WASAPI_AudioSession** ppSessions =
          sessions.getActive (&count);

        // Find the session for the current game and select that first...
        for (int i = 0; i < count; i++)
        {
          if (ppSessions [i]->getProcessId () == GetCurrentProcessId ())
          {
            audio_session = ppSessions [i];
            break;
          }
        }
      }

      else
      {
        CComPtr <IChannelAudioVolume> pChannelVolume =
          audio_session->getChannelAudioVolume ();
        CComPtr <ISimpleAudioVolume>  pVolume        =
          audio_session->getSimpleAudioVolume ();

        UINT channels = 0;

        if (SUCCEEDED (pMeterInfo->GetMeteringChannelCount (&channels)))
        {
          static float channel_peaks_ [32];

          struct
          {
            struct {
              float inst_min = FLT_MAX;  DWORD dwMinSample = 0;  float disp_min = FLT_MAX;
              float inst_max = FLT_MIN;  DWORD dwMaxSample = 0;  float disp_max = FLT_MIN;
            } vu_peaks;

            float peaks [120];
            int   current_idx;
          } static history [32];

          #define VUMETER_TIME 300

          static float master_vol  = -1.0f; // Master Volume
          static BOOL  master_mute =  FALSE;

          struct volume_s
          {
            float volume           =  1.0f; // Current Volume (0.0 when muted)
            float normalized       =  1.0f; // Unmuted Volume (stores volume before muting)

            bool  muted            = false;

            // Will fill-in with unique names for ImGui
            //   (all buttons say the same thing =P)
            //
            char mute_button  [14] = { '\0' };
            char slider_label [8 ] = { '\0' };
          };

          static std::map <int, volume_s> channel_volumes;

          pVolume->GetMasterVolume (&master_vol);
          pVolume->GetMute         (&master_mute);

          const char* szMuteButtonTitle = ( master_mute ? "  Unmute  ###MasterMute" :
                                                          "   Mute   ###MasterMute" );

          if (ImGui::Button (szMuteButtonTitle))
          {
            master_mute ^= TRUE;

            pVolume->SetMute (master_mute, nullptr);
          }

          ImGui::SameLine ();

          float val = master_mute ? 0.0f : 1.0f;

          ImGui::PushStyleColor (ImGuiCol_FrameBg,        ImColor ( 0.3f,  0.3f,  0.3f,  val));
          ImGui::PushStyleColor (ImGuiCol_FrameBgHovered, ImColor ( 0.6f,  0.6f,  0.6f,  val));
          ImGui::PushStyleColor (ImGuiCol_FrameBgActive,  ImColor ( 0.9f,  0.9f,  0.9f,  val));
          ImGui::PushStyleColor (ImGuiCol_SliderGrab,     ImColor ( 1.0f,  1.0f,  1.0f, 1.0f));
          ImGui::PushStyleColor (ImGuiCol_Text,           ImColor::HSV ( 0.15f, 0.0f,
                                                                           0.5f + master_vol * 0.5f) );

          if (ImGui::SliderFloat ("      Master Volume Control  ", &master_vol, 0.0, 1.0, ""))
          {
            if (master_mute)
            {
                                master_mute = FALSE;
              pVolume->SetMute (master_mute, nullptr);
            }

            pVolume->SetMasterVolume (master_vol, nullptr);
          }

          ImGui::SameLine ();

          ImGui::TextColored ( ImColor::HSV ( 0.15f, 0.9f,
                                                0.5f + master_vol * 0.5f),
                                 "(%03.1f%%)  ",
                                   master_vol * 100.0f );

          ImGui::PopStyleColor (5);
          ImGui::Separator     ( );
          ImGui::Columns       (2);

          for (UINT i = 0 ; i < channels; i++)
          {
            if (SUCCEEDED (pMeterInfo->GetChannelsPeakValues (channels, channel_peaks_)))
            {
              history [i].vu_peaks.inst_min = std::min (history [i].vu_peaks.inst_min, channel_peaks_ [i]);
              history [i].vu_peaks.inst_max = std::max (history [i].vu_peaks.inst_max, channel_peaks_ [i]);

              history [i].vu_peaks.disp_min    = history [i].vu_peaks.inst_min;

              if (history [i].vu_peaks.dwMinSample < timeGetTime () - VUMETER_TIME * 3) {
                history [i].vu_peaks.inst_min    = channel_peaks_ [i];
                history [i].vu_peaks.dwMinSample = timeGetTime ();
              }

              history [i].vu_peaks.disp_max    = history [i].vu_peaks.inst_max;

              if (history [i].vu_peaks.dwMaxSample < timeGetTime () - VUMETER_TIME * 3) {
                history [i].vu_peaks.inst_max    = channel_peaks_ [i];
                history [i].vu_peaks.dwMaxSample = timeGetTime ();
              }

              history [i].peaks [history [i].current_idx] = channel_peaks_ [i];
              history [i].current_idx = (history [i].current_idx + 1) % IM_ARRAYSIZE (history [i].peaks);

              ImGui::BeginGroup ();

              const float ht = font_size * 6;

              if (channel_volumes.count (i) == 0)
              {
                session_changed = true;

                snprintf (channel_volumes [i].mute_button, 13, "  Mute  ##%lu", i);
                snprintf (channel_volumes [i].slider_label, 7, "##vol%lu",      i);
              }

              if (pChannelVolume && SUCCEEDED (pChannelVolume->GetChannelVolume (i, &channel_volumes [i].volume)))
              {
                volume_s& ch_vol =
                  channel_volumes [i];

                if (session_changed)
                {
                  channel_volumes [i].muted      = (channel_volumes [i].volume <= 0.001f);
                  channel_volumes [i].normalized = (channel_volumes [i].volume  > 0.001f ?
                                                    channel_volumes [i].volume : 1.0f);
                }

                bool  changed     = false;
                float volume_orig = ch_vol.normalized;

                ImGui::BeginGroup  ();
                ImGui::TextColored (ImVec4 (0.80f, 0.90f, 1.0f,  1.0f), "%-22s", SK_WASAPI_GetChannelName (i));
                                                                                          ImGui::SameLine ( );
                ImGui::TextColored (ImVec4 (0.7f,  0.7f,  0.7f,  1.0f), "      Volume:"); ImGui::SameLine ( );
                ImGui::TextColored (ImColor::HSV (
                                            0.25f, 0.9f,
                                              0.5f + ch_vol.volume * 0.5f),
                                                                        "%03.1f%%   ", 100.0f * ch_vol.volume);
                ImGui::EndGroup    ();

                ImGui::BeginGroup  ();

                       val        = ch_vol.muted ? 0.1f : 0.5f;
                float *pSliderVal = ch_vol.muted ? &ch_vol.normalized : &ch_vol.volume;

                ImGui::PushStyleColor (ImGuiCol_FrameBg,        ImColor::HSV ( ( i + 1 ) / (float)channels, 0.5f, val));
                ImGui::PushStyleColor (ImGuiCol_FrameBgHovered, ImColor::HSV ( ( i + 1 ) / (float)channels, 0.6f, val));
                ImGui::PushStyleColor (ImGuiCol_FrameBgActive,  ImColor::HSV ( ( i + 1 ) / (float)channels, 0.7f, val));
                ImGui::PushStyleColor (ImGuiCol_SliderGrab,     ImColor::HSV ( ( i + 1 ) / (float)channels, 0.9f, 0.9f));

                changed =
                  ImGui::VSliderFloat ( ch_vol.slider_label,
                                          ImVec2 (font_size * 1.5f, ht),
                                            pSliderVal,
                                              0.0f, 1.0f,
                                                "" );

                ImGui::PopStyleColor  (4);

                if (changed)
                {
                   ch_vol.volume =
                     ch_vol.muted  ? ch_vol.normalized :
                                     ch_vol.volume;

                  if ( SUCCEEDED ( pChannelVolume->SetChannelVolume ( i,
                                                                        ch_vol.volume,
                                                                          nullptr )
                                 )
                     )
                  {
                    ch_vol.muted      = false;
                    ch_vol.normalized = ch_vol.volume;
                  }

                  else
                    ch_vol.normalized = volume_orig;
                }

                if ( SK_ImGui::VerticalToggleButton (  ch_vol.mute_button,
                                                      &ch_vol.muted )
                   )
                {
                  if (ch_vol.muted)
                  {
                    ch_vol.normalized = ch_vol.volume;
                    pChannelVolume->SetChannelVolume (i, 0.0f,              nullptr);
                  }
                  else
                    pChannelVolume->SetChannelVolume (i, ch_vol.normalized, nullptr);
                }

                ImGui::EndGroup ();
                ImGui::SameLine ();
              }

              ImGui::BeginGroup ();
              ImGui::PlotLines ( "",
                                  history [i].peaks,
                                    IM_ARRAYSIZE (history [i].peaks),
                                      history [i].current_idx,
                                        "",
                                             history [i].vu_peaks.disp_min,
                                             1.0f,
                                              ImVec2 (ImGui::GetContentRegionAvailWidth (), ht) );

              ImGui::PushStyleColor (ImGuiCol_PlotHistogram,     ImVec4 (0.9f, 0.1f, 0.1f, 0.15f));
              ImGui::ProgressBar    (history [i].vu_peaks.disp_max, ImVec2 (-1.0f, 0.0f));
              ImGui::PopStyleColor  ();

              ImGui::ProgressBar    (channel_peaks_ [i],          ImVec2 (-1.0f, 0.0f));

              ImGui::PushStyleColor (ImGuiCol_PlotHistogram,     ImVec4 (0.1f, 0.1f, 0.9f, 0.15f));
              ImGui::ProgressBar    (history [i].vu_peaks.disp_min, ImVec2 (-1.0f, 0.0f));
              ImGui::PopStyleColor  ();
              ImGui::EndGroup       ();
              ImGui::EndGroup       ();

              if (! (i % 2))
              {
                ImGui::SameLine (); ImGui::NextColumn ();
              } else {
                ImGui::Columns   ( 1 );
                ImGui::Separator (   );
                ImGui::Columns   ( 2 );
              }
            }
          }

          ImGui::Columns (1);
        }

        // Upon failure, deactivate and try to get a new session manager on the next
        //   frame.
        else
        {
          audio_session = nullptr;
          sessions.Deactivate ();
        }
      }

      ImGui::TreePop ();
    }

    if (ImGui::CollapsingHeader ("On Screen Display (OSD)"))
    {
      ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
      ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
      ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));
      ImGui::TreePush       ("");

      if (ImGui::CollapsingHeader ("Basic Monitoring", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::TreePush ("");

        ImGui::Checkbox ("Title/Clock ",   &config.time.show); ImGui::SameLine ();
        ImGui::Checkbox ("Framerate",      &config.fps.show);

        ImGui::Checkbox ("GPU Stats",      &config.gpu.show);

        if (config.gpu.show)
        {
          ImGui::TreePush ("");

          int temp_unit = config.system.prefer_fahrenheit ? 1 : 0;

          ImGui::RadioButton (" Celsius ",    (int*)&temp_unit, 0); ImGui::SameLine ();
          ImGui::RadioButton (" Fahrenheit ", (int*)&temp_unit, 1);  ImGui::SameLine ();
          ImGui::Checkbox    (" Print Slowdown", &config.gpu.print_slowdown);

          config.system.prefer_fahrenheit = temp_unit == 1 ? true : false;

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("On NVIDIA GPUs, this will print driver throttling details (e.g. power saving)");

          ImGui::TreePop  ();
        }

        ImGui::TreePop ();
      }

      if (ImGui::CollapsingHeader ("Extended Monitoring"))
      {
        ImGui::TreePush     ("");

        ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 16.0f);
        ImGui::BeginChild   ("WMI Monitors", ImVec2 (font_size * 50.0f,font_size_multiline * 6.05f), true);

        ImGui::PushStyleColor (ImGuiCol_Text, ImVec4 (1.0f, 0.785f, 0.0784f, 1.0f));
        ImGui::TextWrapped    ("These functions spawn a WMI monitoring service and may take several seconds to start.");
        ImGui::PopStyleColor  ();

        extern void
        __stdcall
        SK_StartPerfMonThreads (void);

        bool spawn = false;

        ImGui::Separator ( );
        ImGui::Columns   (3);

        spawn |= ImGui::Checkbox ("CPU Stats",         &config.cpu.show);

        if (config.cpu.show)
        {
          ImGui::TreePush ("");
          ImGui::Checkbox (" Simplified View",         &config.cpu.simple);
          ImGui::TreePop  ();
        }

        spawn |= ImGui::Checkbox ("Memory Stats",      &config.mem.show);

        ImGui::NextColumn ();

        spawn |= ImGui::Checkbox ("General I/O Stats", &config.io.show);
        spawn |= ImGui::Checkbox ("Pagefile Stats",    &config.pagefile.show);

        ImGui::NextColumn ();

        spawn |= ImGui::Checkbox ("Disk Stats",        &config.disk.show);

        if (config.disk.show)
        {
          ImGui::TreePush ("");
          bool hovered = false;

          ImGui::RadioButton (" Logical Disks",  &config.disk.type, 1);
          hovered = ImGui::IsItemHovered ();
          ImGui::RadioButton (" Physical Disks", &config.disk.type, 0);
          hovered |= ImGui::IsItemHovered ();
          ImGui::TreePop  ();

          if (hovered)
            ImGui::SetTooltip ("Requires Application Restart");
        }

        else { ImGui::NewLine (); ImGui::NewLine (); }

        if (spawn)
          SK_StartPerfMonThreads ();

        ImGui::Columns     (1);
        ImGui::Separator   ( );

        ImGui::Columns     (3, "", false);
        ImGui::NextColumn  ();
        ImGui::NextColumn  ();
        ImGui::Checkbox    ("Remember These Settings", &config.osd.remember_state);
        ImGui::Columns     (1);

        ImGui::EndChild    ();
        ImGui::PopStyleVar ();

        ImGui::TreePop     ();
      }

      if (ImGui::CollapsingHeader ("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
      {
        ImGui::TreePush ("");

        float color [3] = { (float)config.osd.red   / 255.0f,
                            (float)config.osd.green / 255.0f,
                            (float)config.osd.blue  / 255.0f };

        if (ImGui::ColorEdit3 ("OSD Color", color)) {
          color [0] = std::max (std::min (color [0], 1.0f), 0.0f);
          color [1] = std::max (std::min (color [1], 1.0f), 0.0f);
          color [2] = std::max (std::min (color [2], 1.0f), 0.0f);

          config.osd.red   = (int)(color [0] * 255);
          config.osd.green = (int)(color [1] * 255);
          config.osd.blue  = (int)(color [2] * 255);
        }

        if (ImGui::SliderFloat ("OSD Scale", &config.osd.scale, 0.5f, 10.0f))
        {
          extern
          void
          __stdcall
          SK_SetOSDScale ( float  fScale,
                           bool   relative,
                           LPCSTR lpAppName );

          SK_SetOSDScale (config.osd.scale, false, nullptr);
        }

        bool moved = false;

        int x_pos = std::abs (config.osd.pos_x);
        int y_pos = std::abs (config.osd.pos_y);

        bool right_align  = config.osd.pos_x < 0;
        bool bottom_align = config.osd.pos_y < 0;

        moved  = ImGui::SliderInt ("X Origin##OSD",       &x_pos, 1, (int)io.DisplaySize.x); ImGui::SameLine ();
        moved |= ImGui::Checkbox  ("Right-aligned##OSD",  &right_align);
        moved |= ImGui::SliderInt ("Y Origin##OSD",       &y_pos, 1, (int)io.DisplaySize.y); ImGui::SameLine ();
        moved |= ImGui::Checkbox  ("Bottom-aligned##OSD", &bottom_align);

        if (moved)
        {
          extern void
            __stdcall
          SK_SetOSDPos (int x, int y, LPCSTR lpAppName);

          config.osd.pos_x = x_pos * (right_align  ? -1 : 1);
          config.osd.pos_y = y_pos * (bottom_align ? -1 : 1);

          if (right_align && config.osd.pos_x >= 0)
            config.osd.pos_x = -1;

          if (bottom_align && config.osd.pos_y >= 0)
            config.osd.pos_y = -1;

          SK_SetOSDPos (config.osd.pos_x, config.osd.pos_y, nullptr);
        }

        ImGui::TreePop ();
      }

      ImGui::TreePop       ( );
      ImGui::PopStyleColor (3);
    }

    if (SK::SteamAPI::AppID () != 0)
    {
      if ( ImGui::CollapsingHeader ("Steam Enhancements", ImGuiTreeNodeFlags_CollapsingHeader | 
                                                          ImGuiTreeNodeFlags_DefaultOpen ) )
      {
        ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.02f, 0.68f, 0.90f, 0.45f));
        ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.07f, 0.72f, 0.90f, 0.80f));
        ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.14f, 0.78f, 0.87f, 0.80f));
        ImGui::TreePush       ("");

        if (SK_SteamAPI_GetNumPossibleAchievements () > 0)
        {
          static char szProgress [128] = { '\0' };

          float  ratio            = SK::SteamAPI::PercentOfAchievementsUnlocked ();
          size_t num_achievements = SK_SteamAPI_GetNumPossibleAchievements      ();

          snprintf ( szProgress, 127, "%.2f%% of Achievements Unlocked (%lu/%lu)",
                       ratio * 100.0f, (uint32_t)(ratio * (float)num_achievements),
                                       (uint32_t)                num_achievements );

          ImGui::PushStyleColor ( ImGuiCol_PlotHistogram, ImColor (0.90f, 0.72f, 0.07f, 0.80f) ); 
          ImGui::ProgressBar    ( ratio,
                                    ImVec2 (-1, 0),
                                      szProgress );
          ImGui::PopStyleColor  ();

          int friends = SK_SteamAPI_GetNumFriends ();

          if (friends && ImGui::IsItemHovered ())
          {
            ImGui::BeginTooltip ( );

            static int num_records = 0;

            int max_lines = (int)((io.DisplaySize.y * 0.725f) / (font_size_multiline * 0.9f));
            int cur_line  = 0;
              num_records = 0;

            ImGui::BeginGroup     ();

            ImGui::PushStyleColor ( ImGuiCol_Text,          ImColor (255, 255, 255)              ); 
            ImGui::PushStyleColor ( ImGuiCol_PlotHistogram, ImColor (0.90f, 0.72f, 0.07f, 0.80f) ); 

            for (int i = 0; i < (int)((float)friends * SK_SteamAPI_FriendStatPercentage ()); i++)
            {
              size_t      len     = 0;
              const char* szName  = SK_SteamAPI_GetFriendName (i, &len);

              float       percent =
                SK_SteamAPI_GetUnlockedPercentForFriend (i);

              if (percent > 0.0f)
              {
                ImGui::ProgressBar    ( percent, ImVec2 (io.DisplaySize.x * 0.0816f, 0.0f) );
                ImGui::SameLine       ();
                ImGui::PushStyleColor (ImGuiCol_Text, ImColor (.81f, 0.81f, 0.81f));
                ImGui::Text           (szName);
                ImGui::PopStyleColor  (1);

                ++num_records;

                if (cur_line >= max_lines)
                {
                  ImGui::EndGroup   ();
                  ImGui::SameLine   ();
                  ImGui::BeginGroup ();
                  cur_line = 0;
                }

                else
                  ++cur_line;
              }
            }

            ImGui::PopStyleColor  (2);
            ImGui::EndGroup       ( );
            ImGui::EndTooltip     ( );
          }

          if (ImGui::CollapsingHeader ("Achievements") )
          {
            ImGui::TreePush ("");
            ImGui::BeginGroup ();
            extern void
            SK_UnlockSteamAchievement (uint32_t idx);

            if (ImGui::Button (" Test Unlock "))
              SK_UnlockSteamAchievement (0);

            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Perform a FAKE unlock so that you can tune your preferences.");

            ImGui::SameLine ();

            ImGui::Checkbox ("Play Sound ", &config.steam.achievements.play_sound);

            if (config.steam.achievements.play_sound) {
              ImGui::SameLine ();
              
              int i = 0;
              
              if (! _wcsicmp (config.steam.achievements.sound_file.c_str (), L"xbox"))
                i = 1;
              else
                i = 0;
              
              if (ImGui::Combo ("", &i, "PlayStation Network\0Xbox Live\0\0", 2))
              {
                if (i == 0) config.steam.achievements.sound_file = L"psn";
                       else config.steam.achievements.sound_file = L"xbox";
              
                extern void
                SK_SteamAPI_LoadUnlockSound (const wchar_t* wszUnlockSound);
              
                SK_SteamAPI_LoadUnlockSound (config.steam.achievements.sound_file.c_str ());
              }
            }

            ImGui::EndGroup ();
            ImGui::SameLine ();

            ImGui::Checkbox ("Take Screenshot", &config.steam.achievements.take_screenshot);

            ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.68f, 0.02f, 0.45f));
            ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.72f, 0.07f, 0.80f));
            ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.78f, 0.14f, 0.80f));

            if (ImGui::CollapsingHeader ("Enhanced Popup"))
            {
              ImGui::TreePush ("");

              int  mode    = (config.steam.achievements.popup.show + config.steam.achievements.popup.animate);
              bool changed = false;

              ImGui::Text          ("Draw Mode:");                              ImGui::SameLine ();

              changed |=
                ImGui::RadioButton ("Disabled ##AchievementPopup",   &mode, 0); ImGui::SameLine ();
              changed |=
                ImGui::RadioButton ("Stationary ##AchievementPopup", &mode, 1); ImGui::SameLine ();
              ImGui::BeginGroup    ( );
              changed |=
                ImGui::RadioButton ("Animated ##AchievementPopup",   &mode, 2);

                ImGui::SameLine   ( );
                ImGui::Combo      ( "##PopupLoc",         &config.steam.achievements.popup.origin,
                                            "Top-Left\0"
                                            "Top-Right\0"
                                            "Bottom-Left\0"
                                            "Bottom-Right\0\0" );

              if ( changed )
              {
                config.steam.achievements.popup.show    = (mode > 0);
                config.steam.achievements.popup.animate = (mode > 1);

                // Make sure the duration gets set non-zero when this changes
                if (config.steam.achievements.popup.show)
                {
                  if ( config.steam.achievements.popup.duration == 0 )
                    config.steam.achievements.popup.duration = 6666UL;
                }
              }

              if (config.steam.achievements.popup.show)
              {
                ImGui::BeginGroup ( );
                ImGui::TreePush   ("");
                ImGui::Text       ("Duration:"); ImGui::SameLine ();

                float duration = std::max ( 1.0f, ( (float)config.steam.achievements.popup.duration / 1000.0f ) );

                if ( ImGui::SliderFloat ( "##PopupDuration", &duration, 1.0f, 30.0f, "%.2f Seconds" ) )
                {
                  config.steam.achievements.popup.duration = static_cast <LONG> ( duration * 1000.0f );
                }
                ImGui::TreePop   ( );
                ImGui::EndGroup  ( );
              }
              ImGui::EndGroup    ( );
              
              //ImGui::SliderFloat ("Inset Percentage",    &config.steam.achievements.popup.inset, 0.0f, 1.0f, "%.3f%%", 0.01f);
              ImGui::TreePop     ( );
            }

            ImGui::TreePop       ( );
            ImGui::PopStyleColor (3);
          }
        }

        ISteamUtils* utils =
          SK_SteamAPI_Utils ();

        if (utils != nullptr && utils->IsOverlayEnabled () && ImGui::CollapsingHeader ("Overlay Notifications"))
        {
          ImGui::TreePush  ("");

          if (ImGui::Combo ( " ", &config.steam.notify_corner,
                                    "Top-Left\0"
                                    "Top-Right\0"
                                    "Bottom-Left\0"
                                    "Bottom-Right\0"
                                    "(Let Game Decide)\0\0" ))
          {
            SK_Steam_SetNotifyCorner ();
          }

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Applies Only to Traditional Overlay (not Big Picture)");

          ImGui::TreePop ();
        }

        ImGui::PushStyleColor (ImGuiCol_Header,        ImVec4 (0.90f, 0.40f, 0.40f, 0.45f));
        ImGui::PushStyleColor (ImGuiCol_HeaderHovered, ImVec4 (0.90f, 0.45f, 0.45f, 0.80f));
        ImGui::PushStyleColor (ImGuiCol_HeaderActive,  ImVec4 (0.87f, 0.53f, 0.53f, 0.80f));

        if (ImGui::CollapsingHeader ("Compatibility"))
        {
          ImGui::TreePush ("");
          ImGui::Checkbox (" Load Steam Overlay Early  ", &config.steam.preload_overlay);

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("Can make the Steam Overlay work in situations it otherwise would not.");

          ImGui::Checkbox (" Load Steam Client DLL Early  ", &config.steam.preload_client);

          if (ImGui::IsItemHovered ())
            ImGui::SetTooltip ("May prevent some Steam DRM-based games from hanging at startup.");

          ImGui::SameLine ();

          ImGui::Checkbox (" Disable User Stats Receipt Callback", &config.steam.block_stat_callback);

          if (ImGui::IsItemHovered ()) {
            ImGui::BeginTooltip ();
            ImGui::Text         ("Fix for Games that Panic when Flooded with Achievement Data");
            ImGui::Separator    ();
            ImGui::BulletText   ("These Games may shutdown SteamAPI when Special K fetches Friend Achievements");
            ImGui::BulletText   ("If SteamAPI Frame Counter is STUCK, turn this option ON and restart the Game");
            ImGui::EndTooltip   ();
          }

          ImGui::TreePop  ();
        }

        ImGui::PopStyleColor (3);

        bool valid = (! config.steam.silent);

        valid = valid && (! SK_Steam_PiratesAhoy ());

        if (valid)
          ImGui::MenuItem ("I am not a pirate", "", &valid, false);
        else
        {
          if (ImGui::MenuItem ("I am a filthy pirate!"))
          {
            if (GetFileAttributes (L"CPY.ini") != INVALID_FILE_ATTRIBUTES)
            {
              DeleteFileW (L"CPY.ini");
              
              MoveFileW   (L"steam_api64.dll",   L"CPY.ini");
              MoveFileW   (L"steamclient64.dll", L"steam_api64.dll");
              MoveFileW   (L"CPY.ini",           L"steamclient64.dll");
            }
          }
        }

        ImGui::PopStyleColor (3);
        ImGui::TreePop       ( );
      }

      SK_ImGui::BatteryMeter ();

      //ImGui::CollapsingHeader ("Window Management");
      //if (ImGui::CollapsingHeader ("Software Updates")) {
        //config.
      //}

      ImGui::Columns    ( 1 );
      ImGui::Separator  (   );

      if (SK::SteamAPI::GetNumPlayers () > 1)
      {
        ImGui::Columns    ( 2, "SteamSep", true );

        static char szNumber       [16] = { '\0' };
        static char szPrettyNumber [32] = { '\0' };

        const NUMBERFMTA fmt = { 0, 0, 3, ".", ",", 0 };

        snprintf (szNumber, 15, "%li", SK::SteamAPI::GetNumPlayers ());

        GetNumberFormatA ( MAKELCID (LOCALE_USER_DEFAULT, SORT_DEFAULT),
                             0x00,
                               szNumber, &fmt,
                                 szPrettyNumber, 32 );

        ImGui::Text       (" %s Players in-Game on Steam  ", szPrettyNumber);
        ImGui::NextColumn (   );
      }

      extern volatile LONGLONG SK_SteamAPI_CallbackRunCount;

      ImGui::Bullet     ();   ImGui::SameLine ();

      bool pause =
        SK_SteamAPI_GetOverlayState (false);

      if (ImGui::Selectable ( "SteamAPI Frame", &pause ))
        SK_SteamAPI_SetOverlayState (pause);

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip   (       );
        ImGui::Text           ( "In"  );                 ImGui::SameLine ();
        ImGui::PushStyleColor ( ImGuiCol_Text, ImColor (0.95f, 0.75f, 0.25f, 1.0f) ); 
        ImGui::Text           ( "Steam Overlay Aware");  ImGui::SameLine ();
        ImGui::PopStyleColor  (       );
        ImGui::Text           ( "software, click to toggle the game's overlay pause mode." );
        ImGui::EndTooltip     (       );
      }

      ImGui::SameLine ();
      ImGui::Text     ( ": %10llu  ", InterlockedAdd64 (&SK_SteamAPI_CallbackRunCount, 0) );
      ImGui::Columns(1, nullptr, false);

      // If fetching stats and online...
      if ((! SK_SteamAPI_FriendStatsFinished ()) && SK_SteamAPI_GetNumPlayers () > 1)
      {
        float ratio   = SK_SteamAPI_FriendStatPercentage ();
        int   friends = SK_SteamAPI_GetNumFriends        ();

        static char szLabel [512] = { '\0' };

        snprintf ( szLabel, 511,
                     "Fetching Achievements... %.2f%% (%lu/%lu) : %s",
                       100.0f * ratio,
                      (int32_t)(ratio * (float)friends),
                                               friends,
                      SK_SteamAPI_GetFriendName (
                        (int32_t)(ratio * (float)friends)
                      )
                 );
        
        ImGui::PushStyleColor ( ImGuiCol_PlotHistogram, ImColor (0.90f, 0.72f, 0.07f, 0.80f) ); 
        ImGui::PushStyleColor ( ImGuiCol_Text,          ImColor (255, 255, 255)              ); 
        ImGui::ProgressBar (
          SK_SteamAPI_FriendStatPercentage (),
            ImVec2 (-1, 0), szLabel );
        ImGui::PopStyleColor  (2);
      }
    }

  ImVec2 pos  = ImGui::GetWindowPos  ();
  ImVec2 size = ImGui::GetWindowSize ();

  SK_ImGui_LastWindowCenter.x = pos.x + size.x / 2.0f;
  SK_ImGui_LastWindowCenter.y = pos.y + size.y / 2.0f;

  if (recenter)
    SK_ImGui_CenterCursorOnWindow ();

  ImGui::End   ();

  return open;
}

#include <string>
#include <vector>

#include <SpecialK/render_backend.h>

//
// Hook this to override Special K's GUI
//
__declspec (dllexport)
DWORD
SK_ImGui_DrawFrame ( _Unreferenced_parameter_ DWORD  dwFlags, 
                                              LPVOID lpUser )
{
  UNREFERENCED_PARAMETER (dwFlags);
  UNREFERENCED_PARAMETER (lpUser);

  bool d3d9  = false;
  bool d3d11 = false;
  bool gl    = false;

  if (SK_GetCurrentRenderBackend ().api == SK_RenderAPI::OpenGL) {
    gl = true;

    ImGui_ImplGL3_NewFrame ();
  }

  else if ((int)SK_GetCurrentRenderBackend ().api & (int)SK_RenderAPI::D3D9) {
    d3d9 = true;

    ImGui_ImplDX9_NewFrame ();
  }

  else if (SK_GetCurrentRenderBackend ().api == SK_RenderAPI::D3D11) {
    d3d11 = true;

    ImGui_ImplDX11_NewFrame ();
  }

  else
    return 0x00;

  // Default action is to draw the Special K Control panel,
  //   but if you hook this function you can do anything you
  //     want...
  //
  //    To use the Special K Control Panel from a plug-in,
  //      import SK_ImGui_ControlPanel and call that somewhere
  //        from your hook.

  bool keep_open = SK_ImGui_ControlPanel ();

  if (keep_open)
  {
    if (d3d9) {
      extern LPDIRECT3DDEVICE9 g_pd3dDevice;

      if ( SUCCEEDED (
             g_pd3dDevice->BeginScene ()
           )
         )
      {
        ImGui::Render          ();
        g_pd3dDevice->EndScene ();
      }
    }

    else if (d3d11)
      ImGui::Render ();

    else if (gl)
      ImGui::Render ();
  }

  else
    SK_ImGui_Toggle ();

  POINT orig_pos;
  GetCursorPos_Original  (&orig_pos);
  SK_ImGui_Cursor.update ();  
  SetCursorPos_Original  (orig_pos.x, orig_pos.y);

  return 0;
}

BYTE __imgui_keybd_state [512] = { 0 };

// Keys down when the UI started capturing input,
//   the first captured key release event will be
//     sent to the game and the state of that key reset.
//
//  This prevents the game from thinking a key is stuck.
UINT SK_ImGui_ActivationKeys [256] = { 0 };;

__declspec (dllexport)
void
SK_ImGui_Toggle (void)
{
  static DWORD dwLastTime = 0x00;

  bool d3d11 = false;
  bool d3d9  = false;
  bool gl    = false;

  if ((int)SK_GetCurrentRenderBackend ().api & (int)SK_RenderAPI::D3D9)   d3d9  = true;
  if (     SK_GetCurrentRenderBackend ().api ==     SK_RenderAPI::D3D11)  d3d11 = true;
  if (     SK_GetCurrentRenderBackend ().api ==     SK_RenderAPI::OpenGL) gl    = true;

  auto EnableEULAIfPirate = [](void) ->
    bool
    {
      bool pirate = ( SK_SteamAPI_AppID    () != 0 && 
                      SK_Steam_PiratesAhoy () != 0x0 );
      if (pirate)
      {
        if (dwLastTime == 0)
        {
          dwLastTime             = timeGetTime ();

          eula.show              = true;
          eula.never_show_again  = false;

          return true;
        }
      }

      return false;
    };

  if (d3d9 || d3d11 || gl)
  {
    if (SK_ImGui_Visible)
      GetKeyboardState (__imgui_keybd_state);

    SK_ImGui_Visible = (! SK_ImGui_Visible);

    static HMODULE hModTBFix = GetModuleHandle (L"tbfix.dll");
    static HMODULE hModTZFix = GetModuleHandle (L"tzfix.dll");

    if (hModTZFix == nullptr)
    {
      // Turns the hardware cursor on/off as needed
      ImGui_ToggleCursor ();

      // Most games
      if (! hModTBFix)
      {
        // Transition: (Visible -> Invisible)
        if (! SK_ImGui_Visible)
        {
          //SK_ImGui_Cursor.showSystemCursor ();
        }

        else
        {
          //SK_ImGui_Cursor.showImGuiCursor ();

          if (EnableEULAIfPirate ())
            config.imgui.show_eula = true;

          static bool first = true;

          if (first) {
            eula.never_show_again = true;
            eula.show             = config.imgui.show_eula;
            first                 = false;
          }
        }
      }

      // TBFix
      else
        EnableEULAIfPirate ();
    }

    // TZFix
    else
      EnableEULAIfPirate ();

    if (SK_ImGui_Visible)
      SK_Console::getInstance ()->visible = false;


    // Request the number of players in-game whenever the
    //   config UI is toggled ON.
    if (SK::SteamAPI::AppID () != 0 && SK_ImGui_Visible)
      SK::SteamAPI::UpdateNumPlayers ();




    const wchar_t* config_name = SK_GetBackend ();

    if (SK_IsInjected ())
      config_name = L"SpecialK";

    SK_SaveConfig (config_name);


    // Immediately stop capturing keyboard/mouse events,
    //   this is the only way to preserve cursor visibility
    //     in some games (i.e. Tales of Berseria)
    ImGui::GetIO ().WantCaptureKeyboard = (! SK_ImGui_Visible);
    ImGui::GetIO ().WantCaptureMouse    = (! SK_ImGui_Visible);


    // Clear navigation focus on window close
    if (! SK_ImGui_Visible) {
      extern bool nav_usable;
      nav_usable = false;
    }

    ImGui::SetNextWindowFocus ();

    reset_frame_history = true;
  }
}

extern LONG SK_RawInput_MouseX;
extern LONG SK_RawInput_MouseY;
extern POINT SK_RawInput_Mouse;

void
SK_ImGui_CenterCursorOnWindow (void)
{
  ImGuiIO& io =
    ImGui::GetIO ();

  SK_ImGui_Cursor.pos.x = (LONG)(SK_ImGui_LastWindowCenter.x);
  SK_ImGui_Cursor.pos.y = (LONG)(SK_ImGui_LastWindowCenter.y);
  
  io.MousePos.x = SK_ImGui_LastWindowCenter.x;
  io.MousePos.y = SK_ImGui_LastWindowCenter.y;

  POINT screen_pos = SK_ImGui_Cursor.pos;

  if (GetCursor () != nullptr)
    SK_ImGui_Cursor.orig_img = GetCursor ();

  SK_ImGui_Cursor.LocalToScreen (&screen_pos);
  SetCursorPos_Original ( screen_pos.x,
                          screen_pos.y );

  io.WantCaptureMouse = true;

  SK_ImGui_UpdateCursor ();
}


















#if 0
bool
SK_D3D11_TextureModDlg (void)
{
  const float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;

  bool show_dlg = true;

  ImGui::Begin ( "D3D11 Texture Mod Toolkit (v " SK_VERSION_STR_A ")",
                   &show_dlg,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders );

  ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.666f);

  if (ImGui::CollapsingHeader ("Live Texture View", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
  {
    static float last_ht    = 256.0f;
    static float last_width = 256.0f;

    static std::vector <std::string> list_contents;
    static bool                      list_dirty     = false;
    static int                       sel            =     0;

    extern std::vector <uint32_t> textures_used_last_dump;
    extern              uint32_t  tex_dbg_idx;
    extern              uint32_t  debug_tex_id;

    ImGui::BeginChild ("ToolHeadings", ImVec2 (font_size * 66.0f, font_size * 2.5f), false, ImGuiWindowFlags_AlwaysUseWindowPadding);

    if (ImGui::Button ("  Refresh Textures  "))
    {
      SK_ICommandProcessor& command =
        *SK_GetCommandProcessor ();

      command.ProcessCommandLine ("Textures.Trace true");

      tbf::RenderFix::tex_mgr.updateOSD ();

      list_dirty = true;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Refreshes the set of texture checksums used in the last frame drawn.");

    ImGui::SameLine ();

    if (ImGui::Button (" Clear Debug "))
    {
      sel                         = -1;
      debug_tex_id                =  0;
      textures_used_last_dump.clear ();
      last_ht                     =  0;
      last_width                  =  0;
    }

    if (ImGui::IsItemHovered ()) ImGui::SetTooltip ("Exits texture debug mode.");

    ImGui::SameLine ();

    //ImGui::Checkbox ("Highlight Selected Texture in Game",    &config.textures.highlight_debug_tex);

    ImGui::Separator ();
    ImGui::EndChild  ();

    if (list_dirty)
    {
           list_contents.clear ();
                sel = tex_dbg_idx;

      if (debug_tex_id == 0)
        last_ht = 0;


      // The underlying list is unsorted for speed, but that's not at all
      //   intuitive to humans, so sort the thing when we have the RT view open.
      std::sort ( textures_used_last_dump.begin (),
                  textures_used_last_dump.end   () );


      for ( auto it : textures_used_last_dump )
      {
        char szDesc [16] = { };

        sprintf (szDesc, "%08x", it);

        list_contents.push_back (szDesc);
      }
    }

    ImGui::BeginGroup ();

    ImGui::PushStyleVar   (ImGuiStyleVar_ChildWindowRounding, 0.0f);
    ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.9f, 0.7f, 0.5f, 1.0f));

    ImGui::BeginChild ( "Item List",
                        ImVec2 ( font_size * 6.0f, std::max (font_size * 15.0f, last_ht)),
                          true, ImGuiWindowFlags_AlwaysAutoResize );

    if (ImGui::IsWindowHovered ())
      can_scroll = false;

   if (textures_used_last_dump.size ())
   {
     static      int last_sel = 0;
     static bool sel_changed  = false;

     if (sel != last_sel)
       sel_changed = true;

     last_sel = sel;

     for ( int line = 0; line < textures_used_last_dump.size (); line++)
     {
       if (line == sel)
       {
         bool selected = true;
         ImGui::Selectable (list_contents [line].c_str (), &selected);

         if (sel_changed)
         {
           ImGui::SetScrollHere (0.5f); // 0.0f:top, 0.5f:center, 1.0f:bottom
           sel_changed = false;
         }
       }

       else
       {
         bool selected = false;

         if (ImGui::Selectable (list_contents[line].c_str (), &selected))
         {
           sel_changed = true;
           tex_dbg_idx                 =  line;
           sel                         =  line;
           debug_tex_id                =  textures_used_last_dump [line];
         }
       }
     }
   }

   ImGui::EndChild ();

   if (ImGui::IsItemHovered ())
   {
     ImGui::BeginTooltip ();
     ImGui::TextColored  (ImVec4 (0.9f, 0.6f, 0.2f, 1.0f), "The \"debug\" texture will appear black to make identifying textures to modify easier.");
     ImGui::Separator    ();
     ImGui::BulletText   ("Press Ctrl + Shift + [ to select the previous texture from this list");
     ImGui::BulletText   ("Press Ctrl + Shift + ] to select the next texture from this list");
     ImGui::EndTooltip   ();
   }

   ImGui::SameLine     ();
   ImGui::PushStyleVar (ImGuiStyleVar_ChildWindowRounding, 20.0f);

   last_ht    = std::max (last_ht,    16.0f);
   last_width = std::max (last_width, 16.0f);

   if (debug_tex_id != 0x00)
   {
     tbf::RenderFix::Texture* pTex =
       tbf::RenderFix::tex_mgr.getTexture (debug_tex_id);

     extern bool __remap_textures;
            bool has_alternate = (pTex != nullptr && pTex->d3d9_tex->pTexOverride != nullptr);

     if (pTex != nullptr)
     {
        D3DSURFACE_DESC desc;

        if (SUCCEEDED (pTex->d3d9_tex->pTex->GetLevelDesc (0, &desc)))
        {
          ImVec4 border_color = config.textures.highlight_debug_tex ? 
                                  ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                                    (__remap_textures && has_alternate) ? ImVec4 (0.5f,  0.5f,  0.5f, 1.0f) :
                                                                          ImVec4 (0.3f,  1.0f,  0.3f, 1.0f);

          ImGui::PushStyleColor (ImGuiCol_Border, border_color);

          ImGui::BeginGroup     ();
          ImGui::BeginChild     ( "Item Selection",
                                  ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width + 24.0f),
                                (float)desc.Height + font_size * 10.0f),
                                    true,
                                      ImGuiWindowFlags_AlwaysAutoResize );

          if ((! config.textures.highlight_debug_tex) && has_alternate)
          {
            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Click me to make this texture the visible version.");
            
            // Allow the user to toggle texture override by clicking the frame
            if (ImGui::IsItemClicked ())
              __remap_textures = false;
          }

          last_width  = (float)desc.Width;
          last_ht     = (float)desc.Height + font_size * 10.0f;


          int num_lods = pTex->d3d9_tex->pTex->GetLevelCount ();

          ImGui::Text ( "Dimensions:   %lux%lu (%lu %s)",
                          desc.Width, desc.Height,
                            num_lods, num_lods > 1 ? "LODs" : "LOD" );
          ImGui::Text ( "Format:       %ws",
                          SK_D3D9_FormatToStr (desc.Format).c_str () );
          ImGui::Text ( "Data Size:    %.2f MiB",
                          (double)pTex->d3d9_tex->tex_size / (1024.0f * 1024.0f) );
          ImGui::Text ( "Load Time:    %.6f Seconds",
                          pTex->load_time / 1000.0f );

          ImGui::Separator     ();

          if (! TBF_IsTextureDumped (debug_tex_id))
          {
            if ( ImGui::Button ("  Dump Texture to Disk  ") )
            {
              if (! config.textures.quick_load)
                TBF_DumpTexture (desc.Format, debug_tex_id, pTex->d3d9_tex->pTex);
            }

            if (config.textures.quick_load && ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Turn off Texture QuickLoad to use this feature.");
          }

          else
          {
            if ( ImGui::Button ("  Delete Dumped Texture from Disk  ") )
            {
              TBF_DeleteDumpedTexture (desc.Format, debug_tex_id);
            }
          }

          ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
          ImGui::BeginChildFrame (0, ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8), ImGuiWindowFlags_ShowBorders);
          ImGui::Image           ( pTex->d3d9_tex->pTex,
                                     ImVec2 ((float)desc.Width, (float)desc.Height),
                                       ImVec2  (0,0),             ImVec2  (1,1),
                                       ImColor (255,255,255,255), ImColor (255,255,255,128)
                                 );
          ImGui::EndChildFrame   ();
          ImGui::EndChild        ();
          ImGui::EndGroup        ();
          ImGui::PopStyleColor   (2);
        }
     }

     if (has_alternate)
     {
       ImGui::SameLine ();

        D3DSURFACE_DESC desc;

        if (SUCCEEDED (pTex->d3d9_tex->pTexOverride->GetLevelDesc (0, &desc)))
        {
          ImVec4 border_color = config.textures.highlight_debug_tex ? 
                                  ImVec4 (0.3f, 0.3f, 0.3f, 1.0f) :
                                    (__remap_textures) ? ImVec4 (0.3f,  1.0f,  0.3f, 1.0f) :
                                                         ImVec4 (0.5f,  0.5f,  0.5f, 1.0f);

          ImGui::PushStyleColor  (ImGuiCol_Border, border_color);

          ImGui::BeginGroup ();
          ImGui::BeginChild ( "Item Selection2",
                              ImVec2 ( std::max (font_size * 19.0f, (float)desc.Width  + 24.0f),
                                                                    (float)desc.Height + font_size * 10.0f),
                                true,
                                  ImGuiWindowFlags_AlwaysAutoResize );

          if (! config.textures.highlight_debug_tex)
          {
            if (ImGui::IsItemHovered ())
              ImGui::SetTooltip ("Click me to make this texture the visible version.");

            // Allow the user to toggle texture override by clicking the frame
            if (ImGui::IsItemClicked ())
              __remap_textures = true;
          }


          last_width  = std::max (last_width, (float)desc.Width);
          last_ht     = std::max (last_ht,    (float)desc.Height + font_size * 10.0f);


          extern std::wstring
          SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true);


          bool injected  =
            (TBF_GetInjectableTexture (debug_tex_id) != nullptr),
               reloading = false;;

          int num_lods = pTex->d3d9_tex->pTexOverride->GetLevelCount ();

          ImGui::Text ( "Dimensions:   %lux%lu  (%lu %s)",
                          desc.Width, desc.Height,
                             num_lods, num_lods > 1 ? "LODs" : "LOD" );
          ImGui::Text ( "Format:       %ws",
                          SK_D3D9_FormatToStr (desc.Format).c_str () );
          ImGui::Text ( "Data Size:    %.2f MiB",
                          (double)pTex->d3d9_tex->override_size / (1024.0f * 1024.0f) );
          ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), injected ? "Injected Texture" : "Resampled Texture" );

          ImGui::Separator     ();


          if (injected)
          {
            if ( ImGui::Button ("  Reload This Texture  ") && tbf::RenderFix::tex_mgr.reloadTexture (debug_tex_id) )
            {
              reloading    = true;

              tbf::RenderFix::tex_mgr.updateOSD ();
            }
          }

          else {
            ImGui::Button ("  Resample This Texture  "); // NO-OP, but preserves alignment :P
          }

          if (! reloading)
          {
            ImGui::PushStyleColor  (ImGuiCol_Border, ImVec4 (0.95f, 0.95f, 0.05f, 1.0f));
            ImGui::BeginChildFrame (0, ImVec2 ((float)desc.Width + 8, (float)desc.Height + 8), ImGuiWindowFlags_ShowBorders);
            ImGui::Image           ( pTex->d3d9_tex->pTexOverride,
                                       ImVec2 ((float)desc.Width, (float)desc.Height),
                                         ImVec2  (0,0),             ImVec2  (1,1),
                                         ImColor (255,255,255,255), ImColor (255,255,255,128)
                                   );
            ImGui::EndChildFrame   ();
            ImGui::PopStyleColor   (1);
          }

          ImGui::EndChild        ();
          ImGui::EndGroup        ();
          ImGui::PopStyleColor   (1);
        }
      }
    }
    ImGui::EndGroup      ();
    ImGui::PopStyleColor (1);
    ImGui::PopStyleVar   (2);
  }

  ImGui::PopItemWidth ();

  //if (can_scroll)
    //ImGui::SetScrollY (ImGui::GetScrollY () + 5.0f * ImGui::GetFont ()->FontSize * -ImGui::GetIO ().MouseWheel);

  ImGui::End          ();

  return show_dlg;
}
#endif


void
SK_ImGui_KeybindDialog (SK_Keybind* keybind)
{
  const  float font_size = ImGui::GetFont ()->FontSize * ImGui::GetIO ().FontGlobalScale;
  static bool  was_open  = false;

  static BYTE bind_keys [256] = { 0 };

  ImGui::SetNextWindowSizeConstraints ( ImVec2 (font_size * 9, font_size * 3), ImVec2 (font_size * 30, font_size * 6));

  if (ImGui::BeginPopupModal (keybind->bind_name, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_ShowBorders))
  {
    ImGui::GetIO ().WantCaptureKeyboard = false;

    int keys = 256;

    if (! was_open)
    {
      //keybind->vKey = 0;

      GetKeyboardState_Original (bind_keys);

      //for (int i = 0 ; i < keys ; i++ ) bind_keys [i] = (GetKeyState_Original (i) & 0x8000) != 0;

      was_open = true;
    }

    BYTE active_keys [256];
    GetKeyboardState_Original (active_keys);

    //for (int i = 0 ; i < keys ; i++ ) active_keys [i] = (GetKeyState_Original (i) & 0x8000) != 0;

    if (memcmp (active_keys, bind_keys, keys))
    {
      int i = 0;

      for (i = 0x08; i < keys; i++)
      {
        if ( i == VK_LCONTROL || i == VK_RCONTROL || i == VK_CONTROL ||
             i == VK_LSHIFT   || i == VK_RSHIFT   || i == VK_SHIFT   ||
             i == VK_LMENU    || i == VK_RMENU    || i == VK_MENU )
          continue;

        if ( active_keys [i] != bind_keys [i] )
          break;
      }

      if (i != keys)
      {
        keybind->vKey = i;
        was_open      = false;
        ImGui::CloseCurrentPopup ();
        ImGui::GetIO ().WantCaptureKeyboard = true;
      }

      //memcpy (bind_keys, active_keys, keys);
    }

    keybind->ctrl  = (GetAsyncKeyState_Original (VK_CONTROL) & 0x8000) != 0; 
    keybind->shift = (GetAsyncKeyState_Original (VK_SHIFT)   & 0x8000) != 0;
    keybind->alt   = (GetAsyncKeyState_Original (VK_MENU)    & 0x8000) != 0;

    keybind->update ();

    ImGui::Text ("Binding:  %ws", keybind->human_readable.c_str ());

    ImGui::EndPopup ();
  }
}