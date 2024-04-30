#!/bin/bash
#set -x

enableTraces=1 # enable generation of ns-3 traces 
e2lteEnabled=1 # enable e2 reports from lte macro cell
e2nrEnabled=1 # enable e2 reports from nr secondary cells
e2du=1 # enable reporting of DU PM containers
e2cuUp=1 # enable reporting of CU UP PM containers
e2cuCp=1 # enable reporting of CU CP PM containers
trafficModel=3 # Type of the traffic model at the transport layer [0,3], can generate full buffer traffic (0), half nodes in full buffer and half nodes in bursty (1), bursty traffic (2), mixed setup (3)
configuration=0 # 0: NR carrier at 850 MHz, low traffic | 1: NR carrier at 3.5 GHz, low traffic | 2: NR carrier at 28 GHz, high traffic
minSpeed=2000.0 # minimum UE speed in m/s
maxSpeed=500000.0 # maximum UE speed in m/s
simTime=100 # simulation time
e2TermIp="10.105.41.112" # actual E2term IP interface
ueZeroPercentage=-1 # PDCP split for UE RNTI 0 on eNB

# Useful parameters to be configured
N=1 # number of simulations
basicCellId=1 # The next value will be the first cellId
reducedPmValues=0 # use reduced subset of pmValues
EnableE2FileLogging=1 # enable offline generation of data
ues=1 # Number of UEs for each mmWave ENB
influx_ip="10.244.0.172"

# Select 0 or 1 to switch between the optimized or debug build
build=1
builf_conf=0

if [[ build -eq 0 ]];then
  if [[ build_conf -eq 0 ]];then
    # Debug build
    echo "Build ns-3 in debug mode"
    ./waf configure --build-profile=debug --out=build/debug
  else
      # Optimized build
    echo "Build ns-3 in optimized mode"
      ./waf configure --build-profile=optimized --out=build/optimized
  fi
fi

# Select 0 or 1 to switch between the use cases
# Remember to create an empty version of the control file before the start of this script, otherwise it would lead to premature crashes.
use_case=1
if [[ use_case -eq 0 ]];then
  ## Energy Efficiency use case
  echo "Energy Efficiency use case"
  outageThreshold=-5.0 # use -5.0 when handover is not in NoAuto 
  handoverMode="DynamicTtt"
  indicationPeriodicity=0.02 # value in seconds (20 ms)
  controlPath="ts_actions_for_ns3.csv"
  #controlPath="es_actions_for_ns3.csv" # EE control file path
  numberOfRaPreambles=20
elif [[ use_case -eq 1 ]];then
  ## Traffic Steering use case
  echo "Traffic Steering use case"
  outageThreshold=-5.0
  handoverMode="FixedTtt"
  indicationPeriodicity=0.02 # value in seconds (10 ms)
  controlPath="ts_actions_for_ns3.csv" # TS control file path
  numberOfRaPreambles=20
else
  ## Quality of Service use case
  echo "Quality of Service use case"
  outageThreshold=-5.0 # use -5.0 when handover is not in NoAuto 
  handoverMode="DynamicTtt"
  indicationPeriodicity=0.02 # value in seconds (20 ms)
  # controlPath="qos_actions.csv" # QoS control file path, decomment for control
  # ueZeroPercentage=0.1
  numberOfRaPreambles=40
fi

#  NS_LOG="LteEnbNetDevice:LteEnbRrc:LteUeRrc:McEnbPdcp:McUePdcp" 

for i in $(seq 1 $N); do
  echo "Running simulation $i out of $N";
  ./waf --run "scratch/scenario-one --enableTraces=$enableTraces \
                                    --e2lteEnabled=$e2lteEnabled \
                                    --e2nrEnabled=$e2nrEnabled \
                                    --e2du=$e2du \
                                    --simTime=$simTime \
                                    --outageThreshold=$outageThreshold \
                                    --handoverMode=$handoverMode \
                                    --e2cuUp=$e2cuUp \
                                    --e2cuCp=$e2cuCp \
                                    --reducedPmValues=$reducedPmValues \
                                    --e2TermIp=$e2TermIp \
                                    --enableE2FileLogging=$EnableE2FileLogging \
                                    --numberOfRaPreambles=$numberOfRaPreambles\
                                    --indicationPeriodicity=$indicationPeriodicity\
                                    --controlFileName=$controlPath\
                                    --influxdb_ip=$influx_ip";
  sleep 1;
done
