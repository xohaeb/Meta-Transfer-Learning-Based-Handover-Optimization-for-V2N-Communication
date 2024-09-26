/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Budiarto Herman
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
 * Author: Budiarto Herman <budiarto.herman@magister.fi>
 *
 */

#include "my-rl-handover-algorithm.h"
#include <ns3/log.h>
#include <ns3/double.h>
#include <ns3/integer.h>
#include <ns3/uinteger.h>
#include <ns3/lte-common.h>
#include <list>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyRlHandoverAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (MyRlHandoverAlgorithm);


MyRlHandoverAlgorithm::MyRlHandoverAlgorithm ()
  : 
    m_measId (0),
    m_servingCellThreshold (100),
    m_neighbourCellThreshold (0),
    m_handoverManagementSapUser (0)
{
  NS_LOG_FUNCTION (this);
  m_HoRL = Create<MyRLHoEnv> (1080);  
  m_handoverManagementSapProvider = new MemberLteHandoverManagementSapProvider<MyRlHandoverAlgorithm> (this);
}


MyRlHandoverAlgorithm::~MyRlHandoverAlgorithm ()
{
  NS_LOG_FUNCTION (this);
}


TypeId
MyRlHandoverAlgorithm::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::MyRlHandoverAlgorithm")
    .SetParent<LteHandoverAlgorithm> ()
    .SetGroupName ("Lte")
    .AddConstructor<MyRlHandoverAlgorithm> ()
    .AddAttribute ("ServingCellThreshold",
                   "Serving Cell Threshold in RSRP index "
                   "(1 dB equvalent)",
                   UintegerValue (97), // intentionally high value to always trigger
                   MakeUintegerAccessor (&MyRlHandoverAlgorithm::m_servingCellThreshold),
                   MakeUintegerChecker<uint8_t> (0, 97)) 
    .AddAttribute ("NeighbourCellThreshold",
                   "Neighbour Cell Threshold in RSRP index "
                   "(1 dB equvalent)",
                   UintegerValue (0), // intentionally low value to always trigger
                   MakeUintegerAccessor (&MyRlHandoverAlgorithm::m_neighbourCellThreshold),
                   MakeUintegerChecker<uint8_t> (0, 97)) 
  ;
  return tid;
}


void
MyRlHandoverAlgorithm::SetLteHandoverManagementSapUser (LteHandoverManagementSapUser* s)
{
  NS_LOG_FUNCTION (this << s);
  m_handoverManagementSapUser = s;
}


LteHandoverManagementSapProvider*
MyRlHandoverAlgorithm::GetLteHandoverManagementSapProvider ()
{
  NS_LOG_FUNCTION (this);
  return m_handoverManagementSapProvider;
}


void
MyRlHandoverAlgorithm::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  LteRrcSap::ReportConfigEutra reportConfig;
  reportConfig.eventId = LteRrcSap::ReportConfigEutra::EVENT_A5;

  reportConfig.threshold1.choice = LteRrcSap::ThresholdEutra::THRESHOLD_RSRP;
  reportConfig.threshold1.range = m_servingCellThreshold;
  reportConfig.threshold2.choice = LteRrcSap::ThresholdEutra::THRESHOLD_RSRP;
  reportConfig.threshold2.range = m_neighbourCellThreshold;

  reportConfig.reportOnLeave = false;
  reportConfig.triggerQuantity = LteRrcSap::ReportConfigEutra::RSRP;
  reportConfig.reportInterval = LteRrcSap::ReportConfigEutra::MS240;
  m_measId = m_handoverManagementSapUser->AddUeMeasReportConfigForHandover (reportConfig);

  LteHandoverAlgorithm::DoInitialize ();
}


void
MyRlHandoverAlgorithm::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete m_handoverManagementSapProvider;
}


void
MyRlHandoverAlgorithm::DoReportUeMeas (uint16_t rnti,
                                         LteRrcSap::MeasResults measResults)
{
  NS_LOG_FUNCTION (this << rnti << (uint16_t) measResults.measId);
  // std::cout << "current time: " << Simulator::Now ().GetNanoSeconds () / 1.0e9 << std::endl;
  if (measResults.measId == m_measId)
    {
      
      if (measResults.haveMeasResultNeighCells
          && !measResults.measResultListEutra.empty ())
        {
          uint16_t cellNum = measResults.measResultListEutra.size() + 1;
          uint16_t bestCellId, serving_cell = cellNum + 1;
          // std::cout << "cellNum = " << cellNum << std::endl;
          std::vector<uint8_t> RsrpMap(cellNum);
          std::vector<uint8_t> oneHot(cellNum);
          for (int i = 0; i < cellNum; i++)
          {
            RsrpMap[i] = 100;
          }
          int testFlag = 0;
          for (std::list <LteRrcSap::MeasResultEutra>::iterator it = measResults.measResultListEutra.begin ();
               it != measResults.measResultListEutra.end ();
               ++it)
            {
              testFlag += 1;
              if (it->haveRsrpResult)
                {
                  if (IsValidNeighbour (it->physCellId))
                    {
                      RsrpMap [(int) it->physCellId - 1] = (uint8_t)it->rsrpResult;
                      std::cout << "cellID = " << it->physCellId << "; rsrp = " << (int)RsrpMap [(int) it->physCellId - 1] << std::endl;
                    }
                }
              else
                {
                  NS_LOG_WARN ("RSRP measurement is missing from cell ID " << it->physCellId);
                }
            }
          // if (testFlag < cellNum - 1){std::cout << "not enough neighbour cells; test flag = ";
          //   std::cout << testFlag << std::endl;
          // }
          for (uint16_t i = 0; i < cellNum; i++)
             {
               if (RsrpMap[i] == 100)
               {
                 RsrpMap[i] = (uint8_t) measResults.rsrpResult;
                 serving_cell = i + 1;
                 oneHot[i] = 1;
               }
              }
          RsrpMap.reserve(2*cellNum);
          RsrpMap.insert(RsrpMap.end(),oneHot.begin(),oneHot.end());
          // std::cout << "current serving cell: " << (int) serving_cell << std::endl;
          // std::cout << "observation state: " << std::endl;
          // for (int i = 0; i < (int) RsrpMap.size(); i++)
          // {
          //   std::cout << (int)RsrpMap[i] << ' ';
          // }
          // std::cout << std::endl;

          bestCellId = m_HoRL -> PerformHoDecision(RsrpMap);

          // std::cout << "the next cell to handover to: "<< (int) bestCellId << std::endl;
          

          // Inform eNodeB RRC about handover
          if (bestCellId != serving_cell)
          {
            // std::cout << "Handover triggered from cell " << (int) serving_cell << " to " << (int) bestCellId << std::endl;
            m_handoverManagementSapUser->TriggerHandover (rnti, bestCellId);
          }
          // else{std::cout << "No handover triggered" << std::endl;}
          // std::cout << "one loop end flag" << std::endl;
        }
      else
        {
          NS_LOG_UNCOND (this << " Event received without measurement results from neighbouring cells");
        }
    } // end of if (measResults.measId == m_measId)

} // end of DoReportUeMeas


bool
MyRlHandoverAlgorithm::IsValidNeighbour (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);

  /**
   * \todo In the future, this function can be expanded to validate whether the
   *       neighbour cell is a valid target cell, e.g., taking into account the
   *       NRT in ANR and whether it is a CSG cell with closed access.
   */

  return true;
}


} // end of namespace ns3
