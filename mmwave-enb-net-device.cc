/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
*   Copyright (c) 2016, 2018, University of Padova, Dep. of Information Engineering, SIGNET lab.
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   Author: Marco Miozzo <marco.miozzo@cttc.es>
*           Nicola Baldo  <nbaldo@cttc.es>
*
*   Modified by: Marco Mezzavilla < mezzavilla@nyu.edu>
*                         Sourjya Dutta <sdutta@nyu.edu>
*                         Russell Ford <russell.ford@nyu.edu>
*                         Menglei Zhang <menglei@nyu.edu>
*
*       Modified by: Tommaso Zugno <tommasozugno@gmail.com>
*                                Integration of Carrier Aggregation
*/


#include <ns3/llc-snap-header.h>
#include <ns3/simulator.h>
#include <ns3/callback.h>
#include <ns3/node.h>
#include <ns3/packet.h>
#include "mmwave-net-device.h"
#include <ns3/packet-burst.h>
#include <ns3/uinteger.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/pointer.h>
#include <ns3/enum.h>
#include <ns3/uinteger.h>
#include <ns3/double.h>
#include <ns3/string.h>
#include "mmwave-enb-net-device.h"
#include "mmwave-ue-net-device.h"
#include <ns3/lte-enb-rrc.h>
#include <ns3/ipv4-l3-protocol.h>
#include <ns3/ipv6-l3-protocol.h>
#include <ns3/abort.h>
#include <ns3/log.h>
#include <ns3/lte-enb-component-carrier-manager.h>
#include <ns3/mmwave-component-carrier-enb.h>
#include <ns3/config.h>
#include <ns3/lte-rlc-um.h>
#include <ns3/lte-rlc-um-lowlat.h>
#include <ns3/lte-rlc-am.h>
#include<random>
#include<cstdlib>
#include<ctime>

#include <ns3/mmwave-indication-message-helper.h>
#include <ns3/influxdb.h>

#include "encode_e2apv1.hpp"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MmWaveEnbNetDevice");

namespace mmwave {


NS_OBJECT_ENSURE_REGISTERED (MmWaveEnbNetDevice);

/**
* KPM Subscription Request callback.
* This function is triggered whenever a RIC Subscription Request for 
* the KPM RAN Function is received.
*
* \param pdu request message
*/
void 
MmWaveEnbNetDevice::KpmSubscriptionCallback (E2AP_PDU_t* sub_req_pdu)
{
  NS_LOG_DEBUG ("\nReceived RIC Subscription Request, cellId= " << m_cellId << "\n");

  E2Termination::RicSubscriptionRequest_rval_s params = m_e2term->ProcessRicSubscriptionRequest (sub_req_pdu);
  NS_LOG_DEBUG ("requestorId " << +params.requestorId << 
                 ", instanceId " << +params.instanceId << 
                 ", ranFuncionId " << +params.ranFuncionId << 
                 ", actionId " << +params.actionId);  

  if (!m_isReportingEnabled)
  {
    BuildAndSendReportMessage (params);
    m_isReportingEnabled = true; 
  }
    
}

TypeId MmWaveEnbNetDevice::GetTypeId ()
{
  static TypeId
    tid =
    TypeId ("ns3::MmWaveEnbNetDevice")
    .SetParent<MmWaveNetDevice> ()
    .AddConstructor<MmWaveEnbNetDevice> ()
    .AddAttribute ("LteEnbComponentCarrierManager",
                   "The ComponentCarrierManager associated to this EnbNetDevice",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::m_componentCarrierManager),
                   MakePointerChecker <LteEnbComponentCarrierManager> ())
    .AddAttribute ("LteEnbRrc",
                   "The RRC layer associated with the ENB",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::m_rrc),
                   MakePointerChecker <LteEnbRrc> ())
    .AddAttribute ("E2Termination",
                   "The E2 termination object associated to this node",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::SetE2Termination,
                                                &MmWaveEnbNetDevice::GetE2Termination),
                   MakePointerChecker <E2Termination> ())
    .AddAttribute ("CellId",
                   "Cell Identifier",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmWaveEnbNetDevice::m_cellId),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("BasicCellId",
                   "Basic cell ID. This is needed to properly loop over neighbors.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&MmWaveEnbNetDevice::m_basicCellId),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("E2PdcpCalculator",
                   "The PDCP calculator object for E2 reporting",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::m_e2PdcpStatsCalculator),
                   MakePointerChecker <MmWaveBearerStatsCalculator> ())
    .AddAttribute ("E2Periodicity",
                   "Periodicity of E2 reporting (value in seconds)",
                   DoubleValue (0.1),
                   MakeDoubleAccessor (&MmWaveEnbNetDevice::m_e2Periodicity),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("E2RlcCalculator",
                   "The RLC calculator object for E2 reporting",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::m_e2RlcStatsCalculator),
                   MakePointerChecker <MmWaveBearerStatsCalculator> ())    
    .AddAttribute ("E2DuCalculator",
                   "The DU calculator object for E2 reporting",
                   PointerValue (),
                   MakePointerAccessor (&MmWaveEnbNetDevice::m_e2DuCalculator),
                   MakePointerChecker <MmWavePhyTrace> ())
    .AddAttribute ("EnableCuUpReport",
                   "If true, send CuUpReport",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmWaveEnbNetDevice::m_sendCuUp),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableCuCpReport",
                   "If true, send CuCpReport",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmWaveEnbNetDevice::m_sendCuCp),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableDuReport",
                   "If true, send DuReport",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MmWaveEnbNetDevice::m_sendDu),
                   MakeBooleanChecker ())
    .AddAttribute ("ReducedPmValues",
                   "If true, send only a subset of pmValues",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmWaveEnbNetDevice::m_reducedPmValues),
                   MakeBooleanChecker ())
   .AddAttribute ("EnableE2FileLogging",
                   "If true, force E2 indication generation and write E2 fields in csv file",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmWaveEnbNetDevice::m_forceE2FileLogging),
                   MakeBooleanChecker ())
    .AddAttribute ("influxdb_ip",
                   "influx db ip",
                   StringValue(""),
                   MakeStringAccessor (&MmWaveEnbNetDevice::influx_ip),
                   MakeStringChecker())
  ;
  return tid;
}

MmWaveEnbNetDevice::MmWaveEnbNetDevice ()
//:m_cellId(0),
// m_Bandwidth (72),
// m_Earfcn(1),
  : m_componentCarrierManager (0),
    m_isConfigured (false),
    m_isReportingEnabled (false),
    m_reducedPmValues (false),
    m_forceE2FileLogging (false),
    m_cuUpFileName (),
    m_cuCpFileName (),
    m_duFileName ()
{
  NS_LOG_FUNCTION (this);
}


MmWaveEnbNetDevice::~MmWaveEnbNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
MmWaveEnbNetDevice::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  m_isConstructed = true;
  UpdateConfig ();
  for (auto it = m_ccMap.begin (); it != m_ccMap.end (); ++it)
    {
      it->second->Initialize ();
    }
  m_rrc->Initialize ();
  m_componentCarrierManager->Initialize ();

  if (m_sendCuCp == true)
  {
    // connect to callback
    Config::ConnectFailSafe ("/NodeList/*/DeviceList/*/LteEnbRrc/NotifyMmWaveSinr",
    MakeBoundCallback (&MmWaveEnbNetDevice::RegisterNewSinrReadingCallback, this));
  }
}

void
MmWaveEnbNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_rrc->Dispose ();
  m_rrc = 0;

  m_componentCarrierManager->Dispose ();
  m_componentCarrierManager = 0;
  // MmWaveComponentCarrierEnb::DoDispose() will call DoDispose
  // of its PHY, MAC, FFR and scheduler instance
  for (uint32_t i = 0; i < m_ccMap.size (); i++)
    {
      m_ccMap.at (i)->Dispose ();
      m_ccMap.at (i) = 0;
    }

  MmWaveNetDevice::DoDispose ();
}

void
MmWaveEnbNetDevice::RegisterNewSinrReadingCallback(Ptr<MmWaveEnbNetDevice> netDev, std::string context, uint64_t imsi, uint16_t cellId, long double sinr)
{
  //double sinrThisCell = 10 * std::log10 (sinr);
  //NS_LOG_DEBUG("Imsi: " << imsi << " for ceill id: " << cellId << " sinr is: " << sinrThisCell);
  netDev->RegisterNewSinrReading(imsi, cellId, sinr);
}

