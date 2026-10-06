#pragma once
// What the game runs on, read once at start: Wine or Windows, the adapter it draws with
// and how much memory that adapter has, Steam Deck or not. Windows API only; the host
// test build returns an empty Machine.
#include "catalog.hpp"

namespace tuner {
Machine detect_machine();
// The adapter maker from a DXGI vendor id: 0x10DE NVIDIA, 0x1002 AMD, 0x8086 Intel.
std::string vendor_name(unsigned vendor_id);
}
