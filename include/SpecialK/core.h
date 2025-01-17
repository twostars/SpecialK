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

#ifndef __SK__CORE_H__
#define __SK__CORE_H__

#undef COM_NO_WINDOWS_H
#include <Windows.h>

#include <SpecialK/memory_monitor.h>
#include <SpecialK/nvapi.h>

extern HMODULE          backend_dll;
extern CRITICAL_SECTION init_mutex;

// Disable SLI memory in Batman Arkham Knight
extern bool                     USE_SLI;

extern NV_GET_CURRENT_SLI_STATE sli_state;
extern BOOL                     nvapi_init;

extern "C" {
  // We have some really sneaky overlays that manage to call some of our
  //   exported functions before the DLL's even attached -- make them wait,
  //     so we don't crash and burn!
  void WaitForInit (void);

  void __stdcall SK_InitCore     (const wchar_t* backend, void* callback);
  bool __stdcall SK_StartupCore  (const wchar_t* backend, void* callback);
  bool WINAPI    SK_ShutdownCore (const wchar_t* backend);

  void    STDMETHODCALLTYPE SK_BeginBufferSwap (void);
  HRESULT STDMETHODCALLTYPE SK_EndBufferSwap   (HRESULT hr, IUnknown* device = nullptr);

  const wchar_t* __stdcall SK_DescribeHRESULT (HRESULT result);
}

void
__stdcall
SK_SetConfigPath (const wchar_t* path);

const wchar_t*
__stdcall
SK_GetConfigPath (void);

const wchar_t*
SK_GetHostApp (void);

// NOT the working directory, this is the directory that
//   the executable is located in.
const wchar_t*
SK_GetHostPath (void);

const wchar_t*
SK_GetBlacklistFilename (void);

const wchar_t*
__stdcall
SK_GetRootPath (void);

HMODULE
__stdcall
SK_GetDLL (void);

DLL_ROLE
__stdcall
SK_GetDLLRole (void);

void
__cdecl
SK_SetDLLRole (DLL_ROLE role);

bool
__cdecl
SK_IsHostAppSKIM (void);

bool
__stdcall
SK_IsInjected (bool set = false);

ULONG
__stdcall
SK_GetFramesDrawn (void);

enum DLL_ROLE {
  INVALID    = 0x00,

  // Graphics APIs
  DXGI       = 0x01, // D3D 10-12
  D3D9       = 0x02,
  OpenGL     = 0x04,
  Vulkan     = 0x08,

  // Other DLLs
  PlugIn     = 0x00010000, // Stuff like Tales of Zestiria "Fix"
  ThirdParty = 0x80000000
};

#endif /* __SK__CORE_H__ */