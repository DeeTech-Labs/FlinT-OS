#include "ble/peer_address.h"
#include <cstdio>
int main() {
    flint::ble::PeerAddress peer;
    if (!flint::ble::parsePeerAddress("1:012345aBcDeF", peer) || peer.type != 1 ||
        peer.bytes != std::array<uint8_t, 6>{1, 0x23, 0x45, 0xab, 0xcd, 0xef})
        return 1;
    if (!flint::ble::parsePeerAddress("0:000000000000", peer) || peer.type != 0)
        return 1;
    for (const auto text : {"", "0:", "2:012345abcdef", "0-012345abcdef", "0:012345abcdeg",
             "0:012345abcde", "0:012345abcdef0", " 0:012345abcdef", "0:012345abcdef "}) {
        if (flint::ble::parsePeerAddress(text, peer))
            return 1;
        if (peer.type || peer.bytes != std::array<uint8_t, 6>{})
            return 1; // Failure preserves output.
    }
    constexpr char embedded[] = "0:012345\0bcdef";
    if (flint::ble::parsePeerAddress(std::string_view(embedded, 14), peer))
        return 1;
    puts("BLE peer address bounds, byte order, case and invalid inputs: passed");
}
