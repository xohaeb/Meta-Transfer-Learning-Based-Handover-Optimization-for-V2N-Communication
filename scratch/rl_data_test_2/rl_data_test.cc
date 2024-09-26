/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* 
 * Kang Tan <k.tan.3@research.gla.ac.uk>
 */
#include "ns3/antenna-model.h"
#include "ns3/applications-module.h"
#include "ns3/building.h"
#include "ns3/buildings-helper.h"
#include "ns3/buildings-module.h"
#include "ns3/buildings-propagation-loss-model.h"
#include "ns3/config-store-module.h"
#include "ns3/config-store.h"
#include <ns3/constant-position-mobility-model.h>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/lte-module.h"
#include "ns3/lte-global-pathloss-database.h"
#include "ns3/mobility-building-info.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/radio-bearer-stats-calculator.h"
#include <ns3/radio-environment-map-helper.h>
#include "ns3/spectrum-module.h"
#include <ns3/radio-environment-map-helper.h>
#include "ns3/string.h"
#include "ns3/trace-helper.h"

#include <ns3/constant-position-mobility-model.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <list>
#include <string>
#include <time.h>
#include <vector>


using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("RL-Data-Test");

/*
 * Simulation script for Glasgow city centre C-V2X. Implemented with pure LTE V2N networks
 * Parameters which usually don't need to change are listed as global values.
 */

//--Handover related parameters-----------------------------------
uint16_t rsrpHoTh = 30;
double rsrqHoTh_dB=-5; //dB, If the RSRQ of the serving cell is worse than this threshold, neighbour cells are considered for handover.
uint16_t NeighbourCellOffset=2; //0.5dB units. We should avoid ping-pong hence at least 1dB
uint16_t GetRsrqHoThSetting (double rsrq_dB) 	//Table 9.1.1-1 3GPP TS 36.133
{
	uint16_t y;
	if (rsrq_dB < -19.5){
	  y = 0;}
	else if (rsrq_dB > -3){
	  y = 34;}
	else{
	  y = ceil((rsrq_dB+19.5)*2+0.001);}
	return y;
}
uint16_t rsrqHoTh=GetRsrqHoThSetting (rsrqHoTh_dB); //Get setting parameter, Table 9.1.1-1 3GPP TS 36.133


//If setting HoType=="A3RSRP", this parameters should be set
double rsrpHist= 0;// 3;//Histeresys when HOType A3RSRP is used
double rsrpTimeToTrigger=240; //milliseconds, time to trigger.
// ------------------------------------------------------------------------

// BAND 4 Parameters (DL: 2110 MHz	–	2155 MHz, UL: 1710 MHz	–	1755 MHz ) (see 3GPP TS 36.101 Sections 5.5 and 5.7.3)
//See also UIT-r M.1036-2
enum Bandwidth{
  BW1_4=6,
  BW3=15,
  BW5=25,
  BW10=50,
  BW15=75,
  BW20=100
  };
double fc_Dl=2115; // We use the carrier frequency to compute later the EARFCN for Band 4.
double fc_Ul=fc_Dl-400; // We use the Uplink carrier frequency to compute later the EARFC.
Bandwidth bandwidth=BW20;// 10MHz bandwith, we map this bandwidth to the number of Resource Blocks used by ns3.

double alpha = 3; //Path loss exponent when using logDistancePropagationModel
double d0 = 10; // meter, for reference loss when configuring the channel reference prog. loss.

//Radio parameters
double eNbTxPower = 40; //dBm, 10Watts
double ueTxPower = 23;  //dBm, according to maximum value for Class 3 3GPP TS 36.101
double eNbNoiseFigure = 5;  //5dB
double ueNoiseFigure = 9;   //9dB for UE is currently the deafult value.
int QRxLevMin_dBm = -106;   //Camping parameters, Cell selection, min value is -140


// Output files for HO stats
std::ofstream HOstartUE, HOendUE, HOstartENB, HOendENB, HOfailedUE, initialConnection;
std::string HOstartUEfile = "HOstartUE.txt", HOendUEfile = "HOendUE.txt", HOstartENBfile = "HOstartENB.txt", HOendENBfile = "HOendENB.txt", HOfailedUEfile = "HOfailure.txt", initialConnectionfile = "InitConnect.txt";

