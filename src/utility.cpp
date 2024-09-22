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

#include <SpecialK/utility.h>
#include <SpecialK/core.h>

#include <UserEnv.h>
#pragma comment (lib, "userenv.lib")

#include <Shlobj.h>
#pragma comment (lib, "shell32.lib")

#include <process.h>
#include <tlhelp32.h>

#include <Shlwapi.h>
#include <atlbase.h>

#undef min
#undef max

int
SK_MessageBox (std::wstring caption, std::wstring title, uint32_t flags)
{
  return
    MessageBox (NULL, caption.c_str (), title.c_str (),
                flags | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND);
}

std::string
SK_WideCharToUTF8 (const std::wstring& in)
{
  int len = WideCharToMultiByte ( CP_UTF8, 0x00, in.c_str (), -1, nullptr, 0, nullptr, FALSE );

  std::string out;
              out.resize (len);

  WideCharToMultiByte           ( CP_UTF8, 0x00, in.c_str (), (int)in.length (), (char *)out.data (), len, nullptr, FALSE );

  return out;
}

std::wstring
SK_UTF8ToWideChar (const std::string& in)
{
  int len = MultiByteToWideChar ( CP_UTF8, 0x00, in.c_str (), -1, nullptr, 0 );

  std::wstring out;
  out.resize (len);

  MultiByteToWideChar           ( CP_UTF8, 0x00, in.c_str (), (int)in.length (), (wchar_t *)out.data (), len );

  return out;
}

std::wstring
SK_GetDocumentsDir (void)
{
  HANDLE hToken;

  if (! OpenProcessToken (GetCurrentProcess (), TOKEN_READ, &hToken))
    return NULL;

  CComHeapPtr <wchar_t> str;

  SHGetKnownFolderPath (FOLDERID_Documents, 0, hToken, &str);

  return std::wstring (str);
}

std::wstring
SK_GetFontsDir (void)
{
  HANDLE hToken;

  if (! OpenProcessToken (GetCurrentProcess (), TOKEN_READ, &hToken))
    return NULL;

  CComHeapPtr <wchar_t> str;

  SHGetKnownFolderPath (FOLDERID_Fonts, 0, hToken, &str);

  return std::wstring (str);
}

bool
SK_GetDocumentsDir (wchar_t* buf, uint32_t* pdwLen)
{
  HANDLE hToken;

  if (! OpenProcessToken (GetCurrentProcess (), TOKEN_READ, &hToken))
  {
    if (buf != nullptr && pdwLen != nullptr && *pdwLen > 0) {
      *pdwLen = 0;
      *buf    = L'\0';
    }

    return false;
  }

  wchar_t* str;

  if ( SUCCEEDED (
         SHGetKnownFolderPath (
           FOLDERID_Documents, 0, hToken, &str
         )
       )
     )
  {
    if (buf != nullptr && pdwLen != nullptr && *pdwLen > 0) {
      wcsncpy (buf, str, *pdwLen);
    }

    CoTaskMemFree (str);

    return true;
  }

  return false;
}

bool
SK_GetUserProfileDir (wchar_t* buf, uint32_t* pdwLen)
{
  HANDLE hToken;

  if (! OpenProcessToken (GetCurrentProcess (), TOKEN_READ, &hToken))
    return false;

  if (! GetUserProfileDirectory (hToken, buf, (DWORD *)pdwLen))
    return false;

  CloseHandle (hToken);
  return true;
}

bool
SK_CreateDirectories ( const wchar_t* wszPath )
{
  wchar_t* wszSubDir        = _wcsdup (wszPath), *iter;
  wchar_t* wszLastSlash     = wcsrchr (wszSubDir, L'/');
  wchar_t* wszLastBackslash = wcsrchr (wszSubDir, L'\\');

  if (wszLastSlash > wszLastBackslash)
    *wszLastSlash = L'\0';
  else if (wszLastBackslash != nullptr)
    *wszLastBackslash = L'\0';
  else {
    free (wszSubDir);
    return false;
  }

  for (iter = wszSubDir; *iter != L'\0'; iter = CharNextW (iter)) {
    if (*iter == L'\\' || *iter == L'/') {
      *iter = L'\0';

      if (GetFileAttributes (wszPath) == INVALID_FILE_ATTRIBUTES)
        CreateDirectoryW (wszSubDir, nullptr);

      *iter = L'\\';
    }

    // The final subdirectory (FULL PATH)
    if (GetFileAttributes (wszPath) == INVALID_FILE_ATTRIBUTES)
      CreateDirectoryW (wszSubDir, nullptr);
  }

  free (wszSubDir);

  return true;
}

std::wstring
SK_EvalEnvironmentVars (const wchar_t* wszEvaluateMe)
{
  wchar_t* wszEvaluated =
    (wchar_t *)malloc (sizeof (wchar_t) * (MAX_PATH + 2));

  ExpandEnvironmentStringsW ( wszEvaluateMe,
                                wszEvaluated,
                                  MAX_PATH + 2 );

  std::wstring ret_str (wszEvaluated);

  free (wszEvaluated);

  return ret_str;
}

#include <string>

bool
SK_IsTrue (const wchar_t* string)
{
  const wchar_t* pstr = string;

  if ( std::wstring (string).length () == 1 &&
                                 *pstr == L'1' )
    return true;

  if (std::wstring (string).length () != 4)
    return false;

  if (towlower (*pstr) != L't')
    return false;

  pstr = CharNextW (pstr);

  if (towlower (*pstr) != L'r')
    return false;

  pstr = CharNextW (pstr);

  if (towlower (*pstr) != L'u')
    return false;

  pstr = CharNextW (pstr);

  if (towlower (*pstr) != L'e')
    return false;

  return true;
}

// Copies a file preserving file times
void
SK_FullCopy (std::wstring from, std::wstring to)
{
  // Strip Read-Only
  SK_SetNormalFileAttribs (to);

  DeleteFile (to.c_str   ());
  CopyFile   (from.c_str (), to.c_str (), FALSE);

  WIN32_FIND_DATA FromFileData;
  HANDLE hFrom = FindFirstFile (from.c_str (), &FromFileData);

  HANDLE hTo =
    CreateFile ( to.c_str (),
                   GENERIC_READ      | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr,
                         OPEN_EXISTING,
                           GetFileAttributes (to.c_str ()),
                             nullptr );

  // Here's where the magic happens, apply the attributes from the
  //   original file to the new one!
  SetFileTime ( hTo,
                  &FromFileData.ftCreationTime,
                    &FromFileData.ftLastAccessTime,
                      &FromFileData.ftLastWriteTime );

  CloseHandle (hTo);

  FindClose   (hFrom);
}

//BOOL TakeOwnership (LPTSTR lpszOwnFile);

void
SK_SetNormalFileAttribs (std::wstring file)
{
  SetFileAttributes (file.c_str (), FILE_ATTRIBUTE_NORMAL);
}


bool
SK_IsAdmin (void)
{
  bool   bRet   = false;
  HANDLE hToken = 0;

  if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken)) {
    TOKEN_ELEVATION Elevation;
    DWORD cbSize = sizeof (TOKEN_ELEVATION);

    if (GetTokenInformation (hToken, TokenElevation, &Elevation, sizeof (Elevation), &cbSize)) {
      bRet = Elevation.TokenIsElevated != 0;
    }
  }

  if (hToken)
    CloseHandle (hToken);

  return bRet;
}

