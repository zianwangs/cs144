#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
#define mymin(a,b) ((a) < (b) ? (a) : (b))
using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , cur_rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { 
    return _next_seqno - _ackno; 
}

void TCPSender::fill_window() {
    TCPSegment seg;
    if (cur_window == 0) return;
    if (!syn_sent) {
        // string payload = stream_in().read(mymin(cur_window, 1452));
        // seg.payload() = Buffer(move(payload));
        seg.header().syn = 1;
        syn_sent = true;
        /*
        if (cur_window > seg.length_in_sequence_space() && stream_in().eof()) {
            seg.header().fin = 1;
            fin_sent = true;
        }
        */
    } else if (fin_sent) {
        return;
    } else if (!stream_in().eof() && stream_in().buffer_size() == 0) {
        // send_empty_segment();
        return;
    } else if (stream_in().eof() && !fin_sent) {
        seg.header().fin = 1;
        fin_sent = true;
    } else {
        string payload = move(stream_in().read(mymin(cur_window, 1452)));
        seg.payload() = Buffer(move(payload));
        if (cur_window > seg.length_in_sequence_space() && stream_in().eof()) {
            seg.header().fin = 1;
            fin_sent = true;
        }

    }
    if (!timer.isRunning())
        timer.start(cur_rto);
    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();
    cur_window -= seg.length_in_sequence_space();
    _segments_out.push(seg);
    q.push(seg);
    fill_window();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint16_t old_win = win;
    win = window_size;
    uint64_t abs_idx = unwrap(ackno, _isn, _ackno);
    bool poped = false;
    if (abs_idx > _next_seqno) {
        // cur_window = win == 0 ? ???? waiting to be done 
        // what should be done if impossible ackno occurred and win inc???
        return;
    }
    if (abs_idx <= _ackno)
        goto no_ack_but_bigger_win;
    while (!q.empty()) {
        uint64_t target = q.front().length_in_sequence_space() + unwrap(q.front().header().seqno, _isn, _ackno);
        if (abs_idx >= target) {
            poped = true; 
            q.pop();
            _ackno = target;
        } else {
            break;
        }
    }
    if (poped) {
        cur_rto = _initial_retransmission_timeout;
        consec = 0;
        if (!q.empty()) {
            timer.start(cur_rto);
        } else {
            timer.stop();
        }
        cur_window = win == 0 ? 1 : win;
        fill_window();
        return;
    }
no_ack_but_bigger_win:
    if (win > old_win) {
        cur_window += win - old_win;
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!timer.isRunning())
        return;
    timer.elapse(ms_since_last_tick);
    if (!timer.isExpired())
        return;
    _segments_out.push(q.front());
    if (win) {
        ++consec;
        cur_rto = 2 * cur_rto;
    }
    timer.start(cur_rto);
}

unsigned int TCPSender::consecutive_retransmissions() const { return consec; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
