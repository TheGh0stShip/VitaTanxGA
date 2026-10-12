#include "btga/controller_pak.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using btga::ControllerPak;

    ControllerPak pak;
    assert(pak.valid());
    assert(pak.files_used() == 0);
    assert(pak.free_bytes() == 123 * ControllerPak::page_size);

    const auto formatted = pak.image();
    constexpr std::size_t id_offset = ControllerPak::block_size;
    constexpr std::size_t label_offset = 7 * ControllerPak::block_size;
    assert(formatted[id_offset + 25] == 1); // memory device
    assert(formatted[id_offset + 26] == 1); // one bank
    for (std::size_t i = 0; i < ControllerPak::block_size; ++i) {
        assert(formatted[label_offset + i] == 0);
    }

    ControllerPak::Name name{};
    name.company_code = 0x3548;
    name.game_code = 0x4E425145;
    name.game_name[0] = 0x1B;
    name.extension[0] = 0x1A;

    int file_number = -1;
    constexpr std::size_t file_size = 2 * ControllerPak::page_size;
    assert(pak.allocate(name, file_size, file_number) == ControllerPak::Error::none);
    assert(file_number == 0);
    assert(pak.files_used() == 1);
    assert(pak.free_bytes() == 121 * ControllerPak::page_size);

    int found = -1;
    assert(pak.find(name, found) == ControllerPak::Error::none);
    assert(found == file_number);

    std::array<std::uint8_t, file_size> written{};
    for (std::size_t i = 0; i < written.size(); ++i) {
        written[i] = static_cast<std::uint8_t>((i * 37U + 11U) & 0xFFU);
    }
    assert(pak.write(file_number, 0, written) == ControllerPak::Error::none);

    std::array<std::uint8_t, file_size> read{};
    assert(pak.read(file_number, 0, read) == ControllerPak::Error::none);
    assert(read == written);
    assert(pak.read(file_number, 1, read) == ControllerPak::Error::invalid);

    ControllerPak::FileState state{};
    assert(pak.state(file_number, state) == ControllerPak::Error::none);
    assert(state.size == file_size);
    assert(state.name.company_code == name.company_code);
    assert(state.name.game_code == name.game_code);

    const std::vector<std::uint8_t> image(pak.image().begin(), pak.image().end());
    ControllerPak restored;
    assert(restored.import_image(image));
    read.fill(0);
    assert(restored.read(file_number, 0, read) == ControllerPak::Error::none);
    assert(read == written);

    auto damaged_primary = image;
    damaged_primary[ControllerPak::page_size + 1] ^= 0xFF;
    ControllerPak recovered;
    assert(recovered.import_image(damaged_primary));
    read.fill(0);
    assert(recovered.read(file_number, 0, read) == ControllerPak::Error::none);
    assert(read == written);

    auto damaged_both = damaged_primary;
    damaged_both[2 * ControllerPak::page_size + 1] ^= 0xFF;
    assert(!recovered.import_image(damaged_both));

    assert(restored.remove(name) == ControllerPak::Error::none);
    assert(restored.files_used() == 0);
    assert(restored.free_bytes() == 123 * ControllerPak::page_size);
}
