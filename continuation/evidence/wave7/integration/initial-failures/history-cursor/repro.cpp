#include "rage/history_slot_slice.hpp"
#include <array>
#include <iostream>
int main() {
    using namespace rage::offline;
    alignas(16) std::array<std::uint8_t, history_slot_record_bytes> bytes{};
    HistoryDestinationBytes slots[]{{bytes.data(), bytes.size()}};
    HistorySlotArrayView view{5, 4, 0, slots, 1};
    const auto result = resolve_existing_history_slot(view);
    std::cout << "selected index=" << result.index << "; stored cursor=" << view.field_0c
              << "; source expected selected index=0, stored cursor=3\n";
    return result.index == 0 && view.field_0c == 3 ? 0 : 1;
}
