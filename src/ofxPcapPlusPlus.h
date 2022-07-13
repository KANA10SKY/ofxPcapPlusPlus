#pragma once
#ifndef OFXPCAPPLUSPLUS_H
#define OFXPCAPPLUSPLUS_H
#include <iostream>
#include <memory>
//#include <typeinfo>
#include <chrono>

#include "IPv4Layer.h"
#include "Packet.h"
#include "PcapFileDevice.h"
//#include "WinPcapLiveDevice.h"
#include "PcapLiveDeviceList.h"
#include "UdpLayer.h"

#include "ofEventUtils.h"
#include "ofxXmlSettings.h"

namespace {
	const double ts2sec(timespec const& ts) { return ts.tv_sec + (double)ts.tv_nsec * 1e-9; }
	//const float ts2sec(timespec const& ts) { return ts.tv_sec; }
	//const double ts2sec(timespec const& ts) { return (double)ts.tv_nsec * 1e-9; }
	//constexpr float ts2duration(timespec const& ts1, timespec const& ts0) {return ts2sec(ts1) - ts2sec(ts0); }
	//constexpr std::chrono::duration<double, std::ratio<1,1>> timespecToDuration(timespec const& ts) {
	template <class chrono_duration = std::chrono::duration<double, std::ratio<1,1>>>
	constexpr chrono_duration timespecToDuration(timespec const& ts) {
		using namespace std::chrono;
		//return std::chrono::duration<double, std::ratio<1,1>>(duration_cast<nanoseconds>(seconds{ ts.tv_sec } + nanoseconds{ ts.tv_nsec }));
		//return duration_cast<duration<double>>(seconds{ ts.tv_sec } + nanoseconds{ ts.tv_nsec });
		using seconds_f = duration<float, std::ratio<1, 1>>;
		return chrono_duration(seconds{ ts.tv_sec } + nanoseconds{ ts.tv_nsec });
	}
	// constexpr std::chrono::nanoseconds timespecToDuration(timespec const& ts) {
	// 	using namespace std::chrono;
	// 	return duration_cast<nanoseconds>(seconds{ ts.tv_sec } + nanoseconds{ ts.tv_nsec });
	// }
	//template <class chrono_time_unit>
	//constexpr chrono_time_unit timespecToDuration(timespec const& ts) {
	//	using namespace std::chrono;
	//	return duration_cast<chrono_time_unit>(seconds{ ts.tv_sec } + nanoseconds{ ts.tv_nsec });
	//}
	const float get_duration_seconds_f(std::chrono::system_clock::time_point const& t1, std::chrono::system_clock::time_point const& t0) {
	}
	const double calc_timespan(pcpp::RawPacket const& packet1, pcpp::RawPacket const& packet0) {
		auto calc_timespec = [&]() {
			auto ts0 = ts2sec(packet0.getPacketTimeStamp());
			auto ts1 = ts2sec(packet1.getPacketTimeStamp());
			return ts1 - ts0;
		};
		auto calc_chrono = [&]() {
			auto tp0 = timespecToDuration(packet0.getPacketTimeStamp());
			auto tp1 = timespecToDuration(packet1.getPacketTimeStamp());
			return (tp1 - tp0).count();
		};
		return calc_timespec();
		return calc_chrono();
	}
	const double calc_time(pcpp::RawPacketVector const& packets, const int id) {return calc_timespan(*packets.at(id), *packets.at(0));}
	const double calc_time(pcpp::RawPacketVector& packets, pcpp::RawPacket const& first) {return calc_timespan(*packets.front(), first); }
	const double calc_time_next(pcpp::RawPacketVector& packets, pcpp::RawPacket const& first) {return calc_timespan(*(packets.front()+1), first); }
}

class ofxPcapPlusPlus_UdpReplay {
private:
	pcpp::IFileReaderDevice* reader;
	//std::unique_ptr<pcpp::PcapNgFileReaderDevice> reader;
	pcpp::RawPacketVector rawPackets;
	//bool bSent_thisFrame = false;
	//size_t currentID = 0;
	//pcpp::RawPacket packet_first;
	//pcpp::RawPacket* packet_sent;
	size_t id_next = 0;

