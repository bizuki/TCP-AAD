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

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

using namespace ns3;

std::map<Ipv4Address, size_t> adressToInterface;

/**
 * Calculate the throughput
 */
void
CalculateThroughput(Ptr<OutputStreamWrapper> stream, std::vector<Ptr<PacketSink>>& sinks, uint64_t lastTotalRx)
{
    Time now = Simulator::Now();
    uint64_t total = 0;
    for (auto sink : sinks) 
    {
        total += sink->GetTotalRx();
    }

    double cur = (total - lastTotalRx) * 8.0 / sinks.size();
    *stream->GetStream() << now.GetSeconds() << "\t" << cur << '\t' << std::endl;
    Simulator::Schedule(MilliSeconds(10), MakeBoundCallback(&CalculateThroughput, stream, sinks, total));
}

void 
EnableChannel(Ptr<Node> g1, Ipv4Address address)
{
    auto interface = adressToInterface[address];

    // g1->GetObject<Ipv4>()->AddAddress(interface, Ipv4InterfaceAddress(address, "255.255.255.0"));
    g1->GetObject<Ipv4>()->SetUp(interface);
}

void 
DisableChannel(Ptr<Node> g1, Ipv4Address address)
{
    auto interface = adressToInterface[address];

    // g1->GetObject<Ipv4>()->RemoveAddress(interface, address);
    g1->GetObject<Ipv4>()->SetDown(interface);
}

void
ChangeChannel(Ptr<Node> g1, Ipv4Address active, Ipv4Address sleeping)
{
    EnableChannel(g1, sleeping);
    DisableChannel(g1, active);

    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    Simulator::Schedule(Seconds(200), MakeBoundCallback(&ChangeChannel, g1, sleeping, active));
}


