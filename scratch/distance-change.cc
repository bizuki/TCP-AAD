#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/tcp-westwood-plus.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ptr.h"
#include "ns3/stats-module.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/waypoint-mobility-model.h"

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

using namespace ns3;

std::map<Ipv4Address, size_t> adressToInterface;
size_t dataRate = 5;

/**
 * Calculate the throughput
 */
void
CalculateThroughput(Ptr<OutputStreamWrapper> stream, size_t index, Ptr<PacketSink> sink, uint64_t lastTotalRx)
{
    Time now = Simulator::Now();
    double cur = (sink->GetTotalRx() - lastTotalRx) * 8.0;
    *stream->GetStream() << now.GetSeconds() << "\t" << cur << '\t' << index << std::endl;
    Simulator::Schedule(Seconds(1), MakeBoundCallback(&CalculateThroughput, stream, index, sink, sink->GetTotalRx()));
}

void
ChangeChannel(Ptr<Node> node)
{
    auto mobility = node->GetObject<MobilityModel>();
    auto pos = mobility->GetPosition();

    pos.x = (double) (((int) std::round(pos.x) + 200) % 400);
    mobility->SetPosition(pos);
    
    Simulator::Schedule(Seconds(200), MakeBoundCallback(&ChangeChannel, node));
}


