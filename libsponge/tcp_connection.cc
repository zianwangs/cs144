#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().buffer_size(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return timer.time(); }

void TCPConnection::segment_received(const TCPSegment &seg) {
    timer.start(0);
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        inbound_stream().set_error();
        return;
    }
    if (seg.header().fin) fin_received = 1;
    if (seg.header().fin == 1 && !fin_sent) {
        _linger_after_streams_finish = false;
    }
    if (fin_sent && _sender.next_seqno() == seg.header().ackno) {
        fin_acked = true;
    }
    if (_receiver.ackno().has_value() && seg.header().seqno - _receiver.ackno().value() >= static_cast<int>(_receiver.window_size())) {
        if (seg.header().ack == 1)
            _sender.ack_received(seg.header().ackno, seg.header().win);
        goto ACK;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack == 1) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (lingering) goto ACK;
    if (fin_sent && fin_acked && fin_received && _linger_after_streams_finish) {
        lingering = true;
    }
    if (fin_sent && fin_acked && fin_received && !_linger_after_streams_finish) {
        return;
    }
ACK:
     if (seg.length_in_sequence_space() == 0) {
        return;
    }
     if (!syn_sent) {
         _sender.fill_window();
         syn_sent = true;
     } else {
        _sender.send_empty_segment();
     }
    while (!_sender.segments_out().empty()) {
        TCPSegment s = _sender.segments_out().front();
         _sender.segments_out().pop();
        if (s.length_in_sequence_space() == 0) {
            s.header().ack = 1;
            s.header().ackno = _receiver.ackno().value();
            s.header().win = _receiver.window_size();
        } else {
            optional<WrappingInt32> ackno = _receiver.ackno();
            if (ackno.has_value()) {
                s.header().ack = 1;
                s.header().ackno = ackno.value();
                s.header().win = _receiver.window_size();
            }
        }
        _segments_out.push(s);
    }


}

bool TCPConnection::active() const {
    if (_active == false) return false;
    if (outbound_stream().error() && inbound_stream().error()) return false;
    if (lingering) return true;
    if (!outbound_stream().eof()) return true;
    if (!inbound_stream().eof()) return true;
    if (!fin_sent) return true;
    if (bytes_in_flight() != 0) return true;
    return false;

}

size_t TCPConnection::write(const string &data) {
    size_t ans = _sender.stream_in().write(data);
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (seg.header().fin) fin_sent = true;
        _sender.segments_out().pop();
        optional<WrappingInt32> ackno = _receiver.ackno();
        if (ackno.has_value()) {
            seg.header().ack = 1;
            seg.header().ackno = ackno.value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    /*
    if (outbound_stream().eof() && !fin_sent)
        close();
        */
    return ans;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (timer.isRunning())
        timer.elapse(ms_since_last_tick);
    if (lingering) {
        if (timer.time() >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
        return;
    }
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        inbound_stream().set_error();
        outbound_stream().set_error();
        _sender.send_empty_segment();
        while (!_sender.segments_out().empty()) {
            if (_sender.segments_out().front().length_in_sequence_space() != 0) {
                _sender.segments_out().pop();
            } else {
                TCPSegment seg = _sender.segments_out().front();
                _sender.segments_out().pop();
                seg.header().rst = 1;
                _segments_out.push(seg);
                break;
            }
        }
    }
    while (!_sender.segments_out().empty()) {
          TCPSegment seg = _sender.segments_out().front();
          _sender.segments_out().pop();
          optional<WrappingInt32> ackno = _receiver.ackno();
          if (ackno.has_value()) {
              seg.header().ack = 1;
              seg.header().ackno = ackno.value();
              seg.header().win = _receiver.window_size();
          }
          _segments_out.push(seg);
    }

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    if (outbound_stream().eof())
        close();
}

void TCPConnection::close() {
    fin_sent = true;
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        optional<WrappingInt32> ackno = _receiver.ackno();
        if (ackno.has_value()) {
            seg.header().ack = 1;
            seg.header().ackno = ackno.value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }


}

void TCPConnection::connect() {
    _sender.fill_window();
    if (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        _segments_out.push(seg);
    }
    syn_sent = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            inbound_stream().set_error();
            outbound_stream().set_error();
            _sender.send_empty_segment();
            while (!_sender.segments_out().empty()) {
                if (_sender.segments_out().front().length_in_sequence_space()) {
                    _sender.segments_out().pop();
                } else {
                    TCPSegment seg = _sender.segments_out().front();
                    _sender.segments_out().pop();
                    seg.header().rst = 1;
                    _segments_out.push(seg);
                    break;
                }
            }
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