void
MmWaveEnbNetDevice::RegisterNewSinrReading(uint64_t imsi, uint16_t cellId, long double sinr)
{
  // check if the imsi is connected to this DU
  auto ueMap = m_rrc->GetUeMap();
  bool imsiFound = false;

  for (auto ue : ueMap)
  {
    if (ue.second->GetImsi() == imsi)
    {
      imsiFound = true;
      break;
    }
  }

  if (imsiFound)
  {
    // we only need to save the last value, so we erase if exists already a value nd save the new one
    m_l3sinrMap[imsi][cellId] = sinr;
    /*NS_LOG_LOGIC (Simulator::Now ().GetSeconds ()
                  << " enbdev " << m_cellId << " UE " << imsi << " report for " << cellId
                  << " SINR " << sinrThisCell);*/
  }
}


Ptr<MmWaveEnbPhy>
MmWaveEnbNetDevice::GetPhy (void) const
{
  NS_LOG_FUNCTION (this);
  return DynamicCast<MmWaveComponentCarrierEnb> (m_ccMap.at (0))->GetPhy ();
}

Ptr<MmWaveEnbPhy>
MmWaveEnbNetDevice::GetPhy (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_ccMap.at (index))->GetPhy ();
}


uint16_t
MmWaveEnbNetDevice::GetCellId () const
{
  NS_LOG_FUNCTION (this);
  return m_cellId;
}

bool
MmWaveEnbNetDevice::HasCellId (uint16_t cellId) const
{
  for (auto &it : m_ccMap)
    {

      if (DynamicCast<MmWaveComponentCarrierEnb> (it.second)->GetCellId () == cellId)
        {
          return true;
        }
    }
  return false;
}

uint8_t
MmWaveEnbNetDevice::GetBandwidth () const
{
  NS_LOG_FUNCTION (this);
  return m_Bandwidth;
}

void
MmWaveEnbNetDevice::SetBandwidth (uint8_t bw)
{
  NS_LOG_FUNCTION (this << bw);
  m_Bandwidth = bw;
}

Ptr<MmWaveEnbMac>
MmWaveEnbNetDevice::GetMac (void)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_ccMap.at (0))->GetMac ();
}

Ptr<MmWaveEnbMac>
MmWaveEnbNetDevice::GetMac (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_ccMap.at (index))->GetMac ();
}

void
MmWaveEnbNetDevice::SetRrc (Ptr<LteEnbRrc> rrc)
{
  m_rrc = rrc;
}

Ptr<LteEnbRrc>
MmWaveEnbNetDevice::GetRrc (void)
{
  return m_rrc;
}

bool
MmWaveEnbNetDevice::DoSend (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet   << dest << protocolNumber);
  NS_ABORT_MSG_IF (protocolNumber != Ipv4L3Protocol::PROT_NUMBER
                   && protocolNumber != Ipv6L3Protocol::PROT_NUMBER,
                   "unsupported protocol " << protocolNumber << ", only IPv4/IPv6 is supported");
  return m_rrc->SendData (packet);
}

void
MmWaveEnbNetDevice::UpdateConfig (void)
{
  NS_LOG_FUNCTION (this);

  if (m_isConstructed)
    {
      if (!m_isConfigured)
        {
          NS_LOG_LOGIC (this << " Configure cell " << m_cellId);
          // we have to make sure that this function is called only once
          //m_rrc->ConfigureCell (m_Bandwidth, m_Bandwidth, m_Earfcn, m_Earfcn, m_cellId);
          NS_ASSERT (!m_ccMap.empty ());

          // create the MmWaveComponentCarrierConf map used for the RRC setup
          std::map<uint8_t, LteEnbRrc::MmWaveComponentCarrierConf> ccConfMap;
          for (auto it = m_ccMap.begin (); it != m_ccMap.end (); ++it)
            {
              Ptr<MmWaveComponentCarrierEnb> ccEnb = DynamicCast<MmWaveComponentCarrierEnb> (it->second);
              LteEnbRrc::MmWaveComponentCarrierConf ccConf;
              ccConf.m_ccId = ccEnb->GetConfigurationParameters ()->GetCcId ();
              ccConf.m_cellId = ccEnb->GetCellId ();
              ccConf.m_bandwidth = ccEnb->GetBandwidthInRb ();

              ccConfMap[it->first] = ccConf;
            }

          m_rrc->ConfigureCell (ccConfMap);

          // trigger E2Termination activation for when the simulation starts
          // schedule at start time
          if (m_e2term != 0)
            {
              NS_LOG_DEBUG("E2sim start in cell " << m_cellId 
                << " force CSV logging " << m_forceE2FileLogging);              

              if(!m_forceE2FileLogging) {
                Simulator::Schedule (MicroSeconds (0), &E2Termination::Start, m_e2term);
              }
              else {
                m_cuUpFileName = "cu-up-cell-" + std::to_string(m_cellId) + ".txt";
                std::ofstream csv {};
                csv.open (m_cuUpFileName.c_str ());
                csv << "timestamp,ueImsiComplete,DRB.PdcpSduDelayDl (cellAverageLatency),"
                       "m_pDCPBytesUL (0),"
                       "m_pDCPBytesDL (cellDlTxVolume),DRB.PdcpSduVolumeDl_Filter.UEID (txBytes),"
                       "Tot.PdcpSduNbrDl.UEID (txDlPackets),DRB.PdcpSduBitRateDl.UEID"
                       "(pdcpThroughput),"
                       "DRB.PdcpSduDelayDl.UEID (pdcpLatency),QosFlow.PdcpPduVolumeDL_Filter.UEID"
                       "(txPdcpPduBytesNrRlc),DRB.PdcpPduNbrDl.Qos.UEID (txPdcpPduNrRlc)\n";
                csv.close ();

                m_cuCpFileName = "cu-cp-cell-" + std::to_string(m_cellId) + ".txt";
                csv.open (m_cuCpFileName.c_str ());
                csv << "timestamp,ueImsiComplete,numActiveUes,DRB.EstabSucc.5QI.UEID (numDrb),"
                       "DRB.RelActNbr.5QI.UEID (0),L3 serving Id(m_cellId),UE (imsi),L3 serving SINR,"
                       "L3 serving SINR 3gpp,"
                       "L3 neigh Id 1 (cellId),L3 neigh SINR 1,L3 neigh SINR 3gpp 1 (convertedSinr),"
                       "L3 neigh Id 2 (cellId),L3 neigh SINR 2,L3 neigh SINR 3gpp 2 (convertedSinr),"
                       "L3 neigh Id 3 (cellId),L3 neigh SINR 3,L3 neigh SINR 3gpp 3 (convertedSinr),"
                       "L3 neigh Id 4 (cellId),L3 neigh SINR 4,L3 neigh SINR 3gpp 4 (convertedSinr),"
                       "L3 neigh Id 5 (cellId),L3 neigh SINR 5,L3 neigh SINR 3gpp 5 (convertedSinr),"
                       "L3 neigh Id 6 (cellId),L3 neigh SINR 6,L3 neigh SINR 3gpp 6 (convertedSinr),"
                       "L3 neigh Id 7 (cellId),L3 neigh SINR 7,L3 neigh SINR 3gpp 7 (convertedSinr),"
                       "L3 neigh Id 8 (cellId),L3 neigh SINR 8,L3 neigh SINR 3gpp 8 (convertedSinr)"
                       "\n";
                csv.close();

                m_duFileName = "du-cell-" + std::to_string(m_cellId) + ".txt";
                csv.open (m_duFileName.c_str ());
                
                std::string header_csv =
                    "timestamp,ueImsiComplete,plmId,nrCellId,dlAvailablePrbs,"
                    "ulAvailablePrbs,qci,dlPrbUsage,ulPrbUsage";

                std::string cell_header =
                    "TB.TotNbrDl.1,TB.TotNbrDlInitial,TB.TotNbrDlInitial.Qpsk,"
                    "TB.TotNbrDlInitial.16Qam,"
                    "TB.TotNbrDlInitial.64Qam,RRU.PrbUsedDl,TB.ErrTotalNbrDl.1,"
                    "QosFlow.PdcpPduVolumeDL_Filter,CARR.PDSCHMCSDist.Bin1,"
                    "CARR.PDSCHMCSDist.Bin2,"
                    "CARR.PDSCHMCSDist.Bin3,CARR.PDSCHMCSDist.Bin4,CARR.PDSCHMCSDist.Bin5,"
                    "CARR.PDSCHMCSDist.Bin6,L1M.RS-SINR.Bin34,L1M.RS-SINR.Bin46, "
                    "L1M.RS-SINR.Bin58,"
                    "L1M.RS-SINR.Bin70,L1M.RS-SINR.Bin82,L1M.RS-SINR.Bin94,L1M.RS-SINR.Bin127,"
                    "DRB.BufferSize.Qos,DRB.MeanActiveUeDl";

                std::string ue_header =
                    "TB.TotNbrDl.1.UEID,TB.TotNbrDlInitial.UEID,TB.TotNbrDlInitial.Qpsk.UEID,"
                    "TB.TotNbrDlInitial.16Qam.UEID,TB.TotNbrDlInitial.64Qam.UEID,"
                    "TB.ErrTotalNbrDl.1.UEID,"
                    "QosFlow.PdcpPduVolumeDL_Filter.UEID,RRU.PrbUsedDl.UEID,"
                    "CARR.PDSCHMCSDist.Bin1.UEID,"
                    "CARR.PDSCHMCSDist.Bin2.UEID,CARR.PDSCHMCSDist.Bin3.UEID,"
                    "CARR.PDSCHMCSDist.Bin4.UEID,"
                    "CARR.PDSCHMCSDist.Bin5.UEID,"
                    "CARR.PDSCHMCSDist.Bin6.UEID,L1M.RS-SINR.Bin34.UEID, L1M.RS-SINR.Bin46.UEID,"
                    "L1M.RS-SINR.Bin58.UEID,L1M.RS-SINR.Bin70.UEID,L1M.RS-SINR.Bin82.UEID,"
                    "L1M.RS-SINR.Bin94.UEID,L1M.RS-SINR.Bin127.UEID,DRB.BufferSize.Qos.UEID,"
                    "DRB.UEThpDl.UEID, DRB.UEThpDlPdcpBased.UEID";

                csv << header_csv + "," + cell_header + "," + ue_header + "\n";
                csv.close();
                Simulator::Schedule(MicroSeconds(500), &MmWaveEnbNetDevice::BuildAndSendReportMessage, this, E2Termination::RicSubscriptionRequest_rval_s{});
              }

            }
          m_isConfigured = true;
        }

      //m_rrc->SetCsgId (m_csgId, m_csgIndication);
    }
  else
    {
      /*
      * Lower layers are not ready yet, so do nothing now and expect
      * ``DoInitialize`` to re-invoke this function.
      */
    }
}

