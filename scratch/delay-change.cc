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

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

using namespace ns3;

std::map<Ipv4Address, size_t> adressToInterface;
size_t dataRate = 5;
uint64_t maxAdaptabilityTime = 0;
uint64_t currentAdaptabilityTime = 0;
double maxThroughput = 0;
const auto EPS = 1e-2;

/**
 * Calculate the throughput
 */
void
CalculateThroughput(
    Ptr<OutputStreamWrapper> stream, 
    Ptr<GnuplotAggregator> aggregator,
    const std::string& datasetContext,
    std::vector<Ptr<PacketSink>>& sinks, 
    uint64_t lastTotalRx)
{
    Time now = Simulator::Now();
    uint64_t total = 0;
    for (auto sink : sinks) 
    {
        total += sink->GetTotalRx();
    }

    double cur = (total - lastTotalRx) * 8.0 / sinks.size() / (dataRate * 1024 * 1024);
    if (cur < maxThroughput - EPS) {
        currentAdaptabilityTime++;
    } else {
        maxAdaptabilityTime = std::max(maxAdaptabilityTime, currentAdaptabilityTime);
        // *stream->GetStream() << now.GetSeconds() << ' ' << maxAdaptabilityTime << std::endl;
        currentAdaptabilityTime = 0;
    }

    maxThroughput = std::min(std::max(cur, maxThroughput), 0.925391);

    // *stream->GetStream()  << now.GetSeconds() << ' ' << cur << std::endl;

    aggregator->Write2d(datasetContext, now.GetSeconds(), cur);
    Simulator::Schedule(Seconds(1), MakeBoundCallback(&CalculateThroughput, stream, aggregator, datasetContext, sinks, total));
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
ChangeChannel(std::pair<Ptr<Node>, Ptr<Node>> nodes, std::pair<Ipv4Address, Ipv4Address> active, std::pair<Ipv4Address, Ipv4Address> sleeping)
{
    DisableChannel(nodes.first, active.first);
    DisableChannel(nodes.second, active.second);
    EnableChannel(nodes.first, sleeping.first);
    EnableChannel(nodes.second, sleeping.second);

    // Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    Simulator::Schedule(Seconds(200), MakeBoundCallback(&ChangeChannel, nodes, sleeping, active));
}


// TODO: why such low bandwidth at high delay
int
main(int argc, char* argv[])
{
    // LogComponentEnable("TcpSocketBase", LOG_LEVEL_DEBUG);
    // LogComponentEnable("TcpCerl", LOG_LEVEL_DEBUG);
    // LogComponentEnableAll(LOG_LEVEL_DEBUG);
    // LogComponentEnable("Rip", LOG_LEVEL_ALL);

    uint32_t payloadSize = 1472;
    std::string tcpVariant{"TcpCerlX"};
    size_t flows = 1;
    size_t groups = 1;
    size_t groupRttIncrease = 6;
    size_t delayAtOtherRoute = 0;
    std::string bandwidthAtOtherRoute{"10Gbps"};
    double simulationTime = 900;
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
    cmd.AddValue("groups", "Number of groups", groups);
    cmd.AddValue("flows", "Number of active flows in group", flows);
    cmd.AddValue("groupRttIncrease", "Increase of rtt between groups", groupRttIncrease);
    cmd.AddValue("delayAtOtherRoute", "Delay at slow route", delayAtOtherRoute);
    cmd.AddValue("bandwidthAtOtherRoute", "Bandwidth at slow route", bandwidthAtOtherRoute);
    cmd.AddValue("cerlWindow", "Window size of CerlPlusX", cerlWindow);
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

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

    NetDeviceContainer flowConnections;
    NetDeviceContainer sinkConnections;
    NetDeviceContainer groupConnections;

    for (size_t i = 0; i < groups; i++) 
    {
        pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
        pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(groupRttIncrease * i) + "ms"));

        NodeContainer groupConnector;
        groupConnector.Add(g1Node);
        groupConnector.Add(groupBottlenecks.Get(i));
    
        auto cont = pointToPoint.Install(groupConnector);
        groupConnections.Add(NetDeviceContainer(cont.Get(1), cont.Get(0)));

        for (size_t j = 0; j < flows; j++)
        {
            pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + ".1Mbps")));
            pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));
            NodeContainer flowConnector;
            flowConnector.Add(groupBottlenecks.Get(i));
            flowConnector.Add(flowNodes.Get(i * flows + j));

            auto cont = pointToPoint.Install(flowConnector);
            flowDevices.Add(cont.Get(1));
            flowConnections.Add(NetDeviceContainer(cont.Get(1), cont.Get(0)));

            pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
            pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));
            NodeContainer sinkConnector;
            sinkConnector.Add(g2Node);
            sinkConnector.Add(sinkNodes.Get(i * flows + j));

            cont = pointToPoint.Install(sinkConnector);
            sinkDevices.Add(cont.Get(1));
            sinkConnections.Add(NetDeviceContainer(cont.Get(0), cont.Get(1)));
        }
    }

    Ptr<NetDevice> highRtt, lowRtt;

    NetDeviceContainer G1ToG2;

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate(("10Gbps"))));
    pointToPoint.SetChannelAttribute("Delay", StringValue("0ms"));

    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(g2Node);
    
        NetDeviceContainer g1g2 = pointToPoint.Install(p2pNodesChannel);
        G1ToG2.Add(g1g2);
        lowRtt = g1g2.Get(0);
    }  

    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate((bandwidthAtOtherRoute))));
    pointToPoint.SetChannelAttribute("Delay", StringValue(std::to_string(delayAtOtherRoute) + "ms"));

    {
        NodeContainer p2pNodesChannel;
        p2pNodesChannel.Add(g1Node);
        p2pNodesChannel.Add(g2Node);
    
        NetDeviceContainer g1g2 = pointToPoint.Install(p2pNodesChannel);
        G1ToG2.Add(g1g2);
        highRtt = g1g2.Get(0);
    }

    RipHelper rip;

    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(rip, 0);

    InternetStackHelper internet;
    // internet.SetRoutingHelper(list);
    // internet.Install(NodeContainer(g1Node, g2Node));
    // InternetStackHelper internetRest;
    internet.Install(NodeContainer(sinkNodes, flowNodes, groupBottlenecks, g1Node, g2Node));

    Ipv4AddressHelper address;
    std::vector<Ipv4Address> sinkAddresses;
    Ipv4InterfaceContainer otherInterfaces;

    for (size_t i = 0; i < groups; i++)
    {
        std::string tmp = "10." + std::to_string(i + 1) + ".0.0";
        address.SetBase(tmp.c_str(), "255.255.255.0");
        auto inter = address.Assign(NetDeviceContainer(groupConnections.Get(2 * i), groupConnections.Get(2 * i + 1)));
        otherInterfaces.Add(inter);

        for (size_t j = 0; j < flows; j++) 
        {
            tmp = "10." + std::to_string(groups + i + 1) + "." + std::to_string(j + 1) + ".0";
            address.SetBase(tmp.c_str(), "255.255.255.0");
            auto inter = address.Assign(NetDeviceContainer(sinkConnections.Get(i * flows * 2 + 2 * j), sinkConnections.Get(i * flows * 2 + 2 * j + 1)));
            sinkAddresses.push_back(inter.GetAddress(1));
            otherInterfaces.Add(inter);

            tmp = "10." + std::to_string(groups * 2 + i + 1) + "." + std::to_string(j + 1) + ".0";
            address.SetBase(tmp.c_str(), "255.255.255.0");
            inter = address.Assign(NetDeviceContainer(flowConnections.Get(i * flows * 2 + 2 * j), flowConnections.Get(i * flows * 2 + 2 * j + 1)));
            otherInterfaces.Add(inter);
        }
    }
    
    std::string tmp = "10." + std::to_string(groups * 3 + 5) + ".1.0";
    address.SetBase(tmp.c_str(), "255.255.255.0");
    Ipv4InterfaceContainer G1ToG2Interface;
    G1ToG2Interface = address.Assign(NetDeviceContainer(G1ToG2.Get(0), G1ToG2.Get(1)));

    tmp = "10." + std::to_string(groups * 3 + 6) + ".1.0";
    address.SetBase(tmp.c_str(), "255.255.255.0");
    G1ToG2Interface.Add(address.Assign(NetDeviceContainer(G1ToG2.Get(2), G1ToG2.Get(3))));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

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
        OnOffHelper server("ns3::TcpSocketFactory", (InetSocketAddress(sinkAddresses[i], 9)));
        server.SetAttribute("PacketSize", UintegerValue(payloadSize));
        server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Mbps")));
        serverApps.Add(server.Install(serverNodes.Get(i)));
    }


    sinkApps.Start(Seconds(0.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute ("Min", DoubleValue (10));
    x->SetAttribute ("Max", DoubleValue (10.1));
    serverApps.StartWithJitter(Seconds(0), x);

    std::vector<Ptr<PacketSink>> sinks;
    for (size_t i = 0; i < groups * flows; i++) {
        sinks.push_back(StaticCast<PacketSink>(sinkApps.Get(i)));
    }

    std::string fileNameWithoutExtension = "plots4/reroute-" + tcpVariant + "-" + std::to_string(cerlWindow);
    std::string plotTitle = "Throughput to time";
    std::string plotXAxisHeading = "Time (seconds)";
    std::string plotYAxisHeading = "Normalized throughput";
    std::string plotDatasetLabel = tcpVariant;
    std::string datasetContext = "Dataset/Context/String";

    // Create an aggregator.
    Ptr<GnuplotAggregator> aggregator = CreateObject<GnuplotAggregator>(fileNameWithoutExtension);

    // Set the aggregator's properties.
    aggregator->SetTerminal("png");
    aggregator->SetTitle(plotTitle);
    aggregator->SetLegend(plotXAxisHeading, plotYAxisHeading);

    // Add a data set to the aggregator.
    aggregator->Add2dDataset(datasetContext, plotDatasetLabel);

    aggregator->Enable();

    Simulator::Schedule(Seconds(1.1), MakeBoundCallback(&CalculateThroughput, stream, aggregator, datasetContext, sinks, 0));
    
    auto lowAddressG1 = Ipv4Address::ConvertFrom(G1ToG2Interface.GetAddress(1)), lowAddressG2 = Ipv4Address::ConvertFrom(G1ToG2Interface.GetAddress(1));
    auto highAddressG1 = Ipv4Address::ConvertFrom(G1ToG2Interface.GetAddress(2)), highAddressG2 = Ipv4Address::ConvertFrom(G1ToG2Interface.GetAddress(3));
    
    adressToInterface[lowAddressG1] = g1Node->GetObject<Ipv4>()->GetInterfaceForAddress(lowAddressG1);
    adressToInterface[lowAddressG2] = g2Node->GetObject<Ipv4>()->GetInterfaceForAddress(lowAddressG2);
    adressToInterface[highAddressG1] = g1Node->GetObject<Ipv4>()->GetInterfaceForAddress(highAddressG1);
    adressToInterface[highAddressG2] = g2Node->GetObject<Ipv4>()->GetInterfaceForAddress(highAddressG2);

    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    DisableChannel(g1Node, highAddressG1);
    DisableChannel(g2Node, highAddressG2);
    
    Simulator::Schedule(Seconds(200), MakeBoundCallback(&ChangeChannel, std::make_pair(g1Node, g2Node), std::make_pair(lowAddressG1, lowAddressG2), std::make_pair(highAddressG1, highAddressG2)));

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    

    aggregator->Disable();
    *stream->GetStream() << "max delay: " << maxAdaptabilityTime;
    
    Simulator::Destroy();
    return 0;
}
