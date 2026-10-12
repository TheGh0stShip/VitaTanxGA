#include "btga_script.h"

#include <assert.h>

int btga_script_tests(void) {
    const uint8_t header[] = {1, 2, 0, 1};
    const uint8_t stream[] = {8, 7, 1, 0, 30, 21, 'G', 'O', 0, 2, 0};
    BtgaScriptStreamSummary summary;
    uint8_t scene, setting;
    uint16_t count;
    assert(btga_script_header(header, sizeof(header), &scene, &setting, &count));
    assert(scene == 1 && setting == 2 && count == 1);
    assert(btga_script_stream_validate(stream, sizeof(stream), &summary));
    assert(summary.stream_type == 7 && summary.command_count == 5);
    assert(summary.wait_command_count == 1 && summary.text_command_count == 1);
    assert(!btga_script_stream_validate(stream, sizeof(stream) - 1, &summary));
    {
        BtgaScriptCursor cursor;
        assert(btga_script_cursor_init(&cursor, stream, sizeof(stream)));
        assert(btga_script_cursor_tick(&cursor, false) == BTGA_SCRIPT_RUNNING);
        assert(cursor.wait_ticks == 30);
        assert(btga_script_cursor_tick(&cursor, true) == BTGA_SCRIPT_TEXT);
        assert(cursor.text_length == 2 && cursor.text[0] == 'G');
        assert(btga_script_cursor_tick(&cursor, false) == BTGA_SCRIPT_FINISHED);
    }
    return 0;
}