// TODO: why such low bandwidth at high delay
int
main(int argc, char* argv[])
{
    // LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);
    LogComponentEnable("TcpCerl", LOG_LEVEL_DEBUG);
    // LogComponentEnableAll(LOG_LEVEL_DEBUG);

    uint32_t payloadSize = 1472;
    size_t dataRate = 20;
    std::string tcpVariant{"TcpCerlPlus"};
    size_t flows = 1;
    size_t groups = 20;
    size_t groupRttIncrease = 6;
    double simulationTime = 900;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                tcpVariant);
    cmd.AddValue("groups", "Number of groups", groups);
    cmd.AddValue("flows", "Number of active flows in group", flows);
    cmd.AddValue("groupRttIncrease", "Increase of rtt between groups", groupRttIncrease);
    cmd.Parse(argc, argv);

    tcpVariant = std::string("ns3::") + tcpVariant;

    // Select TCP variant. TODO: support different TCP types.
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                        "TypeId " << tcpVariant << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    TypeIdValue(TypeId::LookupByName(tcpVariant)));
    if (tcpVariant == "ns3::TcpCerl" || tcpVariant == "ns3::TcpCerlPlus") 
    {
        Config::SetDefault("ns3::TcpL4Protocol::RecoveryType", TypeIdValue(TypeId::LookupByName("ns3::TcpClassicRecovery")));
    }

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));
    // Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1e9));
    // Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1e9));
    // Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("600000000p")));
    // Config::SetDefault("ns3::PointToPointNetDevice::Mtu", UintegerValue(5000));

    /* Create nodes */
    NodeContainer networkNodes;
    networkNodes.Create(flows * groups * 2 + groups + 2);
    Ptr<Node> g1Node = networkNodes.Get(0);
    Ptr<Node> g2Node = networkNodes.Get(1);

    NodeContainer flowNodes;
    NodeContainer sinkNodes;
    NodeContainer groupBottlenecks;

    for (size_t i = 0; i < groups; i++) 
    {
        groupBottlenecks.Add(networkNodes.Get(2 + i));
        for (size_t j = 0; j < flows; j++)
        {
            flowNodes.Add(networkNodes.Get(2 + groups + i * flows + j));
            sinkNodes.Add(networkNodes.Get(2 + groups + groups * flows + i * flows + j));
        }
    }

    /* Configure P2P connections*/
    PointToPointHelper pointToPoint;

    NetDeviceContainer flowDevices;
    NetDeviceContainer sinkDevices;
    NetDeviceContainer bottleneckToG1Devices;
    NetDeviceContainer uselessDevices;

    for (size_t i = 0; i < groups; i++) 
    {
        pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(groupRttIncrease * i) + "ms"));

        NodeContainer groupConnector;
        groupConnector.Add(g1Node);
        groupConnector.Add(groupBottlenecks.Get(i));
    
        auto cont = pointToPoint.Install(groupConnector);
        bottleneckToG1Devices.Add(cont.Get(1));
        uselessDevices.Add(cont.Get(0));

        for (size_t j = 0; j < flows; j++)
        {
            pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + ".1Mbps")));
            pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));
            NodeContainer flowConnector;
            flowConnector.Add(groupBottlenecks.Get(i));
            flowConnector.Add(flowNodes.Get(i * flows + j));

            auto cont = pointToPoint.Install(flowConnector);
            flowDevices.Add(cont.Get(1));
            uselessDevices.Add(cont.Get(0));

            pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
            pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));
            NodeContainer sinkConnector;
            sinkConnector.Add(g2Node);
            sinkConnector.Add(sinkNodes.Get(i * flows + j));

            cont = pointToPoint.Install(sinkConnector);
            sinkDevices.Add(cont.Get(1));
            uselessDevices.Add(cont.Get(0));
        }
    }

    Ptr<NetDevice> highRtt, lowRtt;

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(("10Gbps"))));
    pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));

    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(g2Node);
    
        NetDeviceContainer g1g2 = pointToPoint.Install(p2pNodesChannel);
        uselessDevices.Add(g1g2.Get(1));
        lowRtt = g1g2.Get(0);
    }  

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(("10Gbps"))));
    pointToPoint.SetChannelAttribute("Delay", StringValue("90ms"));

    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(g2Node);
    
        NetDeviceContainer g1g2 = pointToPoint.Install(p2pNodesChannel);
        uselessDevices.Add(g1g2.Get(1));
        highRtt = g1g2.Get(0);
    }

    /* Internet stack */
    InternetStackHelper stack;
    stack.Install(networkNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer sinkInterfaces;
    sinkInterfaces = address.Assign(sinkDevices);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer flowInterfaces;
    flowInterfaces = address.Assign(flowDevices);
    
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer lowRttInterface;
    Ipv4InterfaceContainer highRttInterface;
    lowRttInterface = address.Assign(lowRtt);
    highRttInterface = address.Assign(highRtt);
    
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer uselessInterfaces;
    uselessInterfaces = address.Assign(uselessDevices);
    uselessInterfaces.Add(address.Assign(bottleneckToG1Devices));



    /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ApplicationContainer sinkApps; 
    ApplicationContainer serverApps;

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("topology.throughput");
    Ptr<OutputStreamWrapper> aggregatedStream = asciiTraceHelper.CreateFileStream("topology-aggregated.throughput");

    NodeContainer& serverNodes = flowNodes;

    for (size_t i = 0; i < flows * groups; i++) 
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
    }


    sinkApps.Start(Seconds(0.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute ("Min", DoubleValue (1));
    x->SetAttribute ("Max", DoubleValue (1.1));
    serverApps.StartWithJitter(Seconds(0), x);

    std::vector<Ptr<PacketSink>> sinks;
    for (size_t i = 0; i < groups * flows; i++) {
        sinks.push_back(StaticCast<PacketSink>(sinkApps.Get(i)));
    }

    Simulator::Schedule(Seconds(1.1), MakeBoundCallback(&CalculateThroughput, stream, sinks, 0));
    auto lowAddress = Ipv4Address::ConvertFrom(lowRttInterface.GetAddress(0));
    auto highAddress = Ipv4Address::ConvertFrom(highRttInterface.GetAddress(0));
    
    adressToInterface[lowAddress] = g1Node->GetObject<Ipv4>()->GetInterfaceForAddress(lowAddress);
    adressToInterface[highAddress] = g1Node->GetObject<Ipv4>()->GetInterfaceForAddress(highAddress);

    DisableChannel(g1Node, highAddress);
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
    
    Simulator::Schedule(Seconds(20), MakeBoundCallback(&ChangeChannel, g1Node, lowAddress, highAddress));

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
