#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    Buffer payload = seg.payload();
    size_t len = seg.length_in_sequence_space();
    if (!syn_received && header.syn) {
        syn_received = true;
        isn = header.seqno;
    }
    if (!syn_received) return;
    /*
    if (seg.header().seqno - ackno().value() >= static_cast<int>(window_size())) {
        return;
    }
    */
   
    size_t checkpoint = unwrap(ackno().value(), isn, stream_out().bytes_written());
    size_t start_abs_idx = unwrap(header.seqno, isn, checkpoint);
    // if (checkpoint + window_size() <= start_abs_idx) return;
    if (checkpoint >= start_abs_idx + len) return;
    size_t start_stream_idx = start_abs_idx;
    if (!header.syn && syn_received) {
        --start_stream_idx;
    }
    if (header.fin)
        fin_received = true;
    _reassembler.push_substring(move(payload.copy()), start_stream_idx, header.fin);

}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn_received)
        return std::nullopt;
    return wrap(stream_out().bytes_written() + syn_received + (fin_received & (unassembled_bytes() == 0)), isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