void
MmWaveEnbNetDevice::SetCcMap (std::map< uint8_t, Ptr<MmWaveComponentCarrier> > ccm)
{
  NS_ASSERT_MSG (!m_isConfigured, "attempt to set CC map after configuration");
  m_ccMap = ccm;
}

Ptr<E2Termination>
MmWaveEnbNetDevice::GetE2Termination() const
{
  return m_e2term;
}

void
MmWaveEnbNetDevice::SetE2Termination(Ptr<E2Termination> e2term)
{
  m_e2term = e2term;
  NS_LOG_DEBUG("Register E2SM");

  if (!m_forceE2FileLogging)
    {
      Ptr<KpmFunctionDescription> kpmFd = Create<KpmFunctionDescription> ();
      e2term->RegisterKpmCallbackToE2Sm (
          0, kpmFd,
          std::bind (&MmWaveEnbNetDevice::KpmSubscriptionCallback, this, std::placeholders::_1));
    }
}

std::string
MmWaveEnbNetDevice::GetImsiString(uint64_t imsi)
{
  std::string ueImsi = std::to_string(imsi);
  std::string ueImsiComplete {};
  if (ueImsi.length() == 1)
  {
    ueImsiComplete = "0000" + ueImsi;
  }
  else if (ueImsi.length() == 2)
  {
    ueImsiComplete = "000" + ueImsi;
  }
  else
  {
    ueImsiComplete = "00" + ueImsi;
  }
  return ueImsiComplete;
}

Ptr<KpmIndicationHeader>
MmWaveEnbNetDevice::BuildRicIndicationHeader (std::string plmId, std::string gnbId,
                                              uint16_t nrCellId)
{
  if (!m_forceE2FileLogging)
    {
      KpmIndicationHeader::KpmRicIndicationHeaderValues headerValues;
      headerValues.m_plmId = plmId;
      headerValues.m_gnbId = gnbId;
      headerValues.m_nrCellId = nrCellId;
      auto time = Simulator::Now ();
      uint64_t timestamp = m_startTime + (uint64_t) time.GetMilliSeconds ();
      NS_LOG_DEBUG ("NR plmid " << plmId << " gnbId " << gnbId << " nrCellId " << nrCellId);
      NS_LOG_DEBUG ("Timestamp " << timestamp);
      headerValues.m_timestamp = timestamp;

      Ptr<KpmIndicationHeader> header =
          Create<KpmIndicationHeader> (KpmIndicationHeader::GlobalE2nodeType::gNB, headerValues);

      return header;
    }
  else
    {
      return nullptr;
    }
}
//plmnid_buf(octstr), nrcellid_buf(gnbid, bitstr), crnti_buf(octstr), bytes (dl), 0 (ul)
Ptr<MmWaveIndicationMessageHelper>
MmWaveEnbNetDevice::BuildRicIndicationMessageCuUp(std::string plmId, std::string gnbId)
{
  Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
      Create<MmWaveIndicationMessageHelper> (IndicationMessageHelper::IndicationMessageType::CuUp,
                                             m_forceE2FileLogging, m_reducedPmValues);

  // get <rnti, UeManager> map of connected UEs
  auto ueMap = m_rrc->GetUeMap();
  // gNB-wide PDCP volume in downlink
  double cellDlTxVolume = 0;
  // rx bytes in downlink
  double cellDlRxVolume = 0;

  // sum of the per-user average latency
  double perUserAverageLatencySum = 0;

  std::unordered_map<uint64_t, std::string> uePmString {};

  for (auto ue : ueMap)
  {
    uint64_t imsi = ue.second->GetImsi();
    std::string rnti = std::to_string(ue.second->GetRnti());
    std::string ueImsiComplete = GetImsiString (imsi);

    // double rxDlPackets = m_e2PdcpStatsCalculator->GetDlRxPackets(imsi, 3); // LCID 3 is used for data
    long txDlPackets = m_e2PdcpStatsCalculator->GetDlTxPackets(imsi, 3); // LCID 3 is used for data
    double txBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 3)  * 8 / 1e3; // in kbit, not byte
    double rxBytes = m_e2PdcpStatsCalculator->GetDlRxData(imsi, 3)  * 8 / 1e3; // in kbit, not byte
    cellDlTxVolume += txBytes;
    cellDlRxVolume += rxBytes;

    long txPdcpPduNrRlc = 0;
    double txPdcpPduBytesNrRlc = 0;

    auto drbMap = ue.second->GetDrbMap();
    for (auto drb : drbMap)
    {
      txPdcpPduNrRlc += drb.second->m_rlc->GetTxPacketsInReportingPeriod();
      txPdcpPduBytesNrRlc += drb.second->m_rlc->GetTxBytesInReportingPeriod();
      drb.second->m_rlc->ResetRlcCounters();
    }

    auto rlcMap = ue.second->GetRlcMap(); // secondary-connected RLCs
    for (auto drb : rlcMap)
    {
      txPdcpPduNrRlc += drb.second->m_rlc->GetTxPacketsInReportingPeriod();
      txPdcpPduBytesNrRlc += drb.second->m_rlc->GetTxBytesInReportingPeriod();
      drb.second->m_rlc->ResetRlcCounters();
    }
    txPdcpPduBytesNrRlc *= 8 / 1e3;

    double pdcpLatency = m_e2PdcpStatsCalculator->GetDlDelay(imsi, 3) / 1e5; // unit: x 0.1 ms
    perUserAverageLatencySum += pdcpLatency;

    double pdcpThroughput = txBytes / m_e2Periodicity; // unit kbps
    double pdcpThroughputRx = rxBytes / m_e2Periodicity; // unit kbps
    
    if (m_drbThrDlPdcpBasedComputationUeid.find (imsi) != m_drbThrDlPdcpBasedComputationUeid.end ())
    {
      m_drbThrDlPdcpBasedComputationUeid.at (imsi) += pdcpThroughputRx; 
    }
    else
    {
      m_drbThrDlPdcpBasedComputationUeid [imsi] = pdcpThroughputRx; 
    }
    ue_pdcp_dl[imsi] = m_drbThrDlPdcpBasedComputationUeid.at (imsi) /*/ 20960.f*/;
    ue_pdcp_ul[imsi] = pdcpThroughput /*/ 20960.f*/;
    long bytes_dl = m_drbThrDlPdcpBasedComputationUeid.at (imsi) * 100;
    long bytes_ul = pdcpThroughput * 100;
    
    // compute bitrate based on RLC statistics, decoupled from pdcp throughput
    double rlcLatency = m_e2RlcStatsCalculator->GetDlDelay(imsi, 3) / 1e9; // unit: s
    double pduStats = m_e2RlcStatsCalculator->GetDlPduSizeStats(imsi, 3)[0] * 8.0 / 1e3; // unit kbit
    double rlcBitrate = (rlcLatency == 0) ? 0 : pduStats / rlcLatency; // unit kbit/s

    m_drbThrDlUeid [imsi] = rlcBitrate; 
    
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << m_cellId << " cell, connected UE with IMSI " << imsi 
      << " ueImsiString " << ueImsiComplete
      << " txDlPackets " << txDlPackets 
      << " txDlPacketsNr " << txPdcpPduNrRlc
      << " txBytes " << txBytes 
      << " rxBytes " << rxBytes 
      << " txDlBytesNr " << txPdcpPduBytesNrRlc
      << " pdcpLatency " << pdcpLatency 
      << " pdcpThroughput " << pdcpThroughput
      << " rlcBitrate " << rlcBitrate);

    m_e2PdcpStatsCalculator->ResetResultsForImsiLcid (imsi, 3);

    uePmString.insert(std::make_pair(imsi, ",,,," + std::to_string(txPdcpPduBytesNrRlc) + "," +
      std::to_string(txPdcpPduNrRlc)));
  
    //connlab
    if (!m_forceE2FileLogging)
    {
      indicationMessageHelper->FillCuUpPmValues (plmId, gnbId, rnti, bytes_dl, bytes_ul);
    }
  }
  //cellDlRxVolume ->total bytes_dl
  cell_pdcp_dl = cellDlRxVolume;
  cell_pdcp_ul = cellDlTxVolume;
  if (!m_forceE2FileLogging)
  {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << m_cellId << " cell volume " << cellDlTxVolume);
  }

  if (m_forceE2FileLogging)
    {
      std::ofstream csv{};
      csv.open (m_cuUpFileName.c_str (), std::ios_base::app);
      if (!csv.is_open ())
        {
          NS_FATAL_ERROR ("Can't open file " << m_cuUpFileName.c_str ());
        }

      uint64_t timestamp = m_startTime + (uint64_t) Simulator::Now ().GetMilliSeconds ();

      // the string is timestamp, ueImsiComplete, DRB.PdcpSduDelayDl (cellAverageLatency),
      // m_pDCPBytesUL (0), m_pDCPBytesDL (cellDlTxVolume), DRB.PdcpSduVolumeDl_Filter.UEID (txBytes),
      // Tot.PdcpSduNbrDl.UEID (txDlPackets), DRB.PdcpSduBitRateDl.UEID (pdcpThroughput),
      // DRB.PdcpSduDelayDl.UEID (pdcpLatency), QosFlow.PdcpPduVolumeDL_Filter.UEID (txPdcpPduBytesNrRlc),
      // DRB.PdcpPduNbrDl.Qos.UEID (txPdcpPduNrRlc)

      for (auto ue : ueMap)
        {
          uint64_t imsi = ue.second->GetImsi ();
          std::string ueImsiComplete = GetImsiString (imsi);

          auto uePms = uePmString.find (imsi)->second;

          std::string to_print = std::to_string (timestamp) + "," + ueImsiComplete + "," + "," +
                                 "," + "," + uePms + "\n";

          csv << to_print;
        }
      csv.close ();
      return nullptr;
    }
  else
  {
    //connlab
    indicationMessageHelper->FillCuUpCellValues (plmId, gnbId);
    return indicationMessageHelper;
  }
}