	// const string key_pcapPath{"settings:pcapPath"};
	// const string path_xml{"settings.xml"};

	//string dstIP = "127.0.0.1";
	u_short dstPort;// = 80;
	//pcpp::PcapLiveDevice* dev_dst;
	//pcpp::PcapLiveDevice* dev_src;
	//pcpp::IPv4Address srcIP;
	pcpp::PcapLiveDevice* ether;
	pcpp::IPv4Address localhost;
	pcpp::IPv4Address dstIP;
	//string dstIP;
	//pcpp::WinPcapLiveDevice* dev;

	bool send_packet(const size_t& id, const u_short& dstPort) { 
		//cout << "sent packet : id: " << id << endl;
		auto output_caution = []() {
			cerr << "Couldn't send packet" << endl;
			return false;
		};
		auto packet = rawPackets.at(id);
		pcpp::Packet parsedPacket(packet);
		if (ether == NULL)
			return output_caution();
		if (!parsedPacket.isPacketOfType(pcpp::IPv4))
			return output_caution();
		auto ipLayer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
		//ipLayer->setDstIPv4Address(dstIP);
		//auto ip = dev->getIPv4Address();
		//ipLayer->setSrcIPv4Address(pcpp::IPv4Address());
		//ipLayer->setDstIPv4Address(dev_dst->getIPv4Address());
		ipLayer->setSrcIPv4Address(localhost);
		ipLayer->setDstIPv4Address(dstIP);
		auto udpLayer = parsedPacket.getLayerOfType<pcpp::UdpLayer>();
		//udpLayer->getUdpHeader()->portSrc = pcpp::hostToNet16(12345);
		udpLayer->getUdpHeader()->portDst = htons(dstPort);
		parsedPacket.computeCalculateFields();
		if (!ether->sendPacket(*packet))
		//if (!dev->sendPacket(&parsedPacket))
			return output_caution();
		return true;
	}
	bool read_pcap(string const& src_pcap) {
		//reader = make_unique<std::decay_t<decltype(*reader)>>(src_pcapng);
		reader = pcpp::IFileReaderDevice::getReader(src_pcap);
		if (reader == NULL) {
			std::cerr << "Cannot determine reader for file type: " << src_pcap << std::endl;
			return false;
		}
		if (!reader->open()) {
			std::cerr << "Error opening the pcap file: " << src_pcap << std::endl;
			return false;
		}
		if (!reader->getNextPackets(rawPackets)) {
			std::cerr << "Couldn't read the first packet in the file" << std::endl;
			return false;
		};
		return true;
	}
	bool setup_pcaplivedevice(string const& _dstIP, bool const& bOutput_DeviceList = true) {
		auto debug_output = [&]() {
			auto devs = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
			//auto devs = pcpp::WinPcapLiveDevice::getAddresses();
			for (auto& dev : devs)
				cout << "Live Device Name: " << dev->getName() << ", IP: " << dev->getIPv4Address() << endl;
		};
		if(bOutput_DeviceList)
			debug_output();
		// dev_dst = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIp(dstIP);
		// if (dev_dst == NULL) {
		// 	printf("Cannot find interface with IPv4 address of '%s'\n", dstIP.c_str());
		// 	return false;
		// };
		// auto devlist = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
		// for (auto const& _dev : devlist) {
		// 	auto ip = _dev->getIPv4Address();
		// 	if (ip != pcpp::IPv4Address::Zero)
		// 		srcIP = ip;
		// }
		// return dev_dst->open();
		auto devlist = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
		for (auto const& _dev : devlist) {
			auto ip = _dev->getIPv4Address();
			if (ip != pcpp::IPv4Address::Zero)
				ether = _dev;
		}
		localhost = ether->getIPv4Address();
		//dstIP = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIp(_dstIP);
		dstIP = pcpp::IPv4Address(_dstIP);
		return ether->open();
	}
	// void dragEvent(ofDragInfo& dragInfo) {
	// 	ofxXmlSettings settings;
	// 	if (!settings.loadFile(path_xml))
	// 		return;
	// 	if (dragInfo.files.size() == 0)
	// 		return;
	// 	auto file = dragInfo.files.at(0);
	// 	for (auto const& ext : vector<string>{ ".pcapng", ".pcap" })
	// 		if (file.find(ext) != string::npos) {
	// 			settings.setValue(key_pcapPath, file);
	// 			settings.saveFile();
	// 			//gui.saveToFile(path_xml);
	// 			//timeline.exec_dragEvent(dragInfo);
	// 			break;
	// 		}
	// }
public:
	//ofxPcapPlusPlus_UdpReplay(string src_pcapng) : reader(src_pcapng) {

