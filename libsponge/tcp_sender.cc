#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout)
    , _outstanding()
    , _abs_sendBase(0)
    , _window_size(1)
    , _bytes_in_flight(0)
    , _consecutive_retransmissions(0)
    , _syn_tag(false)
    , _fin_tag(false) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // if the window_size is 0, we should take it as 1.
    uint16_t window_size = _window_size > 0 ? _window_size : 1;
    // as long as there are new bytes to be read and space available in the window.
    // notice we test whether if there are new bytes to be read in the while block
    // because the SYN special case where there is no bytes.
    while (_bytes_in_flight < window_size) {
        TCPSegment seg;
        // generalize the step.
        // in the beginning, the window size is just 1 and you just send SYN without payload.
        if (!_syn_tag) {
            seg.header().syn = true;
            _syn_tag = true;
        }
        auto payload_length = std::min(TCPConfig::MAX_PAYLOAD_SIZE, window_size - _bytes_in_flight - seg.header().syn);
        std::string payload = _stream.read(payload_length);
        seg.payload() = Buffer(std::move(payload));
        // if read the eof, and there is a position for send fin, then add it.
        if (!_fin_tag && _stream.eof() && _bytes_in_flight + seg.length_in_sequence_space() < window_size) {
            seg.header().fin = true;
            _fin_tag = true;
        }
        auto length = seg.length_in_sequence_space();

        // if length == 0, then break it, notice you should add it otherwise it may loop forever.
        // because this means there is no bytes to be read.
        if (length == 0) {
            break;
        }

        seg.header().seqno = next_seqno();
        _segments_out.push(seg);
        // if timer has not been started.
        if (!_timer.is_running()) {
            _timer.restart();
        }
        _outstanding.emplace(_next_seqno, std::move(seg));
        _next_seqno += length;
        _bytes_in_flight += length;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _abs_sendBase);
    // if the ackno is not the the [_abs_sendBase+1, next_seqno_absolute()+1], do not accept.
    // notice if abs_ackno is _abs_sendBase then it is useful because it may change the window size and fill more.
    if (abs_ackno > next_seqno_absolute() || abs_ackno < _abs_sendBase) {
        return;
    }
    // otherwise the ackno is in the expected range.
    bool has_ack = false;
    while (!_outstanding.empty()) {
        auto &[abs_seqno, seg] = _outstanding.front();
        // if the segment has been fully acknowledged.
        if (abs_seqno + seg.length_in_sequence_space() <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _abs_sendBase += seg.length_in_sequence_space();
            _outstanding.pop();
            has_ack = true;
        } else {
            // if not, just exit the while loop.
            break;
        }
    }
    // if ack successfully.
    if (has_ack) {
        _timer.set_time_out(_initial_retransmission_timeout);
        if (_bytes_in_flight != 0) {
            _timer.restart();
        }
        _consecutive_retransmissions = 0;
    }
    if (_bytes_in_flight == 0) {
        _timer.stop();
    }

    // update the window size and try to fill the window again.
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    // if the timer is out.
    if (_timer.is_time_out()) {
        // retransmit the earliest one that has not been ack.
        _segments_out.push(_outstanding.front().second);
        // if the window size is nonzero.
        if (_window_size > 0) {
            ++_consecutive_retransmissions;
            _timer.set_time_out(_timer.get_time_out() * 2);
        }
        // restart the timer.
        _timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

//! generate and send a TCPSegment that has zero length in sequence space, and with the sequence number set correctly.
//! useful when sned an empty ACK segment in TCPConnection.
void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.emplace(std::move(seg));
}
