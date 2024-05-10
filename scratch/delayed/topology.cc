#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/core-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/aodv-helper.h"
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
#include "ns3/wifi-mac-queue.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/tcp-option-ts.h"
#include "ns3/tcp-option-cwnd.h"
#include "ns3/flow-monitor-helper.h"

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

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

void
Rx(Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> pckt, const TcpHeader& header, Ptr<const TcpSocketBase> sock)
{
    if (!header.HasOption(TcpOption::TS)) return;

    auto delay = Simulator::Now() - MilliSeconds((header.GetOption(TcpOption::TS)->GetObject<TcpOptionTS>())->GetTimestamp());

    *stream->GetStream() << sock->DelayWindow() << ' ' << delay.GetMilliSeconds() << ' ' << std::max(header.GetSequenceNumber(), header.GetAckNumber()) << ' ' << ((header.GetFlags() & TcpHeader::ACK) && pckt->GetSize() == 0) << ' ' << std::endl;
}

double lastDelay = 0;

void
RxOther(Ptr<OutputStreamWrapper> stream, std::string context, Ptr<const Packet> pckt, const TcpHeader& header, Ptr<const TcpSocketBase> sock)
{
    if (!header.HasOption(TcpOption::TS)) return;

    auto delay = (Simulator::Now() - MilliSeconds((header.GetOption(TcpOption::TS)->GetObject<TcpOptionTS>())->GetTimestamp())).GetMilliSeconds();

    double alpha = 0.75;
    lastDelay = lastDelay * alpha + (1 - alpha) * delay;

    *stream->GetStream() << Simulator::Now().GetSeconds() 
        << ' ' << delay 
        << ' ' << lastDelay 
        << ' ' << sock->m_iat * 1000
        << ' ' << (header.GetOption(TcpOption::CWND)->GetObject<TcpOptionCwnd>())->GetCongestionWindow() / sock->GetSegSize() 
        << ' ' << std::endl;
}

