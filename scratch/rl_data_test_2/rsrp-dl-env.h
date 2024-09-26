#pragma once
#include "ns3/ns3-ai-dl.h"
// #include "ns3/ff-mac-common.h"

namespace ns3 {
#define MAX_RBG_NUM 32
struct RsrpFeature
{
  uint8_t Serving_Rsrp;
};
struct RsrpPredicted
{
  uint8_t new_Rsrp;
};
struct RSRPTarget
{
  uint8_t target;
};
class RSRPDL : public Ns3AIDL<RsrpFeature, RsrpPredicted, RSRPTarget>
{
public:
  RSRPDL (void) = delete;
  RSRPDL (uint16_t id);
  
  void SetRsrp (uint8_t rsrp);
  uint8_t GetRsrp (void);

  void SetTarget (uint8_t tar);
  uint8_t GetTarget (void);
};

} // namespace ns3