template <typename A, typename B>
std::pair<B, A>
flip_pair (const std::pair<A, B> &p)
{
  return std::pair<B, A> (p.second, p.first);
}

template <typename A, typename B>
std::multimap<B, A>
flip_map (const std::map<A, B> &src)
{
  std::multimap<B, A> dst;
  std::transform (src.begin (), src.end (), std::inserter (dst, dst.begin ()), flip_pair<A, B>);
  return dst;
}

Ptr<MmWaveIndicationMessageHelper>
MmWaveEnbNetDevice::BuildRicIndicationMessageCuCp(std::string plmId, std::string gnbId)
{
  Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
      Create<MmWaveIndicationMessageHelper> (IndicationMessageHelper::IndicationMessageType::CuCp,
                                             m_forceE2FileLogging, m_reducedPmValues);

  auto ueMap = m_rrc->GetUeMap();

  std::unordered_map<uint64_t, std::string> uePmString {};

  for (auto ue : ueMap)
  {
    uint64_t imsi = ue.second->GetImsi();
    std::string rnti = std::to_string(ue.second->GetRnti());
    std::string ueImsiComplete = GetImsiString(imsi);

    Ptr<MeasurementItemList> ueVal = Create<MeasurementItemList> (ueImsiComplete);

    long numDrb = ue.second->GetDrbMap().size();

    if (!m_reducedPmValues)
      {    
        ueVal->AddItem<long> ("DRB.EstabSucc.5QI.UEID", numDrb);
        ueVal->AddItem<long> ("DRB.RelActNbr.5QI.UEID", 0); // not modeled in the simulator
      } 

    // create L3 RRC reports

    // for the same cell
    double sinrThisCell = 10 * std::log10 (m_l3sinrMap[imsi][m_cellId]);
    double convertedSinr = L3RrcMeasurements::ThreeGppMapSinr (sinrThisCell);

    Ptr<L3RrcMeasurements> l3RrcMeasurementServing;
    if (!indicationMessageHelper->IsOffline ())
      {
        l3RrcMeasurementServing =
            L3RrcMeasurements::CreateL3RrcUeSpecificSinrServing (m_cellId, m_cellId, convertedSinr);
      }
    NS_LOG_DEBUG (Simulator::Now ().GetSeconds ()
                  << " enbdev " << m_cellId << " UE " << imsi << " L3 serving SINR " << sinrThisCell
                  << " L3 serving SINR 3gpp " << convertedSinr);

    std::string servingStr = std::to_string (numDrb) + "," + std::to_string (0) + "," +
                             std::to_string (m_cellId) + "," + std::to_string (imsi) + "," +
                             std::to_string (sinrThisCell) + "," + std::to_string (convertedSinr);
    ue_cell_sinr[imsi] = convertedSinr;
    // For the neighbors
    // TODO create double map, imsi -> cell -> sinr
    // TODO store at most 8 reports for each UE, as per the standard

    double sinr;
    std::string neighStr;
    std::string neighbor_str = "[";

    //invert key and value in sortFlipMap, then sort by value
    std::multimap<long double, uint16_t> sortFlipMap = flip_map (m_l3sinrMap[imsi]);
    //new sortFlipMap structure sortFlipMap < sinr, cellId >
    //The assumption is that the first cell in the scenario is always LTE and the rest NR
    uint16_t nNeighbours = E2SM_REPORT_MAX_NEIGH;
    if (m_l3sinrMap[imsi].size () < nNeighbours)
      {
        nNeighbours = m_l3sinrMap[imsi].size () - 1;
      }
    int itIndex = 0;
    // Save only the first E2SM_REPORT_MAX_NEIGH SINR for each UE which represent the best values among all the SINRs detected by all the cells
    for (std::map<long double, uint16_t>::iterator it = --sortFlipMap.end ();
         it != --sortFlipMap.begin () && itIndex < nNeighbours; it--)
      {
        uint16_t cellId = it->second;
        if (cellId != m_cellId)
          {
            sinr = 10 * std::log10 (it->first); // now SINR is a key due to the sort of the map
            convertedSinr = L3RrcMeasurements::ThreeGppMapSinr (sinr);
            NS_LOG_DEBUG (Simulator::Now ().GetSeconds ()
                          << " enbdev " << m_cellId << " UE " << imsi << " L3 neigh " << cellId
                          << " SINR " << sinr << " sinr encoded " << convertedSinr
                          << " first insert");
            neighStr += "," + std::to_string (cellId) + "," + std::to_string (sinr) + "," +
                        std::to_string (convertedSinr);
            itIndex++;
            neighbor_str += get_neigh_str(neighbor_str, cellId, sinr);
            ue_nbcell_id[imsi].push_back(cellId);
            ue_nbcell_sinr[imsi].push_back(convertedSinr);
          }
          if (it != --sortFlipMap.begin ())
          {
            neighbor_str += ",";
          }
      }
      for (int i =nNeighbours; i< E2SM_REPORT_MAX_NEIGH; i++){
        neighStr += ",,,";
      }
    

    uePmString.insert (std::make_pair (imsi, servingStr + neighStr));
    //connlab
    if (!m_forceE2FileLogging)
    {
      uint8_t *serving_buf = NULL, *neighbor_buf = NULL;
      get_serving_value(serving_buf, sinrThisCell);
      get_neigh_value(neighbor_buf, neighbor_str);
      indicationMessageHelper->FillCuCpPmValues (plmId, gnbId, rnti, serving_buf, neighbor_buf);
    }
  }

  if (m_forceE2FileLogging)
    {
      std::ofstream csv{};
      csv.open (m_cuCpFileName.c_str (), std::ios_base::app);
      if (!csv.is_open ())
        {
          NS_FATAL_ERROR ("Can't open file " << m_cuCpFileName.c_str ());
        }

      //NS_LOG_DEBUG ("m_cuCpFileName open " << m_cuCpFileName);

      // the string is timestamp, ueImsiComplete, numActiveUes, DRB.EstabSucc.5QI.UEID (numDrb), DRB.RelActNbr.5QI.UEID (0), L3 serving Id (m_cellId), UE (imsi), L3 serving SINR, L3 serving SINR 3gpp, L3 neigh Id (cellId), L3 neigh Sinr, L3 neigh SINR 3gpp (convertedSinr)
      // The values for L3 neighbour cells are repeated for each neighbour (7 times in this implementation)

      uint64_t timestamp = m_startTime + (uint64_t) Simulator::Now ().GetMilliSeconds ();

      for (auto ue : ueMap)
        {
          uint64_t imsi = ue.second->GetImsi ();
          std::string ueImsiComplete = GetImsiString (imsi);

          auto uePms = uePmString.find (imsi)->second;

          std::string to_print = std::to_string (timestamp) + "," + ueImsiComplete + "," +
                                 std::to_string (ueMap.size()) + "," + uePms + "\n";

          //NS_LOG_DEBUG (to_print);

          csv << to_print;
        }
      csv.close ();
      return nullptr;
    }
  else
  {
    //connlab
    indicationMessageHelper->FillCuCpCellValues (gnbId, ueMap.size ());
    return indicationMessageHelper;
  }
}


