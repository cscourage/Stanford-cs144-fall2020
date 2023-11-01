#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), 
    _unreassemble({}), _unreassemble_bytes(0), _recvBase(0), _remaining_assemble_capacity(capacity), _eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index == _recvBase && data.size() <= _output.remaining_capacity()) {
        _output.write(data);
        _recvBase += data.size();
        _eof = eof;
        while (!_unreassemble.empty()) {
            auto it = _unreassemble.begin();
            if (it->begin == _recvBase) {
                _output.write(it->data);
                _recvBase += it->length;
                _eof = it->eof;
                _unreassemble_bytes -= it->length;
                _unreassemble.erase(it);
                continue;
            }
            break;
        }
        _remaining_assemble_capacity = _capacity - _output.buffer_size() - _unreassemble_bytes;
        return;
    } else if (data.size() <= _remaining_assemble_capacity && index >= _recvBase) {
        _unreassemble.insert({index, data.size(), data, eof});
        _unreassemble_bytes += data.size();
        _remaining_assemble_capacity = _capacity - _output.buffer_size() - _unreassemble_bytes;
    }

}

size_t StreamReassembler::unassembled_bytes() const { return _unreassemble_bytes; }

bool StreamReassembler::empty() const { return _eof && _unreassemble_bytes == 0; }
