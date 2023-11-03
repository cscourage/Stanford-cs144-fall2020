#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unreassemble({}), _unreassemble_bytes(0), _recvBase(0), _eof(false) {}

// After the receving segment is directly write into the buffer, you must also check the
// _unreassemble whether now they can also push into the buffer. And if so, do it!
void StreamReassembler::merge() {
    while (!_unreassemble.empty()) {
        auto it = _unreassemble.begin();
        if (it->begin + it->length <= _recvBase) {
            _unreassemble_bytes -= it->length;
            _unreassemble.erase(it);
            continue;
        } else if (it->begin <= _recvBase && it->begin + it->length > _recvBase) {
            size_t len = it->begin + it->length - _recvBase;
            std::string real_data = (it->data).substr(_recvBase - it->begin, len);
            _output.write(real_data);
            _recvBase += len;
            _eof = it->eof;
            _unreassemble_bytes -= it->length;
            _unreassemble.erase(it);
            continue;
        }
        break;
    }
}

// In the situation where the receving segment is inserted into the _unreassemble,
// you should check and handle the overlap and containment situation.
void StreamReassembler::overlap_handle(const node &elem) {
    auto iter = _unreassemble.find(elem);
    // if there exist a similar index segment.
    if (iter != _unreassemble.end()) {
        // if the receving one contains the existed one, that erase it and insert the receving one.
        if (elem.length > iter->length) {
            _unreassemble_bytes -= iter->length;
            _unreassemble.erase(iter);
            _unreassemble.insert(elem);
            _unreassemble_bytes += elem.length;
        }
    } else {
        // if not, just insert it.
        _unreassemble.insert(elem);
        _unreassemble_bytes += elem.length;
    }
    // handle all the overlap situation.
    iter = _unreassemble.find(elem);
    // if it has the previous.
    if (iter != _unreassemble.begin()) {
        --iter;
        auto prev = iter;
        ++iter;
        if (prev->begin + prev->length > iter->begin) {
            // if the previous one intersects with but not contain the receving one.
            if (prev->begin + prev->length < iter->begin + iter->length) {
                size_t overlap = prev->begin + prev->length - iter->begin;
                node tmp{};
                tmp.begin = prev->begin;
                tmp.length = prev->length - overlap;
                tmp.data = (prev->data).substr(0, tmp.length);
                _unreassemble.erase(prev);
                _unreassemble.insert(tmp);
                _unreassemble_bytes -= overlap;
            } else {
                // if the previous one contains the receving one.
                _unreassemble_bytes -= iter->length;
                _unreassemble.erase(iter);
                iter = prev;
            }
        }
    }
    // if it has the next.
    if (iter != --_unreassemble.end()) {
        ++iter;
        auto next = iter;
        --iter;
        if (iter->begin + iter->length > next->begin) {
            // if the receving one intersects with but not contain the next one.
            if (iter->begin + iter->length < next->begin + next->length) {
                node tmp{};
                size_t overlap = iter->begin + iter->length - next->begin;
                tmp.begin = next->begin + overlap;
                tmp.length = next->length - overlap;
                tmp.data = (next->data).substr(overlap);
                _unreassemble.erase(next);
                _unreassemble.insert(tmp);
                _unreassemble_bytes -= overlap;
            } else {
                // if the receving one contains the last one.
                _unreassemble_bytes -= next->length;
                _unreassemble.erase(next);
            }
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len;
    std::string real_data;
    // the segment is the expected one.
    if (index == _recvBase) {
        len = std::min(data.size(), _output.remaining_capacity());
        real_data = data.substr(0, len);
        _output.write(real_data);
        _recvBase += len;
        _eof = eof;
        // if cut off so it must not be the last one.
        if (len < data.size()) {
            _eof = false;
        }
        merge();
    } else if (index < _recvBase) {
        // the segment range from less then expected to larger than expected.
        if (index + data.size() > _recvBase) {
            len = std::min(index + data.size() - _recvBase, _output.remaining_capacity());
            real_data = data.substr(_recvBase - index, len);
            _output.write(real_data);
            _recvBase += len;
            _eof = eof;
            // if cut off so it must not be the last one.
            if (len < index + data.size() - _recvBase) {
                _eof = false;
            }
            merge();
        }
    } else {
        // the sliding window's endpoint.
        size_t endpoint = _recvBase + (_capacity - _output.buffer_size());
        // if segment has portion in the sliding window.
        if (index < endpoint) {
            bool eof_tag = eof;
            len = index + data.size() <= endpoint ? data.size() : endpoint - index;
            // if cut off so it must not be the last one.
            if (len != data.size()) {
                eof_tag = false;
            }
            real_data = data.substr(0, len);
            node elem{index, len, real_data, eof_tag};
            // handle the containment and overlap situation.
            overlap_handle(elem);
        }
    }
    if (_eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unreassemble_bytes; }

bool StreamReassembler::empty() const { return _eof; }
