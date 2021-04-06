#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : max(capacity) { }

size_t ByteStream::write(const string &data) {
    size_t remains = max - size, prev = size;
    if (remains >= data.length()) {
        size_writed += data.length();
        for (char c : data)
            q.push_back(c);
        size += data.length();
    } else {
        size_writed += remains;
        for (size_t i = 0; i < remains; ++i)
            q.push_back(data[i]);
        size = max;
    }
    return size - prev;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string cur;
    size_t s = len <= size ? len : size;
    for (size_t i = 0; i < s; ++i)
        cur += q[i];
    return cur;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t k = len <= size ? len : size;
    size_read += k;
    size -= k;
    for (size_t i = 0; i < k; ++i) {
        q.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string str = move(peek_output(len));
    pop_output(len);
    return str;
}

void ByteStream::end_input() { input_end = true; }

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return size; }

bool ByteStream::buffer_empty() const { return size == 0; }

bool ByteStream::eof() const { return input_end && buffer_empty(); }

size_t ByteStream::bytes_written() const { return size_writed; }

size_t ByteStream::bytes_read() const { return size_read; }

size_t ByteStream::remaining_capacity() const { return max - size; }
