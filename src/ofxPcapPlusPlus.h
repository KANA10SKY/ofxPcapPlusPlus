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
		using seconds_f = chrono::duration<float, std::ratio<1, 1>>;
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
}

class ofxPcapPlusPlus_UdpReplay {
private:
	pcpp::PcapNgFileReaderDevice reader;
	//std::unique_ptr<pcpp::PcapNgFileReaderDevice> reader;
public:
	ofxPcapPlusPlus_UdpReplay(string src_pcapng) : reader(src_pcapng) {
		//reader = make_unique<std::decay_t<decltype(*reader)>>(src_pcapng);
		if(!reader.open()) {
			std::cerr << "Error opening the pcap file" << std::endl;
			return;
		}
	};
	~ofxPcapPlusPlus_UdpReplay() {
		// close the file
		reader.close();
	};

	void read_test() {
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

		pcpp::RawPacketVector rawPackets;
		reader.getNextPackets(rawPackets);

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
		return;
	}
};

namespace OFX_PCAPPLUSPLUS_DEV {
	inline void run_test() {
		ofxPcapPlusPlus_UdpReplay udpreplay("data/ptzcam_live20220619_0.pcapng");
		udpreplay.read_test();
	}
}


namespace OFX_PCAPPLUSPLUS_TEST {
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
#endif
