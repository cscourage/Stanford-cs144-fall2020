#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // if receive a RST segment, connection down uncleanly.
    if (seg.header().rst) {
        _set_rst(false);
        return;
    }
    // receiver receive.
    _receiver.segment_received(seg);
    // if ACK is set, tell the TCPSender ackno and window_size.
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno,seg.header().win);
    }
    // if seg occupy seqence number, send a ack segment back.
    if (seg.length_in_sequence_space() > 0) {
        _add_ack_and_window();
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    auto has_written_bytes = _sender.stream_in().write(data);
    // set the syn, seqno, fin, payload.
    _sender.fill_window();
    // ask TCPReceiver for ackno and window_size.
    _add_ack_and_window();
    return has_written_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _add_ack_and_window();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _add_ack_and_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_add_ack_and_window() {
    while (!_sender.segments_out().empty()) {
        auto seg = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        // ask the tcp receiver for the fields ackno and win.
        // if there is an ackno, set ACK flag and the fields in the TCPSegment.
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = std::min(std::numeric_limits<uint16_t>::max, _receiver.window_size());
        }
        _segments_out.emplace(std::move(seg));
    }
}

void TCPConnection::_set_rst(const bool send_rst) {
    // if need to sent rst.
    if (send_rst) {
        TCPSegment seg;
        seg.header().seqno = _sender.next_seqno();
        seg.header().rst = true;
        _segments_out.emplace(std::move(seg));
    }
    // both send or receive rst.
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
    _linger_after_streams_finish = false;
}