bool
SK_IsProcessRunning (const wchar_t* wszProcName)
{
  HANDLE         hProcSnap;
  PROCESSENTRY32 pe32;

  hProcSnap =
    CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

  if (hProcSnap == INVALID_HANDLE_VALUE)
    return false;

  pe32.dwSize = sizeof PROCESSENTRY32;

  if (! Process32First (hProcSnap, &pe32)) {
    CloseHandle (hProcSnap);
    return false;
  }

  do {
    if (! SK_Path_wcsicmp (wszProcName, pe32.szExeFile)) {
      CloseHandle (hProcSnap);
      return true;
    }
  } while (Process32Next (hProcSnap, &pe32));

  CloseHandle (hProcSnap);

  return false;
}






static uint32_t crc32_tab[] = {
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
   0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
   0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
   0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
   0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
   0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
   0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
   0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
   0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
   0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
   0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
   0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
   0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
   0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
   0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
   0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
   0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
   0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
   0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
   0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
   0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
   0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
   0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
   0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
   0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
   0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
   0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
   0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
   0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
   0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
   0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
   0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
   0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
 };

#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <intrin.h>

class InstructionSet
{
    // forward declarations
    class InstructionSet_Internal;

public:
    // getters
    static std::string Vendor(void) { return CPU_Rep.vendor_; }
    static std::string Brand(void) { return CPU_Rep.brand_; }

    static bool SSE3(void) { return CPU_Rep.f_1_ECX_[0]; }
    static bool PCLMULQDQ(void) { return CPU_Rep.f_1_ECX_[1]; }
    static bool MONITOR(void) { return CPU_Rep.f_1_ECX_[3]; }
    static bool SSSE3(void) { return CPU_Rep.f_1_ECX_[9]; }
    static bool FMA(void) { return CPU_Rep.f_1_ECX_[12]; }
    static bool CMPXCHG16B(void) { return CPU_Rep.f_1_ECX_[13]; }
    static bool SSE41(void) { return CPU_Rep.f_1_ECX_[19]; }
    static bool SSE42(void) { return CPU_Rep.f_1_ECX_[20]; }
    static bool MOVBE(void) { return CPU_Rep.f_1_ECX_[22]; }
    static bool POPCNT(void) { return CPU_Rep.f_1_ECX_[23]; }
    static bool AES(void) { return CPU_Rep.f_1_ECX_[25]; }
    static bool XSAVE(void) { return CPU_Rep.f_1_ECX_[26]; }
    static bool OSXSAVE(void) { return CPU_Rep.f_1_ECX_[27]; }
    static bool AVX(void) { return CPU_Rep.f_1_ECX_[28]; }
    static bool F16C(void) { return CPU_Rep.f_1_ECX_[29]; }
    static bool RDRAND(void) { return CPU_Rep.f_1_ECX_[30]; }

    static bool MSR(void) { return CPU_Rep.f_1_EDX_[5]; }
    static bool CX8(void) { return CPU_Rep.f_1_EDX_[8]; }
    static bool SEP(void) { return CPU_Rep.f_1_EDX_[11]; }
    static bool CMOV(void) { return CPU_Rep.f_1_EDX_[15]; }
    static bool CLFSH(void) { return CPU_Rep.f_1_EDX_[19]; }
    static bool MMX(void) { return CPU_Rep.f_1_EDX_[23]; }
    static bool FXSR(void) { return CPU_Rep.f_1_EDX_[24]; }
    static bool SSE(void) { return CPU_Rep.f_1_EDX_[25]; }
    static bool SSE2(void) { return CPU_Rep.f_1_EDX_[26]; }

    static bool FSGSBASE(void) { return CPU_Rep.f_7_EBX_[0]; }
    static bool BMI1(void) { return CPU_Rep.f_7_EBX_[3]; }
    static bool HLE(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[4]; }
    static bool AVX2(void) { return CPU_Rep.f_7_EBX_[5]; }
    static bool BMI2(void) { return CPU_Rep.f_7_EBX_[8]; }
    static bool ERMS(void) { return CPU_Rep.f_7_EBX_[9]; }
    static bool INVPCID(void) { return CPU_Rep.f_7_EBX_[10]; }
    static bool RTM(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[11]; }
    static bool AVX512F(void) { return CPU_Rep.f_7_EBX_[16]; }
    static bool RDSEED(void) { return CPU_Rep.f_7_EBX_[18]; }
    static bool ADX(void) { return CPU_Rep.f_7_EBX_[19]; }
    static bool AVX512PF(void) { return CPU_Rep.f_7_EBX_[26]; }
    static bool AVX512ER(void) { return CPU_Rep.f_7_EBX_[27]; }
    static bool AVX512CD(void) { return CPU_Rep.f_7_EBX_[28]; }
    static bool SHA(void) { return CPU_Rep.f_7_EBX_[29]; }

    static bool PREFETCHWT1(void) { return CPU_Rep.f_7_ECX_[0]; }

    static bool LAHF(void) { return CPU_Rep.f_81_ECX_[0]; }
    static bool LZCNT(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_ECX_[5]; }
    static bool ABM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[5]; }
    static bool SSE4a(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[6]; }
    static bool XOP(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[11]; }
    static bool TBM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[21]; }

    static bool SYSCALL(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[11]; }
    static bool MMXEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[22]; }
    static bool RDTSCP(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[27]; }
    static bool _3DNOWEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[30]; }
    static bool _3DNOW(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[31]; }