// TODO: why such low bandwidth at high delay
int
main(int argc, char* argv[])
{
    uint32_t payloadSize = 1472;                        /* Transport layer payload size in bytes. */
    size_t dataRate = 1;                                /* Application layer datarate. */
    std::string tcpVariant{"TcpNewReno"}; /* TCP variant type. */
    size_t nodes = 1;
    std::string phyRate = "HtMcs7";                     /* Physical layer bitrate. */
    double simulationTime = 10;                         /* Simulation time in seconds. */
    double distanceToAP = 0.0;                          /* Distance to AP stantion from STA node. */
    bool uplink = true;                                 /* Determine the location of server */
    double errorRate = 0.0;                             /* Error rate on wired link */
    size_t lLost = 0;
    size_t cerlWindow = 60;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                tcpVariant);
    cmd.AddValue("nodes", "Number of nodes", nodes);
    cmd.AddValue("distanceToAP", "Distance to AP", distanceToAP);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("errorRate", "Error rate on wired link", errorRate);
    cmd.AddValue("uplink", "Uplink", uplink);
    cmd.AddValue("lLost", "Lost mbps on L", lLost);
    cmd.AddValue("cerlWindow", "Window size of CerlPlusX", cerlWindow);
    
    cmd.Parse(argc, argv);

    tcpVariant = std::string("ns3::") + tcpVariant;

    // Select TCP variant. TODO: support different TCP types.
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                        "TypeId " << tcpVariant << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    TypeIdValue(TypeId::LookupByName(tcpVariant)));

    Config::SetDefault("ns3::TcpCerlX::WindowSize", UintegerValue(cerlWindow));
    
    if (tcpVariant == "ns3::TcpCerl" || tcpVariant == "ns3::TcpCerlPlus" || tcpVariant == "ns3::TcpCerlX") 
    {
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));
    }

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ac);
    
    /* Set up Legacy Channel */
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue(phyRate),
                                       "ControlMode",
                                       StringValue("HtMcs0"));

    /* Create nodes */
    NodeContainer networkNodes;
    networkNodes.Create(nodes * 2 + 2);
    Ptr<Node> g1Node = networkNodes.Get(0);
    Ptr<Node> g2Node = networkNodes.Get(1);

    NodeContainer staNodes;
    NodeContainer appNodes;
    for (size_t i = 0; i < nodes; i++) {
        staNodes.Add(networkNodes.Get(2 + i));
        appNodes.Add(networkNodes.Get(2 + nodes + i));
    }

    /* Configure P2P connections*/
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + ".1Mbps")));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer appDevices;
    NetDeviceContainer uselessDevices;

    for (size_t i = 0; i < nodes; i++) 
    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(appNodes.Get(i));
    
        auto cont = pointToPoint.Install(p2pNodesChannel);
        appDevices.Add(cont.Get(1));
        uselessDevices.Add(cont.Get(0));
    }

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate((std::to_string(dataRate * (nodes - lLost)) + ".5Mbps"))));
    // pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("0.8Mbps")));
    pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("30p")));

    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(g2Node);
    
        NetDeviceContainer g1g2 = pointToPoint.Install(p2pNodesChannel);
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(errorRate));
        em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
        uselessDevices.Add(g1g2);
        g1g2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        // g1g2.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop));
    }    

    /* Configure AP */
    Ssid ssid = Ssid("network");
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, g2Node);

    /* Configure STA */
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    /* Mobility model */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    for (size_t i = 0; i < nodes; i++) 
    {
        // double angle = (M_PI / nodes) * i;
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(g2Node);

    // mobility.SetMobilityModel("ns3::")

    for (size_t i = 0; i < nodes; i++) {
        Ptr<WaypointMobilityModel> waypointModel = CreateObject<WaypointMobilityModel>();
        
        for (size_t time = 100; time <= simulationTime; time += 100) {
            auto remainder = time % 400;
            switch (remainder) {
                case 100:
                case 200: {
                    waypointModel->AddWaypoint(Waypoint(Time(std::to_string(time) + "s"), Vector(remainder, 0, 0)));
                    break;
                }
                case 300:
                case 0: {
                    waypointModel->AddWaypoint(Waypoint(Time(std::to_string(time) + "s"), Vector(400 - remainder, 0, 0)));
                    break;
                }
            }
        }
        auto node = staNodes.Get(i);
        node->AggregateObject(waypointModel);
    }

    /* Internet stack */
    InternetStackHelper stack;
    stack.Install(networkNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer appInterfaces;
    appInterfaces = address.Assign(appDevices);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer uselessInterfaces;
    uselessInterfaces = address.Assign(uselessDevices);

    /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ApplicationContainer sinkApps; 
    ApplicationContainer serverApps;

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("topology.throughput");
    Ptr<OutputStreamWrapper> aggregatedStream = asciiTraceHelper.CreateFileStream("topology-aggregated.throughput");

    Ipv4InterfaceContainer& sinkInterfaces = uplink ? staInterfaces : appInterfaces;
    NodeContainer& serverNodes = uplink ? appNodes : staNodes;
    NodeContainer& sinkNodes = uplink ? staNodes : appNodes;

    for (size_t i = 0; i < nodes; i++) 
    {
        /* Install TCP Receiver on the access point */
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9));
        sinkApps.Add(sinkHelper.Install(sinkNodes.Get(i)));

         /* Install TCP Transmitter on the station */
        OnOffHelper server("ns3::TcpSocketFactory", (InetSocketAddress(sinkInterfaces.GetAddress(i), 9)));
        server.SetAttribute("PacketSize", UintegerValue(payloadSize));
        server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Mbps")));
        serverApps.Add(server.Install(serverNodes.Get(i)));
        Simulator::Schedule(Seconds(1.1), MakeBoundCallback(&CalculateThroughput, stream, i, StaticCast<PacketSink>(sinkApps.Get(i)), 0));
    }

    Simulator::Schedule(Seconds(100), MakeBoundCallback(&ChangeChannel, g2Node));

    sinkApps.Start(Seconds(0.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute ("Min", DoubleValue (1));
    x->SetAttribute ("Max", DoubleValue (1.1));
    serverApps.StartWithJitter(Seconds(0), x);

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    double sum = 0;
    for (size_t i = 0; i < nodes; i++) 
    {
        double averageThroughput = ((StaticCast<PacketSink>(sinkApps.Get(i))->GetTotalRx() * 8) / (1e6 * simulationTime));
        sum += averageThroughput;
        *aggregatedStream->GetStream() << "average: " << i << '\t' << averageThroughput << '\n';
    }

    *aggregatedStream->GetStream() << "average from all: " << sum / nodes << '\n';

    Simulator::Destroy();
    return 0;
}
