#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}
#define myabs(a,b) ((a) > (b) ? (a) - (b) : (b) - (a))
using namespace std; 

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t valid = n & 0xFFFFFFFF;
    return isn + valid;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t base = n - isn;
    uint64_t mask = 0xFFFFFFFF00000000ul;
    uint64_t origin = (checkpoint & mask) + base;
    uint64_t upper = origin + (1ul << 32);
    uint64_t lower = origin - (1ul << 32);
    uint64_t diff1 = myabs(origin, checkpoint);
    uint64_t diff2 = myabs(upper, checkpoint);
    uint64_t diff3 = myabs(lower, checkpoint);
    if (diff1 < diff2 && diff1 < diff3) return origin;
    if (diff2 < diff1 && diff2 < diff3) return upper;
    return lower;
}