/* 
  *Traces sinks here
*/
void NotifyConnectionEstablishedUe (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti) {
  std::cout << context << " UE IMSI " << imsi << ": connected to CellId " << cellid << " with RNTI " << rnti << std::endl;
  if (!initialConnection.is_open()){
    initialConnection.open(initialConnectionfile);
    }
  initialConnection << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << imsi << " " << cellid << " " <<std::endl; 
}

void NotifyConnectionEstablishedEnb (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti) {
  std::cout << context << " eNB CellId " << cellid << ": successful connection of UE with IMSI " << imsi << " RNTI " << rnti << std::endl;
}

void NotifyAndSaveHandoverStartUe (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti, uint16_t targetCellId) {
  std::cout << context << " UE IMSI " << imsi << ": previously connected to CellId " << cellid << " with RNTI " << rnti << ", doing handover to CellId " << targetCellId << std::endl;
  if (!HOstartUE.is_open()){
    HOstartUE.open(HOstartUEfile);
    }
  HOstartUE << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << imsi << " " << cellid << " " << rnti << " " << targetCellId <<std::endl; 
}

void NotifyAndSaveHandoverEndOkUe (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti) {
  std::cout << context << " UE IMSI " << imsi << ": successful handover to CellId " << cellid << " with RNTI " << rnti << " at "<< Simulator::Now ().GetMilliSeconds () << std::endl;
  if (!HOendUE.is_open()){
    HOendUE.open(HOendUEfile);
    }
  HOendUE << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << imsi << " " << cellid << " " << rnti <<std::endl; 
}

void NotifyAndSaveHandoverStartEnb (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti, uint16_t targetCellId) {
  std::cout << context << " eNB CellId " << cellid << ": start handover of UE with IMSI " << imsi << " RNTI " << rnti << " to CellId " << targetCellId << std::endl;
  if (!HOstartENB.is_open()){
    HOstartENB.open(HOstartENBfile);
    }
  HOstartENB << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << cellid << " " << imsi << " " << rnti << " " << targetCellId <<std::endl; 
}

void NotifyAndSaveHandoverEndOkEnb (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti) {
  std::cout << context << " eNB CellId " << cellid << ": completed handover of UE with IMSI " << imsi << " RNTI " << rnti << std::endl;
  if (!HOendENB.is_open())
     {HOendENB.open(HOendENBfile);}
  HOendENB << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << cellid << " " << imsi << " " << rnti << std::endl; 
}

void NotifyAndSaveHandoverFailure (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti) {
  std::cout << context << " eNB CellId " << cellid << ": handover failure of UE with IMSI " << imsi << " RNTI " << rnti << std::endl;
  if (!HOfailedUE.is_open())
     {HOfailedUE.open(HOfailedUEfile);}
  HOfailedUE << Simulator::Now ().GetNanoSeconds () / 1.0e9 << " " << cellid << " " << imsi << " " << rnti << std::endl;
}


// Prints actual position and velocity when a course change event occurs
static void
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition (); // Get position
  Vector vel = mobility->GetVelocity (); // Get velocity

  // Prints position and velocities
  *os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
      << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
      << ", z=" << vel.z << std::endl;
}