private:
    static const InstructionSet_Internal CPU_Rep;

    class InstructionSet_Internal
    {
    public:
        InstructionSet_Internal()
            : nIds_{ 0 },
            nExIds_{ 0 },
            isIntel_{ false },
            isAMD_{ false },
            f_1_ECX_{ 0 },
            f_1_EDX_{ 0 },
            f_7_EBX_{ 0 },
            f_7_ECX_{ 0 },
            f_81_ECX_{ 0 },
            f_81_EDX_{ 0 },
            data_{},
            extdata_{}
        {
            //int cpuInfo[4] = {-1};
            std::array<int, 4> cpui;

            // Calling __cpuid with 0x0 as the function_id argument
            // gets the number of the highest valid function ID.
            __cpuid(cpui.data(), 0);
            nIds_ = cpui[0];

            for (int i = 0; i <= nIds_; ++i)
            {
                __cpuidex(cpui.data(), i, 0);
                data_.push_back(cpui);
            }

            // Capture vendor string
            char vendor[0x20];
            memset(vendor, 0, sizeof(vendor));
            *reinterpret_cast<int*>(vendor) = data_[0][1];
            *reinterpret_cast<int*>(vendor + 4) = data_[0][3];
            *reinterpret_cast<int*>(vendor + 8) = data_[0][2];
            vendor_ = vendor;
            if (vendor_ == "GenuineIntel")
            {
                isIntel_ = true;
            }
            else if (vendor_ == "AuthenticAMD")
            {
                isAMD_ = true;
            }

            // load bitset with flags for function 0x00000001
            if (nIds_ >= 1)
            {
                f_1_ECX_ = data_[1][2];
                f_1_EDX_ = data_[1][3];
            }

            // load bitset with flags for function 0x00000007
            if (nIds_ >= 7)
            {
                f_7_EBX_ = data_[7][1];
                f_7_ECX_ = data_[7][2];
            }

            // Calling __cpuid with 0x80000000 as the function_id argument
            // gets the number of the highest valid extended ID.
            __cpuid(cpui.data(), 0x80000000);
            nExIds_ = cpui[0];

            char brand[0x40];
            memset(brand, 0, sizeof(brand));

            for (int i = 0x80000000; i <= nExIds_; ++i)
            {
                __cpuidex(cpui.data(), i, 0);
                extdata_.push_back(cpui);
            }

            // load bitset with flags for function 0x80000001
            if (nExIds_ >= 0x80000001)
            {
                f_81_ECX_ = extdata_[1][2];
                f_81_EDX_ = extdata_[1][3];
            }

            // Interpret CPU brand string if reported
            if (nExIds_ >= 0x80000004)
            {
                memcpy(brand, extdata_[2].data(), sizeof(cpui));
                memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
                memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
                brand_ = brand;
            }
        };

        int nIds_;
        int nExIds_;
        std::string vendor_;
        std::string brand_;
        bool isIntel_;
        bool isAMD_;
        std::bitset<32> f_1_ECX_;
        std::bitset<32> f_1_EDX_;
        std::bitset<32> f_7_EBX_;
        std::bitset<32> f_7_ECX_;
        std::bitset<32> f_81_ECX_;
        std::bitset<32> f_81_EDX_;
        std::vector<std::array<int, 4>> data_;
        std::vector<std::array<int, 4>> extdata_;
    };
};

// Initialize static member data
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

extern "C"
uint32_t
__cdecl
crc32 (uint32_t crc, const void *buf, size_t size)
{
  const uint8_t *p;

  p = (uint8_t *)buf;
  crc = crc ^ ~0U;

  while (size--)
    crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

  return crc ^ ~0U;
}






/*
  Copyright (c) 2013 - 2014, 2016 Mark Adler, Robert Vazan, Max Vysokikh

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define NOMINMAX

#include <algorithm>

#define POLY        0x82f63b78
#define LONG_SHIFT  8192
#define SHORT_SHIFT 256

typedef const uint8_t *buffer;

static uint32_t table        [16][256];
static uint32_t long_shifts  [ 4][256];
static uint32_t short_shifts [ 4][256];

static bool _tableInitialized = false;

extern "C" void __cdecl calculate_table (void);

/* Table-driven software version as a fall-back.  This is about 15 times slower
   than using the hardware instructions.  This assumes little-endian integers,
   as is the case on Intel processors that the assembler code here is for. */