// TODO: сделать тот алгос
// TODO: добавить шум (udp трафик)
int
main(int argc, char* argv[])
{
    // LogComponentEnable("PropagationLossModel", LOG_ALL);
    // LogComponentEnable("PropagationLossModel", LOG_PREFIX_ALL);
    // LogComponentEnable("TcpSocketBase", LOG_ALL);
    // LogComponentEnable("TcpSocketBase", LOG_PREFIX_ALL);
    // LogComponentEnable("TcpRecoveryOps", LOG_ALL);
    // LogComponentEnable("TcpRecoveryOps", LOG_PREFIX_ALL);


    uint32_t payloadSize = 1472;                        /* Transport layer payload size in bytes. */
    size_t dataRate = 5;                                /* Application layer datarate. */
    std::string tcpVariant{"TcpNewReno"}; /* TCP variant type. */
    size_t hops = 2;
    std::string phyRate = "HtMcs0";                     /* Physical layer bitrate. */
    double simulationTime = 10;                         /* Simulation time in seconds. */

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                tcpVariant);
    cmd.AddValue("hops", "Number of nodes", hops);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);

    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    
    
    cmd.Parse(argc, argv);

    simulationTime += 5;

    ns3::RngSeedManager::SetRun(1);

    tcpVariant = std::string("ns3::") + tcpVariant;

    // Select TCP variant. TODO: support different TCP types.
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                        "TypeId " << tcpVariant << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    TypeIdValue(TypeId::LookupByName(tcpVariant)));

    // Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyRate));
    Config::SetDefault("ns3::TcpSocketBase::CongestionWindowOption", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketBase::DynamicDelayWindow", BooleanValue(true));
    // Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n);
    
    /* Set up Legacy Channel */
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue("HtMcs0"),
                                       "ControlMode",
                                       StringValue("HtMcs0"));

    auto size = hops + 1;
    /* Create nodes */
    NodeContainer networkNodes;
    networkNodes.Create(size * size);
    Ptr<Node> serverNode = networkNodes.Get(0);
    Ptr<Node> clientNode = networkNodes.Get(size * size - 1);

    NodeContainer hopNodes;
    for (size_t i = 0; i < size * size - 1; i++) {
        hopNodes.Add(networkNodes.Get(1 + i));
    }

    /* Configure STA */
    // Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", QueueSizeValue(QueueSize("30p")));  
    Ssid ssid = Ssid("network");
    wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));


    NetDeviceContainer devices;
    devices = wifiHelper.Install(wifiPhy, wifiMac, networkNodes);

    /* Mobility model */
    MobilityHelper mobility;
    
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(200),
                                  "DeltaY",
                                  DoubleValue(200),
                                  "GridWidth",
                                  UintegerValue(size * size),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(networkNodes);

    AodvHelper aodv;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;

    auto s = OutputStreamWrapper(&std::cout);

    aodv.PrintRoutingTableAllAt(Seconds(10), Ptr(&s));
    
    list.Add(staticRouting, 0);
    list.Add(aodv, 10);

    /* Internet stack */
    InternetStackHelper stack;
    stack.SetRoutingHelper(list); // has effect on the next Install ()

    stack.Install(networkNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);

    /* Populate routing table */
    // Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ApplicationContainer sinkApps; 
    ApplicationContainer serverApps;

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("topology.throughput");
    Ptr<OutputStreamWrapper> streamDelay = asciiTraceHelper.CreateFileStream("topology.delay");
    Ptr<OutputStreamWrapper> streamCwnd = asciiTraceHelper.CreateFileStream("topology.cwnd");
    Ptr<OutputStreamWrapper> aggregatedStream = asciiTraceHelper.CreateFileStream("topology-aggregated.throughput");

    {
        /* Install TCP Receiver on the access point */
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9));
        sinkApps.Add(sinkHelper.Install(clientNode));

         /* Install TCP Transmitter on the station */
        OnOffHelper server("ns3::TcpSocketFactory", (InetSocketAddress(interfaces.GetAddress(size * size - 1), 9)));
        server.SetAttribute("PacketSize", UintegerValue(payloadSize));
        server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Mbps")));
        serverApps.Add(server.Install(serverNode));
        Simulator::Schedule(Seconds(5.1), MakeBoundCallback(&CalculateThroughput, stream, 0, StaticCast<PacketSink>(sinkApps.Get(0)), 0));
    }

    sinkApps.Start(Seconds(5.0));

    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute ("Min", DoubleValue (1));
    x->SetAttribute ("Max", DoubleValue (1.1));
    serverApps.StartWithJitter(Seconds(5), x);

    auto connect = [size, &clientNode, streamDelay, streamCwnd]() {
        std::string recieve_path = "/NodeList/" + std::to_string(size * size - 1) + "/$ns3::TcpL4Protocol/SocketList/*/Rx";
        Config::Connect(recieve_path, MakeBoundCallback(&Rx, streamDelay));
        Config::Connect(recieve_path, MakeBoundCallback(&RxOther, streamCwnd));
        recieve_path = "/NodeList/" + std::to_string(0) + "/$ns3::TcpL4Protocol/SocketList/*/Rx";
        Config::Connect(recieve_path, MakeBoundCallback(&Rx, streamDelay));
    };

    Simulator::Schedule(Seconds(6.1), connect);

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();


    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    flowMonitor->SerializeToXmlFile("flows.xml", true, true);
    double sum = 0;
    for (size_t i = 0; i < 1; i++) 
    {
        double averageThroughput = ((StaticCast<PacketSink>(sinkApps.Get(i))->GetTotalRx() * 8) / (1e6 * (simulationTime - 5)));
        sum += averageThroughput;
        *aggregatedStream->GetStream() << "average: " << i << '\t' << averageThroughput << '\n';
    }

    *aggregatedStream->GetStream() << "average from all: " << sum << '\n';

    Simulator::Destroy();
    return 0;
}
