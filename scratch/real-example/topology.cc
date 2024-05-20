#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/applications-module.h"
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
#include "ns3/tcp-option-ts.h"
#include "ns3/tcp-option-cwnd.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-mac.h"
#include "ns3/amsdu-subframe-header.h"
#include "ns3/ampdu-subframe-header.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ptr.h"

NS_LOG_COMPONENT_DEFINE("real-example");

using namespace ns3;

/**
 * Calculate the throughput
 */
void
CalculateThroughput(Ptr<OutputStreamWrapper> stream, size_t index, Ptr<PacketSink> sink, uint64_t lastTotalRx)
{
    Time now = Simulator::Now();
    double cur = (sink->GetTotalRx() - lastTotalRx) * 8.0;
    *stream->GetStream() << now.GetSeconds() << "\t" << cur << '\t' << index << std::endl;
    Simulator::Schedule(MilliSeconds(100), MakeBoundCallback(&CalculateThroughput, stream, index, sink, sink->GetTotalRx()));
}

double prev;
int cont = 0;
int max = 0;

void
RxOther(Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> pckt, const TcpHeader& header, Ptr<const TcpSocketBase> sock)
{
    if (!header.HasOption(TcpOption::TS)) return;

    // auto delay = (Simulator::Now() - MilliSeconds((header.GetOption(TcpOption::TS)->GetObject<TcpOptionTS>())->GetTimestamp())).GetMilliSeconds();

    double alpha = 0.75;
    double smoothedIat = sock->m_baseIat * alpha + (1 - alpha) * sock->m_iat;

    if (sock->m_iat <= prev)
    {
        cont++;
        max = std::max(max, cont);
    } else
    {
        cont = 0;
    }
    prev = smoothedIat * 3;


    *stream->GetStream() << Simulator::Now().GetSeconds() 
        // << ' ' << delay 
        // << ' ' << lastDelay 
        // << ' ' << sock->m_iat * 1000
        // << ' ' << smoothedIat * 1000
        // << ' ' << max
        // << ' ' << cont
        // << ' ' << sock->GetTimeRatio()
        // << ' ' << sock->GetDelayTimeout().GetSeconds()
        << ' ' << sock->m_delAckCount 
        // << ' ' << sock->DelayWindow()
        // << ' ' << sock->GetDelayTimeout().GetSeconds()
        // << ' ' << (header.GetOption(TcpOption::CWND)->GetObject<TcpOptionCwnd>())->GetCongestionWindow() / sock->GetSegSize() 
        << ' ' << std::endl;
}

void RxDrop(std::string context, Ptr<const Packet> pckt)
{
    // std::cout << Simulator::Now() << "PacketDropped" << std::endl;
}

