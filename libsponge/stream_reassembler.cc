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


void StreamReassembler::merge() {
    while (!_unreassemble.empty()) {
        auto it = _unreassemble.begin();
        if (it->begin + it->length <= _recvBase) {
            _unreassemble.erase(it);
            _unreassemble_bytes -= it->length;
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


// void discard() {
//     // if overflow you should take actions.
//     if (_output.buffer_size() + _unreassemble_bytes > _capacity) {

//     }
// }


//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len;
    std::string real_data;
    if (index == _recvBase) {
        len = std::min(data.size(), _output.remaining_capacity());
        real_data = data.substr(0, len);
        _output.write(real_data);
        _recvBase += len;
        _eof = eof;
        merge();
        //discard();
        _remaining_assemble_capacity = _capacity - _output.buffer_size() - _unreassemble_bytes;
    } else if (index < _recvBase) {
        if (index + data.size() > _recvBase) {
            len = std::min(index + data.size() - _recvBase, _output.remaining_capacity());
            real_data = data.substr(_recvBase - index, len);
            _output.write(real_data);
            _recvBase += len;
            _eof = eof;
            merge();
            //discard();
            _remaining_assemble_capacity = _capacity - _output.buffer_size() - _unreassemble_bytes;
        }
    } else {
        // notice it need to handle the overlap situation.
        len = std::min(data.size(), _remaining_assemble_capacity);
        real_data = data.substr(0, len);
        _unreassemble.insert({index, len, real_data, eof});
        // need begin to deal with overlap.
        _unreassemble_bytes += data.size();
        _remaining_assemble_capacity = _capacity - _output.buffer_size() - _unreassemble_bytes;
    }
    if (_eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unreassemble_bytes; }

bool StreamReassembler::empty() const { return _eof; }
