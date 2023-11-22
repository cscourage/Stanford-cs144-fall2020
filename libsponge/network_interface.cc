#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _arp_table({}), _arp_request_ip_ttl({}), _arp_request_ip_datagram({}) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::_send(const EthernetAddress &dst, const uint16_t type, const BufferList &payload) {
    EthernetFrame frame;
    frame.header().dst = dst;
    frame.header().src = _ethernet_address;
    frame.header().type = type;
    frame.payload() = payload;
    _frames_out.emplace(std::move(frame));
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    auto it = _arp_table.find(next_hop_ip);
    // if the ARP table has relative entry.
    if (it != _arp_table.end()) {
        _send(it->second.mac, EthernetHeader::TYPE_IPv4, dgram.payload());
    } else {
        // if ARP don't hit and in the last 5 seconds there is no request for the next_hop_ip.
        if (_arp_request_ip_ttl.find(next_hop_ip) == _arp_request_ip_ttl.end()) {
            ARPMessage arp_quest;
            arp_quest.opcode = ARPMessage::OPCODE_REQUEST;
            arp_quest.sender_ethernet_address = _ethernet_address;
            arp_quest.sender_ip_address = _ip_address.ipv4_numeric();
            arp_quest.target_ip_address = next_hop_ip;
            _send(ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, BufferList(std::move(arp_quest.serialize())));
            // put the next_hop_ip into the request ttl datastructure.
            _arp_request_ip_ttl[next_hop_ip] = ARP_REQUEST_WAIT_TIME;
        }
        // record the ip datagram, notice one ip may has many datagram sent to.
        _arp_request_ip_datagram[next_hop_ip].emplace_back(next_hop, dgram);
    }
    
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return std::nullopt;
    }
    // if the inbound fram is IPv4
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ip_dgram;
        if (ip_dgram.parse(frame.payload()) == ParseResult::NoError) {
            return ip_dgram;
        }
        return std::nullopt;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        // if the inbound fram is ARP Message.
        ARPMessage arp_message;
        if (arp_message.parse(frame.payload()) == ParseResult::NoError) {
            auto my_ip = _ip_address.ipv4_numeric();
            auto arp_dst_ip = arp_message.target_ip_address;
            auto arp_src_ip = arp_message.sender_ip_address;
            // if the ARP request is sent for me.
            if (arp_message.opcode == ARPMessage::OPCODE_REQUEST && my_ip == arp_dst_ip) {
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = my_ip;
                arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
                arp_reply.target_ip_address = arp_src_ip;
                _send(arp_message.sender_ethernet_address, EthernetHeader::TYPE_ARP, BufferList(std::move(arp_reply.serialize())));
            }
            // learned from the ARP request both for me or not for me because it broadcast.
            _arp_table[arp_src_ip] = {arp_message.sender_ethernet_address, ARP_REQUEST_WAIT_TIME};
            // now it has updated the ARP table, so if the ip has datagram that wait in the queue, then you should send it.
            auto it = _arp_request_ip_datagram.find(arp_src_ip);
            if (it != _arp_request_ip_datagram.end()) {
                for (const auto &[next_hop, dgram] : it->second) {
                    _send(arp_message.sender_ethernet_address, EthernetHeader::TYPE_IPv4, BufferList(std::move(dgram.serialize())));
                }
                _arp_request_ip_datagram.erase(it);
            }
            auto iter = _arp_request_ip_ttl.find(arp_src_ip);
            if (iter != _arp_request_ip_ttl.end()) {
                _arp_request_ip_ttl.erase(iter);
            }
        }
        return std::nullopt;
    } else {
        // any other cases not metioned in this lab.
        return std::nullopt;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // handle the expired ARP table entry
    for (auto it = _arp_table.begin(); it != _arp_table.end(); ) {
        if (it->second.ttl <= ms_since_last_tick) {
            it = _arp_table.erase(it);
        } else {
            it->second.ttl -= ms_since_last_tick;
            ++it;
        }
    }
    // handle the _arp_request_ip_ttl and _arp_request_ip_datagram
    for (auto it = _arp_request_ip_ttl.begin(); it != _arp_request_ip_ttl.end();) {
        if (it->second <= ms_since_last_tick) {
            auto iter = _arp_request_ip_datagram.find(it->first);
            if (iter != _arp_request_ip_datagram.end()) {
                _arp_request_ip_datagram.erase(iter);
            }
            it = _arp_request_ip_ttl.erase(it);
        } else {
            it->second -= ms_since_last_tick;
            ++it;
        }
    }
}