uint32_t 
MmWaveEnbNetDevice::GetRlcBufferOccupancy(Ptr<LteRlc> rlc) const
{
  if (DynamicCast<LteRlcAm>(rlc) != 0)
  {
    return DynamicCast<LteRlcAm>(rlc)->GetTxBufferSize(); 
  }
  else if(DynamicCast<LteRlcUm>(rlc) != 0)
  {
    return DynamicCast<LteRlcUm>(rlc)->GetTxBufferSize(); 
  }
  else if(DynamicCast<LteRlcUmLowLat>(rlc) != 0)
  {
    return DynamicCast<LteRlcUmLowLat>(rlc)->GetTxBufferSize(); 
  }
  else 
  {
    return 0;
  }
}


Ptr<MmWaveIndicationMessageHelper>
MmWaveEnbNetDevice::BuildRicIndicationMessageDu(std::string plmId, uint16_t nrCellId, std::string gnbId)
{
  Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
      Create<MmWaveIndicationMessageHelper> (IndicationMessageHelper::IndicationMessageType::Du,
                                             m_forceE2FileLogging, m_reducedPmValues);
  
  auto ueMap = m_rrc->GetUeMap();

  uint32_t macPduCellSpecific = 0;
  uint32_t macPduInitialCellSpecific = 0;
  uint32_t macVolumeCellSpecific = 0;
  uint32_t macQpskCellSpecific = 0;
  uint32_t mac16QamCellSpecific = 0;
  uint32_t mac64QamCellSpecific = 0;
  uint32_t macRetxCellSpecific = 0;
  uint32_t macMac04CellSpecific = 0;
  uint32_t macMac59CellSpecific = 0;
  uint32_t macMac1014CellSpecific = 0;
  uint32_t macMac1519CellSpecific = 0;
  uint32_t macMac2024CellSpecific = 0;
  uint32_t macMac2529CellSpecific = 0;

  uint32_t macSinrBin1CellSpecific = 0;
  uint32_t macSinrBin2CellSpecific = 0;
  uint32_t macSinrBin3CellSpecific = 0;
  uint32_t macSinrBin4CellSpecific = 0;
  uint32_t macSinrBin5CellSpecific = 0;
  uint32_t macSinrBin6CellSpecific = 0;
  uint32_t macSinrBin7CellSpecific = 0;
  
  uint32_t rlcBufferOccupCellSpecific = 0;

  uint32_t macPrbsCellSpecific = 0;

  std::unordered_map<uint64_t, std::string> uePmStringDu {};
  
  for (auto ue : ueMap)
  {
    uint64_t imsi = ue.second->GetImsi();
    std::string ueImsiComplete = GetImsiString(imsi);
    uint16_t rnti = ue.second->GetRnti();
    std::string rnti_str = std::to_string(rnti);

    uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId);

    macPduCellSpecific += macPduUe;
 
    uint32_t macPduInitialUe = m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId);
    macPduInitialCellSpecific += macPduInitialUe;

    uint32_t macVolume = m_e2DuCalculator->GetMacVolumeUeSpecific(rnti, m_cellId);
    macVolumeCellSpecific += macVolume;

    uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId);
    macQpskCellSpecific += macQpsk;

    uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId);
    mac16QamCellSpecific += mac16Qam;

    uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId);
    mac64QamCellSpecific += mac64Qam;

    uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId);
    macRetxCellSpecific += macRetx;

    // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID and UE ID within a given time window)
    double macNumberOfSymbols = m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, m_cellId);
    
    auto phyMac = GetMac()->GetConfigurationParameters();
    // Denominator = (Periodicity of the report time window in ms*number of TTIs per ms*14)
    Time reportingWindow = Simulator::Now () - m_e2DuCalculator->GetLastResetTime (rnti, m_cellId);
    double denominatorPrb = std::ceil (reportingWindow.GetNanoSeconds () / phyMac->GetSlotPeriod ().GetNanoSeconds ()) * 14; 
    
    NS_LOG_DEBUG ("macNumberOfSymbols " << macNumberOfSymbols << " denominatorPrb " << denominatorPrb);

    // Average Number of PRBs allocated for the UE = (NR/DR)*139 (where 139 is the total number of PRBs available per NR cell, given numerology 2 with 60 kHz SCS)
    double macPrb = 0;
    if (denominatorPrb != 0)
    {
      macPrb = macNumberOfSymbols / denominatorPrb * 139; // TODO fix this for different numerologies
    }
    macPrbsCellSpecific += macPrb;

    uint32_t macMac04 = m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, m_cellId);
    macMac04CellSpecific += macMac04;

    uint32_t macMac59 = m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, m_cellId);
    macMac59CellSpecific += macMac59;

    uint32_t macMac1014 = m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, m_cellId);
    macMac1014CellSpecific += macMac1014;

    uint32_t macMac1519 = m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, m_cellId);
    macMac1519CellSpecific += macMac1519;

    uint32_t macMac2024 = m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, m_cellId);
    macMac2024CellSpecific += macMac2024;

    uint32_t macMac2529 = m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, m_cellId);
    macMac2529CellSpecific += macMac2529;

    uint32_t macSinrBin1 = m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, m_cellId);
    macSinrBin1CellSpecific += macSinrBin1;

    uint32_t macSinrBin2 = m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, m_cellId);
    macSinrBin2CellSpecific += macSinrBin2;

    uint32_t macSinrBin3 = m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, m_cellId);
    macSinrBin3CellSpecific += macSinrBin3;

    uint32_t macSinrBin4 = m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, m_cellId);
    macSinrBin4CellSpecific += macSinrBin4;

    uint32_t macSinrBin5 = m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, m_cellId);
    macSinrBin5CellSpecific += macSinrBin5;

    uint32_t macSinrBin6 = m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, m_cellId);
    macSinrBin6CellSpecific += macSinrBin6;

    uint32_t macSinrBin7 = m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, m_cellId);
    macSinrBin7CellSpecific += macSinrBin7;
    
    // get buffer occupancy info
    uint32_t rlcBufferOccup = 0;
    auto drbMap = ue.second->GetDrbMap();
    for (auto drb : drbMap)
    {
      auto rlc = drb.second->m_rlc;
      rlcBufferOccup += GetRlcBufferOccupancy(rlc);
    }
    auto rlcMap = ue.second->GetRlcMap(); // secondary-connected RLCs
    for (auto drb : rlcMap)
    {
      auto rlc = drb.second->m_rlc;
      rlcBufferOccup += GetRlcBufferOccupancy(rlc);
    }
    rlcBufferOccupCellSpecific += rlcBufferOccup;

    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << m_cellId << " cell, connected UE with IMSI " << imsi 
        << " rnti " << rnti     
      << " macPduUe " << macPduUe
      << " macPduInitialUe " << macPduInitialUe
      << " macVolume " << macVolume 
      << " macQpsk " << macQpsk
      << " mac16Qam " << mac16Qam
      << " mac64Qam " << mac64Qam
      << " macRetx " << macRetx
      << " macPrb " << macPrb
      << " macMac04 " << macMac04
      << " macMac59 " << macMac59
      << " macMac1014 " << macMac1014
      << " macMac1519 " << macMac1519
      << " macMac2024 " << macMac2024
      << " macMac2529 " << macMac2529
      << " macSinrBin1 " << macSinrBin1
      << " macSinrBin2 " << macSinrBin2
      << " macSinrBin3 " << macSinrBin3
      << " macSinrBin4 " << macSinrBin4
      << " macSinrBin5 " << macSinrBin5
      << " macSinrBin6 " << macSinrBin6
      << " macSinrBin7 " << macSinrBin7
      << " rlcBufferOccup " << rlcBufferOccup
    );

    // UE-specific Downlink IP combined EN-DC throughput from LTE eNB. Unit is kbps. Pdcp based computation
    // This value is not requested anymore, so it has been removed from the delivery, but it will be still logged;
    double drbThrDlPdcpBasedUeid = m_drbThrDlPdcpBasedComputationUeid.find (imsi) != m_drbThrDlPdcpBasedComputationUeid.end () ? m_drbThrDlPdcpBasedComputationUeid.at (imsi) : 0;    

    // UE-specific Downlink IP combined EN-DC throughput from LTE eNB. Unit is kbps. Rlc based computation
    double drbThrDlUeid = m_drbThrDlUeid.find (imsi) != m_drbThrDlUeid.end () ? m_drbThrDlUeid.at (imsi) : 0;

    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << m_cellId << " cell, connected UE with IMSI " << imsi 
            << " rnti " << rnti     
            << " macSinrBin1 " << macSinrBin1
            << " macSinrBin2 " << macSinrBin2
            << " macSinrBin3 " << macSinrBin3
            << " macSinrBin4 " << macSinrBin4
            << " macSinrBin5 " << macSinrBin5
            << " macSinrBin6 " << macSinrBin6
            << " macSinrBin7 " << macSinrBin7
            << " throughput pdcp " << drbThrDlPdcpBasedUeid
            << " throughput ueid " << drbThrDlUeid
        );


    uePmStringDu.insert(std::make_pair(imsi, std::to_string(macPduUe) + ","
                                            + std::to_string(macPduInitialUe)+","
                                            + std::to_string(macQpsk)+","
                                            + std::to_string(mac16Qam)+","
                                            + std::to_string(mac64Qam)+","
                                            + std::to_string(macRetx)+","
                                            + std::to_string(macVolume)+","
                                            + std::to_string(macPrb)+","
                                            + std::to_string(macMac04)+","
                                            + std::to_string(macMac59)+","
                                            + std::to_string(macMac1014)+","
                                            + std::to_string(macMac1519)+","
                                            + std::to_string(macMac2024)+","
                                            + std::to_string(macMac2529)+","
                                            + std::to_string(macSinrBin1)+","
                                            + std::to_string(macSinrBin2)+","
                                            + std::to_string(macSinrBin3)+","
                                            + std::to_string(macSinrBin4)+","
                                            + std::to_string(macSinrBin5)+","
                                            + std::to_string(macSinrBin6)+","
                                            + std::to_string(macSinrBin7)+","
                                            + std::to_string(rlcBufferOccup)+','
                                            + std::to_string(drbThrDlUeid)+','
                                            + std::to_string(drbThrDlPdcpBasedUeid)
                                            ));
    
    // reset UE
    m_e2DuCalculator->ResetPhyTracesForRntiCellId(rnti, m_cellId);
    //connlab
    long dlAvailablePrbs = 139;
    long prb_usage_dl = std::min ((long) (macPrb / dlAvailablePrbs * 100), (long) 100);
    ue_prb_usage[imsi] = prb_usage_dl;
    if (!m_forceE2FileLogging) {
      indicationMessageHelper->FillDuPmValues (plmId, gnbId, rnti_str, prb_usage_dl, 0);
    }
    /*E2AP_PDU *pdu_du_ue = new E2AP_PDU; 
    indicationMessageHelper->FillDuValues (plmId, gnbId, rnti_str, prb_usage_dl, 0, 
                                              header_buf, header_size, pdu_du_ue, 
                                              params.requestorId, params.instanceId, params.ranFuncionId, params.actionId);
    NS_LOG_DEBUG ("Send NR DU");
    m_e2term->SendE2Message (pdu_du_ue);
    delete pdu_du_ue;*/
  }
  m_drbThrDlPdcpBasedComputationUeid.clear ();
  m_drbThrDlUeid.clear ();

  // Denominator = (Total number of rows (TTIs) within a given time window* 14)
  // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID within a given time window) * 139
  // Average Number of PRBs allocated for the UE = (NR/DR) (where 139 is the total number of PRBs available per NR cell, given numerology 2 with 60 kHz SCS)
  double prbUtilizationDl = macPrbsCellSpecific;

  /*NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << m_cellId << " cell, connected UEs number " << ueMap.size() 
      << " macPduCellSpecific " << macPduCellSpecific
      << " macPduInitialCellSpecific " << macPduInitialCellSpecific
      << " macVolumeCellSpecific " << macVolumeCellSpecific 
      << " macQpskCellSpecific " << macQpskCellSpecific
      << " mac16QamCellSpecific " << mac16QamCellSpecific
      << " mac64QamCellSpecific " << mac64QamCellSpecific
      << " macRetxCellSpecific " << macRetxCellSpecific
      << " macPrbsCellSpecific " << macPrbsCellSpecific //<< " " << macNumberOfSymbolsCellSpecific << " " << denominatorPrb
      << " macMac04CellSpecific " << macMac04CellSpecific
      << " macMac59CellSpecific " << macMac59CellSpecific
      << " macMac1014CellSpecific " << macMac1014CellSpecific
      << " macMac1519CellSpecific " << macMac1519CellSpecific
      << " macMac2024CellSpecific " << macMac2024CellSpecific
      << " macMac2529CellSpecific " << macMac2529CellSpecific
      << " macSinrBin1CellSpecific " << macSinrBin1CellSpecific
      << " macSinrBin2CellSpecific " << macSinrBin2CellSpecific
      << " macSinrBin3CellSpecific " << macSinrBin3CellSpecific
      << " macSinrBin4CellSpecific " << macSinrBin4CellSpecific
      << " macSinrBin5CellSpecific " << macSinrBin5CellSpecific
      << " macSinrBin6CellSpecific " << macSinrBin6CellSpecific
      << " macSinrBin7CellSpecific " << macSinrBin7CellSpecific
    );*/

  long dlAvailablePrbs = 139; // TODO this is for the current configuration, make it configurable
  long ulAvailablePrbs = 139; // TODO this is for the current configuration, make it configurable
  long qci = 1;
  long dlPrbUsage =  std::min ((long) (prbUtilizationDl / dlAvailablePrbs * 100), (long) 100); // percentage of used PRBs    
  long ulPrbUsage = 0; // TODO for future implementation
  cell_ava_prb_ul = ulAvailablePrbs;
  cell_ava_prb_dl = dlAvailablePrbs;

  if (m_forceE2FileLogging) {
    std::ofstream csv {};
    csv.open (m_duFileName.c_str (),  std::ios_base::app);
    if (!csv.is_open ())
    {
      NS_FATAL_ERROR ("Can't open file " << m_duFileName.c_str ());
    }

    uint64_t timestamp = m_startTime + (uint64_t) Simulator::Now().GetMilliSeconds ();

    
    // the string is timestamp, ueImsiComplete, plmId, nrCellId, dlAvailablePrbs, ulAvailablePrbs, qci , dlPrbUsage, ulPrbUsage, /*CellSpecificValues*/, /* UESpecificValues */

    /*
      CellSpecificValues: 
        TB.TotNbrDl.1, TB.TotNbrDlInitial, TB.TotNbrDlInitial.Qpsk, TB.TotNbrDlInitial.16Qam, TB.TotNbrDlInitial.64Qam, RRU.PrbUsedDl,
        TB.ErrTotalNbrDl.1, QosFlow.PdcpPduVolumeDL_Filter, CARR.PDSCHMCSDist.Bin1, CARR.PDSCHMCSDist.Bin2, CARR.PDSCHMCSDist.Bin3,
        CARR.PDSCHMCSDist.Bin4, CARR.PDSCHMCSDist.Bin5, CARR.PDSCHMCSDist.Bin6, L1M.RS-SINR.Bin34, L1M.RS-SINR.Bin46, L1M.RS-SINR.Bin58,
        L1M.RS-SINR.Bin70, L1M.RS-SINR.Bin82, L1M.RS-SINR.Bin94, L1M.RS-SINR.Bin127, DRB.BufferSize.Qos, DRB.MeanActiveUeDl
    */

    std::string to_print_cell = plmId + "," 
        + std::to_string (nrCellId) + "," 
        + std::to_string (dlAvailablePrbs) + "," 
        + std::to_string (ulAvailablePrbs) + "," 
        + std::to_string (qci) + "," 
        + std::to_string (dlPrbUsage) + "," 
        + std::to_string (ulPrbUsage) + "," 
        + std::to_string (macPduCellSpecific) + "," 
        + std::to_string (macPduInitialCellSpecific) + "," 
        + std::to_string (macQpskCellSpecific) + "," 
        + std::to_string (mac16QamCellSpecific) + "," 
        + std::to_string (mac64QamCellSpecific) + ","
        + std::to_string ((long) std::ceil (prbUtilizationDl)) + "," 
        + std::to_string (macRetxCellSpecific) + "," 
        + std::to_string (macVolumeCellSpecific) + "," 
        + std::to_string (macMac04CellSpecific) + "," 
        + std::to_string (macMac59CellSpecific) + "," 
        + std::to_string (macMac1014CellSpecific) + "," 
        + std::to_string (macMac1519CellSpecific) + "," 
        + std::to_string (macMac2024CellSpecific) + "," 
        + std::to_string (macMac2529CellSpecific) + "," 
        + std::to_string (macSinrBin1CellSpecific) + "," 
        + std::to_string (macSinrBin2CellSpecific) + "," 
        + std::to_string (macSinrBin3CellSpecific) + "," 
        + std::to_string (macSinrBin4CellSpecific) + "," 
        + std::to_string (macSinrBin5CellSpecific) + "," 
        + std::to_string (macSinrBin6CellSpecific) + "," 
        + std::to_string (macSinrBin7CellSpecific) + "," 
        + std::to_string (rlcBufferOccupCellSpecific) + "," 
        + std::to_string (ueMap.size ());

    /* 
      UESpecificValues:

          TB.TotNbrDl.1.UEID, TB.TotNbrDlInitial.UEID, TB.TotNbrDlInitial.Qpsk.UEID, TB.TotNbrDlInitial.16Qam.UEID,TB.TotNbrDlInitial.64Qam.UEID, TB.ErrTotalNbrDl.1.UEID, QosFlow.PdcpPduVolumeDL_Filter.UEID,
          RRU.PrbUsedDl.UEID, CARR.PDSCHMCSDist.Bin1.UEID, CARR.PDSCHMCSDist.Bin2.UEID, CARR.PDSCHMCSDist.Bin3.UEID, CARR.PDSCHMCSDist.Bin5.UEID, CARR.PDSCHMCSDist.Bin6.UEID, 
          L1M.RS-SINR.Bin34.UEID, L1M.RS-SINR.Bin46.UEID, L1M.RS-SINR.Bin58.UEID, L1M.RS-SINR.Bin70.UEID, L1M.RS-SINR.Bin82.UEID, L1M.RS-SINR.Bin94.UEID, L1M.RS-SINR.Bin127.UEID, 
          DRB.BufferSize.Qos.UEID, DRB.UEThpDl.UEID, DRB.UEThpDlPdcpBased.UEID
    */

    for (auto ue : ueMap)
    {
      uint64_t imsi = ue.second->GetImsi();
      std::string ueImsiComplete = GetImsiString(imsi);

      auto uePms = uePmStringDu.find(imsi)->second;

      std::string to_print = std::to_string (timestamp) + ","
      + ueImsiComplete + "," 
      + to_print_cell +  ","
      + uePms + "\n";

      csv << to_print;
    }
    csv.close ();
    return nullptr;
  }
  else
  {
    //connlab
    uint8_t *sst_buf = (uint8_t*)"1";
    uint8_t *sd_buf = (uint8_t*)"100";
	  long *l_dl_prbs = (long*)calloc(1, sizeof(long));
	  long *l_ul_prbs = (long*)calloc(1, sizeof(long));
	  *l_dl_prbs = (long)dlAvailablePrbs;
	  *l_ul_prbs = (long)ulAvailablePrbs;
    
    indicationMessageHelper->FillDuCellValues(plmId, gnbId, qci, dlPrbUsage, ulPrbUsage, sst_buf,sd_buf, l_dl_prbs, l_ul_prbs);
    return indicationMessageHelper; 
  }
}