extern "C"
uint32_t
__cdecl
crc32c_append_sw (uint32_t crci, buffer input, size_t length)
{
  buffer next = input;
#ifdef _M_X64
  uint64_t crc;
#else
  uint32_t crc;
#endif

  crc = crci ^ 0xffffffff;
#ifdef _M_X64
  while (length && ((uintptr_t)next & 7) != 0)
  {
    crc = table [0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    --length;
  }
  while (length >= 16)
  {
             crc ^= *(uint64_t *)next;
    uint64_t high = *(uint64_t *)(next + 8);

    crc = table[15][ crc         & 0xff]
        ^ table[14][(crc >> 8)   & 0xff]
        ^ table[13][(crc >> 16)  & 0xff]
        ^ table[12][(crc >> 24)  & 0xff]
        ^ table[11][(crc >> 32)  & 0xff]
        ^ table[10][(crc >> 40)  & 0xff]
        ^ table[ 9][(crc >> 48)  & 0xff]
        ^ table[ 8][ crc >> 56         ]
        ^ table[ 7][ high        & 0xff]
        ^ table[ 6][(high >> 8)  & 0xff]
        ^ table[ 5][(high >> 16) & 0xff]
        ^ table[ 4][(high >> 24) & 0xff]
        ^ table[ 3][(high >> 32) & 0xff]
        ^ table[ 2][(high >> 40) & 0xff]
        ^ table[ 1][(high >> 48) & 0xff]
        ^ table[ 0][ high >> 56        ];

    next   += 16;
    length -= 16;
  }
#else
  while (length && ((uintptr_t)next & 3) != 0)
  {
      crc = table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
      --length;
  }
  while (length >= 12)
  {
              crc ^= *(uint32_t *)next;
    uint32_t high  = *(uint32_t *)(next + 4);
    uint32_t high2 = *(uint32_t *)(next + 8);

    crc = table[11][ crc          & 0xff]
        ^ table[10][(crc >>  8)   & 0xff]
        ^ table[ 9][(crc >> 16)   & 0xff]
        ^ table[ 8][ crc >> 24          ]
        ^ table[ 7][ high         & 0xff]
        ^ table[ 6][(high >>  8)  & 0xff]
        ^ table[ 5][(high >> 16)  & 0xff]
        ^ table[ 4][ high >> 24         ]
        ^ table[ 3][ high2        & 0xff]
        ^ table[ 2][(high2 >>  8) & 0xff]
        ^ table[ 1][(high2 >> 16) & 0xff]
        ^ table[ 0][ high2 >> 24        ];

    next   += 12;
    length -= 12;
  }
#endif
  while (length)
  {
    crc = table [0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    --length;
  }
  return (uint32_t)crc ^ 0xffffffff;
}

/* Apply the zeros operator table to crc. */
static
inline
uint32_t
shift_crc (uint32_t shift_table[][256], uint32_t crc)
{
  return shift_table [0][ crc        & 0xff]
       ^ shift_table [1][(crc >> 8)  & 0xff]
       ^ shift_table [2][(crc >> 16) & 0xff]
       ^ shift_table [3][ crc >> 24];
}

/* Compute CRC-32C using the Intel hardware instruction. */
extern "C"
uint32_t
__cdecl
crc32c_append_hw (uint32_t crc, buffer buf, size_t len)
{
  buffer next = buf;
  buffer end;
#ifdef _M_X64
  uint64_t crc0, crc1, crc2;      /* need to be 64 bits for crc32q */
#else
  uint32_t crc0, crc1, crc2;
#endif

  /* pre-process the crc */
  crc0 = crc ^ 0xffffffff;

  /* compute the crc for up to seven leading bytes to bring the data pointer
     to an eight-byte boundary */
  while (len && ((uintptr_t)next & 7) != 0)
  {
    crc0 = _mm_crc32_u8 (static_cast <uint32_t> (crc0), *next);
    ++next;
    --len;
  }

#ifdef _M_X64
  /* compute the crc on sets of LONG_SHIFT*3 bytes, executing three independent crc
     instructions, each on LONG_SHIFT bytes -- this is optimized for the Nehalem,
     Westmere, Sandy Bridge, and Ivy Bridge architectures, which have a
     throughput of one crc per cycle, but a latency of three cycles */
  while (len >= 3 * LONG_SHIFT)
  {
    crc1 = 0;
    crc2 = 0;
    end  = next + LONG_SHIFT;

    do
    {
      crc0 = _mm_crc32_u64 (crc0, *reinterpret_cast <const uint64_t *>(next));
      crc1 = _mm_crc32_u64 (crc1, *reinterpret_cast <const uint64_t *>(next     + LONG_SHIFT));
      crc2 = _mm_crc32_u64 (crc2, *reinterpret_cast <const uint64_t *>(next + 2 * LONG_SHIFT));
      next += 8;
    } while (next < end);

    crc0 = shift_crc (long_shifts, static_cast <uint32_t> (crc0)) ^ crc1;
    crc0 = shift_crc (long_shifts, static_cast <uint32_t> (crc0)) ^ crc2;

    next += 2 * LONG_SHIFT;
    len  -= 3 * LONG_SHIFT;
  }

  /* do the same thing, but now on SHORT_SHIFT*3 blocks for the remaining data less
     than a LONG_SHIFT*3 block */
  while (len >= 3 * SHORT_SHIFT)
  {
    crc1 = 0;
    crc2 = 0;
    end  = next + SHORT_SHIFT;

    do
    {
      crc0 = _mm_crc32_u64 (crc0, *reinterpret_cast <const uint64_t *>(next));
      crc1 = _mm_crc32_u64 (crc1, *reinterpret_cast <const uint64_t *>(next     + SHORT_SHIFT));
      crc2 = _mm_crc32_u64 (crc2, *reinterpret_cast <const uint64_t *>(next + 2 * SHORT_SHIFT));
      next += 8;
    } while (next < end);

    crc0 = shift_crc (short_shifts, static_cast <uint32_t> (crc0)) ^ crc1;
    crc0 = shift_crc (short_shifts, static_cast <uint32_t> (crc0)) ^ crc2;

    next += 2 * SHORT_SHIFT;
    len  -= 3 * SHORT_SHIFT;
  }

  /* compute the crc on the remaining eight-byte units less than a SHORT_SHIFT*3
  block */
  end = next + (len - (len & 7));
  while (next < end)
  {
    crc0 = _mm_crc32_u64 (crc0, *reinterpret_cast <const uint64_t *> (next));
    next += 8;
  }
#else
  /* compute the crc on sets of LONG_SHIFT*3 bytes, executing three independent crc
  instructions, each on LONG_SHIFT bytes -- this is optimized for the Nehalem,
  Westmere, Sandy Bridge, and Ivy Bridge architectures, which have a
  throughput of one crc per cycle, but a latency of three cycles */
  while (len >= 3 * LONG_SHIFT)
  {
    crc1 = 0;
    crc2 = 0;
    end  = next + LONG_SHIFT;

    do
    {
      crc0 = _mm_crc32_u32 (crc0, *reinterpret_cast <const uint32_t *>(next));
      crc1 = _mm_crc32_u32 (crc1, *reinterpret_cast <const uint32_t *>(next     + LONG_SHIFT));
      crc2 = _mm_crc32_u32 (crc2, *reinterpret_cast <const uint32_t *>(next + 2 * LONG_SHIFT));
      next += 4;
    } while (next < end);

    crc0 = shift_crc (long_shifts, static_cast <uint32_t> (crc0)) ^ crc1;
    crc0 = shift_crc (long_shifts, static_cast <uint32_t> (crc0)) ^ crc2;

    next += 2 * LONG_SHIFT;
    len  -= 3 * LONG_SHIFT;
  }

  /* do the same thing, but now on SHORT_SHIFT*3 blocks for the remaining data less
  than a LONG_SHIFT*3 block */
  while (len >= 3 * SHORT_SHIFT)
  {
    crc1 = 0;
    crc2 = 0;
    end  = next + SHORT_SHIFT;

    do
    {
      crc0 = _mm_crc32_u32 (crc0, *reinterpret_cast <const uint32_t *>(next));
      crc1 = _mm_crc32_u32 (crc1, *reinterpret_cast <const uint32_t *>(next     + SHORT_SHIFT));
      crc2 = _mm_crc32_u32 (crc2, *reinterpret_cast <const uint32_t *>(next + 2 * SHORT_SHIFT));
      next += 4;
    } while (next < end);

    crc0 = shift_crc (short_shifts, static_cast <uint32_t> (crc0)) ^ crc1;
    crc0 = shift_crc (short_shifts, static_cast <uint32_t> (crc0)) ^ crc2;

    next += 2 * SHORT_SHIFT;
    len  -= 3 * SHORT_SHIFT;
  }

  /* compute the crc on the remaining eight-byte units less than a SHORT_SHIFT*3
  block */
  end = next + (len - (len & 7));
  while (next < end)
  {
    crc0 = _mm_crc32_u32(crc0, *reinterpret_cast<const uint32_t *>(next));
    next += 4;
  }
#endif
  len &= 7;

  /* compute the crc for up to seven trailing bytes */
  while (len)
  {
    crc0 = _mm_crc32_u8 (static_cast <uint32_t> (crc0), *next);
    ++next;
    --len;
  }

  /* return a post-processed crc */
  return static_cast <uint32_t> (crc0) ^ 0xffffffff;
}

extern "C"
int
__cdecl
crc32c_hw_available (void)
{
  int info [4];
  __cpuid (info, 1);
  return (info [2] & (1 << 20)) != 0;

}

extern "C"
void
__cdecl
calculate_table (void)
{
  for (int i = 0; i < 256; i++)
  {
    uint32_t res = (uint32_t)i;
    for (int t = 0; t < 16; t++) {
      for (int k = 0; k < 8; k++)
        res = (res & 1) == 1 ? POLY ^ (res >> 1) :
                                      (res >> 1);
      table[t][i] = res;
    }
  }

  _tableInitialized = true;
}

extern "C"
void
__cdecl
calculate_table_hw (void)
{
  for (int i = 0; i < 256; i++) 
  {
    uint32_t res = (uint32_t)i;
    for (int k = 0; k < 8 * (SHORT_SHIFT - 4); k++)
      res = (res & 1) == 1 ? POLY ^ (res >> 1) :
                                    (res >> 1);

    for (int t = 0; t < 4; t++) {
      for (int k = 0; k < 8; k++)
        res = (res & 1) == 1 ? POLY ^ (res >> 1) :
                                      (res >> 1);

      short_shifts [3 - t][i] = res;
    }

    for (int k = 0; k < 8 * (LONG_SHIFT - 4 - SHORT_SHIFT); k++)
      res = (res & 1) == 1 ? POLY ^ (res >> 1) :
                                    (res >> 1);

    for(int t = 0; t < 4; t++) {
      for (int k = 0; k < 8; k++)
        res = (res & 1) == 1 ? POLY ^ (res >> 1) :
                                      (res >> 1);
      long_shifts [3 - t][i] = res;
    }
  }
}

static uint32_t (*append_func)(uint32_t, buffer, size_t) = nullptr;

#include <SpecialK/log.h>

extern "C"
void
__cdecl
__crc32_init (void)
{
  static bool             init = false;
  static CRITICAL_SECTION cs_init;

  if (! init) {
    InitializeCriticalSection (&cs_init);
    init = true;
  }

  if (append_func == nullptr)
  {
    EnterCriticalSection (&cs_init);

    if (append_func != nullptr)
      return;

    // somebody can call sw version directly, so, precalculate table for this version
    calculate_table ();

    if (crc32c_hw_available ()) {
      //dll_log.Log (L"[ Checksum ] Using Hardware (SSE 4.2) CRC32C Algorithm");
      calculate_table_hw ();
      append_func = crc32c_append_hw;
    }

    else {
      //dll_log.Log (L"[ Checksum ] Using Software (Adler Optimized) CRC32C Algorithm");
      append_func = crc32c_append_sw;
    }

    LeaveCriticalSection (&cs_init);
  }
}

extern "C"
uint32_t
__cdecl
crc32c (uint32_t crc, buffer input, size_t length)
{
  __crc32_init ();

  return append_func (crc, input, length);
}

LPVOID
SK_GetProcAddress (const wchar_t* wszModule, const char* szFunc)
{
  return GetProcAddress (GetModuleHandle (wszModule), szFunc);
}

std::wstring
SK_GetModuleName (HMODULE hDll)
{
  wchar_t wszDllFullName [ MAX_PATH + 2 ] = { L'\0' };
          wszDllFullName [   MAX_PATH   ] =   L'\0';

  GetModuleFileName (hDll, wszDllFullName, MAX_PATH - 1);

  const wchar_t* wszShort = wcsrchr (wszDllFullName, L'\\');

  if (wszShort == nullptr)
    wszShort = wszDllFullName;
  else
    wszShort = CharNextW (wszShort);

  return wszShort;
}

std::wstring
SK_GetModuleFullName (HMODULE hDll)
{
  wchar_t wszDllFullName [ MAX_PATH + 2 ] = { L'\0' };
          wszDllFullName [   MAX_PATH   ] =   L'\0';

  GetModuleFileName (hDll, wszDllFullName, MAX_PATH - 1);

  return wszDllFullName;
}

PROCESSENTRY32
FindProcessByName (const wchar_t* wszName)
{
  HANDLE         hProcessSnap;
  PROCESSENTRY32 pe32 = { 0 };

  hProcessSnap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);

  if (hProcessSnap == INVALID_HANDLE_VALUE)
    return pe32;

  pe32.dwSize = sizeof (PROCESSENTRY32);

  if (! Process32First (hProcessSnap, &pe32))
  {
    CloseHandle (hProcessSnap);
    return pe32;
  }

  do
  {
    if (wcsstr (pe32.szExeFile, wszName))
      return pe32;
  } while (Process32Next (hProcessSnap, &pe32));

  CloseHandle (hProcessSnap);
  return pe32;
}

std::wstring
SK_GetRTSSInstallDir (void)
{
  PROCESSENTRY32 pe32 =
    FindProcessByName (L"RTSS.exe");

  wchar_t wszPath [MAX_PATH] = { '\0' };

  if (wcsstr (pe32.szExeFile, L"RTSS.exe")) {
    HANDLE hProcess =
      OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION , FALSE, pe32.th32ProcessID);

    DWORD len = MAX_PATH;
    QueryFullProcessImageName (hProcess, 0, wszPath, &len);

    wchar_t* wszRTSS =
      wcsstr (wszPath, L"RTSS.exe");

    if (wszRTSS != nullptr)
      *wszRTSS = L'\0';

    CloseHandle (hProcess);
  }

  return wszPath;
}

