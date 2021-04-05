#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), max(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        last_ackno = index + data.length();
        eof_occurred = true;
    }
    size_t idx = index, remains = max - _output.buffer_size();
    string str = data;
    if (idx < ackno && idx + str.length() > ackno) {
        str = str.substr(ackno - idx, idx + str.length() - ackno);
        idx = ackno;
    }

    auto end = m.end(), it = m.lower_bound(idx);
    if (idx + str.length() <= ackno)
        goto check_eof;
    if (str.length() == 0)
        goto check_eof;
    if (it == end) {
        idx += str.length();
        size += str.length();
        goto check_size;
    }
    if (idx >= it->first - it->second.length()) {
        if (it->first < idx + str.length()) {
            size += idx + str.length() - it->first;
            str = it->second + str.substr(it->first - idx, idx + str.length() - it->first);
            idx = it->first - it->second.length() + str.length();
            it = m.erase(it);
            goto absorb;
        } else goto check_eof;
    }
    if (idx + str.length() >= it->first - it->second.length()) {
        if (it->first > idx + str.length()) {
            size += it->first - it->second.length() - idx;
            str += it->second.substr(idx + str.length() - (it->first - it->second.length()), it->first - idx - str.length());
        } else size += str.length() - it->second.length();
        idx += str.length();
        it = m.erase(it);
        goto absorb;
    }

    idx += str.length();
    size += str.length();
    goto check_size;

absorb:
    while (it != end) {
        if (idx >= it->first - it->second.length()) {
            if (idx < it->first) {
                size -= idx - (it->first - it->second.length());
                str += it->second.substr(idx - (it->first - it->second.length()), it->first - idx);
            } else size -= it->second.length();
            idx = it->first > idx ? it->first : idx;
            it = m.erase(it);
        } else break;
    }
check_size:
    if (size > remains) {
        str = str.substr(0, str.length() + remains - size);
        idx -= (size - remains);
        size = remains;
        if (str.length() == 0)
            goto send;
    }

// insert:
    m[idx] = str;
send:
    if (!m.empty() && m.begin()->first - m.begin()->second.length() == ackno) {
        ackno = m.begin()->first;
        _output.write(m.begin()->second);
        size -= m.begin()->second.length();
        m.erase(m.begin());
    }

check_eof:
    if (eof_occurred && last_ackno == _output.bytes_written())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    return size;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