void
MmWaveEnbNetDevice::BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params)
{

  std::string plmId = "111";
  std::string gnbId = std::to_string(m_cellId);
  //m_cellId = nrcellid
  // TODO here we can get something from RRC and onward
  NS_LOG_DEBUG("MmWaveEnbNetDevice " << m_cellId << " BuildAndSendMessage at time " << Simulator::Now().GetSeconds());
  
  auto ueMap = m_rrc->GetUeMap();
  NS_LOG_UNCOND("--------------------------------------------------");
  NS_LOG_UNCOND("NR UE num: " << ueMap.size() << " Cell id: " << m_cellId);
  NS_LOG_UNCOND("NR Cuup: " << m_sendCuUp << " Cucp: " << m_sendCuCp << " Du: " << m_sendDu);
  for (auto ue : ueMap)
  {
    uint64_t imsi = ue.second->GetImsi();
    //std::string ueImsiComplete = GetImsiString(imsi);
    NS_LOG_UNCOND("NR UE IMSI: " << imsi);
  }
  NS_LOG_UNCOND("--------------------------------------------------");
  if(m_sendCuUp)
  {
    // Create CU-UP
    Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper = BuildRicIndicationMessageCuUp(plmId, gnbId);
    // Send DU only if offline logging is disabled
    NS_LOG_UNCOND("CU-UP KPM Data");
    if (!m_forceE2FileLogging && header != nullptr && indicationMessageHelper != nullptr)
    {
      E2AP_PDU *pdu_cuup_ue = new E2AP_PDU; 
      indicationMessageHelper->EncodeIndication ((uint8_t*) header->m_buffer, header->m_size, pdu_cuup_ue, 
                                              params.requestorId, params.instanceId, params.ranFuncionId, params.actionId);
      NS_LOG_DEBUG ("Send NR CU-UP");
      m_e2term->SendE2Message (pdu_cuup_ue);
      delete pdu_cuup_ue;        
    }
  }

  if(m_sendCuCp)
  {
    // Create and send CU-CP
    Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper = BuildRicIndicationMessageCuCp(plmId, gnbId);
    // Send DU only if offline logging is disabled
    NS_LOG_UNCOND("CU-CP KPM Data");
    if (!m_forceE2FileLogging && header != nullptr && indicationMessageHelper != nullptr)
    {
      E2AP_PDU *pdu_cuup_ue = new E2AP_PDU; 
      indicationMessageHelper->EncodeIndication ((uint8_t*) header->m_buffer, header->m_size, pdu_cuup_ue, 
                                              params.requestorId, params.instanceId, params.ranFuncionId, params.actionId);
      NS_LOG_DEBUG ("Send NR CU-CP");
      m_e2term->SendE2Message (pdu_cuup_ue);
      delete pdu_cuup_ue;        
    }
  }

  if(m_sendDu)
  {
    // Create DU
    Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
    //params, (uint8_t*) header->m_buffer, header->m_size
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper = BuildRicIndicationMessageDu(plmId, m_cellId, gnbId);
    // Send DU only if offline logging is disabled
    NS_LOG_UNCOND("DU KPM Data");
    if (!m_forceE2FileLogging && header != nullptr && indicationMessageHelper != nullptr)
    {
      E2AP_PDU *pdu_cuup_ue = new E2AP_PDU; 
      indicationMessageHelper->EncodeIndication ((uint8_t*) header->m_buffer, header->m_size, pdu_cuup_ue, 
                                              params.requestorId, params.instanceId, params.ranFuncionId, params.actionId);
      NS_LOG_DEBUG ("Send NR DU");
      m_e2term->SendE2Message (pdu_cuup_ue);
      delete pdu_cuup_ue;        
    }
  }

  NS_LOG_UNCOND("UE Report");
  influxdb_cpp::server_info si(influx_ip, 8086, "UEData", "admin", "1234");
  for (auto it = ue_nbcell_id.begin(); it != ue_nbcell_id.end(); it++)
  {
    uint64_t imsi = it->first;
    NS_LOG_UNCOND ("UE IMSI: " << imsi 
                    << ", PDCP DL Throughput: " << ue_pdcp_dl[imsi] 
                    << ", PDCP UL Throughput: " << ue_pdcp_ul[imsi]);
    
    NS_LOG_UNCOND ("Serving Cell: " << m_cellId << " Sinr: " << ue_cell_sinr[imsi] << " Prb Usage: " << ue_prb_usage[imsi]);
    NS_LOG_UNCOND ("Nb Cell 1: " << ue_nbcell_id[imsi][0] << " Sinr: " << ue_nbcell_sinr[imsi][0]);
    NS_LOG_UNCOND ("Nb Cell 2: " << ue_nbcell_id[imsi][1] << " Sinr: " << ue_nbcell_sinr[imsi][1]);
    NS_LOG_UNCOND ("Nb Cell 3: " << ue_nbcell_id[imsi][2] << " Sinr: " << ue_nbcell_sinr[imsi][2]);
    NS_LOG_UNCOND ("Nb Cell 4: " << ue_nbcell_id[imsi][3] << " Sinr: " << ue_nbcell_sinr[imsi][3]);
    NS_LOG_UNCOND ("Nb Cell 5: " << ue_nbcell_id[imsi][4] << " Sinr: " << ue_nbcell_sinr[imsi][4]);
    
    
    influxdb_cpp::builder()
    .meas("detector")
    .field("ue-id", std::to_string(imsi))
    .field("pdcp_dl", ue_pdcp_dl[imsi])
    .post_http(si);
    
    if(std::to_string(ue_nbcell_id[imsi][3]) == std::to_string(6)){
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<> disttribe(97,150);
     int random_number=disttribe(gen);
     ue_nbcell_sinr[imsi][0] = random_number;
    }
    
    
    
    
    srand(static_cast<unsigned int>(time(0)));
    int anomaly=rand()%2;
    
    int ue_x,ue_y;
    int w1=(static_cast<int>(ue_cell_sinr[imsi]))*0.1;
    int w2=(static_cast<int>(ue_nbcell_sinr[imsi][0]))*0.1;
    int w3=(static_cast<int>(ue_nbcell_sinr[imsi][1]))*0.1;
    int x1,x2,x3;
    int y1,y2,y3;
    
    if(std::stoi(gnbId)==2)
    {
      x1=2000;
      y1=2000;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==2)
    {
      x2=2000;
      y2=2000;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==2)
    {
      x3=2000;
      y3=2000;
    }

    if(std::stoi(gnbId)==3)
    {
      x1=3000;
      y1=2000;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==3)
    {
      x2=3000;
      y2=2000;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==3)
    {
      x3=3000;
      y3=2000;
    }

    if(std::stoi(gnbId)==4)
    {
      x1=2309;
      y1=2951;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==4)
    {
      x2=2309;
      y2=2951;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==4)
    {
      x3=2309;
      y3=2951;
    }

    if(std::stoi(gnbId)==5)
    {
      x1=1190;
      y1=2587;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==5)
    {
      x2=1190;
      y2=2587;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==5)
    {
      x3=1190;
      y3=2587;
    }

    if(std::stoi(gnbId)==6)
    {
      x1=1190;
      y1=1412;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==6)
    {
      x2=1190;
      y2=1412;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==6)
    {
      x3=1190;
      y3=1412;
    }

    if(std::stoi(gnbId)==7)
    {
      x1=2309;
      y1=1048;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][0]))==7)
    {
      x2=2309;
      y2=1048;
    }
    else if(std::stoi(std::to_string(ue_nbcell_id[imsi][1]))==7)
    {
      x3=2309;
      y3=1048;
    }
    ue_x=(w1*x1+w2*x2+w3*x3)/(w1+w2+w3);
    ue_y=(w1*y1+w2*y2+w3*y3)/(w1+w2+w3);




    influxdb_cpp::builder()
    .meas("liveUE")
    .field("nrCellIdentity", gnbId)
    .field("nbCellIdentity_0", std::to_string(ue_nbcell_id[imsi][0]))
    .field("nbCellIdentity_1", std::to_string(ue_nbcell_id[imsi][1]))
    .field("nbCellIdentity_2", std::to_string(ue_nbcell_id[imsi][2]))
    .field("nbCellIdentity_3", std::to_string(ue_nbcell_id[imsi][3]))
    .field("nbCellIdentity_4", std::to_string(ue_nbcell_id[imsi][4]))
    .field("du-id", m_cellId)
    .field("ue-id", std::to_string(imsi))
    .field("prb_usage", ue_prb_usage[imsi])
    .field("PDCP.UEThpDl",ue_pdcp_dl[imsi])
    .field("RRU.PrbUsedDl",0)
    .field("x",ue_x)
    .field("y",ue_y)
    .field("RF.serving.RSRP",0)
    .field("RF.serving.RSRQ",0)
    .field("RF.serving.RSSINR", ue_cell_sinr[imsi])
    .field("rsrp_nb0",0)
    .field("rsrp_nb1",0)
    .field("rsrp_nb2",0)
    .field("rsrp_nb3",0)
    .field("rsrp_nb4",0)
    .field("rsrq_nb0",0)
    .field("rsrq_nb1",0)
    .field("rsrq_nb2",0)
    .field("rsrq_nb3",0)
    .field("rsrq_nb4",0)
    .field("rssinr_nb_0", ue_nbcell_sinr[imsi][0])
    .field("rssinr_nb_1", ue_nbcell_sinr[imsi][1])
    .field("rssinr_nb_2", ue_nbcell_sinr[imsi][2])
    .field("rssinr_nb_3", ue_nbcell_sinr[imsi][3])
    .field("rssinr_nb_4", ue_nbcell_sinr[imsi][4])
    .field("Viavi.UE.anomalies",anomaly)
    .field("targetTput", 0.25)
    .post_http(si);
    
    //NS_LOG_UNCOND("--------------------------------------------------");
    ue_nbcell_id[imsi].clear();
    ue_nbcell_sinr[imsi].clear();
  }
  ue_nbcell_id.clear();
  ue_nbcell_sinr.clear();
  ue_cell_sinr.clear();
  ue_prb_usage.clear();
  ue_pdcp_dl.clear();
  ue_pdcp_ul.clear();
  
  double gnb_x,gnb_y;
    if (std::stoi(gnbId)==2)
    {
      gnb_x=2000;
      gnb_y=2000;
    }
    else if (std::stoi(gnbId)==3)
    {
      gnb_x=3000;
      gnb_y=2000;
    }
    else if (std::stoi(gnbId)==4)
    {
      gnb_x=2309.02;
      gnb_y=2951.06;
    }
    else if(std::stoi(gnbId)==5)
    {
      gnb_x=1190.98;
      gnb_y=2587.79;
    }
    else if(std::stoi(gnbId)==6)
    {
      gnb_x=1190.98;
      gnb_y=1412.21;
    }
    else if(std::stoi(gnbId)==7)
    {
      gnb_x=2309.02;
      gnb_y=1048.94;
    } 
    else{
      gnb_x=0;
      gnb_y=0;
    }
    
  
  influxdb_cpp::builder()
  .meas("liveCell")
  .field("nrCellIdentity", gnbId)
  .field("du-id", m_cellId)
  .field("availPrbDl", cell_ava_prb_dl)
  .field("availPrbUl", cell_ava_prb_ul)
  .field("pdcpBytesDl", cell_pdcp_dl)
  .field("pdcpBytesUl", cell_pdcp_ul)
  .field("measPeriodPdcpBytes", m_e2Periodicity * 100)
  .field("measPeriodPrb", m_e2Periodicity * 100)
  .field("throughput",0)
  .field("x",gnb_x)
  .field("y",gnb_y)
  .post_http(si);

  if (!m_forceE2FileLogging)
    Simulator::ScheduleWithContext (1, Seconds (m_e2Periodicity),
                                    &MmWaveEnbNetDevice::BuildAndSendReportMessage, this, params);
  else
    Simulator::Schedule (Seconds (m_e2Periodicity), &MmWaveEnbNetDevice::BuildAndSendReportMessage,
                         this, params);
}

void
MmWaveEnbNetDevice::SetStartTime (uint64_t st)
{
  m_startTime = st;
}

}
}