	//void restart_send(double const& time_sec) {
	//void reset_send(double const& newstart_sec) {
	bool reset_starttime(double const& newstart_sec) {
		// if (time_sec == 0.00000) {
		// 	id2send = 0;
		// 	return;
		// }

		// for (int i = 0; i < rawPackets.size(); ++i) {
		// 	auto timespan = calc_time(rawPackets, i);
		// 	//if (calc_time(rawPackets, i) > time_sec)
		// 	if (timespan > newstart_sec) {
		// 		//send_packet(i);
		// 		id_next = i-1;
		// 		return;
		// 	}
		// }
		// id_next = rawPackets.size() - 1;

		id_next = 0;
		if (!rawPackets.size())
			return false;
		while (id_next < rawPackets.size() and calc_time(rawPackets, id_next) < newstart_sec)
			++id_next;
		if(id_next > 0)
			--id_next;
		return true;

		// send_packet(id_next);
		// cout << "reset and sent packet : " << id_next << " : " << newstart_sec << endl;
	};
	bool reset_settings(double const& newstart_sec, string const& dstIP, u_short const& _dstPort) {
		if (!reset_starttime(newstart_sec))
			return false;
		if(!setup_pcaplivedevice(dstIP))
			return false;
		dstPort = _dstPort;
		return true;
	}
	bool setup(string const& src_pcap, string const& dstIP, u_short const& _dstPort, double const& start_sec = 0.0) {
		////ofAddListener(ofEvents().fileDragEvent, this, [&](ofDragInfo& arg) {});
		//ofAddListener(ofEvents().fileDragEvent, this, &ofxPcapPlusPlus_UdpReplay::dragEvent);
		if (!read_pcap(src_pcap))
			return false;
		return reset_settings(start_sec, dstIP, _dstPort);
	}
	bool try_sendNext(double const& current_sec, const u_short& dstPort, bool const& bPrint = false) {
		//while(calc_time_next(rawPackets, firstPacket))
		if (!rawPackets.size() or id_next >= rawPackets.size())
			return false;
		while (id_next < rawPackets.size() and calc_time(rawPackets, id_next) <= current_sec) {
			if (!send_packet(id_next, dstPort))
				return false;
			if (bPrint)
				cout << "sent packet : " << id_next << " : pcap time: " << calc_time(rawPackets, id_next) << " : timelien time: " << current_sec << endl;
			if (id_next < rawPackets.size())
				++id_next;
			// else
			// 	break;
		}
		return true;

		// if (current_sec < calc_time(rawPackets, id_next))
		// 	return false;
		// if (!send_packet(id_next, dstPort))
		// 	return false;
		// if(bPrint)
		// 	cout << "sent packet : " << id_next << " : pcap time: " << calc_time(rawPackets, id_next) << " : timelien time: " << current_sec << endl;
		// if (id_next < rawPackets.size())
		// 	++id_next;
		// return true;

		// if (current_sec > calc_time(rawPackets, id_next)) {
		// 	//packet_sent = rawPackets.getAndRemoveFromVector();
		// 	//cout << "sent packet: id: " << id2send << endl;
		// 	send_packet(id_next);
		// 	cout << "sent packet : " << id_next << " : " << current_sec << endl;
		// 	++id_next;
		// }
	}
	bool try_sendNext(double const& current_sec, bool const& bPrint = false) { return try_sendNext(current_sec, dstPort, bPrint); }
	// void _try_to_send(double const& time_sec) {
	// 	auto find_nearestFrame = [&]() {
	// 		//rawPackets.find
	// 		for (int i = 0; i < rawPackets.size(); ++i) 
	// 			if (calc_time(rawPackets, i) > time_sec)
	// 				return size_t(i - 1);
	// 		return rawPackets.size() - 1;
	// 		// for (const auto& packet : rawPackets)
	// 		// 	if (ts2sec(packet->getPacketTimeStamp()) > time_sec)
	// 		// 		return packet;
	// 	};
	// 	auto _id = find_nearestFrame();
	// 	if (currentID != _id) {
	// 		currentID = _id;
	// 		//cout << "sent packet - time: " << ts2sec(rawPackets.at(currentID)->getPacketTimeStamp()) << endl;
	// 		cout << "sent packet - id: " << currentID << endl;
	// 	}
	// 	//if(time_sec > )
	// 	// if (!bSent_thisFrame) {
	// 	// 	bSent_thisFrame = true;
	// 	// }
	// }