/* 
  * Main simulation
*/
int main(int argc, char *argv[])
{
  // RngSeedManager::SetSeed (84);
  std::cout << "current random seed is: " << (int) RngSeedManager::GetSeed() << std::endl;
  // Some simulation configuration
  std::string HoType; //HO algorithms, "A3RSRP", "A2A4RSRQ", "myRL" 
  uint16_t numEnbs = 9;
  uint16_t enbHeight = 35;
  int numUes;
  double duration;
  Time simTime;
  Time interPacketInterval = MilliSeconds (50);
  bool generateRem = false;
  bool disableDl = false;
  bool disableUl = true;
  bool useHelper = false;
  std::string fadingName = "./FadingCh/fading_trace_EVA_120kmph.fad";
  std::string traceFile, logFile;
  
  // Switch for building and fading
  bool enableFading = true;
  bool useThreeGpp = false;
  std::string pathLossModel = "ns3::LogDistancePropagationLossModel"; 
  if(useThreeGpp){pathLossModel = "ns3::ThreeGppUmaPropagationLossModel";}

  uint16_t packetSize = 4096;
  //Initialise the command line interface
  CommandLine cmd;

  //Add default values
  //cmd.AddValue ("radius", "the radius of the disc where UEs are placed around an eNB", radius);
  cmd.AddValue ("numUes", "how many UEs are attached to each eNB", numUes);
  cmd.AddValue ("hoType", "The handover algorithm to use", HoType);
  cmd.AddValue ("simTime", "Total duration of the simulation (seconds)", duration);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
  cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
  cmd.AddValue ("useHelper", "Build the backhaul network using the helper or it is built in the example", useHelper);
  cmd.AddValue ("generateRem", "Whether to generate a REM output or not", generateRem);
  cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
  cmd.AddValue ("logFile", "Log file", logFile);
  //Parse commands
  cmd.Parse (argc, argv);
  
  // Default value onfiguration for LTE
  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));// Sounding Reference Signal Periodicity
  //Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (false));
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (eNbTxPower));
  Config::SetDefault ("ns3::LteUePhy::TxPower", DoubleValue (ueTxPower));
  Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure",DoubleValue (eNbNoiseFigure));
  Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue (ueNoiseFigure));
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (true));
  Config::SetDefault ("ns3::LteUePowerControl::ClosedLoop", BooleanValue (true));
  Config::SetDefault ("ns3::LteUePowerControl::AccumulationEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::LteUePowerControl::Alpha", DoubleValue (1.0));
  //Neighbor Relations
  Config::SetDefault ("ns3::LteHelper::AnrEnabled", BooleanValue (true));//Automatic Neighbor relations activated. Intalled through LteHelper
  Config::SetDefault ("ns3::LteAnr::Threshold", UintegerValue (0));//minimum RSRQ threshold required to detect a Nbr. RSRQ range is [0..34]
																//Section 9.1.7 of 3GPP TS 36.133
  Config::SetDefault ("ns3::LteEnbRrc::QRxLevMin", IntegerValue(QRxLevMin_dBm/2));//Section 6.3.4 of 3GPP TS 36.133
  //Config::SetDefault ("ns3::LteEnbRrc::qQualMin", // It is not available in the attribute list.
  if (traceFile.empty () || duration <= 0 || logFile.empty () || HoType.empty()) 
    {
      std::cout << "Usage of " << argv[0] << " :\n\n"
      "./waf --run \"ns2-mobility-trace"
      " --traceFile=src/mobility/examples/default.ns_movements"
      " --nodeNum=2 --simTime=100.0 --logFile=ns2-mob.log --hoType=myRL \" \n\n"
      "NOTE: ns2-traces-file could be an absolute or relative path. You could use the file default.ns_movements\n"
      "      included in the same directory of this example file.\n\n"
      "NOTE 2: Number of nodes present in the trace file must match with the command line argument and must\n"
      "        be a positive number. Note that you must know it before to be able to load it.\n\n"
      "NOTE 3: Duration must be a positive number. Note that you must know it before to be able to load it.\n\n";

      return 0;
    }

  simTime = Seconds(duration);

 /* 
  * Mobility Setups, 
  */

  // Mobility model for UEs first, so that ns2MobilityHelper does not screw things up.

  NodeContainer ueNodes;
  ueNodes.Create(numUes);

  Ns2MobilityHelper ueMobility = Ns2MobilityHelper (traceFile);
  std::ofstream os;
  os.open(logFile.c_str());
  ueMobility.Install();

  // Mobility model for BSs
  NodeContainer eNbs;
  eNbs.Create(numEnbs);

  uint16_t numBS = numEnbs / 3;
  double x_BS[numBS] = {175, 750, 1325};
  double y_BS = 17.5 + 35;
  
  Ptr<ListPositionAllocator> positionAllocEnbs = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numBS; i++) 
  {
    for (uint16_t j = 0; j < 3; j++)
    {
      if (i == 1){
        positionAllocEnbs->Add(Vector(x_BS[i], -y_BS, enbHeight));
      }
      else{
        positionAllocEnbs->Add(Vector(x_BS[i], y_BS, enbHeight));
      }
    }
  }


  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (positionAllocEnbs);
  enbMobility.Install (eNbs); 


  // Create an LTE helper for basic stuff
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  /* 
   * Configure fading model
   */
  if (enableFading)
{
  lteHelper->SetAttribute ("FadingModel", StringValue ("ns3::TraceFadingLossModel"));

  std::ifstream ifTraceFile;
  // ifTraceFile.open ("../../src/lte/model/fading-traces/fading_trace_EVA_60kmph.fad", std::ifstream::in);
  ifTraceFile.open (fadingName, std::ifstream::in);
  if (ifTraceFile.good ())
    {
      // script launched by test.py
      lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue (fadingName));//("../../src/lte/model/fading-traces/fading_trace_EVA_60kmph.fad"));
    }
  else
    {
      // script launched as an example
      lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue ("fading_trace_EVA_60kmph.fad"));
    }
  // Same as the default values in LENA examples except time length
  lteHelper->SetFadingModelAttribute ("TraceLength", TimeValue (Seconds (300.0)));
  lteHelper->SetFadingModelAttribute ("SamplesNum", UintegerValue (10000));
  lteHelper->SetFadingModelAttribute ("WindowSize", TimeValue (Seconds (0.5)));
  lteHelper->SetFadingModelAttribute ("RbNum", UintegerValue (100));
  std::cout << "Fading enabled." << std::endl;
  }
  else {std::cout << "Fading not enabled." << std::endl;} //end of fadingTrace
  
  // configure bandwidth
  uint8_t nRBs;
  switch (bandwidth)
  {
    case BW1_4:
        nRBs=6;
        break;
    case	BW3:
        nRBs=15;
        break;
    case BW5:
        nRBs=25;
        break;
    case BW10:
        nRBs=50;
        break;
    case BW15:
        nRBs=75;
        break;
    default: //BW20
        nRBs=100;
  };
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (nRBs));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (nRBs));
  
  //Configure pathloss model
  if (pathLossModel == "ns3::LogDistancePropagationLossModel"){
  double lambda =3*pow(10,8)/(fc_Dl*pow(10,6)); // wavelength in m, we will use the downlink
  double L0_dB=10*log10((16*pow((3.14159*d0),2))/pow(lambda,2));//Reference loss at d0, dB.
  std::cout << L0_dB << std::endl;
  lteHelper->SetAttribute ("PathlossModel", StringValue (pathLossModel));
  lteHelper->SetPathlossModelAttribute("Exponent", DoubleValue (alpha));
  lteHelper->SetPathlossModelAttribute ("ReferenceDistance", DoubleValue (d0));
  lteHelper->SetPathlossModelAttribute("ReferenceLoss", DoubleValue(L0_dB));
  std::cout << "Pathloss model log distance setup complete." << std::endl;

  }
  if(pathLossModel == "ns3::HybridBuildingsPropagationLossModel")
  {
    lteHelper->SetAttribute ("PathlossModel", StringValue (pathLossModel));
    lteHelper->SetPathlossModelAttribute ("ShadowSigmaExtWalls", DoubleValue (0));
    lteHelper->SetPathlossModelAttribute ("ShadowSigmaOutdoor", DoubleValue (1));
    lteHelper->SetPathlossModelAttribute ("Los2NlosThr", DoubleValue (1e4));
    lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
    std::cout << "Pathloss model hybrid buildings setup complete." << std::endl;
  }
  if(pathLossModel == "ns3::ThreeGppUmaPropagationLossModel")
  { 
    lteHelper->SetAttribute ("PathlossModel", StringValue (pathLossModel));
    lteHelper->SetPathlossModelAttribute ("Frequency", DoubleValue(fc_Dl*1e6));
    lteHelper->SetPathlossModelAttribute ("ShadowingEnabled", BooleanValue (false));
    std::cout << "Pathloss model 3GPP Uma setup complete." << std::endl;
  }
  std::cout << "test ends" << std::endl;

  // Create the Internet by connecting remoteHost to pgw. Setup routing too
  // Get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet by connecting remoteHost to pgw. Setup routing too
  /* IPv6 */
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv6AddressHelper ipv6h;
  ipv6h.SetBase (Ipv6Address ("6001:db80::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer internetIpIfaces = ipv6h.Assign (internetDevices);
  internetIpIfaces.SetForwarding (0, true);
  internetIpIfaces.SetDefaultRouteInAllNodes (0);
  // interface 0 is localhost, 1 is the p2p device
  Ipv6Address remoteHostAddr = internetIpIfaces.GetAddress (1, 1);
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> remoteHostStaticRouting = ipv6RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv6> ());
  remoteHostStaticRouting->AddNetworkRouteTo ("7777:f00d::", Ipv6Prefix (64), internetIpIfaces.GetAddress (0, 1), 1, 0);

 
  // Setup MAC scheduler 
  lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");

  // Setup the HO algorithm
  if (HoType == "A2A4RSRQ")
  {NS_LOG_INFO ("eA2A4 RSRQ Based HO");
    lteHelper->SetHandoverAlgorithmType ("ns3::A2A4RsrqHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute ("ServingCellThreshold",UintegerValue (rsrqHoTh));
    lteHelper->SetHandoverAlgorithmAttribute ("NeighbourCellOffset", UintegerValue (NeighbourCellOffset));
  }
  else if (HoType == "A3RSRP")
  { NS_LOG_INFO("Handover Type: eA3 RSRP based HO");
    lteHelper->SetHandoverAlgorithmType ("ns3::A3RsrpHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute ("Hysteresis", DoubleValue (rsrpHist));
    lteHelper->SetHandoverAlgorithmAttribute ("TimeToTrigger", TimeValue (MilliSeconds (rsrpTimeToTrigger)));
  }
  else if (HoType == "myRL")
  {
        lteHelper->SetHandoverAlgorithmType ("ns3::MyRlHandoverAlgorithm");
  }
  else{NS_LOG_UNCOND("Unknown HO type"); }


  // Create NetDevices for nodes 
  NetDeviceContainer enbDevs;
  double AntennaOrientation[numEnbs] = {10, 90, 170, 190, 270, 350, 10, 90, 170};

  // Install eNB devices
  for (uint16_t i = 0; i < numEnbs; i++)
  {
    lteHelper->SetEnbAntennaModelType ("ns3::CosineAntennaModel");
    lteHelper->SetEnbAntennaModelAttribute ("Orientation", DoubleValue (AntennaOrientation[i] - 180)); // Adjust offset because ns-3 use X-axis as 0
    // Same settings as Shuja's previous work
    lteHelper->SetEnbAntennaModelAttribute ("MaxGain", DoubleValue (15.0));  
    lteHelper->SetEnbAntennaModelAttribute ("HorizontalBeamwidth", DoubleValue (40.0));
    enbDevs = lteHelper->InstallEnbDevice (eNbs.Get(i));
  }
  std::cout <<"BS good \n";

  NetDeviceContainer ueDevs = lteHelper->InstallUeDevice (ueNodes);
  std::cout <<"UE good \n";

  // Install the IP stack on the UEs

  /* IPv6  */

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv6InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv6Address (NetDeviceContainer (ueDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv6StaticRouting> ueStaticRouting = ipv6RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv6> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress6 (), 1);
    }

 // Install and start applications on UEs and remote host
  uint16_t dlPort = 1100;
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      if (!disableDl)
        {
          UdpServerHelper dlPacketSinkHelper (dlPort);
          dlPacketSinkHelper.SetAttribute ("PacketWindowSize", UintegerValue (256));
          serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

          // Simulator::Schedule(MilliSeconds(20), &PrintLostUdpPackets, DynamicCast<UdpServer>(serverApps.Get(serverApps.GetN()-1)), lostFilename);

          UdpClientHelper dlClient (ueIpIface.GetAddress (u, 1), dlPort);
          dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          dlClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFF));
          dlClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
          clientApps.Add (dlClient.Install (remoteHost));

        }
      if (!disableUl)
        {
          ++ulPort;
          UdpServerHelper ulPacketSinkHelper (ulPort);
          ulPacketSinkHelper.SetAttribute ("PacketWindowSize", UintegerValue (256));
          serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          ulClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
          clientApps.Add (ulClient.Install (ueNodes.Get (u)));
        }
    }
  
  lteHelper->AddX2Interface (eNbs); // Connect eNBs via X2 interface
  lteHelper->Attach(ueDevs); // Attach all nodes to eNBs.
  
  // Start applications
  std::cout << "Application Starts \n";

  serverApps.Start (Seconds (1)); // reserve some time for setups 
  clientApps.Start (Seconds (1));
  clientApps.Stop (simTime - Seconds(1)); 

  lteHelper->EnableTraces ();

  // connect custom trace sinks for RRC connection establishment and handover notification
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                   MakeCallback (&NotifyAndSaveHandoverStartEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                   MakeCallback (&NotifyAndSaveHandoverStartUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                   MakeCallback (&NotifyAndSaveHandoverEndOkEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                   MakeCallback (&NotifyAndSaveHandoverEndOkUe));
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeBoundCallback (&CourseChange, &os));

  std::cout << "Simulation starts \n";

  Simulator::Stop (simTime);

  Simulator::Run ();
  Simulator::Destroy ();

  os.close();
  return 0;
}