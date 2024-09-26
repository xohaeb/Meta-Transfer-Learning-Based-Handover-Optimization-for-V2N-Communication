/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Technische Universität Berlin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Piotr Gawlowicz <gawlowicz@tkn.tu-berlin.de>
 */

#include "ho-rl-env.h"
#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/node-list.h"
#include "ns3/log.h"
#include <sstream>
#include <iostream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MyRlHoEnv");

// NS_OBJECT_ENSURE_REGISTERED (MyRLHoEnv);

MyRLHoEnv::MyRLHoEnv(uint16_t id) : Ns3AIRL<HoEnv, HoAct, HoInfo>(id)
{
  SetCond(2, 0);
}

MyRLHoEnv::~MyRLHoEnv()
{
  NS_LOG_FUNCTION(this);
}


uint8_t MyRLHoEnv::GetReward(uint16_t targetCellID, std::vector<uint8_t> rsrpObs)
{
  NS_LOG_FUNCTION(this);

  return *std::max_element(rsrpObs.begin(), rsrpObs.end()) - rsrpObs[targetCellID-1];
  // NS_LOG_UNCOND("MyGetReward: " << reward);
}

bool MyRLHoEnv::CheckIfReady(std::vector<uint8_t> rsrpRecord)
{
  NS_LOG_FUNCTION(this);
  return std::all_of(rsrpRecord.begin(), rsrpRecord.end(), [](int i){ return i < 100; } );
}

uint16_t MyRLHoEnv::PerformHoDecision(std::vector<uint8_t> rsrpObs)
{
  if (CheckIfReady(rsrpObs))
  {
    // entity->Notify();
    auto env = EnvSetterCond();
    for (uint8_t i = 0; i < rsrpObs.size(); ++i)
    {
      env->rsrpMap[i] = rsrpObs.at(i);
    }
    SetCompleted();

    auto act = ActionGetterCond();
    m_TargetCellID = act->nextCellId;

    auto env_after = EnvGetterCond();
    env_after->rsrpReward = GetReward(m_TargetCellID, rsrpObs);
    
    GetCompleted();
  }
  return m_TargetCellID;
}

} // namespace ns3