int
main(int argc, char* argv[])
{
    // LogComponentEnable("TcpCerl", LOG_ALL);
    // LogComponentEnable("TcpCerl", LOG_PREFIX_ALL);
    // LogComponentEnable("TcpSocketBase", LOG_ALL);
    // LogComponentEnable("TcpSocketBase", LOG_PREFIX_ALL);
    // LogComponentEnable("TcpRecoveryOps", LOG_ALL);
    // LogComponentEnable("TcpRecoveryOps", LOG_PREFIX_ALL);


    uint32_t payloadSize = 1472;                        /* Transport layer payload size in bytes. */
    size_t dataRate = 26;                               /* Application layer datarate. */
    std::string tcpVariant{"TcpCubic"};                 /* TCP variant type. */
    size_t tcpNodes = 1;                                /* Number of TCP nodes */
    size_t udpNodes = 0;                                /* Number of udp nodes. */
    std::string phyRate = "HtMcs7";                     /* Physical layer bitrate. */
    double simulationTime = 10;                         /* Simulation time in seconds. */
    double distanceToAP = 10.0;                         /* Distance to AP station from STA node. */
    bool uplink = true;                                 /* Determine the location of server */
    double errorRate = 0;                               /* Error rate on wired link */
    bool dack = false;                                  /* Simulation time in seconds. */
    bool dtime = false;
    bool dtimeLimit = false;
    bool cwndEnabled = false;
    size_t lLost = 0;
    size_t ampdu = 65535;
    size_t amsdu = 0;
    double lambda = 1.5;
    size_t rngSeed = 1;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                tcpVariant);
    cmd.AddValue("tcpNodes", "Number of nodes", tcpNodes);
    cmd.AddValue("udpNodes", "Number of udp nodes", udpNodes);
    cmd.AddValue("distanceToAP", "Distance to AP", distanceToAP);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("errorRate", "Error rate on wired link", errorRate);
    cmd.AddValue("uplink", "Uplink", uplink);
    cmd.AddValue("lLost", "Lost mbps on L", lLost);
    cmd.AddValue("Ampdu", "Ampdu size", ampdu);
    cmd.AddValue("Amsdu", "Amsdu size", amsdu);
    cmd.AddValue("dack", "Use dack", dack);
    cmd.AddValue("dtime", "Dynamic timeout", dtime);
    cmd.AddValue("cwndEnabled", "Enable cwnd option", cwndEnabled);
    cmd.AddValue("dtimeLimit", "Dynamic timeout with upper limit on delayed acks", dtimeLimit);
    cmd.AddValue("lambda", "Lambda", lambda);
    cmd.AddValue("rngSeed", "rng seed", rngSeed);
    
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(rngSeed);

    auto nodes = tcpNodes + udpNodes;

    tcpVariant = std::string("ns3::") + tcpVariant;

    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                        "TypeId " << tcpVariant << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    TypeIdValue(TypeId::LookupByName(tcpVariant)));


    /* Configure TCP Options */
    // NOTE: you need my codebase with dynamic delay implemented to use these.
    if (cwndEnabled)
    {
        Config::SetDefault("ns3::TcpSocketBase::CongestionWindowOption", BooleanValue(true));
    }

    NS_ASSERT(!dack || !dtime);
    if (dack)
    {
        Config::SetDefault("ns3::TcpSocketBase::DynamicDelayWindow", BooleanValue(true));
    }
    if (dtime)
    {
        Config::SetDefault("ns3::TcpSocketBase::DynamicTimeout", BooleanValue(true));
    }
    Config::SetDefault("ns3::TcpSocketBase::Lambda", DoubleValue(lambda));
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
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

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

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate((std::to_string(dataRate * (tcpNodes - lLost)) + ".5Mbps"))));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));
    // Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("30p")));

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
    }    

    /* Configure AP */
    Ssid ssid = Ssid("network");
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, g2Node);
    
    Ptr<WifiNetDevice> wifi_dev;
    wifi_dev = DynamicCast<WifiNetDevice>(apDevice.Get(0));
    wifi_dev->GetMac()->SetAttribute("BE_MaxAmpduSize", UintegerValue(ampdu));
    wifi_dev->GetMac()->SetAttribute("BE_MaxAmsduSize", UintegerValue(amsdu));

    /* Configure STA */
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);
    for (size_t i = 0; i < nodes; i++)
    {
        wifi_dev = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        wifi_dev->GetMac()->SetAttribute("BE_MaxAmpduSize", UintegerValue(ampdu));
        wifi_dev->GetMac()->SetAttribute("BE_MaxAmsduSize", UintegerValue(amsdu));
    }

    /* Mobility model */
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    for (size_t i = 0; i < nodes; i++) 
    {
        double angle = (M_PI / nodes) * i;
        positionAlloc->Add(Vector(sin(angle) * distanceToAP, cos(angle) * distanceToAP, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(g2Node);
    mobility.Install(staNodes);

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


    auto dtimeType = (dtimeLimit ? (cwndEnabled ? "limited-dtime-cwnd" : "limited-dtime") : "dtime");
    auto dackType = (dack ? "dack" : (dtime ? dtimeType : "default"));

    std::string dackString 
        = dackType + std::to_string((size_t)(lambda * 100)) 
        + ".dr-" + std::to_string(dataRate) 
        + ".rng-" + std::to_string(rngSeed) 
        + ".tcp-" + std::to_string(tcpNodes) 
        + ".udp-" + std::to_string(udpNodes)
        + ".delayed";

    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("results/topology.throughput." + dackString);
    Ptr<OutputStreamWrapper> streamCwnd = asciiTraceHelper.CreateFileStream("results/topology.cwnd." + dackString);
    Ptr<OutputStreamWrapper> aggregatedStream = asciiTraceHelper.CreateFileStream("results/topology-aggregated.throughput." + dackString);

    Ipv4InterfaceContainer& sinkInterfaces = uplink ? staInterfaces : appInterfaces;
    NodeContainer& serverNodes = uplink ? appNodes : staNodes;
    NodeContainer& sinkNodes = uplink ? staNodes : appNodes;

    for (size_t i = 0; i < tcpNodes; i++) 
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

    sinkApps.Start(Seconds(0.0));


    ApplicationContainer udpApps; 
    for (size_t i = 0; i < udpNodes; i++) 
    {
        UdpEchoServerHelper server(8);
        udpApps.Add(server.Install(sinkNodes.Get(i + tcpNodes)));

        UdpEchoClientHelper client(sinkInterfaces.GetAddress(i + tcpNodes), 8);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        serverApps.Add(server.Install(serverNodes.Get(i + tcpNodes)));
    }
    udpApps.Start(Seconds(0.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute ("Min", DoubleValue (1));
    x->SetAttribute ("Max", DoubleValue (1.1));
    serverApps.StartWithJitter(Seconds(0), x);

    auto connect = [nodes, tcpNodes, &staNodes, streamCwnd]() {
        auto recieve_path = "/NodeList/" + std::to_string(2) + "/$ns3::TcpL4Protocol/SocketList/*/Rx";
        Config::Connect(recieve_path, MakeBoundCallback(&RxOther, streamCwnd));
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback(&RxDrop));

        // recieve_path = "/NodeList/" + std::to_string(2) + "/$ns3::TcpL4Protocol/SocketList/*/DelAckCount";
        // Config::Connect(recieve_path,
        //             MakeCallback(&MacRxTrace));
    };


    Simulator::Schedule(Seconds(1.1), connect);

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    double sum = 0;
    for (size_t i = 0; i < tcpNodes; i++) 
    {
        double averageThroughput = ((StaticCast<PacketSink>(sinkApps.Get(i))->GetTotalRx() * 8) / (1e6 * simulationTime * dataRate));
        sum += averageThroughput;
        *aggregatedStream->GetStream() << "average: " << i << '\t' << averageThroughput << '\n';
    }

    *aggregatedStream->GetStream() << "average from all: " << sum / tcpNodes << '\n';
    std::cout << "average from all: " << sum / tcpNodes << '\n';

    Simulator::Destroy();
    return 0;
}
