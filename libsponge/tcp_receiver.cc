#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();
    // if the syn has not been set.
    if (!_isn.has_value()) {
        // if the inbound segment is also not SYN.
        if (!header.syn) {
            return;
        }
        // otherwise the bound is SYN.
        _isn = header.seqno;
    }
    uint64_t checkpoint = _reassembler.recvBase();
    uint64_t abs_seqno = unwrap(header.seqno, _isn.value(), checkpoint);
    uint64_t stream_index = abs_seqno - 1 + (header.syn ? 1 : 0);
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()) {
        return std::nullopt;
    }
    uint64_t abs_seqno = _reassembler.recvBase() + 1 + (_reassembler.stream_out().input_ended() ? 1 : 0);
    return wrap(abs_seqno, _isn.value());
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
