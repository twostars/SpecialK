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
#ifndef __SK__STEAM_API_H__
#define __SK__STEAM_API_H__

#define _CRT_SECURE_NO_WARNINGS
#include <steamapi/steam_api.h>

#include <stdint.h>
#include <string>
#include <SpecialK/utility.h>

namespace SK
{
  namespace SteamAPI
  {
    void Init     (bool preload);
    void Shutdown (void);
    void Pump     (void);

    void __stdcall SetOverlayState (bool active);
    bool __stdcall GetOverlayState (bool real);

    void __stdcall UpdateNumPlayers (void);
    int  __stdcall GetNumPlayers    (void);

    float __stdcall PercentOfAchievementsUnlocked (void);

    bool __stdcall TakeScreenshot  (void);

    uint32_t    AppID   (void);
    std::string AppName (void);

    // The state that we are explicitly telling the game
    //   about, not the state of the actual overlay...
    extern bool overlay_state;
  }
}

//
// Internal data stored in the Achievement Manager, this is
//   the publicly visible data...
//
//   I do not want to expose the poorly designed interface
//     of the full achievement manager outside the DLL,
//       so there exist a few flattened API functions
//         that can communicate with it and these are
//           the data they provide.
//
struct SK_SteamAchievement
{
  // If we were to call ISteamStats::GetAchievementName (...),
  //   this is the index we could use.
  int         idx_;

  const char* name_;          // UTF-8 (I think?)
  const char* human_name_;    // UTF-8
  const char* desc_;          // UTF-8
  
  float       global_percent_;
  
  struct
  {
    int unlocked; // Number of friends who have unlocked
    int possible; // Number of friends who may be able to unlock
  } friends_;
  
  struct
  {
    uint8_t*  achieved;
    uint8_t*  unachieved;
  } icons_;
  
  struct
  {
    int current;
    int max;
  
    __forceinline float getPercent (void)
    {
      return 100.0f * (float)current / (float)max;
    }
  } progress_;
  
  bool        unlocked_;
  __time32_t  time_;
};


size_t SK_SteamAPI_GetNumPossibleAchievements (void);

std::vector <SK_SteamAchievement *>& SK_SteamAPI_GetUnlockedAchievements (void);
std::vector <SK_SteamAchievement *>& SK_SteamAPI_GetLockedAchievements   (void);
std::vector <SK_SteamAchievement *>& SK_SteamAPI_GetAllAchievements      (void);

float  SK_SteamAPI_GetUnlockedPercentForFriend      (uint32_t friend_idx);
size_t SK_SteamAPI_GetUnlockedAchievementsForFriend (uint32_t friend_idx, BOOL* pStats);
size_t SK_SteamAPI_GetLockedAchievementsForFriend   (uint32_t friend_idx, BOOL* pStats);
size_t SK_SteamAPI_GetSharedAchievementsForFriend   (uint32_t friend_idx, BOOL* pStats);


// Returns true if all friend stats have been pulled from the server
      bool  __stdcall SK_SteamAPI_FriendStatsFinished  (void);

// Percent (0.0 - 1.0) of friend achievement info fetched
     float  __stdcall SK_SteamAPI_FriendStatPercentage (void);

       int  __stdcall SK_SteamAPI_GetNumFriends        (void);
const char* __stdcall SK_SteamAPI_GetFriendName        (uint32_t friend_idx, size_t* pLen = nullptr);


bool __stdcall SK_SteamAPI_TakeScreenshot           (void);
bool __stdcall SK_IsSteamOverlayActive              (void);

void    __stdcall SK_SteamAPI_UpdateNumPlayers      (void);
int32_t __stdcall SK_SteamAPI_GetNumPlayers         (void);

float __stdcall SK_SteamAPI_PercentOfAchievementsUnlocked (void);
                                                    
void           SK_SteamAPI_LogAllAchievements       (void);
void           SK_UnlockSteamAchievement            (uint32_t idx);
                                                    
bool           SK_SteamImported                     (void);
void           SK_TestSteamImports                  (HMODULE hMod);
                                                    
void           SK_HookCSteamworks                   (void);
void           SK_HookSteamAPI                      (void);
                                                    
void           SK_Steam_ClearPopups                 (void);
void           SK_Steam_DrawOSD                     (void);

bool           SK_Steam_LoadOverlayEarly            (void);

void           SK_Steam_InitCommandConsoleVariables (void);

ISteamUtils*   SK_SteamAPI_Utils                    (void);

uint32_t __stdcall SK_Steam_PiratesAhoy                 (void);


#include <stack>

// Barely functional Steam Key/Value Parser
//   -> Does not handle unquoted kv pairs.
class SK_Steam_KeyValues
{
public:
  static
  std::vector <std::string>
  getKeys ( const std::string                &input,
            const std::deque  <std::string>  &sections,
                  std::vector <std::string>* values = nullptr )
  {
    std::vector <std::string> ret;

    if (sections.empty () || input.empty ())
      return ret;

    struct {
      std::deque <std::string> path;

      struct {
        std::string actual;
        std::string test;
      } heap;

      void heapify (std::deque <std::string> const* sections = nullptr)
      {
        int i = 0;

        auto& in  = (sections == nullptr) ? path        : *sections;
        auto& out = (sections == nullptr) ? heap.actual : heap.test;

        out = "";

        for ( auto& str : in )
        {
          if (i++ > 0)
            out += "\x01";

            out += str;
        }
      }
    } search_tree;

    search_tree.heapify (&sections);

    std::string name   = "";
    std::string value  = "";
    int         quotes = 0;

    const auto clear =
   [&](void) noexcept
    {
      name.clear  ();
      value.clear ();
      quotes = 0;
    };

    for (auto c : input)
    {
      if (c == '"')
        ++quotes;

      else if (c != '{')
      {
        if (quotes == 1)
        {
          name += c;
        }

        if (quotes == 3)
        {
          value += c;
        }
      }

      if (quotes == 4)
      {
        if (0 == _stricmp ( search_tree.heap.test.c_str   (),
                            search_tree.heap.actual.c_str () ) )
        {
          ret.emplace_back (name);

          if (values != nullptr)
            values->emplace_back (value);
        }

        clear ();
      }

      if (c == '{')
      {
        search_tree.path.push_back (name);
        search_tree.heapify        (    );

        clear ();
      }

      else if (c == '}')
      {
        search_tree.path.pop_back ();

        clear ();
      }
    }

    return ret;
  }

  static
  std::string
  getValue ( const std::string              &input,
             const std::deque <std::string> &sections,
             const std::string              &key )
  {
    std::vector <std::string> values;
    std::vector <std::string> keys (
      SK_Steam_KeyValues::getKeys (input, sections, &values)
    );

    size_t idx = 0;
    for ( auto it : keys )
    {
      if (it._Equal (key))
        return values [idx];

      ++idx;
    }

    return "";
  }

  static
  std::wstring
  getValueAsUTF16 ( const std::string              &input,
                    const std::deque <std::string> &sections,
                    const std::string              &key )
  {
    std::vector <std::string> values;
    std::vector <std::string> keys (
      SK_Steam_KeyValues::getKeys (input, sections, &values)
    );

    int idx = 0;

    for ( auto it : keys )
    {
      if (it._Equal (key))
      {
        return
          SK_UTF8ToWideChar (
            values [idx]
          );
      }

      ++idx;
    }

    return L"";
  }
};

#endif /* __SK__STEAM_API_H__ */