#include <SpecialK/ini.h>

iSK_INI*
SK_GetDLLConfig (void)
{
  extern iSK_INI* dll_ini;
  return dll_ini;
}


extern BOOL APIENTRY DllMain (HMODULE hModule,
                              DWORD   ul_reason_for_call,
                              LPVOID  /* lpReserved */);

#include <SpecialK/dxgi_backend.h>
#include <SpecialK/d3d9_backend.h>
#include <SpecialK/opengl_backend.h>

extern
const wchar_t*
__stdcall
SK_GetBackend (void);

extern volatile ULONG __SK_DLL_Ending;

void
__stdcall
SK_SelfDestruct (void)
{
  if (InterlockedIncrement (&__SK_DLL_Ending) == 1)
  {
    //FreeLibrary (SK_GetDLL ());
    const wchar_t* wszBackend = SK_GetBackend ();

    if (! _wcsicmp (wszBackend, L"d3d9"))
      SK::D3D9::Shutdown ();
    else if (! _wcsicmp (wszBackend, L"dxgi"))
      SK::DXGI::Shutdown ();
#ifndef SK_BUILD__INSTALLER
    else if (! _wcsicmp (wszBackend, L"OpenGL32"))
      SK::OpenGL::Shutdown ();
#endif
  }
}

HMODULE
SK_GetCallingDLL (LPVOID pReturn)
{
  HMODULE hCallingMod;
  GetModuleHandleEx ( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                        (LPCWSTR)pReturn,
                          &hCallingMod );

  return hCallingMod;
}

std::wstring
SK_GetCallerName (LPVOID pReturn)
{
  return SK_GetModuleName (SK_GetCallingDLL (pReturn));
}

extern
std::wstring
__stdcall
SK_GetPluginName (void);

void
SK_KillFRAPS (void)
{
#ifdef _WIN64
  if (GetModuleHandle (L"fraps64.dll")) {
#else
  if (GetModuleHandle (L"fraps32.dll")) {
#endif
    std::wstring warning;

    warning += L"FRAPS is running, it must be disabled for ";
    warning += SK_GetPluginName ();
    warning += L" to work!";

    MessageBox ( NULL,
                   warning.c_str (),
                     L"Please Close FRAPS",
                       MB_OK          | MB_ICONSTOP |
                       MB_SYSTEMMODAL | MB_SETFOREGROUND );

    ExitProcess (0x00);
  }
}

#include <queue>

std::queue <DWORD>
SK_SuspendAllOtherThreads (void)
{
  std::queue <DWORD> threads;

  HANDLE hSnap =
    CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);

  if (hSnap != INVALID_HANDLE_VALUE)
  {
    THREADENTRY32 tent;
    tent.dwSize = sizeof THREADENTRY32;

    if (Thread32First (hSnap, &tent))
    {
      bool locked = 
        dll_log.lock ();

      do
      {
        if ( tent.dwSize >= FIELD_OFFSET (THREADENTRY32, th32OwnerProcessID) +
                                  sizeof (tent.th32OwnerProcessID) )
        {
          if ( tent.th32ThreadID       != GetCurrentThreadId  () &&
               tent.th32OwnerProcessID == GetCurrentProcessId () )
          {
            HANDLE hThread =
              OpenThread (THREAD_ALL_ACCESS, FALSE, tent.th32ThreadID);

            if (hThread != NULL) {
              threads.push  (tent.th32ThreadID);

              SuspendThread (hThread);
              CloseHandle   (hThread);
            }
          }
        }

        tent.dwSize = sizeof (tent);
      } while (Thread32Next (hSnap, &tent));

      if (locked)
        dll_log.unlock ();
    }

    CloseHandle (hSnap);
  }

  return threads;
}

void
SK_ResumeThreads (std::queue <DWORD> threads)
{
  while (! threads.empty ())
  {
    DWORD tid = threads.front ();

    HANDLE hThread =
      OpenThread (THREAD_ALL_ACCESS, FALSE, tid);

    if (hThread != NULL)
    {
      ResumeThread (hThread);
      CloseHandle (hThread);
    }

    threads.pop ();
  }
}

