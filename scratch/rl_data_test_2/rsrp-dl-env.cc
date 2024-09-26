#include "rsrp-dl-env.h"

namespace ns3 {
RSRPDL::RSRPDL (uint16_t id) : Ns3AIDL<RsrpFeature, RsrpPredicted, RSRPTarget>(id)
{
  SetCond (2, 0);
}

void
RSRPDL::SetRsrp (uint8_t rsrp)
{
  auto feature = FeatureSetterCond ();
  feature->Serving_Rsrp = rsrp;
  SetCompleted ();
}
uint8_t
RSRPDL::GetRsrp (void)
{
  auto pred = PredictedGetterCond ();
  uint8_t ret = pred->new_Rsrp;
  GetCompleted ();
  return ret;
}

void RSRPDL::SetTarget (uint8_t tar)
{
  auto target = TargetSetterCond ();
  target->target = tar;
  SetCompleted ();
}
uint8_t RSRPDL::GetTarget (void)
{
  auto tar = TargetGetterCond ();
  uint8_t ret = tar->target;
  GetCompleted ();
  return ret;
}
} // namespace ns3
