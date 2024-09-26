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
#pragma once

#ifndef MY_GYM_ENTITY_H
#define MY_GYM_ENTITY_H

#include "ns3/stats-module.h"
#include "ns3/ns3-ai-module.h"
#include "ns3/spectrum-module.h"

namespace ns3
{

#define CELL_NUM 15

struct HoEnv
{
  uint8_t rsrpReward; // needed even if not used
  uint8_t rsrpMap[2*CELL_NUM];
};
struct HoAct
{
  uint16_t nextCellId;
};
struct HoInfo
{
  uint32_t cellNum = CELL_NUM;
};

class MyRLHoEnv : public Ns3AIRL<HoEnv, HoAct, HoInfo>
{
  MyRLHoEnv();

public:
  MyRLHoEnv(uint16_t id);

  virtual ~MyRLHoEnv();
  // static TypeId GetTypeId (void);

  uint8_t GetReward(uint16_t targetCellID, std::vector<uint8_t> rsrpObs);

  // int PerformHoDecision (uint16_t cellId, uint8_t Nrsrp, uint8_t SCellRsrp);
  uint16_t PerformHoDecision ( std::vector<uint8_t> rsrpObs);
  // void CollectChannelOccupation(uint32_t chanId, uint32_t occupied);
  bool CheckIfReady(std::vector<uint8_t> rsrpRecord);
  // void ClearObs();

private:
  std::vector<uint8_t> m_RsrpObs;
  uint16_t m_SCellID;
  uint16_t m_TargetCellID;


};

} // namespace ns3

#endif // MY_GYM_ENTITY_H