void
__stdcall
SK_TestImports (          HMODULE  hMod,
                 sk_import_test_s *pTests,
                              int  nCount )
{
  DBG_UNREFERENCED_PARAMETER (hMod);

  int hits = 0;

  __try
  {
    uintptr_t                pImgBase =
      (uintptr_t)GetModuleHandle (nullptr);

             MEMORY_BASIC_INFORMATION minfo;
    VirtualQuery ((LPCVOID)pImgBase, &minfo, sizeof minfo);

    pImgBase =
      (uintptr_t)minfo.BaseAddress;

    PIMAGE_NT_HEADERS        pNtHdr   =
      PIMAGE_NT_HEADERS ( pImgBase + PIMAGE_DOS_HEADER (pImgBase)->e_lfanew );

    PIMAGE_DATA_DIRECTORY    pImgDir  =
        &pNtHdr->OptionalHeader.DataDirectory [IMAGE_DIRECTORY_ENTRY_IMPORT];

    PIMAGE_IMPORT_DESCRIPTOR pImpDesc =
      PIMAGE_IMPORT_DESCRIPTOR (
        pImgBase + pImgDir->VirtualAddress
      );

    //dll_log.Log (L"[Import Tbl] Size=%lu", pImgDir->Size);

    if (pImgDir->Size < (1024 * 8192))
    {
      uintptr_t end = (uintptr_t)pImpDesc + pImgDir->Size;

      while ((uintptr_t)pImpDesc < end)
      {
        if (pImpDesc->Name == 0x00)
        {
          ++pImpDesc;
          continue;
        }

        const char* szImport =
          (const char *)(pImgBase + (pImpDesc++)->Name);

        //dll_log.Log (L"%hs", szImport);

        for (int i = 0; i < nCount; i++)
        {
          if ((! pTests [i].used) && StrStrIA (szImport, pTests [i].szModuleName))
          {
            pTests [i].used = true;
                     ++hits;
          }
        }

        //if (hits == nCount)
          //break;
      }
    }
  }

  __except ( ( GetExceptionCode () == EXCEPTION_ACCESS_VIOLATION ) ? 
               EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH ) {
    dll_log.Log ( L"[Import Tbl] Access Violation Attempting to "
                  L"Walk Import Table." );
  }

  if (hits == 0)
  {
    // Resort to checking for DLL residency instead
    for (int i = 0; i < nCount; i++)
    {
      HMODULE hMod;
      if ( GetModuleHandleExA ( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  pTests [i].szModuleName,
                                    &hMod ) )
        pTests [i].used = true;
    }
  }
}

void
SK_TestRenderImports ( HMODULE hMod,
                       bool*   gl,
                       bool*   vulkan,
                       bool*   d3d9,
                       bool*   dxgi,
                       bool*   d3d11 )
{
  static sk_import_test_s tests [] = { { "OpenGL32.dll", false },
                                       { "vulkan-1.dll", false },
                                       { "d3d9.dll",     false },
                                       //{ "dxgi.dll",     false },
                                       { "d3d11.dll",    false } };

  SK_TestImports (hMod, tests, sizeof (tests) / sizeof sk_import_test_s);

  *gl     = tests [0].used;
  *vulkan = tests [1].used;
  *d3d9   = tests [2].used;
//*dxgi   = tests [3].used;
  *d3d11  = tests [4].used;
}

int
SK_Path_wcsicmp (const wchar_t* wszStr1, const wchar_t* wszStr2)
{
  int ret =
    CompareString ( LOCALE_INVARIANT,
                      NORM_IGNORECASE | NORM_IGNOREWIDTH |
                      NORM_IGNORENONSPACE,
                        wszStr1, lstrlenW (wszStr1),
                          wszStr2, lstrlenW (wszStr2) );

  // To make this a drop-in replacement for wcsicmp, subtract
  //   2 from non-zero return values
  return (ret != 0) ?
    (ret - 2) : 0;
}

const wchar_t*
SK_Path_wcsrchr (const wchar_t* wszStr, wchar_t wchr)
{
             int len     = 0;
  const wchar_t* pwszStr = wszStr;

  for (len = 0; len < MAX_PATH; ++len, pwszStr = CharNextW (pwszStr))
  {
    if (*pwszStr == L'\0')
      break;
  }

  const wchar_t* wszSearch = pwszStr;

  while (wszSearch >= wszStr)
  {
    if (*wszSearch == wchr)
      break;

    wszSearch = CharPrevW (wszStr, wszSearch);
  }

  return (wszSearch < wszStr) ?
           nullptr : wszSearch;
}

const wchar_t*
SK_Path_wcsstr (const wchar_t* wszStr, const wchar_t* wszSubStr)
{
#if 0
  int            len     =
    min (lstrlenW (wszSubStr), MAX_PATH);

  const wchar_t* it       = wszStr;
  const wchar_t* wszScan  = it;
  const wchar_t* wszBegin = it;

  int            idx     = 0;

  while (it < (wszStr + MAX_PATH)) {
    bool match = (*wszScan == wszSubStr [idx]);

    if (match) {
      if (++idx == len)
        return wszBegin;

      ++it;
    }

    else {
      if (it > (wszStr + MAX_PATH - len))
        break;

      it  = ++wszBegin;
      idx = 0;
    }
  }

  return (it <= (wszStr + MAX_PATH - len)) ?
           wszBegin : nullptr;
#else
  return StrStrIW (wszStr, wszSubStr);
#endif
}


#include <Winver.h>
//#pragma comment (lib, "Mincore_Downlevel.lib") // Windows 8     (Delay-Load)
#pragma comment (lib, "version.lib")             // Windows 2000+ (Normal Import)

bool
__stdcall
SK_IsDLLSpecialK (const wchar_t* wszName)
{
  UINT     cbTranslatedBytes = 0,
           cbProductBytes    = 0;

  uint8_t  cbData      [4096] = {     0 };
  wchar_t  wszPropName [64]   = { L'\0' };

  wchar_t* wszProduct = nullptr; // Will point somewhere in cbData

  struct LANGANDCODEPAGE {
    WORD wLanguage;
    WORD wCodePage;
  } *lpTranslate = nullptr;

  if (GetFileAttributes (wszName) == INVALID_FILE_ATTRIBUTES)
    return false;

  GetFileVersionInfoEx ( FILE_VER_GET_NEUTRAL | FILE_VER_GET_PREFETCHED,
                           wszName,
                             0x00,
                               4096,
                                 cbData );

  if (VerQueryValue ( cbData,
                      TEXT ("\\VarFileInfo\\Translation"),
                        (LPVOID *)&lpTranslate,
                                  &cbTranslatedBytes ) && cbTranslatedBytes)
  {
    wsprintfW ( wszPropName,
                  L"\\StringFileInfo\\%04x%04x\\ProductName",
                    lpTranslate [0].wLanguage,
                      lpTranslate [0].wCodePage );

    VerQueryValue ( cbData,
                      wszPropName,
                        (LPVOID *)&wszProduct,
                                  &cbProductBytes );

    return (cbProductBytes && (! SK_Path_wcsicmp (wszProduct, L"Special K")));
  }

  return false;
}