	ofxPcapPlusPlus_UdpReplay() {};
	ofxPcapPlusPlus_UdpReplay(string const& src_pcap, string const& dstIP, u_short const& dstPort, double const& start_sec = 0.0) {
		setup(src_pcap, dstIP, dstPort, start_sec);
	};
	~ofxPcapPlusPlus_UdpReplay() {
		// close the file
		reader->close();
		delete reader;
	};

public:
};

namespace OFX_PCAPPLUSPLUS_TEST {
	void read_test_v1(string const& src_pcap) {
		//ofxPcapPlusPlus_UdpReplay pcpp(src_pcap);
		//pcpp.
		// read the first (and only) packet from the file
		auto reader = pcpp::IFileReaderDevice::getReader(src_pcap);
		pcpp::RawPacket rawPacket;
		if (!reader->getNextPacket(rawPacket))
		{
			std::cerr << "Couldn't read the first packet in the file" << std::endl;
			return;
		}

		// parse the raw packet into a parsed packet
		pcpp::Packet parsedPacket(&rawPacket);

		// verify the packet is IPv4
		if (parsedPacket.isPacketOfType(pcpp::IPv4))
		{
			// extract source and dest IPs
			pcpp::IPv4Address srcIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPv4Address();
			pcpp::IPv4Address destIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getDstIPv4Address();

			// print source and dest IPs
			std::cout << "Source IP is '" << srcIP << "'; Dest IP is '" << destIP << "'" << std::endl;

		}

		pcpp::RawPacketVector rawPackets;
		reader->getNextPackets(rawPackets);

		auto timestampcalc_stdchrono = [&]() {
			auto rawPacket1 = rawPackets.at(0);
			namespace chrono = std::chrono;
			auto timestamp = rawPacket.getPacketTimeStamp();
			auto tp0 = timespecToDuration(timestamp);
			//auto tp0 = timespecToDuration<seconds_f>(rawPackets.at(0)->getPacketTimeStamp());
			//auto tp1 = timespecToDuration<seconds_f>(rawPackets.at(1)->getPacketTimeStamp());
			//auto tp0 = timespecToDuration(rawPackets.at(0)->getPacketTimeStamp());
			//auto tp1 = timespecToDuration(rawPackets.at(1)->getPacketTimeStamp());
			auto tp1 = timespecToDuration(rawPackets.at(110363)->getPacketTimeStamp());
			std::cout << "Timestamp:ts0:'" << tp0.count() << "'" << std::endl;
			std::cout << "Timestamp:ts1:'" << tp1.count() << "'" << std::endl;
			std::cout << std::fixed << std::setprecision(6) << "Time tp1-tp0:'" << (tp1 - tp0).count() << "'" << std::endl;
		};
		auto timestampcalc_timespec = [&]() {
			auto ts0 = ts2sec(rawPacket.getPacketTimeStamp());
			auto ts1 = ts2sec(rawPackets.at(110363)->getPacketTimeStamp());
			auto ts2 = ts2sec(rawPackets.at(1)->getPacketTimeStamp());
			std::cout << "Timestamp:ts0:'" << ts0 << "'" << std::endl;
			std::cout << "Timestamp:ts1:'" << ts1 << "'" << std::endl;
			//std::cout << "Timestamp Duration:ts0-ts1:'" << ts2duration(rawPacket.getPacketTimeStamp(), rawPackets.at(1)->getPacketTimeStamp()) << "'" << std::endl;
			//std::cout << "Timestamp Duration:ts0-ts1:'" << ts1 - ts0 << "'" << std::endl;
			std::cout << std::fixed << std::setprecision(6) << "Time ts0-ts1:'" << ts1 - ts0 << "'" << std::endl;
		};

		timestampcalc_stdchrono();
		timestampcalc_timespec();
		std::cout << std::fixed << std::setprecision(6) << "Time ts0-ts1:'" << calc_timespan(*rawPackets.at(110363), rawPacket) << "'" << std::endl;
		return;
	}
	void read_test_v2(string const& src_pcap) {
		auto reader = pcpp::IFileReaderDevice::getReader(src_pcap);
		pcpp::RawPacketVector rawPackets;
		reader->getNextPackets(rawPackets);
		pcpp::RawPacket rawPacket;
		if (!reader->getNextPacket(rawPacket))
		{
			std::cerr << "Couldn't read the first packet in the file" << std::endl;
			return;
		}
		//std::cout << std::fixed << std::setprecision(6) << "Time ts0-ts1:'" << calc_timespan(*rawPackets.at(110364), *rawPackets.at(0)) << "'" << std::endl;
		std::cout << std::fixed << std::setprecision(6) << "Elapsed Time: packetN-packet0:'" << calc_time(rawPackets, 110364) << "'" << std::endl;
		return;
	}
	inline void simple_test_read(string pcap="data/1_packet.pcap") {
		// open a pcap file for reading
		pcpp::PcapFileReaderDevice reader(pcap);
		if (!reader.open())
		{
			std::cerr << "Error opening the pcap file" << std::endl;
			return;
		}

		// read the first (and only) packet from the file
		pcpp::RawPacket rawPacket;
		if (!reader.getNextPacket(rawPacket))
		{
			std::cerr << "Couldn't read the first packet in the file" << std::endl;
			return;
		}

		// parse the raw packet into a parsed packet
		pcpp::Packet parsedPacket(&rawPacket);

		// verify the packet is IPv4
		if (parsedPacket.isPacketOfType(pcpp::IPv4))
		{
			// extract source and dest IPs
			pcpp::IPv4Address srcIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPv4Address();
			pcpp::IPv4Address destIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getDstIPv4Address();

			// print source and dest IPs
			std::cout << "Source IP is '" << srcIP << "'; Dest IP is '" << destIP << "'" << std::endl;
		}

		// close the file
		reader.close();
		return;
	}
	inline void simple_test_read_ng(string pcap="data/hoge.pcapng") {
		// open a pcap file for reading
		pcpp::PcapNgFileReaderDevice reader(pcap);
		if (!reader.open())
		{
			std::cerr << "Error opening the pcap file" << std::endl;
			return;
		}

		// read the first (and only) packet from the file
		pcpp::RawPacket rawPacket;
		if (!reader.getNextPacket(rawPacket))
		{
			std::cerr << "Couldn't read the first packet in the file" << std::endl;
			return;
		}

		// parse the raw packet into a parsed packet
		pcpp::Packet parsedPacket(&rawPacket);

		// verify the packet is IPv4
		if (parsedPacket.isPacketOfType(pcpp::IPv4))
		{
			// extract source and dest IPs
			pcpp::IPv4Address srcIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getSrcIPv4Address();
			pcpp::IPv4Address destIP = parsedPacket.getLayerOfType<pcpp::IPv4Layer>()->getDstIPv4Address();

			// print source and dest IPs
			std::cout << "Source IP is '" << srcIP << "'; Dest IP is '" << destIP << "'" << std::endl;
		}

		// close the file
		reader.close();
		return;
	}
}
namespace OFX_PCAPPLUSPLUS_DEV {
	inline void run_test() {
		const string path = "data/ptzcam_live20220619_0.pcapng";
		//ofxPcapPlusPlus_UdpReplay udpreplay("data/ptzcam_live20220619_0.pcapng");
		OFX_PCAPPLUSPLUS_TEST::read_test_v2(path);
	}
}



#endif
