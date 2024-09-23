/**
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
#include <SpecialK/config.h>
#include <SpecialK/core.h>
#include <SpecialK/dxgi_interfaces.h>
#include <SpecialK/parameter.h>
#include <SpecialK/import.h>
#include <SpecialK/utility.h>
#include <SpecialK/ini.h>
#include <SpecialK/log.h>
#include <SpecialK/steam_api.h>

#include <SpecialK/DLL_VERSION.H>
#include <SpecialK/input/input.h>

#include <unordered_map>

#define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1

const wchar_t*       SK_VER_STR = SK_VERSION_STR_W;

iSK_INI*             dll_ini         = nullptr;
iSK_INI*             osd_ini         = nullptr;
iSK_INI*             achievement_ini = nullptr;
sk_config_t          config;
sk::ParameterFactory g_ParameterFactory;

struct {
  struct {
    sk::ParameterBool*    show;
  } time;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
  } io;

  struct {
    sk::ParameterBool*    show;
  } fps;

  struct {
    sk::ParameterBool*    show;
  } memory;

  struct {
    sk::ParameterBool*    show;
  } SLI;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
    sk::ParameterBool*    simple;
  } cpu;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterBool*    print_slowdown;
    sk::ParameterFloat*   interval;
  } gpu;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
    sk::ParameterInt*     type;
  } disk;

  struct {
    sk::ParameterBool*    show;
    sk::ParameterFloat*   interval;
  } pagefile;
} monitoring;

struct {
  sk::ParameterBool*      show;

  struct {
    sk::ParameterBool*    pump;
    sk::ParameterFloat*   pump_interval;
  } update_method;

  struct {
    sk::ParameterInt*     red;
    sk::ParameterInt*     green;
    sk::ParameterInt*     blue;
  } text;

  struct {
    sk::ParameterFloat*   scale;
    sk::ParameterInt*     pos_x;
    sk::ParameterInt*     pos_y;
  } viewport;

  struct {
    sk::ParameterBool*    remember;
  } state;
} osd;

struct {
  sk::ParameterFloat*     scale;
  sk::ParameterBool*      show_eula;
} imgui;

struct {
  struct {
    sk::ParameterStringW*   sound_file;
    sk::ParameterBool*      play_sound;
    sk::ParameterBool*      take_screenshot;

    struct {
      sk::ParameterBool*    show;
      sk::ParameterBool*    show_title;
      sk::ParameterBool*    animate;
      sk::ParameterStringW* origin;
      sk::ParameterFloat*   inset;
      sk::ParameterInt*     duration;
    } popup;
  } achievements;

  struct {
    sk::ParameterInt*     appid;
    sk::ParameterInt*     init_delay;
    sk::ParameterBool*    auto_pump;
    sk::ParameterStringW* notify_corner;
    sk::ParameterBool*    block_stat_callback;
    sk::ParameterBool*    filter_stat_callbacks;
    sk::ParameterBool*    load_early;
    sk::ParameterBool*    early_overlay;
  } system;

  struct {
    sk::ParameterBool*    silent;
  } log;
} steam;

struct {
  struct {
    sk::ParameterBool*    override;
    sk::ParameterStringW* compatibility;
    sk::ParameterStringW* num_gpus;
    sk::ParameterStringW* mode;
  } sli;

  struct {
    sk::ParameterBool*    disable;
  } api;
} nvidia;

sk::ParameterBool*        enable_cegui;
sk::ParameterFloat*       mem_reserve;
sk::ParameterBool*        debug_output;
sk::ParameterBool*        game_output;
sk::ParameterBool*        handle_crashes;
sk::ParameterBool*        prefer_fahrenheit;
sk::ParameterBool*        ignore_rtss_delay;
sk::ParameterInt*         init_delay;
sk::ParameterInt*         log_level;
sk::ParameterBool*        trace_libraries;
sk::ParameterBool*        strict_compliance;
sk::ParameterBool*        resolve_symbol_names;
sk::ParameterBool*        silent;
sk::ParameterStringW*     version;

struct {
  struct {
    sk::ParameterFloat*   target_fps;
    sk::ParameterFloat*   limiter_tolerance;
    sk::ParameterInt*     prerender_limit;
    sk::ParameterInt*     present_interval;
    sk::ParameterInt*     buffer_count;
    sk::ParameterInt*     max_delta_time;
    sk::ParameterBool*    flip_discard;
    sk::ParameterInt*     refresh_rate;
    sk::ParameterBool*    wait_for_vblank;
  } framerate;
  struct {
    sk::ParameterInt*     adapter_override;
    sk::ParameterStringW* max_res;
    sk::ParameterStringW* min_res;
    sk::ParameterInt*     swapchain_wait;
    sk::ParameterStringW* scaling_mode;
    sk::ParameterStringW* exception_mode;
    sk::ParameterStringW* scanline_order;
    sk::ParameterStringW* rotation;
    sk::ParameterBool*    test_present;
    sk::ParameterBool*    debug_layer;
  } dxgi;
  struct {
    sk::ParameterBool*    force_d3d9ex;
    sk::ParameterBool*    force_fullscreen;
    sk::ParameterInt*     hook_type;
    sk::ParameterBool*    impure;
  } d3d9;
} render;

struct {
  struct {
    sk::ParameterBool*    precise_hash;
    sk::ParameterBool*    dump;
    sk::ParameterBool*    inject;
    sk::ParameterBool*    cache;
    sk::ParameterStringW* res_root;
  } d3d11;
  struct {
    sk::ParameterInt*     min_evict;
    sk::ParameterInt*     max_evict;
    sk::ParameterInt*     min_size;
    sk::ParameterInt*     max_size;
    sk::ParameterInt*     min_entries;
    sk::ParameterInt*     max_entries;
    sk::ParameterBool*    ignore_non_mipped;
  } cache;
} texture;

struct {
  struct {
    sk::ParameterBool*    manage;
    sk::ParameterBool*    keys_activate;
    sk::ParameterFloat*   timeout;
    sk::ParameterBool*    ui_capture;
    sk::ParameterBool*    hw_cursor;
    sk::ParameterBool*    no_warp_ui;
    sk::ParameterBool*    no_warp_visible;
    sk::ParameterBool*    block_invisible;
  } cursor;

  struct {
    sk::ParameterBool*    disable_ps4_hid;
    sk::ParameterBool*    rehook_xinput;
    sk::ParameterBool*    hook_dinput8;
    sk::ParameterBool*    hook_hid;
    sk::ParameterBool*    hook_xinput;

    struct {
      sk::ParameterInt*  ui_slot;
      sk::ParameterInt*  placeholders;
    } xinput;
  } gamepad;
} input;

struct {
  sk::ParameterBool*      borderless;
  sk::ParameterBool*      center;
  struct {
    sk::ParameterStringW* x;
    sk::ParameterStringW* y;
  } offset;
  sk::ParameterBool*      background_render;
  sk::ParameterBool*      background_mute;
  sk::ParameterBool*      confine_cursor;
  sk::ParameterBool*      unconfine_cursor;
  sk::ParameterBool*      persistent_drag;
  sk::ParameterBool*      fullscreen;
  sk::ParameterStringW*   override;
  sk::ParameterBool*      fix_mouse_coords;
} window;

struct {
  sk::ParameterBool*      ignore_raptr;
  sk::ParameterBool*      disable_raptr;
  sk::ParameterBool*      rehook_loadlibrary;
  sk::ParameterBool*      disable_nv_bloat;

  struct {
    sk::ParameterBool*    rehook_reset;
    sk::ParameterBool*    rehook_present;
    sk::ParameterBool*    hook_reset_vtable;
    sk::ParameterBool*    hook_present_vtable;
  } d3d9;
} compatibility;

struct {
  struct {
    sk::ParameterBool*    hook;
  }   d3d9,  d3d9ex,
      d3d11, d3d12,
      OpenGL,
      Vulkan;
} apis;


extern const wchar_t*
SK_Steam_PopupOriginToWStr (int origin);
extern int
SK_Steam_PopupOriginWStrToEnum (const wchar_t* str);

bool
SK_LoadConfig (std::wstring name) {
  return SK_LoadConfigEx (name);
}

bool
SK_LoadConfigEx (std::wstring name, bool create)
{
  // Load INI File
  std::wstring full_name;
  std::wstring osd_config;
  std::wstring achievement_config;

  full_name = SK_GetConfigPath () +
                name              +
                  L".ini";

  if (create)
    SK_CreateDirectories (full_name.c_str ());

  static bool         init     = false;
  static bool         empty    = true;
  static std::wstring last_try = name;

  // Allow a second load attempt using a different name
  if (last_try != name)
  {
    init     = false;
    last_try = name;
  }

  osd_config =
    SK_GetDocumentsDir () + L"\\My Mods\\SpecialK\\Global\\osd.ini";

  achievement_config =
    SK_GetDocumentsDir () + L"\\My Mods\\SpecialK\\Global\\achievements.ini";

  
  if (! init)
  {
   dll_ini =
    new iSK_INI (full_name.c_str ());

  empty    = dll_ini->get_sections ().empty ();

  SK_CreateDirectories (osd_config.c_str ());

  osd_ini =
    new iSK_INI (osd_config.c_str ());

  achievement_ini =
    new iSK_INI (achievement_config.c_str ());

  // NOTE: Compatibility interface with Special K's newer implementation.
  // The newer implementation works a bit differently, storing everything in an array,
  // but we can still use it make the old version more readable.
#define ConfigEntry(param,descrip,ini,sec,key) \
        g_ParameterFactory.create_parameter (  \
          (&param), \
          (descrip) \
        ); \
        (param)->register_to_ini( \
          (ini), \
          (sec), \
          (key) \
        )

  //
  // Create Parameters
  //

  //// nb: If you want any hope of reading this table, turn line wrapping off.
  //

  // Performance Monitoring  (Global Settings)
  //////////////////////////////////////////////////////////////////////////
  ConfigEntry (monitoring.io.show,                     L"Show IO Monitoring",                                        osd_ini,         L"Monitor.IO",            L"Show");
  ConfigEntry (monitoring.io.interval,                 L"IO Monitoring Interval",                                    osd_ini,         L"Monitor.IO",            L"Interval");

  ConfigEntry (monitoring.disk.show,                   L"Show Disk Monitoring",                                      osd_ini,         L"Monitor.Disk",          L"Show");
  ConfigEntry (monitoring.disk.interval,               L"Disk Monitoring Interval",                                  osd_ini,         L"Monitor.Disk",          L"Interval");
  ConfigEntry (monitoring.disk.type,                   L"Disk Monitoring Type (0 = Physical, 1 = Logical)",          osd_ini,         L"Monitor.Disk",          L"Type");

  ConfigEntry (monitoring.cpu.show,                    L"Show CPU Monitoring",                                       osd_ini,         L"Monitor.CPU",           L"Show");
  ConfigEntry (monitoring.cpu.interval,                L"CPU Monitoring Interval (seconds)",                         osd_ini,         L"Monitor.CPU",           L"Interval");
  ConfigEntry (monitoring.cpu.simple,                  L"Minimal CPU Info",                                          osd_ini,         L"Monitor.CPU",           L"Simple");

  ConfigEntry (monitoring.gpu.show,                    L"Show GPU Monitoring",                                       osd_ini,         L"Monitor.GPU",           L"Show");
  ConfigEntry (monitoring.gpu.interval,                L"GPU Monitoring Interval (msecs)",                           osd_ini,         L"Monitor.GPU",           L"Interval");
  ConfigEntry (monitoring.gpu.print_slowdown,          L"Print GPU Slowdown Reason (NVIDA GPUs)",                    osd_ini,         L"Monitor.GPU",           L"PrintSlowdown");

  ConfigEntry (monitoring.pagefile.show,               L"Show Pagefile Monitoring",                                  osd_ini,         L"Monitor.Pagefile",      L"Show");
  ConfigEntry (monitoring.pagefile.interval,           L"Pagefile Monitoring INterval (seconds)",                    osd_ini,         L"Monitor.Pagefile",      L"Interval");

  ConfigEntry (monitoring.memory.show,                 L"Show Memory Monitoring",                                    osd_ini,         L"Monitor.Memory",        L"Show");
  ConfigEntry (monitoring.fps.show,                    L"Show Framerate Monitoring",                                 osd_ini,         L"Monitor.FPS",           L"Show");
#if 0 // unsupported config
  ConfigEntry (monitoring.fps.frametime,               L"Show Frametime in Framerate Counter",                       osd_ini,         L"Monitor.FPS",           L"DisplayFrametime");
  ConfigEntry (monitoring.fps.advanced,                L"Show Advanced Statistics in Framerate Counter",             osd_ini,         L"Monitor.FPS",           L"AdvancedStatistics");
#endif
  ConfigEntry (monitoring.time.show,                   L"Show System Clock",                                         osd_ini,         L"Monitor.Time",          L"Show");

  ConfigEntry (prefer_fahrenheit,                      L"Prefer Fahrenheit Units",                                   osd_ini,         L"SpecialK.OSD",          L"PreferFahrenheit");

  ConfigEntry (imgui.scale,                            L"ImGui Scale",                                               osd_ini,         L"ImGui.Global",          L"FontScale");
#if 0 // unsupported config
  ConfigEntry (imgui.show_playtime,                    L"Display Playing Time in Config UI",                         osd_ini,         L"ImGui.Global",          L"ShowPlaytime");
  ConfigEntry (imgui.show_gsync_status,                L"Show G-Sync Status on Control Panel",                       osd_ini,         L"ImGui.Global",          L"ShowGSyncStatus");
  ConfigEntry (imgui.mac_style_menu,                   L"Use Mac-style Menu Bar",                                    osd_ini,         L"ImGui.Global",          L"UseMacStyleMenu");
  ConfigEntry (imgui.show_input_apis,                  L"Show Input APIs currently in-use",                          osd_ini,         L"ImGui.Global",          L"ShowActiveInputAPIs");
#endif

  // Input
  //////////////////////////////////////////////////////////////////////////

#if 0 // unsupported config
  ConfigEntry (input.keyboard.catch_alt_f4,            L"If the game does not handle Alt+F4, offer a replacement",   dll_ini,         L"Input.Keyboard",        L"CatchAltF4");
  ConfigEntry (input.keyboard.disabled_to_game,        L"Completely stop all keyboard input from reaching the Game", dll_ini,         L"Input.Keyboard",        L"DisabledToGame");

  ConfigEntry (input.mouse.disabled_to_game,           L"Completely stop all mouse input from reaching the Game",    dll_ini,         L"Input.Mouse",           L"DisabledToGame");
#endif

  ConfigEntry (input.cursor.manage,                    L"Manage Cursor Visibility (due to inactivity)",              dll_ini,         L"Input.Cursor",          L"Manage");

  ConfigEntry (input.cursor.keys_activate,             L"Keyboard Input Activates Cursor",                           dll_ini,         L"Input.Cursor",          L"KeyboardActivates");
  ConfigEntry (input.cursor.timeout,                   L"Inactivity Timeout (in milliseconds)",                      dll_ini,         L"Input.Cursor",          L"Timeout");
  ConfigEntry (input.cursor.ui_capture,                L"Forcefully Capture Mouse Cursor in UI Mode",                dll_ini,         L"Input.Cursor",          L"ForceCaptureInUI");
  ConfigEntry (input.cursor.hw_cursor,                 L"Use a Hardware Cursor for Special K's UI Features",         dll_ini,         L"Input.Cursor",          L"UseHardwareCursor");
  ConfigEntry (input.cursor.block_invisible,           L"Block Mouse Input if Hardware Cursor is Invisible",         dll_ini,         L"Input.Cursor",          L"BlockInvisibleCursorInput");
#if 0 // unsupported config
  ConfigEntry (input.cursor.fix_synaptics,             L"Fix Synaptic Touchpad Scroll",                              dll_ini,         L"Input.Cursor",          L"FixSynapticsTouchpadScroll");
  ConfigEntry (input.cursor.use_relative_input,        L"Use Raw Input Relative Motion if Needed",                   dll_ini,         L"Input.Cursor",          L"UseRelativeInput");
  ConfigEntry (input.cursor.antiwarp_deadzone,         L"Percentage of Screen that the game may try to move the "
                                                       L"cursor to for mouselook.",                                  dll_ini,         L"Input.Cursor",          L"AntiwarpDeadzonePercent");
#endif
  ConfigEntry (input.cursor.no_warp_ui,                L"Prevent Games from Warping Cursor while Config UI is Open", dll_ini,         L"Input.Cursor",          L"NoWarpUI");
  ConfigEntry (input.cursor.no_warp_visible,           L"Prevent Games from Warping Cursor while Cursor is Visible", dll_ini,         L"Input.Cursor",          L"NoWarpVisibleGameCursor");

#if 0 // unsupported config
  ConfigEntry (input.gamepad.disabled_to_game,         L"Disable ALL Gamepad Input (across all APIs)",               dll_ini,         L"Input.Gamepad",         L"DisabledToGame");
#endif

  ConfigEntry (input.gamepad.disable_ps4_hid,          L"Disable PS4 HID Interface (prevent double-input)",          dll_ini,         L"Input.Gamepad",         L"DisablePS4HID");
#if 0 // unsupported config
  ConfigEntry (input.gamepad.haptic_ui,                L"Give tactile feedback on gamepads when navigating the UI",  dll_ini,         L"Input.Gamepad",         L"AllowHapticUI");
#endif
  ConfigEntry (input.gamepad.hook_dinput8,             L"Install hooks for DirectInput 8",                           dll_ini,         L"Input.Gamepad",         L"EnableDirectInput8");
  ConfigEntry (input.gamepad.hook_hid,                 L"Install hooks for HID",                                     dll_ini,         L"Input.Gamepad",         L"EnableHID");
#if 0 // unsupported config
  ConfigEntry (input.gamepad.native_ps4,               L"Native PS4 Mode (temporary)",                               dll_ini,         L"Input.Gamepad",         L"EnableNativePS4");
  ConfigEntry (input.gamepad.disable_rumble,           L"Disable Rumble from ALL SOURCES (across all APIs)",         dll_ini,         L"Input.Gamepad",         L"DisableRumble");
#endif

  ConfigEntry (input.gamepad.hook_xinput,              L"Install hooks for XInput",                                  dll_ini,         L"Input.XInput",          L"Enable");
  ConfigEntry (input.gamepad.rehook_xinput,            L"Re-install XInput hooks if hookchain is modified",          dll_ini,         L"Input.XInput",          L"Rehook");
  ConfigEntry (input.gamepad.xinput.ui_slot,           L"XInput Controller that owns the config UI",                 dll_ini,         L"Input.XInput",          L"UISlot");
  ConfigEntry (input.gamepad.xinput.placeholders,      L"XInput Controller Slots to Fake Connectivity On",           dll_ini,         L"Input.XInput",          L"PlaceholderMask");
#if 0 // unsupported config
  ConfigEntry (input.gamepad.xinput.assignment,        L"Re-Assign XInput Slots",                                    dll_ini,         L"Input.XInput",          L"SlotReassignment");
  //DEPRECATED  (                                                                                                                     L"Input.XInput",          L"DisableRumble"),
#endif

#if 0 // unsupported config
  ConfigEntry (input.gamepad.steam.ui_slot,            L"Steam Controller that owns the config UI",                  dll_ini,         L"Input.Steam",           L"UISlot"),
#endif

  // Thread Monitoring
  //////////////////////////////////////////////////////////////////////////

#if 0 // unsupported config
  ConfigEntry (threads.enable_mem_alloc_trace,         L"Trace per-Thread Memory Allocation in Threads Widget",      dll_ini,         L"Threads.Analyze",       L"MemoryAllocation");
  ConfigEntry (threads.enable_file_io_trace,           L"Trace per-Thread File I/O Activity in Threads Widget",      dll_ini,         L"Threads.Analyze",       L"FileActivity");
#endif

  // Window Management
  //////////////////////////////////////////////////////////////////////////

  ConfigEntry (window.borderless,                      L"Borderless Window Mode",                                    dll_ini,         L"Window.System",         L"Borderless");
  ConfigEntry (window.center,                          L"Center the Window",                                         dll_ini,         L"Window.System",         L"Center");
  ConfigEntry (window.background_render,               L"Render While Window is in Background",                      dll_ini,         L"Window.System",         L"RenderInBackground");
  ConfigEntry (window.background_mute,                 L"Mute While Window is in Background",                        dll_ini,         L"Window.System",         L"MuteInBackground");
  ConfigEntry (window.offset.x,                        L"X Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"XOffset");
  ConfigEntry (window.offset.y,                        L"Y Offset (Percent or Absolute)",                            dll_ini,         L"Window.System",         L"YOffset");
  ConfigEntry (window.confine_cursor,                  L"Confine the Mouse Cursor to the Game Window",               dll_ini,         L"Window.System",         L"ConfineCursor");
  ConfigEntry (window.unconfine_cursor,                L"Unconfine the Mouse Cursor from the Game Window",           dll_ini,         L"Window.System",         L"UnconfineCursor");
  ConfigEntry (window.persistent_drag,                 L"Remember where the window is dragged to",                   dll_ini,         L"Window.System",         L"PersistentDragPos");
  ConfigEntry (window.fullscreen,                      L"Make the Game Window Fill the Screen (scale to fit)",       dll_ini,         L"Window.System",         L"Fullscreen");
  ConfigEntry (window.override,                        L"Force the Client Region to this Size in Windowed Mode",     dll_ini,         L"Window.System",         L"OverrideRes");
  ConfigEntry (window.fix_mouse_coords,                L"Re-Compute Mouse Coordinates for Resized Windows",          dll_ini,         L"Window.System",         L"FixMouseCoords");
#if 0 // unsupported config
  ConfigEntry (window.always_on_top,                   L"Prevent (0) or Force (1) a game's window Always-On-Top",    dll_ini,         L"Window.System",         L"AlwaysOnTop");
  ConfigEntry (window.disable_screensaver,             L"Prevent the Windows Screensaver from activating",           dll_ini,         L"Window.System",         L"DisableScreensaver");
  ConfigEntry (window.dont_hook_wndproc,               L"Disable WndProc / ClassProc hooks (wrap instead of hook)",  dll_ini,         L"Window.System",         L"DontHookWndProc");
#endif

  // Compatibility
  //////////////////////////////////////////////////////////////////////////

  // TODO: Remove (21bb54db8e8fef90a51163bda1d396a7d0800c7e):
  ConfigEntry (compatibility.ignore_raptr,             L"Ignore Raptr Warning",                                      dll_ini,         L"Compatibility.General", L"IgnoreRaptr");
  ConfigEntry (compatibility.disable_raptr,            L"Forcefully Disable Raptr",                                  dll_ini,         L"Compatibility.General", L"DisableRaptr");
  //

  ConfigEntry (compatibility.disable_nv_bloat,         L"Disable All NVIDIA BloatWare (GeForce Experience)",         dll_ini,         L"Compatibility.General", L"DisableBloatWare_NVIDIA");
  ConfigEntry (compatibility.rehook_loadlibrary,       L"Rehook LoadLibrary When RTSS/Steam/ReShade hook it",        dll_ini,         L"Compatibility.General", L"RehookLoadLibrary");

#if 0 // unsupported config
  ConfigEntry (apis.last_known,                        L"Last Known Render API",                                     dll_ini,         L"API.Hook",              L"LastKnown");
#endif

#if 0 // unsupported config
//#ifdef _M_IX86
  ConfigEntry (apis.ddraw.hook,                        L"Enable DirectDraw Hooking",                                 dll_ini,         L"API.Hook",              L"ddraw");
  ConfigEntry (apis.d3d8.hook,                         L"Enable Direct3D 8 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d8");
//#endif
#endif

  ConfigEntry (apis.d3d9.hook,                         L"Enable Direct3D 9 Hooking",                                 dll_ini,         L"API.Hook",              L"d3d9");
  ConfigEntry (apis.d3d9ex.hook,                       L"Enable Direct3D 9Ex Hooking",                               dll_ini,         L"API.Hook",              L"d3d9ex");
  ConfigEntry (apis.d3d11.hook,                        L"Enable Direct3D 11 Hooking",                                dll_ini,         L"API.Hook",              L"d3d11");

//#ifdef _M_AMD64
  ConfigEntry (apis.d3d12.hook,                        L"Enable Direct3D 12 Hooking",                                dll_ini,         L"API.Hook",              L"d3d12");
  ConfigEntry (apis.Vulkan.hook,                       L"Enable Vulkan Hooking",                                     dll_ini,         L"API.Hook",              L"Vulkan");
//#endif

  ConfigEntry (apis.OpenGL.hook,                       L"Enable OpenGL Hooking",                                     dll_ini,         L"API.Hook",              L"OpenGL");

  // Misc.
  //////////////////////////////////////////////////////////////////////////

    // Hidden setting (it will be read, but not written -- setting this is discouraged and I intend to phase it out)
  ConfigEntry (mem_reserve,                            L"Memory Reserve Percentage",                                 dll_ini,         L"Manage.Memory",         L"ReservePercent");


    // General Mod System Settings
  //////////////////////////////////////////////////////////////////////////

  // NOTE: Removed from newer Special K
  ConfigEntry (init_delay,                             L"Initialization Delay (msecs)",                              dll_ini,         L"SpecialK.System",       L"InitDelay");
  ConfigEntry (silent,                                 L"Log Silence",                                               dll_ini,         L"SpecialK.System",       L"Silent");
  ConfigEntry (strict_compliance,                      L"Strict DLL Loader Compliance",                              dll_ini,         L"SpecialK.System",       L"StrictCompliant");
  // NOTE: Setting added by this fork until we can improve symbol name resolution times (4 minutes to start a game due to this is unacceptable)
  ConfigEntry (resolve_symbol_names,                   L"Resolve Module Symbol Names",                               dll_ini,         L"SpecialK.System",       L"ResolveSymbolNames");
  ConfigEntry (trace_libraries,                        L"Trace DLL Loading (needed for dynamic API detection)",      dll_ini,         L"SpecialK.System",       L"TraceLoadLibrary");
  ConfigEntry (log_level,                              L"Log Verbosity (0=General, 5=Insane Debug)",                 dll_ini,         L"SpecialK.System",       L"LogLevel");
  ConfigEntry (handle_crashes,                         L"Use Custom Crash Handler",                                  dll_ini,         L"SpecialK.System",       L"UseCrashHandler");
  ConfigEntry (debug_output,                           L"Print Application's Debug Output in real-time",             dll_ini,         L"SpecialK.System",       L"DebugOutput");
  ConfigEntry (game_output,                            L"Log Application's Debug Output",                            dll_ini,         L"SpecialK.System",       L"GameOutput");
  ConfigEntry (ignore_rtss_delay,                      L"Ignore RTSS Delay Incompatibilities",                       dll_ini,         L"SpecialK.System",       L"IgnoreRTSSHookDelay");
  ConfigEntry (enable_cegui,                           L"Enable CEGUI (lazy loading)",                               dll_ini,         L"SpecialK.System",       L"EnableCEGUI");
#if 0 // unsupported config
  ConfigEntry (safe_cegui,                             L"Safely Initialize CEGUI",                                   dll_ini,         L"SpecialK.System",       L"SafeInitCEGUI");
#endif
  ConfigEntry (version,                                L"The last version that wrote the config file",               dll_ini,         L"SpecialK.System",       L"Version");


#if 0 // unsupported config
  ConfigEntry (display.force_fullscreen,               L"Force Fullscreen Mode",                                     dll_ini,         L"Display.Output",        L"ForceFullscreen");
  ConfigEntry (display.force_windowed,                 L"Force Windowed Mode",                                       dll_ini,         L"Display.Output",        L"ForceWindowed");
  ConfigEntry (render.osd.hdr_luminance,               L"OSD's Luminance (cd.m^-2) in HDR games",                    dll_ini,         L"Render.OSD",            L"HDRLuminance");
#endif

  // Framerate Limiter
  //////////////////////////////////////////////////////////////////////////


  ConfigEntry (render.framerate.target_fps,            L"Framerate Target (negative signed values are non-limiting)",dll_ini,         L"Render.FrameRate",      L"TargetFPS");
#if 0 // unsupported config
  ConfigEntry (render.framerate.target_fps_bg,         L"Framerate Target (window in background;  0.0 = same as fg)",dll_ini,         L"Render.FrameRate",      L"BackgroundFPS");
#endif
  ConfigEntry (render.framerate.limiter_tolerance,     L"Limiter Tolerance",                                         dll_ini,         L"Render.FrameRate",      L"LimiterTolerance");
#if 0 // unsupported config
  ConfigEntry (render.framerate.busy_wait_ratio,       L"Ratio of time spent Busy-Waiting to Event-Waiting",         dll_ini,         L"Render.FrameRate",      L"MaxBusyWaitPercent");
#endif
  ConfigEntry (render.framerate.wait_for_vblank,       L"Limiter Will Wait for VBLANK",                              dll_ini,         L"Render.FrameRate",      L"WaitForVBLANK");
  ConfigEntry (render.framerate.buffer_count,          L"Number of Backbuffers in the Swapchain",                    dll_ini,         L"Render.FrameRate",      L"BackBufferCount");
  ConfigEntry (render.framerate.present_interval,      L"Presentation Interval (VSYNC)",                             dll_ini,         L"Render.FrameRate",      L"PresentationInterval");
  ConfigEntry (render.framerate.prerender_limit,       L"Maximum Frames to Render-Ahead",                            dll_ini,         L"Render.FrameRate",      L"PreRenderLimit");
#if 0 // unsupported config
  ConfigEntry (render.framerate.sleepless_render,      L"Sleep Free Render Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessRenderThread");
  ConfigEntry (render.framerate.sleepless_window,      L"Sleep Free Window Thread",                                  dll_ini,         L"Render.FrameRate",      L"SleeplessWindowThread");
  ConfigEntry (render.framerate.enable_mmcss,          L"Enable Multimedia Class Scheduling for FPS Limiter Sleep",  dll_ini,         L"Render.FrameRate",      L"EnableMMCSS");
#endif
  ConfigEntry (render.framerate.refresh_rate,          L"Fullscreen Refresh Rate",                                   dll_ini,         L"Render.FrameRate",      L"RefreshRate");
#if 0 // unsupported config
  ConfigEntry (render.framerate.rescan_ratio,          L"Fullscreen Rational Scan Rate (precise refresh rate)",      dll_ini,         L"Render.FrameRate",      L"RescanRatio");
  ConfigEntry (render.framerate.enforcement_policy,    L"Place Framerate Limiter Wait Before/After Present, etc.",   dll_ini,         L"Render.FrameRate",      L"LimitEnforcementPolicy");

  ConfigEntry (render.framerate.control.render_ahead,  L"Maximum number of CPU-side frames to work ahead of GPU.",   dll_ini,         L"FrameRate.Control",     L"MaxRenderAheadFrames");
  ConfigEntry (render.framerate.override_cpu_count,    L"Number of CPU cores to tell the game about",                dll_ini,         L"FrameRate.Control",     L"OverrideCPUCoreCount");

  ConfigEntry (render.framerate.allow_dwm_tearing,     L"Enable DWM Tearing (Windows 10+)",                          dll_ini,         L"Render.DXGI",           L"AllowTearingInDWM");
  ConfigEntry (render.framerate.drop_late_frames,      L"Enable Flip Model to Render (and drop) frames at rates >"
#endif


  // OpenGL
  //////////////////////////////////////////////////////////////////////////

#if 0 // unsupported config
  ConfigEntry (render.osd.draw_in_vidcap,              L"Changes hook order in order to allow recording the OSD.",   dll_ini,         L"Render.OSD",            L"ShowInVideoCapture");
#endif


  // D3D9
  //////////////////////////////////////////////////////////////////////////

  ConfigEntry (render.d3d9.force_d3d9ex,               L"Force D3D9Ex Context",                                      dll_ini,         L"Render.D3D9",           L"ForceD3D9Ex");
  ConfigEntry (render.d3d9.impure,                     L"Force PURE device off",                                     dll_ini,         L"Render.D3D9",           L"ForceImpure");
#if 0 // unsupported config
  ConfigEntry (render.d3d9.enable_texture_mods,        L"Enable Texture Modding Support",                            dll_ini,         L"Render.D3D9",           L"EnableTextureMods");
#endif


  // D3D10/11/12
  //////////////////////////////////////////////////////////////////////////

  ConfigEntry (render.framerate.max_delta_time,        L"Maximum Frame Delta Time",                                  dll_ini,         L"Render.DXGI",           L"MaxDeltaTime");
  ConfigEntry (render.framerate.flip_discard,          L"Use Flip Discard - Windows 10+",                            dll_ini,         L"Render.DXGI",           L"UseFlipDiscard");
#if 0 // unsupported config
  ConfigEntry (render.framerate.disable_flip_model,    L"Disable Flip Model - Fix AMD Drivers in Yakuza0",           dll_ini,         L"Render.DXGI",           L"DisableFlipModel");
#endif

  ConfigEntry (render.dxgi.adapter_override,           L"Override DXGI Adapter",                                     dll_ini,         L"Render.DXGI",           L"AdapterOverride");
  ConfigEntry (render.dxgi.max_res,                    L"Maximum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MaxRes");
  ConfigEntry (render.dxgi.min_res,                    L"Minimum Resolution To Report",                              dll_ini,         L"Render.DXGI",           L"MinRes");

  ConfigEntry (render.dxgi.swapchain_wait,             L"Time to wait in msec. for SwapChain",                       dll_ini,         L"Render.DXGI",           L"SwapChainWait");
  ConfigEntry (render.dxgi.scaling_mode,               L"Scaling Preference (DontCare | Centered | Stretched"
                                                       L" | Unspecified)",                                           dll_ini,         L"Render.DXGI",           L"Scaling");
  ConfigEntry (render.dxgi.exception_mode,             L"D3D11 Exception Handling (DontCare | Raise | Ignore)",      dll_ini,         L"Render.DXGI",           L"ExceptionMode");
  ConfigEntry (render.dxgi.debug_layer,                L"DXGI Debug Layer Support",                                  dll_ini,         L"Render.DXGI",           L"EnableDebugLayer");
  ConfigEntry (render.dxgi.scanline_order,             L"Scanline Order (DontCare | Progressive | LowerFieldFirst |"
                                                       L" UpperFieldFirst )",                                        dll_ini,         L"Render.DXGI",           L"ScanlineOrder");
  ConfigEntry (render.dxgi.rotation,                   L"Screen Rotation (DontCare | Identity | 90 | 180 | 270 )",   dll_ini,         L"Render.DXGI",           L"Rotation");
  ConfigEntry (render.dxgi.test_present,               L"Test SwapChain Presentation Before Actually Presenting",    dll_ini,         L"Render.DXGI",           L"TestSwapChainPresent");
#if 0 // unsupported config
  ConfigEntry (render.dxgi.safe_fullscreen,            L"Prevent DXGI Deadlocks in Improperly Written Games",        dll_ini,         L"Render.DXGI",           L"SafeFullscreenMode");
  ConfigEntry (render.dxgi.enhanced_depth,             L"Use 32-bit Depth + 8-bit Stencil + 24-bit Padding",         dll_ini,         L"Render.DXGI",           L"Use64BitDepthStencil");
  ConfigEntry (render.dxgi.deferred_isolation,         L"Isolate D3D11 Deferred Context Queues instead of Tracking"
                                                       L" in Immediate Mode.",                                       dll_ini,         L"Render.DXGI",           L"IsolateD3D11DeferredContexts");
  ConfigEntry (render.dxgi.msaa_samples,               L"Override ON-SCREEN Multisample Antialiasing Level;-1=None", dll_ini,         L"Render.DXGI",           L"OverrideMSAA");
  ConfigEntry (render.dxgi.skip_present_test,          L"Nix the swapchain present flag: DXGI_PRESENT_TEST to "
                                                       L"workaround bad third-party software that doesn't handle it"
                                                       L" correctly.",                                               dll_ini,         L"Render.DXGI",           L"SkipSwapChainPresentTest");
#endif


  ConfigEntry (texture.d3d11.cache,                    L"Cache Textures",                                            dll_ini,         L"Textures.D3D11",        L"Cache");
  ConfigEntry (texture.d3d11.precise_hash,             L"Precise Hash Generation",                                   dll_ini,         L"Textures.D3D11",        L"PreciseHash");

  ConfigEntry (texture.d3d11.inject,                   L"Inject Textures",                                           dll_ini,         L"Textures.D3D11",        L"Inject");
#if 0 // unsupported config
  ConfigEntry (texture.d3d11.injection_keeps_format,   L"Allow image format to change during texture injection",     dll_ini,         L"Textures.D3D11",        L"InjectionKeepsFormat");
  ConfigEntry (texture.d3d11.gen_mips,                 L"Create complete mipmap chain for textures without them",    dll_ini,         L"Textures.D3D11",        L"GenerateMipmaps");
  ConfigEntry (texture.res_root,                       L"Resource Root",                                             dll_ini,         L"Textures.General",      L"ResourceRoot");
  ConfigEntry (texture.dump_on_load,                   L"Dump Textures while Loading",                               dll_ini,         L"Textures.General",      L"DumpOnFirstLoad");
#endif
  ConfigEntry (texture.cache.min_entries,              L"Minimum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MinEntries");
  ConfigEntry (texture.cache.max_entries,              L"Maximum Cached Textures",                                   dll_ini,         L"Textures.Cache",        L"MaxEntries");
  ConfigEntry (texture.cache.min_evict,                L"Minimum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MinEvict");
  ConfigEntry (texture.cache.max_evict,                L"Maximum Textures to Evict",                                 dll_ini,         L"Textures.Cache",        L"MaxEvict");
  ConfigEntry (texture.cache.min_size,                 L"Minimum Data Size to Evict",                                dll_ini,         L"Textures.Cache",        L"MinSizeInMiB");
  ConfigEntry (texture.cache.max_size,                 L"Maximum Data Size to Evict",                                dll_ini,         L"Textures.Cache",        L"MaxSizeInMiB");

#if 0 // unsupported config
  ConfigEntry (texture.cache.ignore_non_mipped,        L"Ignore textures without mipmaps?",                          dll_ini,         L"Textures.Cache",        L"IgnoreNonMipmapped");
  ConfigEntry (texture.cache.allow_staging,            L"Enable texture caching/dumping/injecting staged textures",  dll_ini,         L"Textures.Cache",        L"AllowStaging");
  ConfigEntry (texture.cache.allow_unsafe_refs,        L"For games with broken resource reference counting, allow"
                                                       L" textures to be cached anyway (needed for injection).",     dll_ini,         L"Textures.Cache",        L"AllowUnsafeRefCounting");
  ConfigEntry (texture.cache.manage_residency,         L"Actively manage D3D11 teture residency",                    dll_ini,         L"Textures.Cache",        L"ManageResidency");
#endif

  ConfigEntry (nvidia.api.disable,                     L"Disable NvAPI",                                             dll_ini,         L"NVIDIA.API",            L"Disable");
#if 0 // unsupported config
  ConfigEntry (nvidia.api.disable_hdr,                 L"Prevent Game from Using NvAPI HDR Features",                dll_ini,         L"NVIDIA.API",            L"DisableHDR");
  ConfigEntry (nvidia.bugs.snuffed_ansel,              L"By default, Special K disables Ansel at first launch, but"
                                                       L" users have an option under 'Help|..' to turn it back on.", dll_ini,         L"NVIDIA.Bugs",           L"AnselSleepsWithFishes");
#endif
  ConfigEntry (nvidia.sli.compatibility,               L"SLI Compatibility Bits",                                    dll_ini,         L"NVIDIA.SLI",            L"CompatibilityBits");
  ConfigEntry (nvidia.sli.num_gpus,                    L"SLI GPU Count",                                             dll_ini,         L"NVIDIA.SLI",            L"NumberOfGPUs");
  ConfigEntry (nvidia.sli.mode,                        L"SLI Mode",                                                  dll_ini,         L"NVIDIA.SLI",            L"Mode");
  ConfigEntry (nvidia.sli.override,                    L"Override Driver Defaults",                                  dll_ini,         L"NVIDIA.SLI",            L"Override");

#if 0 // unsupported config
  ConfigEntry (amd.adl.disable,                        L"Disable AMD's ADL library",                                 dll_ini,         L"AMD.ADL",               L"Disable");
#endif

  ConfigEntry (imgui.show_eula,                        L"Show Software EULA",                                        dll_ini,         L"SpecialK.System",       L"ShowEULA");
#if 0 // unsupported config
  ConfigEntry (imgui.disable_alpha,                    L"Disable Alpha Transparency (reduce flicker)",               dll_ini,         L"ImGui.Render",          L"DisableAlpha");
  ConfigEntry (imgui.antialias_lines,                  L"Reduce Aliasing on (but dim) Line Edges",                   dll_ini,         L"ImGui.Render",          L"AntialiasLines");
  ConfigEntry (imgui.antialias_contours,               L"Reduce Aliasing on (but widen) Window Borders",             dll_ini,         L"ImGui.Render",          L"AntialiasContours");

  ConfigEntry (dpi.disable,                            L"Disable DPI Scaling",                                       dll_ini,         L"DPI.Scaling",           L"Disable");
  ConfigEntry (dpi.per_monitor_aware,                  L"Windows 8.1+ Monitor DPI Awareness (needed for UE4)",       dll_ini,         L"DPI.Scaling",           L"PerMonitorAware");
  ConfigEntry (dpi.per_monitor_all_threads,            L"Further fixes required for UE4",                            dll_ini,         L"DPI.Scaling",           L"MonitorAwareOnAllThreads");

  ConfigEntry (reverse_engineering.file.trace_reads,   L"Log file read activity to logs/file_read.log",              dll_ini,         L"FileIO.Trace",          L"LogReads");
  ConfigEntry (reverse_engineering.file.ignore_reads,  L"Don't log activity for files in this list",                 dll_ini,         L"FileIO.Trace",          L"IgnoreReads");
  ConfigEntry (reverse_engineering.file.trace_writes,  L"Log file write activity to logs/file_write.log",            dll_ini,         L"FileIO.Trace",          L"LogWrites");
  ConfigEntry (reverse_engineering.file.ignore_writes, L"Don't log activity for files in this list",                 dll_ini,         L"FileIO.Trace",          L"IgnoreWrites");

  ConfigEntry (cpu.power_scheme_guid,                  L"Power Policy (GUID) to Apply At Application Start",         dll_ini,         L"CPU.Power",             L"PowerSchemeGUID");
#endif

  // The one odd-ball Steam achievement setting that can be specified per-game
  ConfigEntry (steam.achievements.sound_file,          L"Achievement Sound File",                                    dll_ini,         L"Steam.Achievements",    L"SoundFile");

  // Steam Achievement Enhancements  (Global Settings)
  //////////////////////////////////////////////////////////////////////////

  // NOTE: These were later moved to steam_ini
  ConfigEntry (steam.achievements.play_sound,          L"Silence is Bliss?",                                         achievement_ini, L"Steam.Achievements",    L"PlaySound");
  ConfigEntry (steam.achievements.take_screenshot,     L"Precious Memories",                                         achievement_ini, L"Steam.Achievements",    L"TakeScreenshot");
#if 0 // unsupported config
  ConfigEntry (steam.achievements.fetch_friend_stats,  L"Friendly Competition",                                      achievement_ini, L"Steam.Achievements",    L"FetchFriendStats");
#endif
  ConfigEntry (steam.achievements.popup.origin,        L"Achievement Popup Position",                                achievement_ini, L"Steam.Achievements",    L"PopupOrigin");
  ConfigEntry (steam.achievements.popup.animate,       L"Achievement Notification Animation",                        achievement_ini, L"Steam.Achievements",    L"AnimatePopup");
  ConfigEntry (steam.achievements.popup.show_title,    L"Achievement Popup Includes Game Title?",                    achievement_ini, L"Steam.Achievements",    L"ShowPopupTitle");
  ConfigEntry (steam.achievements.popup.inset,         L"Achievement Notification Inset X",                          achievement_ini, L"Steam.Achievements",    L"PopupInset");
  ConfigEntry (steam.achievements.popup.duration,      L"Achievement Popup Duration (in ms)",                        achievement_ini, L"Steam.Achievements",    L"PopupDuration");

  ConfigEntry (steam.system.notify_corner,             L"Overlay Notification Position  (non-Big Picture Mode)",     achievement_ini, L"Steam.System",          L"NotifyCorner");
  ConfigEntry (steam.system.appid,                     L"Steam AppID",                                               dll_ini,         L"Steam.System",          L"AppID");
  ConfigEntry (steam.system.init_delay,                L"Delay SteamAPI initialization if the game doesn't do it",   dll_ini,         L"Steam.System",          L"AutoInitDelay");
  ConfigEntry (steam.system.auto_pump,                 L"Should we force the game to run Steam callbacks?",          dll_ini,         L"Steam.System",          L"AutoPumpCallbacks");
  ConfigEntry (steam.system.block_stat_callback,       L"Block the User Stats Receipt Callback?",                    dll_ini,         L"Steam.System",          L"BlockUserStatsCallback");
  ConfigEntry (steam.system.filter_stat_callbacks,     L"Filter Unrelated Data from the User Stats Receipt Callback",dll_ini,         L"Steam.System",          L"FilterExternalDataFromCallbacks");
  ConfigEntry (steam.system.load_early,                L"Load the Steam Client DLL Early?",                          dll_ini,         L"Steam.System",          L"PreLoadSteamClient");
  ConfigEntry (steam.system.early_overlay,             L"Load the Steam Overlay Early",                              dll_ini,         L"Steam.System",          L"PreLoadSteamOverlay");
#if 0 // unsupported config
  ConfigEntry (steam.system.force_load,                L"Forcefully load steam_api{64}.dll",                         dll_ini,         L"Steam.System",          L"ForceLoadSteamAPI");
  ConfigEntry (steam.system.auto_inject,               L"Automatically load steam_api{64}.dll into any game whose "
                                                       L"path includes SteamApps\\common, but doesn't use steam_api",dll_ini,         L"Steam.System",          L"AutoInjectSteamAPI");
  ConfigEntry (steam.system.reuse_overlay_pause,       L"Pause Overlay Aware games when control panel is visible",   dll_ini,         L"Steam.System",          L"ReuseOverlayPause");
  ConfigEntry (steam.social.online_status,             L"Always apply a social state (defined by EPersonaState) at"
                                                       L" application start",                                        dll_ini,         L"Steam.Social",          L"OnlineStatus");
  ConfigEntry (steam.system.dll_path,                  L"Path to a known-working SteamAPI dll for this game.",       dll_ini,         L"Steam.System",          L"SteamPipeDLL");
  ConfigEntry (steam.callbacks.throttle,               L"-1=Unlimited, 0-oo=Upper bound limit to SteamAPI rate",     dll_ini,         L"Steam.System",          L"CallbackThrottle");

  // This option is per-game, since it has potential compatibility issues...
  ConfigEntry (steam.screenshots.smart_capture,        L"Enhanced screenshot speed and HUD options; D3D11-only.",    dll_ini,         L"Steam.Screenshots",     L"EnableSmartCapture");

  // These are all system-wide for all Steam games
  ConfigEntry (steam.overlay.hdr_luminance,            L"Make the Steam Overlay visible in HDR mode!",               steam_ini,       L"Steam.Overlay",         L"Luminance_scRGB");
  ConfigEntry (steam.screenshots.include_osd_default,  L"Should a screenshot triggered BY Steam include SK's OSD?",  steam_ini,       L"Steam.Screenshots",     L"DefaultKeybindCapturesOSD");
  ConfigEntry (steam.screenshots.keep_png_copy,        L"Keep a .PNG compressed copy of each screenshot?",           steam_ini,       L"Steam.Screenshots",     L"KeepLosslessPNG");

  Keybind     (&config.steam.screenshots.game_hud_free_keybind,
                                                       L"Take a screenshot without the HUD",                         steam_ini,       L"Steam.Screenshots");
  Keybind     (&config.steam.screenshots.sk_osd_free_keybind,
                                                       L"Take a screenshot without SK's OSD",                        steam_ini,       L"Steam.Screenshots");
  Keybind     (&config.steam.screenshots.sk_osd_insertion_keybind,
                                                       L"Take a screenshot and insert SK's OSD",                     steam_ini,       L"Steam.Screenshots");
#endif

  // Swashbucklers pay attention
  //////////////////////////////////////////////////////////////////////////

  ConfigEntry (steam.log.silent,                       L"Makes steam_api.log go away [DISABLES STEAMAPI FEATURES]",  dll_ini,         L"Steam.Log",             L"Silent");
#if 0 // unsupported config
  ConfigEntry (steam.cloud.blacklist,                  L"CSV list of files to block from cloud sync.",               dll_ini,         L"Steam.Cloud",           L"FilesNotToSync");
  ConfigEntry (steam.drm.spoof_BLoggedOn,              L"Fix For Stupid Games That Don't Know How DRM Works",        dll_ini,         L"Steam.DRMWorks",        L"SpoofBLoggedOn");
#endif

  // NOTE: These don't exist anymore.
  if (SK_IsInjected () || (SK_GetDLLRole () & DLL_ROLE::D3D9))
  {
    ConfigEntry (compatibility.d3d9.rehook_present,    L"Rehook D3D9 Present On Device Reset",                       dll_ini,         L"Compatibility.D3D9",    L"RehookPresent");
    ConfigEntry (compatibility.d3d9.rehook_reset,      L"Rehook D3D9 Reset On Device Reset",                         dll_ini,         L"Compatibility.D3D9",    L"RehookReset");
    ConfigEntry (compatibility.d3d9.hook_present_vtable,
                                                       L"Use VFtable Override for Present",                          dll_ini,         L"Compatibility.D3D9",    L"UseVFTableForPresent");
    ConfigEntry (compatibility.d3d9.hook_reset_vtable, L"Use VFtable Override for Reset",                            dll_ini,         L"Compatibility.D3D9",    L"UseVFTableForReset");

    ConfigEntry (render.d3d9.hook_type,                L"Hook Technique",                                            dll_ini,         L"Render.D3D9",           L"HookType");
    ConfigEntry (render.d3d9.force_fullscreen,         L"Force Fullscreen Mode",                                     dll_ini,         L"Render.D3D9",           L"ForceFullscreen");
  }

  if (SK_IsInjected () || (SK_GetDLLRole () & (DLL_ROLE::DXGI)))
  {
    ConfigEntry (render.dxgi.rotation,                 L"Screen Rotation (DontCare | Identity | 90 | 180 | 270 )",   dll_ini,         L"Render.DXGI",           L"Rotation");
    ConfigEntry (render.dxgi.test_present,             L"Test SwapChain Presentation Before Actually Presenting",    dll_ini,         L"Render.DXGI",           L"TestSwapChainPresent");

    ConfigEntry (texture.d3d11.dump,                   L"Dump Textures",                                             dll_ini,         L"Textures.D3D11",        L"Dump");
    ConfigEntry (texture.d3d11.res_root,               L"Resource Root",                                             dll_ini,         L"Textures.D3D11",        L"ResourceRoot");
  }


  // OSD (not in newer versions)
  //////////////////////////////////////////////////////////////////////////

  ConfigEntry (osd.show,                               L"OSD Visibility",                                            osd_ini,         L"SpecialK.OSD",          L"Show");
  ConfigEntry (osd.update_method.pump,                 L"Refresh the OSD irrespective of frame completion",          osd_ini,         L"SpecialK.OSD",          L"AutoPump");
  ConfigEntry (osd.update_method.pump_interval,        L"Time in seconds between OSD updates",                       osd_ini,         L"SpecialK.OSD",          L"PumpInterval");
  ConfigEntry (osd.text.red,                           L"OSD Color (Red)",                                           osd_ini,         L"SpecialK.OSD",          L"TextColorRed");
  ConfigEntry (osd.text.green,                         L"OSD Color (Green)",                                         osd_ini,         L"SpecialK.OSD",          L"TextColorGreen");
  ConfigEntry (osd.text.blue,                          L"OSD Color (Blue)",                                          osd_ini,         L"SpecialK.OSD",          L"TextColorBlue");
  ConfigEntry (osd.viewport.pos_x,                     L"OSD Position (X)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionX");
  ConfigEntry (osd.viewport.pos_y,                     L"OSD Position (Y)",                                          osd_ini,         L"SpecialK.OSD",          L"PositionY");
  ConfigEntry (osd.viewport.scale,                     L"OSD Scale",                                                 osd_ini,         L"SpecialK.OSD",          L"Scale");
  ConfigEntry (osd.state.remember,                     L"Remember status monitoring state",                          osd_ini,         L"SpecialK.OSD",          L"RememberMonitoringState");
  ConfigEntry (monitoring.SLI.show,                    L"Show SLI Monitoring",                                       osd_ini,         L"Monitor.SLI",           L"Show");

#undef ConfigEntry

  iSK_INI::_TSectionMap& sections =
    dll_ini->get_sections ();

  iSK_INI::_TSectionMap::const_iterator sec =
    sections.begin ();

  int import = 0;

  while (sec != sections.end ())
  {
    if (wcsstr ((*sec).first.c_str (), L"Import.")) {
      imports [import].filename = 
         static_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Filename")
             );
      imports [import].filename->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Filename" );

      imports [import].when = 
         static_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Timeframe")
             );
      imports [import].when->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"When" );

      imports [import].role = 
         static_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Role")
             );
      imports [import].role->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Role" );

      imports [import].architecture = 
         static_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Import Architecture")
             );
      imports [import].architecture->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Architecture" );

      imports [import].blacklist = 
         static_cast <sk::ParameterStringW *>
             (g_ParameterFactory.create_parameter <std::wstring> (
                L"Blacklisted Executables")
             );
      imports [import].blacklist->register_to_ini (
        dll_ini,
          (*sec).first.c_str (),
            L"Blacklist" );

      imports [import].filename->load     ();
      imports [import].when->load         ();
      imports [import].role->load         ();
      imports [import].architecture->load ();
      imports [import].blacklist->load    ();

      imports [import].hLibrary = NULL;

      ++import;

      if (import > SK_MAX_IMPORTS)
        break;
    }

    ++sec;
  }

  config.window.border_override    = true;


                                            //
  config.system.trace_load_library = true;  // Generally safe even with the 
                                            //   worst third-party software;
                                            //
                                            //  NEEDED for injector API detect


  config.system.strict_compliance  = false; // Will deadlock in DLLs that call
                                            //   LoadLibrary from DllMain
                                            //
                                            //  * NVIDIA Ansel, MSI Nahimic,
                                            //      Razer *, RTSS (Sometimes)
                                            //

  extern bool SK_DXGI_SlowStateCache;
              SK_DXGI_SlowStateCache = config.render.dxgi.slow_state_cache;


  // Default = Don't Care
  config.render.dxgi.exception_mode = -1;
  config.render.dxgi.scaling_mode   = -1;


  enum class SK_GAME_ID {
    Tyranny,              // Tyranny.exe
    Shadowrun_HongKong,   // SRHK.exe
    TidesOfNumenera,      // TidesOfNumenera.exe
    MassEffect_Andromeda, // MassEffectAndromeda.exe
    MadMax,               // MadMax.exe
    Dreamfall_Chapters,   // Dreamfall Chapters.exe
    TheWitness,           // witness_d3d11.exe, witness64_d3d11.exe
    Obduction,            // Obduction-Win64-Shipping.exe
    TheWitcher3,          // witcher3.exe
    ResidentEvil7,        // re7.exe
    DragonsDogma,         // DDDA.exe
    EverQuest             // eqgame.exe
  };

  std::unordered_map <std::wstring, SK_GAME_ID> games;

  games.emplace ( L"Tyranny.exe",                  SK_GAME_ID::Tyranny              );
  games.emplace ( L"SRHK.exe",                     SK_GAME_ID::Shadowrun_HongKong   );
  games.emplace ( L"TidesOfNumenera.exe",          SK_GAME_ID::TidesOfNumenera      );
  games.emplace ( L"MassEffectAndromeda.exe",      SK_GAME_ID::MassEffect_Andromeda );
  games.emplace ( L"MadMax.exe",                   SK_GAME_ID::MadMax               );
  games.emplace ( L"Dreamfall Chapters.exe",       SK_GAME_ID::Dreamfall_Chapters   );
  games.emplace ( L"TheWitness.exe",               SK_GAME_ID::TheWitness           );
  games.emplace ( L"Obduction-Win64-Shipping.exe", SK_GAME_ID::Obduction            );
  games.emplace ( L"witcher3.exe",                 SK_GAME_ID::TheWitcher3          );
  games.emplace ( L"re7.exe",                      SK_GAME_ID::ResidentEvil7        );
  games.emplace ( L"DDDA.exe",                     SK_GAME_ID::DragonsDogma         );
  games.emplace ( L"eqgame.exe",                   SK_GAME_ID::EverQuest            );

  //
  // Application Compatibility Overrides
  // ===================================
  //
  if (games.count (std::wstring (SK_GetHostApp ())))
  {
    switch (games [std::wstring (SK_GetHostApp ())])
    {
      case SK_GAME_ID::Tyranny:
        // Cannot auto-detect API?!
        config.apis.dxgi.d3d11.hook       = false;
        config.apis.dxgi.d3d12.hook       = false;
        config.apis.OpenGL.hook           = false;
        config.steam.filter_stat_callback = true; // Will stop running SteamAPI when it receives
                                                  //   data it didn't ask for
                                                  
        break;


      case SK_GAME_ID::Shadowrun_HongKong:
        config.compatibility.d3d9.rehook_reset = true;
        break;


      case SK_GAME_ID::TidesOfNumenera:
        // API Auto-Detect Broken (0.7.43)
        //
        //   => Auto-Detection Thinks Game is OpenGL
        //
        config.apis.d3d9.hook       = true;
        config.apis.d3d9ex.hook     = false;
        config.apis.dxgi.d3d11.hook = false;
        config.apis.dxgi.d3d12.hook = false;
        config.apis.OpenGL.hook     = false;
        config.apis.Vulkan.hook     = false;
        break;


      case SK_GAME_ID::MassEffect_Andromeda:
        // Disable Exception Handling Instead of Crashing at Shutdown
        config.render.dxgi.exception_mode      = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;

        // Not a Steam game :(
        config.steam.silent                    = true;

        config.system.strict_compliance        = false; // Uses NVIDIA Ansel, so this won't work!

        config.apis.d3d9.hook                  = false;
        config.apis.d3d9ex.hook                = false;
        config.apis.dxgi.d3d12.hook            = false;
        config.apis.OpenGL.hook                = false;
        config.apis.Vulkan.hook                = false;

        config.input.ui.capture_hidden         = true; // Mouselook is a bitch
        SK_ImGui_Cursor.prefs.no_warp.ui_open  = true;
        SK_ImGui_Cursor.prefs.no_warp.visible  = true;

        config.textures.d3d11.cache            = true;
        config.textures.cache.ignore_nonmipped = true;
        config.textures.cache.max_size         = 4096;

        config.render.dxgi.slow_state_cache    = true;
        break;


      case SK_GAME_ID::MadMax:
        // Misnomer: This uses D3D11 interop to backup D3D11.1+ states,
        //   only MadMax needs this AS FAR AS I KNOW.
        config.render.dxgi.slow_state_cache = false;
        SK_DXGI_SlowStateCache              = config.render.dxgi.slow_state_cache;
        break;


      case SK_GAME_ID::Dreamfall_Chapters:
        // One of only a handful of games where the interop hack does not work
        config.render.dxgi.slow_state_cache = true;
        SK_DXGI_SlowStateCache              = config.render.dxgi.slow_state_cache;

        config.system.trace_load_library    = true;
        config.system.strict_compliance     = false;

        // Game has mouselook problems without this
        config.input.ui.capture_mouse       = true;

        // Chances are good that we will not catch SteamAPI early enough to hook callbacks, so
        //   auto-pump.
        config.steam.auto_pump_callbacks    = true;
        config.steam.preload_client         = true;
        config.steam.filter_stat_callback   = true; // Will stop running SteamAPI when it receives
                                                    //   data it didn't ask for

        config.apis.dxgi.d3d12.hook         = false;
        config.apis.dxgi.d3d11.hook         = true;
        config.apis.d3d9.hook               = true;
        config.apis.d3d9ex.hook             = true;
        config.apis.OpenGL.hook             = false;
        config.apis.Vulkan.hook             = false;
        break;


      case SK_GAME_ID::TheWitness:
        config.system.trace_load_library    = true;
        config.system.strict_compliance     = false; // Uses Ansel

        // Game has mouselook problems without this
        config.input.ui.capture_mouse       = true;
        break;


      case SK_GAME_ID::Obduction:
        config.system.trace_load_library = true;  // Need to catch SteamAPI DLL load
        config.system.strict_compliance  = false; // Cannot block threads while loading DLLs
                                                  //   (uses an incorrectly written DLL)
        break;


      case SK_GAME_ID::TheWitcher3:
        config.system.strict_compliance   = false; // Uses NVIDIA Ansel, so this won't work!
        config.steam.filter_stat_callback = true;  // Will stop running SteamAPI when it receives
                                                   //   data it didn't ask for

        config.apis.dxgi.d3d12.hook       = false;
        config.apis.d3d9.hook             = false;
        config.apis.d3d9ex.hook           = false;
        config.apis.OpenGL.hook           = false;
        config.apis.Vulkan.hook           = false;

        config.textures.cache.ignore_nonmipped = true; // Invalid use of immutable textures
        break;


      case SK_GAME_ID::ResidentEvil7:
        config.system.trace_load_library = true;  // Need to catch SteamAPI DLL load
        config.system.strict_compliance  = false; // Cannot block threads while loading DLLs
                                                  //   (uses an incorrectly written DLL)
        break;


      case SK_GAME_ID::DragonsDogma:
        // BROKEN (by GeForce Experience)
        //
        // TODO: Debug the EXACT cause of NVIDIA's Deadlock
        //
        config.compatibility.disable_nv_bloat = true;  // PREVENT DEADLOCK CAUSED BY NVIDIA!

        config.system.trace_load_library      = true;  // Need to catch NVIDIA Bloat DLLs
        config.system.strict_compliance       = false; // Cannot block threads while loading DLLs
                                                       //   (uses an incorrectly written DLL)

        config.steam.auto_pump_callbacks      = false;
        config.steam.preload_client           = true;

        config.apis.d3d9.hook                 = true;
        config.apis.dxgi.d3d11.hook           = false;
        config.apis.dxgi.d3d12.hook           = false;
        config.apis.d3d9ex.hook               = false;
        config.apis.OpenGL.hook               = false;
        config.apis.Vulkan.hook               = false;
        break;


      case SK_GAME_ID::EverQuest:
        // Fix-up rare issues during Server Select -> Game
        //config.compatibility.d3d9.rehook_reset = true;
        break;
    }
  }

  init = true; }



  //
  // Load Parameters
  //
  if (compatibility.ignore_raptr->load ())
    config.compatibility.ignore_raptr = compatibility.ignore_raptr->get_value ();
  if (compatibility.disable_raptr->load ())
    config.compatibility.disable_raptr = compatibility.disable_raptr->get_value ();
  if (compatibility.disable_nv_bloat->load ())
    config.compatibility.disable_nv_bloat = compatibility.disable_nv_bloat->get_value ();
  if (compatibility.rehook_loadlibrary->load ())
    config.compatibility.rehook_loadlibrary = compatibility.rehook_loadlibrary->get_value ();


  if (osd.state.remember->load ())
    config.osd.remember_state = osd.state.remember->get_value ();

  if (imgui.scale->load ())
    config.imgui.scale = imgui.scale->get_value ();

  if (imgui.show_eula->load ())
    config.imgui.show_eula = imgui.show_eula->get_value ();


  if (monitoring.io.show->load () && config.osd.remember_state)
    config.io.show = monitoring.io.show->get_value ();
  if (monitoring.io.interval->load ())
    config.io.interval = monitoring.io.interval->get_value ();

  if (monitoring.fps.show->load ())
    config.fps.show = monitoring.fps.show->get_value ();

  if (monitoring.memory.show->load () && config.osd.remember_state)
    config.mem.show = monitoring.memory.show->get_value ();
  if (mem_reserve->load ())
    config.mem.reserve = mem_reserve->get_value ();

  if (monitoring.cpu.show->load () && config.osd.remember_state)
    config.cpu.show = monitoring.cpu.show->get_value ();
  if (monitoring.cpu.interval->load ())
    config.cpu.interval = monitoring.cpu.interval->get_value ();
  if (monitoring.cpu.simple->load ())
    config.cpu.simple = monitoring.cpu.simple->get_value ();

  if (monitoring.gpu.show->load ())
    config.gpu.show = monitoring.gpu.show->get_value ();
  if (monitoring.gpu.print_slowdown->load ())
    config.gpu.print_slowdown = monitoring.gpu.print_slowdown->get_value ();
  if (monitoring.gpu.interval->load ())
    config.gpu.interval = monitoring.gpu.interval->get_value ();

  if (monitoring.disk.show->load () && config.osd.remember_state)
    config.disk.show = monitoring.disk.show->get_value ();
  if (monitoring.disk.interval->load ())
    config.disk.interval = monitoring.disk.interval->get_value ();
  if (monitoring.disk.type->load ())
    config.disk.type = monitoring.disk.type->get_value ();

  //if (monitoring.pagefile.show->load () && config.osd.remember_state)
    //config.pagefile.show = monitoring.pagefile.show->get_value ();
  if (monitoring.pagefile.interval->load ())
    config.pagefile.interval = monitoring.pagefile.interval->get_value ();

  if (monitoring.time.show->load ())
    config.time.show = monitoring.time.show->get_value ();

  if (monitoring.SLI.show->load ())
    config.sli.show = monitoring.SLI.show->get_value ();


  if (apis.d3d9.hook->load ())
    config.apis.d3d9.hook = apis.d3d9.hook->get_value ();

  if (apis.d3d9ex.hook->load ())
    config.apis.d3d9ex.hook = apis.d3d9ex.hook->get_value ();

  if (apis.d3d11.hook->load ())
    config.apis.dxgi.d3d11.hook = apis.d3d11.hook->get_value ();

  if (apis.d3d12.hook->load ())
    config.apis.dxgi.d3d12.hook = apis.d3d12.hook->get_value ();

  if (apis.OpenGL.hook->load ())
    config.apis.OpenGL.hook = apis.OpenGL.hook->get_value ();

  if (apis.Vulkan.hook->load ())
    config.apis.Vulkan.hook = apis.Vulkan.hook->get_value ();

  if (nvidia.api.disable->load ())
    config.apis.NvAPI.enable = (! nvidia.api.disable->get_value ());


  if ( SK_IsInjected () ||
         ( SK_GetDLLRole () & DLL_ROLE::D3D9 ||
           SK_GetDLLRole () & DLL_ROLE::DXGI ) ) {
    // SLI only works in Direct3D
    if (nvidia.sli.compatibility->load ())
      config.nvidia.sli.compatibility =
        nvidia.sli.compatibility->get_value ();
    if (nvidia.sli.mode->load ())
      config.nvidia.sli.mode =
        nvidia.sli.mode->get_value ();
    if (nvidia.sli.num_gpus->load ())
      config.nvidia.sli.num_gpus =
        nvidia.sli.num_gpus->get_value ();
    if (nvidia.sli.override->load ())
      config.nvidia.sli.override =
        nvidia.sli.override->get_value ();

    if (render.framerate.target_fps->load ())
      config.render.framerate.target_fps =
        render.framerate.target_fps->get_value ();
    if (render.framerate.limiter_tolerance->load ())
      config.render.framerate.limiter_tolerance =
        render.framerate.limiter_tolerance->get_value ();
    if (render.framerate.wait_for_vblank->load ())
      config.render.framerate.wait_for_vblank =
        render.framerate.wait_for_vblank->get_value ();
    if (render.framerate.buffer_count->load ())
      config.render.framerate.buffer_count =
        render.framerate.buffer_count->get_value ();
    if (render.framerate.prerender_limit->load ())
      config.render.framerate.pre_render_limit =
        render.framerate.prerender_limit->get_value ();
    if (render.framerate.present_interval->load ())
      config.render.framerate.present_interval =
        render.framerate.present_interval->get_value ();

    if (render.framerate.refresh_rate) {
      if (render.framerate.refresh_rate->load ())
        config.render.framerate.refresh_rate =
          render.framerate.refresh_rate->get_value ();
    }

    if (SK_IsInjected () || SK_GetDLLRole () & DLL_ROLE::D3D9) {
      if (compatibility.d3d9.rehook_present->load ())
        config.compatibility.d3d9.rehook_present =
          compatibility.d3d9.rehook_present->get_value ();
      if (compatibility.d3d9.rehook_reset->load ())
        config.compatibility.d3d9.rehook_reset =
          compatibility.d3d9.rehook_reset->get_value ();

      if (compatibility.d3d9.hook_present_vtable->load ())
        config.compatibility.d3d9.hook_present_vftbl =
          compatibility.d3d9.hook_present_vtable->get_value ();
      if (compatibility.d3d9.hook_reset_vtable->load ())
        config.compatibility.d3d9.hook_reset_vftbl =
          compatibility.d3d9.hook_reset_vtable->get_value ();

      if (render.d3d9.force_d3d9ex->load ())
        config.render.d3d9.force_d3d9ex =
          render.d3d9.force_d3d9ex->get_value ();
      if (render.d3d9.impure->load ())
        config.render.d3d9.force_impure =
          render.d3d9.impure->get_value ();
      if (render.d3d9.force_fullscreen->load ())
        config.render.d3d9.force_fullscreen =
          render.d3d9.force_fullscreen->get_value ();
      if (render.d3d9.hook_type->load ())
        config.render.d3d9.hook_type =
          render.d3d9.hook_type->get_value ();
    }

    if (SK_IsInjected () || SK_GetDLLRole () & DLL_ROLE::DXGI) {
      if (render.framerate.max_delta_time->load ())
        config.render.framerate.max_delta_time =
          render.framerate.max_delta_time->get_value ();
      if (render.framerate.flip_discard->load ()) {
        config.render.framerate.flip_discard =
          render.framerate.flip_discard->get_value ();

        extern bool SK_DXGI_use_factory1;
        if (config.render.framerate.flip_discard)
          SK_DXGI_use_factory1 = true;
      }

      if (render.dxgi.adapter_override->load ())
        config.render.dxgi.adapter_override =
          render.dxgi.adapter_override->get_value ();

      if (render.dxgi.max_res->load ()) {
        swscanf ( render.dxgi.max_res->get_value_str ().c_str (),
                    L"%lux%lu",
                    &config.render.dxgi.res.max.x,
                      &config.render.dxgi.res.max.y );
      }
      if (render.dxgi.min_res->load ()) {
        swscanf ( render.dxgi.min_res->get_value_str ().c_str (),
                    L"%lux%lu",
                    &config.render.dxgi.res.min.x,
                      &config.render.dxgi.res.min.y );
      }

      if (render.dxgi.scaling_mode->load ())
      {
        if (! _wcsicmp (
                render.dxgi.scaling_mode->get_value_str ().c_str (),
                L"Unspecified"
              )
           )
        {
          config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_UNSPECIFIED;
        }

        else if (! _wcsicmp (
                     render.dxgi.scaling_mode->get_value_str ().c_str (),
                     L"Centered"
                   )
                )
        {
          config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_CENTERED;
        }

        else if (! _wcsicmp (
                     render.dxgi.scaling_mode->get_value_str ().c_str (),
                     L"Stretched"
                   )
                )
        {
          config.render.dxgi.scaling_mode = DXGI_MODE_SCALING_STRETCHED;
        }
      }

      if (render.dxgi.scanline_order->load ())
      {
        if (! _wcsicmp (
                render.dxgi.scanline_order->get_value_str ().c_str (),
                L"Unspecified"
              )
           )
        {
          config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        }

        else if (! _wcsicmp (
                     render.dxgi.scanline_order->get_value_str ().c_str (),
                     L"Progressive"
                   )
                )
        {
          config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        }

        else if (! _wcsicmp (
                     render.dxgi.scanline_order->get_value_str ().c_str (),
                     L"LowerFieldFirst"
                   )
                )
        {
          config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
        }

        else if (! _wcsicmp (
                     render.dxgi.scanline_order->get_value_str ().c_str (),
                     L"UpperFieldFirst"
                   )
                )
        {
          config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
        }

        // If a user specifies Interlaced, default to Lower Field First
        else if (! _wcsicmp (
                     render.dxgi.scanline_order->get_value_str ().c_str (),
                     L"Interlaced"
                   )
                )
        {
          config.render.dxgi.scanline_order = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
        }
      }

      if (render.dxgi.debug_layer->load ())
        config.render.dxgi.debug_layer = render.dxgi.debug_layer->get_value ();

      if (render.dxgi.exception_mode->load ())
      {
        if (! _wcsicmp (
                render.dxgi.exception_mode->get_value_str ().c_str (),
                L"Raise"
              )
           )
        {
          #define D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR 1
          config.render.dxgi.exception_mode = D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR;
        }

        else if (! _wcsicmp (
                     render.dxgi.exception_mode->get_value_str ().c_str (),
                     L"Ignore"
                   )
                )
        {
          config.render.dxgi.exception_mode = 0;
        }
        else
          config.render.dxgi.exception_mode = -1;
      }

      if (render.dxgi.test_present->load ())
        config.render.dxgi.test_present = render.dxgi.test_present->get_value ();

      if (render.dxgi.swapchain_wait->load ())
        config.render.framerate.swapchain_wait = render.dxgi.swapchain_wait->get_value ();

      if (texture.d3d11.cache->load ())
        config.textures.d3d11.cache = texture.d3d11.cache->get_value ();
      if (texture.d3d11.precise_hash->load ())
        config.textures.d3d11.precise_hash = texture.d3d11.precise_hash->get_value ();
      if (texture.d3d11.dump->load ())
        config.textures.d3d11.dump = texture.d3d11.dump->get_value ();
      if (texture.d3d11.inject->load ())
        config.textures.d3d11.inject = texture.d3d11.inject->get_value ();
      if (texture.d3d11.res_root->load ())
        config.textures.d3d11.res_root = texture.d3d11.res_root->get_value ();

      if (texture.cache.max_entries->load ())
        config.textures.cache.max_entries = texture.cache.max_entries->get_value ();
      if (texture.cache.min_entries->load ())
        config.textures.cache.min_entries = texture.cache.min_entries->get_value ();
      if (texture.cache.max_evict->load ())
        config.textures.cache.max_evict = texture.cache.max_evict->get_value ();
      if (texture.cache.min_evict->load ())
        config.textures.cache.min_evict = texture.cache.min_evict->get_value ();
      if (texture.cache.max_size->load ())
        config.textures.cache.max_size = texture.cache.max_size->get_value ();
      if (texture.cache.min_size->load ())
        config.textures.cache.min_size = texture.cache.min_size->get_value ();
      if (texture.cache.ignore_non_mipped->load ())
        config.textures.cache.ignore_nonmipped = texture.cache.ignore_non_mipped->get_value ();

      extern void WINAPI SK_DXGI_SetPreferredAdapter (int override_id);

      if (config.render.dxgi.adapter_override != -1)
        SK_DXGI_SetPreferredAdapter (config.render.dxgi.adapter_override);
    }
  }

  if (input.cursor.manage->load ())
    config.input.cursor.manage = input.cursor.manage->get_value ();
  if (input.cursor.keys_activate->load ())
    config.input.cursor.keys_activate = input.cursor.keys_activate->get_value ();
  if (input.cursor.timeout->load ())
    config.input.cursor.timeout = (int)(1000.0 * input.cursor.timeout->get_value ());
  if (input.cursor.ui_capture->load ())
    config.input.ui.capture = input.cursor.ui_capture->get_value ();
  if (input.cursor.hw_cursor->load ())
    config.input.ui.use_hw_cursor = input.cursor.hw_cursor->get_value ();
  if (input.cursor.no_warp_ui->load ())
    SK_ImGui_Cursor.prefs.no_warp.ui_open = input.cursor.no_warp_ui->get_value ();
  if (input.cursor.no_warp_visible->load ())
    SK_ImGui_Cursor.prefs.no_warp.visible = input.cursor.no_warp_visible->get_value ();
  if (input.cursor.block_invisible->load ())
    config.input.ui.capture_hidden = input.cursor.block_invisible->get_value ();

  if (input.gamepad.disable_ps4_hid->load ())
    config.input.gamepad.disable_ps4_hid = input.gamepad.disable_ps4_hid->get_value ();
  if (input.gamepad.rehook_xinput->load ())
    config.input.gamepad.rehook_xinput = input.gamepad.rehook_xinput->get_value ();
  if (input.gamepad.hook_xinput->load ())
    config.input.gamepad.hook_xinput = input.gamepad.hook_xinput->get_value ();
  if (input.gamepad.hook_dinput8->load ())
    config.input.gamepad.hook_dinput8 = input.gamepad.hook_dinput8->get_value ();
  if (input.gamepad.hook_hid->load ())
    config.input.gamepad.hook_hid = input.gamepad.hook_hid->get_value ();

  if (input.gamepad.xinput.placeholders->load ()) {
    int placeholder_mask = input.gamepad.xinput.placeholders->get_value ();

    config.input.gamepad.xinput.placehold [0] = ( placeholder_mask & 0x1 );
    config.input.gamepad.xinput.placehold [1] = ( placeholder_mask & 0x2 );
    config.input.gamepad.xinput.placehold [2] = ( placeholder_mask & 0x4 );
    config.input.gamepad.xinput.placehold [3] = ( placeholder_mask & 0x8 );
  }

  if (input.gamepad.xinput.ui_slot->load ())
    config.input.gamepad.xinput.ui_slot = input.gamepad.xinput.ui_slot->get_value ();

  if (window.borderless->load ()) {
    config.window.borderless = window.borderless->get_value ();
  }

  if (window.center->load ())
    config.window.center = window.center->get_value ();
  if (window.background_render->load ())
    config.window.background_render = window.background_render->get_value ();
  if (window.background_mute->load ())
    config.window.background_mute = window.background_mute->get_value ();
  if (window.offset.x->load ()) {
    std::wstring offset = window.offset.x->get_value ();

    if (wcsstr (offset.c_str (), L"%")) {
      config.window.offset.x.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.x.percent);
      config.window.offset.x.percent /= 100.0f;
    } else {
      config.window.offset.x.percent = 0.0f;
      swscanf (offset.c_str (), L"%lu", &config.window.offset.x.absolute);
    }
  }
  if (window.offset.y->load ()) {
    std::wstring offset = window.offset.y->get_value ();

    if (wcsstr (offset.c_str (), L"%")) {
      config.window.offset.y.absolute = 0;
      swscanf (offset.c_str (), L"%f%%", &config.window.offset.y.percent);
      config.window.offset.y.percent /= 100.0f;
    } else {
      config.window.offset.y.percent = 0.0f;
      swscanf (offset.c_str (), L"%lu", &config.window.offset.y.absolute);
    }
  }
  if (window.confine_cursor->load ())
    config.window.confine_cursor = window.confine_cursor->get_value ();
  if (window.unconfine_cursor->load ())
    config.window.unconfine_cursor = window.unconfine_cursor->get_value ();
  if (window.persistent_drag->load ())
    config.window.persistent_drag = window.persistent_drag->get_value ();
  if (window.fullscreen->load ())
    config.window.fullscreen = window.fullscreen->get_value ();
  if (window.fix_mouse_coords->load ())
    config.window.res.override.fix_mouse =
      window.fix_mouse_coords->get_value ();
  if (window.override->load ()) {
    swscanf ( window.override->get_value_str ().c_str (),
                L"%lux%lu",
                &config.window.res.override.x,
                  &config.window.res.override.y );
  }

  if (steam.achievements.play_sound->load ())
    config.steam.achievements.play_sound =
    steam.achievements.play_sound->get_value ();
  if (steam.achievements.sound_file->load ())
    config.steam.achievements.sound_file =
      steam.achievements.sound_file->get_value ();
  if (steam.achievements.take_screenshot->load ())
    config.steam.achievements.take_screenshot =
      steam.achievements.take_screenshot->get_value ();
  if (steam.achievements.popup.animate->load ())
    config.steam.achievements.popup.animate =
      steam.achievements.popup.animate->get_value ();
  if (steam.achievements.popup.show_title->load ())
    config.steam.achievements.popup.show_title =
      steam.achievements.popup.show_title->get_value ();
  if (steam.achievements.popup.origin->load ()) {
    config.steam.achievements.popup.origin =
      SK_Steam_PopupOriginWStrToEnum (
        steam.achievements.popup.origin->get_value ().c_str ()
      );
  } else {
    config.steam.achievements.popup.origin = 3;
  }
  if (steam.achievements.popup.inset->load ())
    config.steam.achievements.popup.inset =
      steam.achievements.popup.inset->get_value ();
  if (steam.achievements.popup.duration->load ())
    config.steam.achievements.popup.duration =
      steam.achievements.popup.duration->get_value ();

  if (config.steam.achievements.popup.duration == 0)  {
    config.steam.achievements.popup.show        = false;
    config.steam.achievements.pull_friend_stats = false;
    config.steam.achievements.pull_global_stats = false;
  }

  if (steam.log.silent->load ())
    config.steam.silent = steam.log.silent->get_value ();

  if (steam.system.appid->load ())
    config.steam.appid = steam.system.appid->get_value ();
  if (steam.system.init_delay->load ())
    config.steam.init_delay = steam.system.init_delay->get_value ();
  if (steam.system.auto_pump->load ())
    config.steam.auto_pump_callbacks = steam.system.auto_pump->get_value ();
  if (steam.system.block_stat_callback->load ())
    config.steam.block_stat_callback = steam.system.block_stat_callback->get_value ();
  if (steam.system.filter_stat_callbacks->load ())
    config.steam.filter_stat_callback = steam.system.filter_stat_callbacks->get_value ();
  if (steam.system.load_early->load ())
    config.steam.preload_client = steam.system.load_early->get_value ();
  if (steam.system.early_overlay->load ())
    config.steam.preload_overlay = steam.system.early_overlay->get_value ();
  if (steam.system.notify_corner->load ())
    config.steam.notify_corner =
      SK_Steam_PopupOriginWStrToEnum (
        steam.system.notify_corner->get_value ().c_str ()
    );


  if (osd.show->load ())
    config.osd.show = osd.show->get_value ();

  if (osd.update_method.pump->load ())
    config.osd.pump = osd.update_method.pump->get_value ();

  if (osd.update_method.pump_interval->load ())
    config.osd.pump_interval = osd.update_method.pump_interval->get_value ();

  if (osd.text.red->load ())
    config.osd.red = osd.text.red->get_value ();
  if (osd.text.green->load ())
    config.osd.green = osd.text.green->get_value ();
  if (osd.text.blue->load ())
    config.osd.blue = osd.text.blue->get_value ();

  if (osd.viewport.pos_x->load ())
    config.osd.pos_x = osd.viewport.pos_x->get_value ();
  if (osd.viewport.pos_y->load ())
    config.osd.pos_y = osd.viewport.pos_y->get_value ();
  if (osd.viewport.scale->load ())
    config.osd.scale = osd.viewport.scale->get_value ();


  if (init_delay->load ())
    config.system.init_delay = init_delay->get_value ();
  if (silent->load ())
    config.system.silent = silent->get_value ();
  if (trace_libraries->load ())
    config.system.trace_load_library = trace_libraries->get_value ();
  if (strict_compliance->load ())
    config.system.strict_compliance = strict_compliance->get_value ();
  if (resolve_symbol_names->load ())
    config.system.resolve_symbol_names = resolve_symbol_names->get_value ();
  if (log_level->load ())
    config.system.log_level = log_level->get_value ();
  if (prefer_fahrenheit->load ())
    config.system.prefer_fahrenheit = prefer_fahrenheit->get_value ();

  if (ignore_rtss_delay->load ())
    config.system.ignore_rtss_delay = ignore_rtss_delay->get_value ();

  if (handle_crashes->load ())
    config.system.handle_crashes = handle_crashes->get_value ();

  if (debug_output->load ())
    config.system.display_debug_out = debug_output->get_value ();

  if (game_output->load ())
    config.system.game_output = game_output->get_value ();

  if (enable_cegui->load ())
    config.cegui.enable = enable_cegui->get_value ();

  if (version->load ())
    config.system.version = version->get_value ();


  //
  // EMERGENCY OVERRIDES
  //
  config.input.ui.use_raw_input = false;



  config.imgui.font.default.file  = "arial.ttf";
  config.imgui.font.default.size  = 18.0f;

  config.imgui.font.japanese.file = "msgothic.ttc";
  config.imgui.font.japanese.size = 18.0f;

  config.imgui.font.cyrillic.file = "arial.ttf";
  config.imgui.font.cyrillic.size = 18.0f;

  config.imgui.font.korean.file   = "malgun.ttf";
  config.imgui.font.korean.size   = 18.0f;
                                  
  config.imgui.font.chinese.file  = "msyh.ttc";
  config.imgui.font.chinese.size  = 18.0f;



  if (empty)
    return false;

  return true;
}

void
SK_SaveConfig ( std::wstring name,
                bool         close_config )
{
  //
  // Shuttind down before initializaiton would be damn near fatal if we didn't catch this! :)
  //
  if (dll_ini == nullptr)
    return;

  compatibility.ignore_raptr->set_value       (config.compatibility.ignore_raptr);
  compatibility.disable_raptr->set_value      (config.compatibility.disable_raptr);
  compatibility.disable_nv_bloat->set_value   (config.compatibility.disable_nv_bloat);
  compatibility.rehook_loadlibrary->set_value (config.compatibility.rehook_loadlibrary);

  monitoring.memory.show->set_value           (config.mem.show);
  mem_reserve->set_value                      (config.mem.reserve);

  monitoring.fps.show->set_value              (config.fps.show);

  monitoring.io.show->set_value               (config.io.show);
  monitoring.io.interval->set_value           (config.io.interval);

  monitoring.cpu.show->set_value              (config.cpu.show);
  monitoring.cpu.interval->set_value          (config.cpu.interval);
  monitoring.cpu.simple->set_value            (config.cpu.simple);

  monitoring.gpu.show->set_value              (config.gpu.show);
  monitoring.gpu.print_slowdown->set_value    (config.gpu.print_slowdown);
  monitoring.gpu.interval->set_value          (config.gpu.interval);

  monitoring.disk.show->set_value             (config.disk.show);
  monitoring.disk.interval->set_value         (config.disk.interval);
  monitoring.disk.type->set_value             (config.disk.type);

  monitoring.pagefile.show->set_value         (config.pagefile.show);
  monitoring.pagefile.interval->set_value     (config.pagefile.interval);

  if (! (nvapi_init && sk::NVAPI::nv_hardware && sk::NVAPI::CountSLIGPUs () > 1))
    config.sli.show = false;

  monitoring.SLI.show->set_value              (config.sli.show);
  monitoring.time.show->set_value             (config.time.show);

  osd.show->set_value                         (config.osd.show);
  osd.update_method.pump->set_value           (config.osd.pump);
  osd.update_method.pump_interval->set_value  (config.osd.pump_interval);
  osd.text.red->set_value                     (config.osd.red);
  osd.text.green->set_value                   (config.osd.green);
  osd.text.blue->set_value                    (config.osd.blue);
  osd.viewport.pos_x->set_value               (config.osd.pos_x);
  osd.viewport.pos_y->set_value               (config.osd.pos_y);
  osd.viewport.scale->set_value               (config.osd.scale);
  osd.state.remember->set_value               (config.osd.remember_state);

  imgui.scale->set_value                      (config.imgui.scale);
  imgui.show_eula->set_value                  (config.imgui.show_eula);

  apis.d3d9.hook->set_value                   (config.apis.d3d9.hook);
  apis.d3d9ex.hook->set_value                 (config.apis.d3d9ex.hook);
  apis.d3d11.hook->set_value                  (config.apis.dxgi.d3d11.hook);
  apis.d3d12.hook->set_value                  (config.apis.dxgi.d3d12.hook);
  apis.OpenGL.hook->set_value                 (config.apis.OpenGL.hook);
  apis.Vulkan.hook->set_value                 (config.apis.Vulkan.hook);

  input.cursor.manage->set_value              (config.input.cursor.manage);
  input.cursor.keys_activate->set_value       (config.input.cursor.keys_activate);
  input.cursor.timeout->set_value             ((float)config.input.cursor.timeout / 1000.0f);
  input.cursor.ui_capture->set_value          (config.input.ui.capture);
  input.cursor.hw_cursor->set_value           (config.input.ui.use_hw_cursor);
  input.cursor.block_invisible->set_value     (config.input.ui.capture_hidden);
  input.cursor.no_warp_ui->set_value          (SK_ImGui_Cursor.prefs.no_warp.ui_open);
  input.cursor.no_warp_visible->set_value     (SK_ImGui_Cursor.prefs.no_warp.visible);

  input.gamepad.disable_ps4_hid->set_value    (config.input.gamepad.disable_ps4_hid);
  input.gamepad.rehook_xinput->set_value      (config.input.gamepad.rehook_xinput);

  int placeholder_mask = 0x0;

  placeholder_mask |= (config.input.gamepad.xinput.placehold [0] ? 0x1 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [1] ? 0x2 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [2] ? 0x4 : 0x0);
  placeholder_mask |= (config.input.gamepad.xinput.placehold [3] ? 0x8 : 0x0);

  input.gamepad.xinput.placeholders->set_value (placeholder_mask);
  input.gamepad.xinput.ui_slot->set_value      (config.input.gamepad.xinput.ui_slot);

  window.borderless->set_value                (config.window.borderless);
  window.center->set_value                    (config.window.center);
  window.background_render->set_value         (config.window.background_render);
  window.background_mute->set_value           (config.window.background_mute);
  if (config.window.offset.x.absolute != 0) {
    wchar_t wszAbsolute [16];
    _swprintf (wszAbsolute, L"%lu", config.window.offset.x.absolute);
    window.offset.x->set_value (wszAbsolute);
  } else {
    wchar_t wszPercent [16];
    _swprintf (wszPercent, L"%08.6f", 100.0f * config.window.offset.x.percent);
    SK_RemoveTrailingDecimalZeros (wszPercent);
    lstrcatW (wszPercent, L"%");
    window.offset.x->set_value (wszPercent);
  }
  if (config.window.offset.y.absolute != 0) {
    wchar_t wszAbsolute [16];
    _swprintf (wszAbsolute, L"%lu", config.window.offset.y.absolute);
    window.offset.y->set_value (wszAbsolute);
  } else {
    wchar_t wszPercent [16];
    _swprintf (wszPercent, L"%08.6f", 100.0f * config.window.offset.y.percent);
    SK_RemoveTrailingDecimalZeros (wszPercent);
    lstrcatW (wszPercent, L"%");
    window.offset.y->set_value (wszPercent);
  }
  window.confine_cursor->set_value            (config.window.confine_cursor);
  window.unconfine_cursor->set_value          (config.window.unconfine_cursor);
  window.persistent_drag->set_value           (config.window.persistent_drag);
  window.fullscreen->set_value                (config.window.fullscreen);
  window.fix_mouse_coords->set_value          (config.window.res.override.fix_mouse);

  wchar_t wszFormattedRes [64] = { L'\0' };

  wsprintf ( wszFormattedRes, L"%lux%lu",
               config.window.res.override.x,
                 config.window.res.override.y );

  window.override->set_value (wszFormattedRes);

  if ( SK_IsInjected () ||
      (SK_GetDLLRole () & DLL_ROLE::D3D9 || SK_GetDLLRole () & DLL_ROLE::DXGI) )
  {
    extern float target_fps;

    render.framerate.target_fps->set_value        (target_fps);
    render.framerate.limiter_tolerance->set_value (config.render.framerate.limiter_tolerance);
    render.framerate.wait_for_vblank->set_value   (config.render.framerate.wait_for_vblank);
    render.framerate.prerender_limit->set_value   (config.render.framerate.pre_render_limit);
    render.framerate.buffer_count->set_value      (config.render.framerate.buffer_count);
    render.framerate.present_interval->set_value  (config.render.framerate.present_interval);

    if (render.framerate.refresh_rate != nullptr)
      render.framerate.refresh_rate->set_value (config.render.framerate.refresh_rate);

    // SLI only works in Direct3D
    nvidia.sli.compatibility->set_value          (config.nvidia.sli.compatibility);
    nvidia.sli.mode->set_value                   (config.nvidia.sli.mode);
    nvidia.sli.num_gpus->set_value               (config.nvidia.sli.num_gpus);
    nvidia.sli.override->set_value               (config.nvidia.sli.override);

    if (  SK_IsInjected () ||
        ( SK_GetDLLRole () & DLL_ROLE::DXGI ) )
    {
      render.framerate.max_delta_time->set_value (config.render.framerate.max_delta_time);
      render.framerate.flip_discard->set_value   (config.render.framerate.flip_discard);

      texture.d3d11.cache->set_value        (config.textures.d3d11.cache);
      texture.d3d11.precise_hash->set_value (config.textures.d3d11.precise_hash);
      texture.d3d11.dump->set_value         (config.textures.d3d11.dump);
      texture.d3d11.inject->set_value       (config.textures.d3d11.inject);
      texture.d3d11.res_root->set_value     (config.textures.d3d11.res_root);

      texture.cache.max_entries->set_value (config.textures.cache.max_entries);
      texture.cache.min_entries->set_value (config.textures.cache.min_entries);
      texture.cache.max_evict->set_value   (config.textures.cache.max_evict);
      texture.cache.min_evict->set_value   (config.textures.cache.min_evict);
      texture.cache.max_size->set_value    (config.textures.cache.max_size);
      texture.cache.min_size->set_value    (config.textures.cache.min_size);

      texture.cache.ignore_non_mipped->set_value (config.textures.cache.ignore_nonmipped);

      wsprintf ( wszFormattedRes, L"%lux%lu",
                   config.render.dxgi.res.max.x,
                     config.render.dxgi.res.max.y );

      render.dxgi.max_res->set_value (wszFormattedRes);

      wsprintf ( wszFormattedRes, L"%lux%lu",
                   config.render.dxgi.res.min.x,
                     config.render.dxgi.res.min.y );

      render.dxgi.min_res->set_value (wszFormattedRes);

      render.dxgi.swapchain_wait->set_value (config.render.framerate.swapchain_wait);

      switch (config.render.dxgi.scaling_mode)
      {
        case DXGI_MODE_SCALING_UNSPECIFIED:
          render.dxgi.scaling_mode->set_value (L"Unspecified");
          break;
        case DXGI_MODE_SCALING_CENTERED:
          render.dxgi.scaling_mode->set_value (L"Centered");
          break;
        case DXGI_MODE_SCALING_STRETCHED:
          render.dxgi.scaling_mode->set_value (L"Stretched");
          break;
        default:
          render.dxgi.scaling_mode->set_value (L"DontCare");
          break;
      }

      switch (config.render.dxgi.scanline_order)
      {
        case DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED:
          render.dxgi.scanline_order->set_value (L"Unspecified");
          break;
        case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
          render.dxgi.scanline_order->set_value (L"Progressive");
          break;
        case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
          render.dxgi.scanline_order->set_value (L"LowerFieldFirst");
          break;
        case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
          render.dxgi.scanline_order->set_value (L"UpperFieldFirst");
          break;
        default:
          render.dxgi.scanline_order->set_value (L"DontCare");
          break;
      }

      switch (config.render.dxgi.exception_mode)
      {
        case D3D11_RAISE_FLAG_DRIVER_INTERNAL_ERROR:
          render.dxgi.exception_mode->set_value (L"Raise");
          break;
        case 0:
          render.dxgi.exception_mode->set_value (L"Ignore");
          break;
        default:
          render.dxgi.exception_mode->set_value (L"DontCare");
          break;
      }

      render.dxgi.debug_layer->set_value (config.render.dxgi.debug_layer);
    }

    if (SK_IsInjected () || SK_GetDLLRole () & DLL_ROLE::D3D9)
    {
      render.d3d9.force_d3d9ex->set_value     (config.render.d3d9.force_d3d9ex);
      render.d3d9.force_fullscreen->set_value (config.render.d3d9.force_fullscreen);
      render.d3d9.hook_type->set_value        (config.render.d3d9.hook_type);
    }
  }

  steam.achievements.sound_file->set_value      (config.steam.achievements.sound_file);
  steam.achievements.play_sound->set_value      (config.steam.achievements.play_sound);
  steam.achievements.take_screenshot->set_value (config.steam.achievements.take_screenshot);
  steam.achievements.popup.origin->set_value    (
    SK_Steam_PopupOriginToWStr (config.steam.achievements.popup.origin)
  );
  steam.achievements.popup.inset->set_value      (config.steam.achievements.popup.inset);

  if (! config.steam.achievements.popup.show)
    config.steam.achievements.popup.duration = 0;

  steam.achievements.popup.duration->set_value   (config.steam.achievements.popup.duration);
  steam.achievements.popup.animate->set_value    (config.steam.achievements.popup.animate);
  steam.achievements.popup.show_title->set_value (config.steam.achievements.popup.show_title);

  if (config.steam.appid == 0) {
    if (SK::SteamAPI::AppID () != 0 &&
        SK::SteamAPI::AppID () != 1)
      config.steam.appid = SK::SteamAPI::AppID ();
  }

  steam.system.appid->set_value                 (config.steam.appid);
  steam.system.init_delay->set_value            (config.steam.init_delay);
  steam.system.auto_pump->set_value             (config.steam.auto_pump_callbacks);
  steam.system.block_stat_callback->set_value   (config.steam.block_stat_callback);
  steam.system.filter_stat_callbacks->set_value (config.steam.filter_stat_callback);
  steam.system.load_early->set_value            (config.steam.preload_client);
  steam.system.early_overlay->set_value         (config.steam.preload_overlay);
  steam.system.notify_corner->set_value         (
    SK_Steam_PopupOriginToWStr (config.steam.notify_corner)
  );

  steam.log.silent->set_value                (config.steam.silent);

  init_delay->set_value                      (config.system.init_delay);
  silent->set_value                          (config.system.silent);
  log_level->set_value                       (config.system.log_level);
  prefer_fahrenheit->set_value               (config.system.prefer_fahrenheit);

  apis.d3d9.hook->store                   ();
  apis.d3d9ex.hook->store                 ();
  apis.d3d11.hook->store                  ();
  apis.d3d12.hook->store                  ();
  apis.OpenGL.hook->store                 ();
  apis.Vulkan.hook->store                 ();

  compatibility.ignore_raptr->store       ();
  compatibility.disable_raptr->store      ();
  compatibility.disable_nv_bloat->store   ();
  compatibility.rehook_loadlibrary->store ();

  osd.state.remember->store               ();

  monitoring.memory.show->store           ();
  mem_reserve->store                      ();

  monitoring.SLI.show->store              ();
  monitoring.time.show->store             ();

  monitoring.fps.show->store              ();

  monitoring.io.show->store               ();
  monitoring.io.interval->store           ();

  monitoring.cpu.show->store               ();
  monitoring.cpu.interval->store           ();
  monitoring.cpu.simple->store             ();

  monitoring.gpu.show->store               ();
  monitoring.gpu.print_slowdown->store     ();
  monitoring.gpu.interval->store           ();

  monitoring.disk.show->store              ();
  monitoring.disk.interval->store          ();
  monitoring.disk.type->store              ();

  monitoring.pagefile.show->store          ();
  monitoring.pagefile.interval->store      ();

  input.cursor.manage->store               ();
  input.cursor.keys_activate->store        ();
  input.cursor.timeout->store              ();
  input.cursor.ui_capture->store           ();
  input.cursor.hw_cursor->store            ();
  input.cursor.block_invisible->store      ();
  input.cursor.no_warp_ui->store           ();
  input.cursor.no_warp_visible->store      ();

  input.gamepad.disable_ps4_hid->store     ();
  input.gamepad.rehook_xinput->store       ();
  input.gamepad.xinput.ui_slot->store      ();
  input.gamepad.xinput.placeholders->store ();

  window.borderless->store                 ();
  window.center->store                     ();
  window.background_render->store          ();
  window.background_mute->store            ();
  window.offset.x->store                   ();
  window.offset.y->store                   ();
  window.confine_cursor->store             ();
  window.unconfine_cursor->store           ();
  window.persistent_drag->store            ();
  window.fullscreen->store                 ();
  window.fix_mouse_coords->store           ();
  window.override->store                   ();

  nvidia.api.disable->store                ();

  if (  SK_IsInjected ()                  || 
      ( SK_GetDLLRole () & DLL_ROLE::DXGI ||
        SK_GetDLLRole () & DLL_ROLE::D3D9 ) ) {
    render.framerate.target_fps->store        ();
    render.framerate.limiter_tolerance->store ();
    render.framerate.wait_for_vblank->store   ();
    render.framerate.buffer_count->store      ();
    render.framerate.prerender_limit->store   ();
    render.framerate.present_interval->store  ();

    if (sk::NVAPI::nv_hardware) {
      nvidia.sli.compatibility->store        ();
      nvidia.sli.mode->store                 ();
      nvidia.sli.num_gpus->store             ();
      nvidia.sli.override->store             ();
    }

    if (  SK_IsInjected () ||
        ( SK_GetDLLRole () & DLL_ROLE::DXGI ) ) {
      render.framerate.max_delta_time->store ();
      render.framerate.flip_discard->store   ();

      texture.d3d11.cache->store        ();
      texture.d3d11.precise_hash->store ();
      texture.d3d11.dump->store         ();
      texture.d3d11.inject->store       ();
      texture.d3d11.res_root->store     ();

      texture.cache.max_entries->store ();
      texture.cache.min_entries->store ();
      texture.cache.max_evict->store   ();
      texture.cache.min_evict->store   ();
      texture.cache.max_size->store    ();
      texture.cache.min_size->store    ();

      texture.cache.ignore_non_mipped->store ();

      render.dxgi.max_res->store        ();
      render.dxgi.min_res->store        ();
      render.dxgi.scaling_mode->store   ();
      render.dxgi.scanline_order->store ();
      render.dxgi.exception_mode->store ();
      render.dxgi.debug_layer->store    ();

      render.dxgi.swapchain_wait->store ();
    }

    if (  SK_IsInjected () ||
        ( SK_GetDLLRole () & DLL_ROLE::D3D9 ) ) {
      render.d3d9.force_d3d9ex->store      ();
      render.d3d9.force_fullscreen->store  ();
      render.d3d9.hook_type->store         ();
    }
  }

  if (render.framerate.refresh_rate != nullptr)
    render.framerate.refresh_rate->store ();

  osd.show->store                        ();
  osd.update_method.pump->store          ();
  osd.update_method.pump_interval->store ();
  osd.text.red->store                    ();
  osd.text.green->store                  ();
  osd.text.blue->store                   ();
  osd.viewport.pos_x->store              ();
  osd.viewport.pos_y->store              ();
  osd.viewport.scale->store              ();

  imgui.scale->store                     ();
  imgui.show_eula->store                 ();

  steam.achievements.sound_file->store       ();
  steam.achievements.play_sound->store       ();
  steam.achievements.take_screenshot->store  ();
  steam.achievements.popup.show_title->store ();
  steam.achievements.popup.animate->store    ();
  steam.achievements.popup.duration->store   ();
  steam.achievements.popup.inset->store      ();
  steam.achievements.popup.origin->store     ();
  steam.system.notify_corner->store          ();
  steam.system.appid->store                  ();
  steam.system.init_delay->store             ();
  steam.system.auto_pump->store              ();
  steam.system.block_stat_callback->store    ();
  steam.system.filter_stat_callbacks->store  ();
  steam.system.load_early->store             ();
  steam.system.early_overlay->store          ();
  steam.log.silent->store                    ();

  init_delay->store                      ();
  silent->store                          ();
  log_level->store                       ();
  prefer_fahrenheit->store               ();

  ignore_rtss_delay->set_value           (config.system.ignore_rtss_delay);
  ignore_rtss_delay->store               ();

  handle_crashes->set_value              (config.system.handle_crashes);
  handle_crashes->store                  ();

  game_output->set_value                 (config.system.game_output);
  game_output->store                     ();

  // Only add this to the INI file if it differs from default
  if (config.system.display_debug_out != debug_output->get_value ())
  {
    debug_output->set_value              (config.system.display_debug_out);
    debug_output->store                  ();
  }

  enable_cegui->set_value                (config.cegui.enable);
  enable_cegui->store                    ();

  trace_libraries->set_value             (config.system.trace_load_library);
  trace_libraries->store                 ();

  strict_compliance->set_value           (config.system.strict_compliance);
  strict_compliance->store               ();

  resolve_symbol_names->set_value        (config.system.resolve_symbol_names);
  resolve_symbol_names->store            ();

  version->set_value                     (SK_VER_STR);
  version->store                         ();

  if (! (nvapi_init && sk::NVAPI::nv_hardware))
    dll_ini->remove_section (L"NVIDIA.SLI");

  wchar_t wszFullName [ MAX_PATH + 2 ] = { L'\0' };

  lstrcatW (wszFullName, SK_GetConfigPath ());
  lstrcatW (wszFullName,       name.c_str ());
  lstrcatW (wszFullName,             L".ini");

  dll_ini->write ( wszFullName );
  osd_ini->write ( std::wstring ( SK_GetDocumentsDir () +
                     L"\\My Mods\\SpecialK\\Global\\osd.ini"
                   ).c_str () );
  achievement_ini->write ( std::wstring ( SK_GetDocumentsDir () +
                     L"\\My Mods\\SpecialK\\Global\\achievements.ini"
                   ).c_str () );

  if (close_config) {
    if (dll_ini != nullptr) {
      delete dll_ini;
      dll_ini = nullptr;
    }

    if (osd_ini != nullptr) {
      delete osd_ini;
      osd_ini = nullptr;
    }

    if (achievement_ini != nullptr) {
      delete achievement_ini;
      achievement_ini = nullptr;
    }
  }
}

const wchar_t*
__stdcall
SK_GetVersionStr (void)
{
  return SK_VER_STR;
}


#include <unordered_map>

std::unordered_map <std::wstring, BYTE> humanKeyNameToVirtKeyCode;
std::unordered_map <BYTE, std::wstring> virtKeyCodeToHumanKeyName;

#include <queue>

void
SK_Keybind::update (void)
{
  human_readable = L"";

  std::wstring key_name = virtKeyCodeToHumanKeyName [vKey];

  if (! key_name.length ())
    return;

  std::queue <std::wstring> words;

  if (ctrl)
    words.push (L"Ctrl");

  if (alt)
    words.push (L"Alt");

  if (shift)
    words.push (L"Shift");

  words.push (key_name);

  while (! words.empty ())
  {
    human_readable += words.front ();
    words.pop ();

    if (! words.empty ())
      human_readable += L"+";
  }
}

void
SK_Keybind::parse (void)
{
  vKey = 0x00;

  static bool init = false;

  if (! init)
  {
    for (int i = 0; i < 0xFF; i++)
    {
      wchar_t name [32] = { L'\0' };

      switch (i)
      {
        case VK_F1:     wcscat (name, L"F1");           break;
        case VK_F2:     wcscat (name, L"F2");           break;
        case VK_F3:     wcscat (name, L"F3");           break;
        case VK_F4:     wcscat (name, L"F4");           break;
        case VK_F5:     wcscat (name, L"F5");           break;
        case VK_F6:     wcscat (name, L"F6");           break;
        case VK_F7:     wcscat (name, L"F7");           break;
        case VK_F8:     wcscat (name, L"F8");           break;
        case VK_F9:     wcscat (name, L"F9");           break;
        case VK_F10:    wcscat (name, L"F10");          break;
        case VK_F11:    wcscat (name, L"F11");          break;
        case VK_F12:    wcscat (name, L"F12");          break;
        case VK_F13:    wcscat (name, L"F13");          break;
        case VK_F14:    wcscat (name, L"F14");          break;
        case VK_F15:    wcscat (name, L"F15");          break;
        case VK_F16:    wcscat (name, L"F16");          break;
        case VK_F17:    wcscat (name, L"F17");          break;
        case VK_F18:    wcscat (name, L"F18");          break;
        case VK_F19:    wcscat (name, L"F19");          break;
        case VK_F20:    wcscat (name, L"F20");          break;
        case VK_F21:    wcscat (name, L"F21");          break;
        case VK_F22:    wcscat (name, L"F22");          break;
        case VK_F23:    wcscat (name, L"F23");          break;
        case VK_F24:    wcscat (name, L"F24");          break;
        case VK_PRINT:  wcscat (name, L"Print Screen"); break;
        case VK_SCROLL: wcscat (name, L"Scroll Lock");  break;
        case VK_PAUSE:  wcscat (name, L"Pause Break");  break;

        default:
        {
          unsigned int scanCode =
            ( MapVirtualKey (i, 0) & 0xFF );

          BYTE buf [256] = { 0 };
          unsigned short int temp;
          
          bool asc = (i <= 32);

          if (! asc && i != VK_DIVIDE)
             asc = ToAscii ( i, scanCode, buf, &temp, 1 );

          scanCode            <<= 16;
          scanCode   |= ( 0x1 <<  25  );

          if (! asc)
            scanCode |= ( 0x1 << 24   );
    
          GetKeyNameText ( scanCode,
                             name,
                               32 );
        } break;
      }

    
      if ( i != VK_CONTROL && i != VK_MENU     &&
           i != VK_SHIFT   && i != VK_OEM_PLUS && i != VK_OEM_MINUS )
      {
        humanKeyNameToVirtKeyCode [   name] = (BYTE)i;
        virtKeyCodeToHumanKeyName [(BYTE)i] =    name;
      }
    }
    
    humanKeyNameToVirtKeyCode [L"Plus"]  = VK_OEM_PLUS;
    humanKeyNameToVirtKeyCode [L"Minus"] = VK_OEM_MINUS;
    humanKeyNameToVirtKeyCode [L"Ctrl"]  = VK_CONTROL;
    humanKeyNameToVirtKeyCode [L"Alt"]   = VK_MENU;
    humanKeyNameToVirtKeyCode [L"Shift"] = VK_SHIFT;
    
    virtKeyCodeToHumanKeyName [VK_CONTROL]    = L"Ctrl";
    virtKeyCodeToHumanKeyName [VK_MENU]       = L"Alt";
    virtKeyCodeToHumanKeyName [VK_SHIFT]      = L"Shift";
    virtKeyCodeToHumanKeyName [VK_OEM_PLUS]   = L"Plus";
    virtKeyCodeToHumanKeyName [VK_OEM_MINUS]  = L"Minus";

    init = true;
  }

  wchar_t wszKeyBind [128] = { L'\0' };

  wcsncpy (wszKeyBind, human_readable.c_str (), 128);

  wchar_t* wszBuf;
  wchar_t* wszTok = std::wcstok (wszKeyBind, L"+", &wszBuf);

  ctrl  = false;
  alt   = false;
  shift = false;

  if (wszTok == nullptr)
  {
    vKey  = humanKeyNameToVirtKeyCode [wszKeyBind];
  }

  while (wszTok)
  {
    BYTE vKey_ = humanKeyNameToVirtKeyCode [wszTok];

    if (vKey_ == VK_CONTROL)
      ctrl = true;
    else if (vKey_ == VK_SHIFT)
      shift = true;
    else if (vKey_ == VK_MENU)
      alt = true;
    else
      vKey = vKey_;

    wszTok = std::wcstok (nullptr, L"+", &wszBuf);
  }
}