std::wstring
__stdcall
SK_GetDLLVersionStr (const wchar_t* wszName)
{
  UINT     cbTranslatedBytes = 0,
           cbProductBytes    = 0,
           cbVersionBytes    = 0;

  uint8_t  cbData      [4096] = {     0 };
  wchar_t  wszPropName [64]   = { L'\0' };

  wchar_t* wszFileDescrip = nullptr; // Will point somewhere in cbData
  wchar_t* wszFileVersion = nullptr; // "

  struct LANGANDCODEPAGE {
    WORD wLanguage;
    WORD wCodePage;
  } *lpTranslate = nullptr;

  if (GetFileAttributes (wszName) == INVALID_FILE_ATTRIBUTES)
    return L"N/A";

  GetFileVersionInfoEx ( FILE_VER_GET_NEUTRAL | FILE_VER_GET_PREFETCHED,
                           wszName,
                             0x00,
                               4096,
                                 cbData );

  if ( VerQueryValue ( cbData,
                         TEXT ("\\VarFileInfo\\Translation"),
                           (LPVOID *)&lpTranslate,
                                     &cbTranslatedBytes ) && cbTranslatedBytes ) {

    wsprintfW ( wszPropName,
                  L"\\StringFileInfo\\%04x%04x\\FileDescription",
                    lpTranslate [0].wLanguage,
                      lpTranslate [0].wCodePage );

    VerQueryValue ( cbData,
                      wszPropName,
                        (LPVOID *)&wszFileDescrip,
                                  &cbProductBytes );

    wsprintfW ( wszPropName,
                  L"\\StringFileInfo\\%04x%04x\\FileVersion",
                    lpTranslate [0].wLanguage,
                      lpTranslate [0].wCodePage );

    VerQueryValue ( cbData,
                      wszPropName,
                        (LPVOID *)&wszFileVersion,
                                  &cbVersionBytes );
  }

  if (cbTranslatedBytes == 0 || (cbProductBytes == 0 && cbVersionBytes == 0))
    return L"  ";

  std::wstring ret = L"";

  if (cbProductBytes)
    ret += wszFileDescrip;

  ret += L"  ";

  if (cbVersionBytes)
    ret += wszFileVersion;

  return ret;
}



LPVOID __SK_base_img_addr = nullptr;
LPVOID __SK_end_img_addr  = nullptr;

void*
__stdcall
SK_Scan (const uint8_t* pattern, size_t len, const uint8_t* mask)
{
  uint8_t* base_addr = (uint8_t *)GetModuleHandle (nullptr);

   MEMORY_BASIC_INFORMATION minfo;
  VirtualQuery (base_addr, &minfo, sizeof minfo);

           base_addr = (uint8_t *)minfo.BaseAddress;
  uint8_t* end_addr  = (uint8_t *)minfo.BaseAddress + minfo.RegionSize;

#if 0
  if (base_addr != (uint8_t *)0x400000) {
    dll_log->Log ( L"[ Sig Scan ] Expected module base addr. 40000h, but got: %ph",
                     base_addr );
  }
#endif

  size_t pages = 0;

// Scan up to 32 MiB worth of data
#define PAGE_WALK_LIMIT (base_addr) + (1 << 26)

  //
  // For practical purposes, let's just assume that all valid games have less than 32 MiB of
  //   committed executable image data.
  //
  while (VirtualQuery (end_addr, &minfo, sizeof minfo) && end_addr < PAGE_WALK_LIMIT)
  {
    if (minfo.Protect & PAGE_NOACCESS || (! (minfo.Type & MEM_IMAGE)))
      break;

    pages += VirtualQuery (end_addr, &minfo, sizeof minfo);

    end_addr = (uint8_t *)minfo.BaseAddress + minfo.RegionSize;
  } 

  if (end_addr > PAGE_WALK_LIMIT) {
#if 0
    dll_log->Log ( L"[ Sig Scan ] Module page walk resulted in end addr. out-of-range: %ph",
                     end_addr );
    dll_log->Log ( L"[ Sig Scan ]  >> Restricting to %ph",
                     PAGE_WALK_LIMIT );
#endif
    end_addr = PAGE_WALK_LIMIT;
  }

#if 0
  dll_log->Log ( L"[ Sig Scan ] Module image consists of %lu pages, from %ph to %ph",
                   pages,
                     base_addr,
                       end_addr );
#endif

  __SK_base_img_addr = base_addr;
  __SK_end_img_addr  = end_addr;

  uint8_t*  begin = (uint8_t *)base_addr;
  uint8_t*  it    = begin;
  int       idx   = 0;

  while (it < end_addr)
  {
    VirtualQuery (it, &minfo, sizeof minfo);

    // Bail-out once we walk into an address range that is not resident, because
    //   it does not belong to the original executable.
    if (minfo.RegionSize == 0)
      break;

    uint8_t* next_rgn =
     (uint8_t *)minfo.BaseAddress + minfo.RegionSize;

    if ( (! (minfo.Type    & MEM_IMAGE))  ||
         (! (minfo.State   & MEM_COMMIT)) ||
             minfo.Protect & PAGE_NOACCESS ) {
      it    = next_rgn;
      idx   = 0;
      begin = it;
      continue;
    }

    // Do not search past the end of the module image!
    if (next_rgn >= end_addr)
      break;

    while (it < next_rgn) {
      uint8_t* scan_addr = it;

      bool match = (*scan_addr == pattern [idx]);

      // For portions we do not care about... treat them
      //   as matching.
      if (mask != nullptr && (! mask [idx]))
        match = true;

      if (match) {
        if (++idx == len)
          return (void *)begin;

        ++it;
      }

      else {
        // No match?!
        if (it > end_addr - len)
          break;

        it  = ++begin;
        idx = 0;
      }
    }
  }

  return nullptr;
}

uint64_t
__stdcall
SK_GetFileSize (const wchar_t* wszFile)
{
  WIN32_FILE_ATTRIBUTE_DATA
    file_attrib_data = { 0 };

  if ( GetFileAttributesEx ( wszFile,
                               GetFileExInfoStandard,
                                 &file_attrib_data ) )
  {
    return ULARGE_INTEGER { file_attrib_data.nFileSizeLow,
                            file_attrib_data.nFileSizeHigh }.QuadPart;
  }

  return 0ULL;
}

typedef void (__stdcall *SK_HashProgressCallback_pfn)(uint64_t current, uint64_t total);

enum sk_hash_algo {
  SK_NO_HASH    = 0x0,

  // CRC32 (XOR operands swapped; exaggerates small differences in data)
  SK_CRC32_KAL  = 0x1,

  //
  // CRC32C ( Hardware Algorithm:  SSE4.2                  )
  //        ( Software Algorithm:  Optimized by Mark Adler )
  //
  SK_CRC32C     = 0x2,

  SK_NUM_HASHES = 2
};

uint32_t
__stdcall
SK_GetFileHash_32 (sk_hash_algo algorithm, const wchar_t* wszFile, SK_HashProgressCallback_pfn callback = nullptr)
{
  uint32_t _hash32 = 0UL;

  uint64_t size =
    SK_GetFileSize (wszFile);

  // Invalid file
  if (size == 0)
    return _hash32;

  switch (algorithm)
  {
    // Traditional CRC32 (but XOR operands swapped)
    case SK_CRC32_KAL:
    case SK_CRC32C:
    {
      DWORD dwFileAttribs =
        GetFileAttributes (wszFile);

      HANDLE hFile =
        CreateFile ( wszFile,
                       GENERIC_READ,
                         FILE_SHARE_READ |
                         FILE_SHARE_WRITE,
                           nullptr,
                             OPEN_EXISTING,
                               dwFileAttribs,
                                 nullptr );

      if (hFile == INVALID_HANDLE_VALUE)
        return _hash32;

      // Read up to 16 MiB at a time.
      const uint32_t MAX_CHUNK =
        (1024UL * 1024UL * 16UL);

      const uint32_t read_size =
        size <= std::numeric_limits <uint32_t>::max () ?
          std::min (MAX_CHUNK, (uint32_t)size) :
                    MAX_CHUNK;

      uint8_t* buf = (uint8_t *)malloc (read_size);

      DWORD    dwReadChunk = 0UL;
      uint64_t qwReadTotal = 0ULL;

      do
      {
        ReadFile ( hFile,
                     buf,
                       read_size,
                         &dwReadChunk,
                           nullptr );

        switch (algorithm)
        {
          case SK_CRC32_KAL:
            _hash32 = crc32 ( _hash32, buf, dwReadChunk );
            break;

          case SK_CRC32C:
            _hash32 = crc32c ( _hash32, buf, dwReadChunk );
            break;
        }
        qwReadTotal += dwReadChunk;

        if (callback != nullptr)
          callback (qwReadTotal, size);
      } while ( qwReadTotal < size && dwReadChunk > 0 );

      CloseHandle (hFile);

      return _hash32;
    }
    break;

    // Invalid Algorithm
    default:
      return _hash32;
  }
}

uint32_t
__stdcall
SK_GetFileCRC32 (const wchar_t* wszFile, SK_HashProgressCallback_pfn callback)
{
  return SK_GetFileHash_32 (SK_CRC32_KAL, wszFile, callback);
}

uint32_t
__stdcall
SK_GetFileCRC32C (const wchar_t* wszFile, SK_HashProgressCallback_pfn callback)
{
  __crc32_init ();

  return SK_GetFileHash_32 (SK_CRC32C, wszFile, callback);
}

#if 0
void
__stdcall
SK_wcsrep ( const wchar_t*   wszIn,
                  wchar_t** pwszOut,
            const wchar_t*   wszOld,
            const wchar_t*   wszNew )
{
        wchar_t* wszTemp;
  const wchar_t* wszFound = wcsstr (wszIn, wszOld);

  if (! wszFound) {
    *pwszOut =
      (wchar_t *)malloc (wcslen (wszIn) + 1);

    wcscpy (*pwszOut, wszIn);
    return;
  }

  int idx = wszFound - wszIn;

  *pwszOut =
    (wchar_t *)realloc (  *pwszOut,
                            wcslen (wszIn)  - wcslen (wszOld) +
                            wcslen (wszNew) + 1 );

  wcsncpy ( *pwszOut, wszIn, idx    );
  wcscpy  ( *pwszOut + idx,  wszNew );
  wcscpy  ( *pwszOut + idx + wcslen (wszNew),
               wszIn + idx + wcslen (wszOld) );

  wszTemp =
    (wchar_t *)malloc (idx + wcslen (wszNew) + 1);

  wcsncpy (wszTemp, *pwszOut, idx + wcslen (wszNew));
  wszTemp [idx + wcslen (wszNew)] = '\0';

  SK_wcsrep ( wszFound + wcslen (wszOld),
                pwszOut,
                  wszOld,
                    wszNew );

  wszTemp =
    (wchar_t *)realloc ( wszTemp,
                           wcslen ( wszTemp) +
                           wcslen (*pwszOut) + 1 );

  lstrcatW (wszTemp, *pwszOut);

  free (*pwszOut);

  *pwszOut = wszTemp;
}
#endif

size_t
SK_RemoveTrailingDecimalZeros (wchar_t* wszNum, size_t bufLen)
{
  // Remove trailing 0's after the .
  size_t len = bufLen == 0 ?
                  wcslen (wszNum) :
        std::min (wcslen (wszNum), bufLen);

  for (size_t i = (len - 1); i > 1; i--) {
    if (wszNum [i] == L'0' && wszNum [i - 1] != L'.')
      len--;
    if (wszNum [i] != L'0' && wszNum [i] != L'\0')
      break;
  }

  wszNum [len] = L'\0';

  return len;
}

size_t
SK_RemoveTrailingDecimalZeros (char* szNum, size_t bufLen)
{
  // Remove trailing 0's after the .
  size_t len = bufLen == 0 ?
                  strlen (szNum) :
        std::min (strlen (szNum), bufLen);

  for (size_t i = (len - 1); i > 1; i--) {
    if (szNum [i] == '0' && szNum [i - 1] != '.')
      len--;
    if (szNum [i] != '0' && szNum [i] != '\0')
      break;
  }

  szNum [len] = '\0';

  return len;
}


void
SK_ElevateToAdmin (void)
{
  extern const wchar_t*
  SK_GetFullyQualifiedApp (void);

  extern const wchar_t*
  SK_GetHostPath (void);

  extern std::wstring
  SK_GetModuleFullName (HMODULE hDll);

  wchar_t wszRunDLLCmd [MAX_PATH * 4] = { L'\0' };
  wchar_t wszShortPath [MAX_PATH + 2] = { L'\0' };
  wchar_t wszFullname  [MAX_PATH + 2] = { L'\0' };

  wcsncpy (wszFullname, SK_GetModuleFullName (SK_GetDLL ()).c_str (), MAX_PATH - 1);
 
  if ((! GetShortPathName   (wszFullname, wszShortPath, MAX_PATH - 1)) ||
         GetFileAttributesW (wszShortPath) == INVALID_FILE_ATTRIBUTES  ||
         StrStrIW           (wszShortPath, L" "))
  {
    SK_MessageBox ( L"Your computer is broken; please enable DOS 8.3 filename generation.",
                      L"Cannot Elevate To Admin Because of Bad Filesystem Policy.",
                        MB_OK | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_ICONASTERISK | MB_TOPMOST );
    ExitProcess   (0x00);
  }

  _swprintf ( wszRunDLLCmd,
               L"RunDll32.exe %s,RunDLL_ElevateMe %s",
                 wszShortPath,
                   SK_GetFullyQualifiedApp () );

  STARTUPINFOW        sinfo = { 0 };
  PROCESS_INFORMATION pinfo = { 0 };

  sinfo.cb = sizeof STARTUPINFOW;

  CreateProcess ( nullptr, wszRunDLLCmd,             nullptr, nullptr,
                  FALSE,   CREATE_NEW_PROCESS_GROUP, nullptr, SK_GetHostPath (),
                  &sinfo,  &pinfo );

  TerminateProcess    (GetCurrentProcess (), 0x00);
}