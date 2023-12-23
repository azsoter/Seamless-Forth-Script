/*
* forth_internal.h
*
*  Created on: Jan 30, 2023
*      Author: Andras Zsoter
*
* Copyright (c) 2023 Andras Zsoter
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*/

#include <forth_config.h>
#include <forth_internal.h>
#include <forth.h>
#include <string.h>

// A quick and dirty block editor.
// Probably nobody is going to edit a lot of stuff on the target system where this Forth engine is running
// but it is nice to have some ability to edit stuff if one needs that.
//
// Key biding may look weird but I am trying to use keys that are not processed by various terminals, telnet, etc.
//

#if defined(FORTH_INCLUDE_BLOCKS) && defined(FORTH_INCLUDE_BLOCK_EDITOR)
static void forth_show_block(forth_runtime_context_t *ctx, forth_cell_t src, forth_cell_t x, forth_cell_t y)
{
    ctx->at_xy(ctx, x, y);
    forth_PUSH(ctx, src);
    forth_list(ctx);
}

static void forth_edit_at_xy(forth_runtime_context_t *ctx, forth_cell_t x, forth_cell_t y)
{
    ctx->at_xy(ctx, x + 8, y + 1);
}

static forth_cell_t forth_key_event(forth_runtime_context_t *ctx)
{
    forth_ekey(ctx);
    ctx->user_break = 0;
    return forth_POP(ctx);
}

void forth_edit(forth_runtime_context_t *ctx)
{
    forth_cell_t src;
    forth_cell_t event;
    uint8_t *buffer;
    uint8_t c;
    uint8_t x0 = 0;
    uint8_t y0 = 4;
    uint8_t x = 0;
    uint8_t y = 0;
    uint16_t position = 0;
    uint16_t pos;
    int8_t done = 0;
    int8_t dirty = 0;
    int8_t f;
    uint8_t line_buffer[64];

    if ((0 == ctx->at_xy) || (0 == ctx->page))
    {
        forth_THROW(ctx, -21); // 	Unsupported operation.
    }

    memset(line_buffer, FORTH_CHAR_SPACE, sizeof(line_buffer));

    src = forth_POP(ctx);
    //forth_page(ctx);
    forth_PUSH(ctx, src);
    forth_block(ctx);
    buffer = (uint8_t *)forth_POP(ctx);

    forth_page(ctx);
    forth_TYPE0(ctx, "Press ESC to exit the editor."); forth_cr(ctx);
    forth_TYPE0(ctx,"CTRL-C/CTRL-Y: Copy Line, CTRL-E: Insert Empty Line, CTRL-X: Cut Line");
    forth_cr(ctx);
    forth_TYPE0(ctx,"CTRL-R: Replace (swap) Line, CTRL-W/CTRL-V: OverWrite Line");
    forth_show_block(ctx, src, x0, y0);

    while(!done)
    {

        forth_show_block(ctx, src, x0, y0);

        if (position > 1023)
        {
            position = 1023;
        }

        y = position / 64;
        x = position % 64;

        forth_edit_at_xy(ctx, x0 + x, y0 + y);

        event = forth_key_event(ctx);

        switch(event)
        {
            case FORTH_KEY_ESCAPE:
                done = 1;
                break;

            case FORTH_KEY_ENTER:
                //position = 64 * ((position + 63) / 64);
                position = (position + 64) & ~(forth_cell_t)63;

                if (1023 < position)
                {
                   position -= 64;
                }
                break;

            case FORTH_KEY_RIGHT:
                if (1023 > position)
                {
                    position += 1;
                }
                break;

            case FORTH_KEY_LEFT:
                if (0 < position)
                {
                    position -= 1;
                }
                break;

            case FORTH_KEY_UP:
                if (64 <= position)
                {
                    position -= 64;
                }
                break;

            case FORTH_KEY_DOWN:
                if (1023 >= (64 + position))
                {
                    position += 64;
                }
                break;

            case FORTH_KEY_HOME:
                position = position & ~(forth_cell_t)63;
                break;

            case FORTH_KEY_END:
                position = position & ~(forth_cell_t)63;
                position += 63;
                break;

            case FORTH_KEY_BS: /** FALLTHROUGH **/
            case FORTH_KEY_BACKSPACE:
                if ((0 < position) && (0 < x))
                {
                    position -= 1;
                    pos = position;
                    while(63 != (pos & 63))
                    {
                        buffer[pos] = buffer[pos + 1];
                        pos += 1;
                    }
                    buffer[pos] = FORTH_CHAR_SPACE;
                    dirty = 1;
                }
                break;

            case FORTH_KEY_DELETE:
                pos = position;
                while(63 != (pos & 63))
                {
                    buffer[pos] = buffer[pos + 1];
                    pos += 1;
                }
                buffer[pos] = FORTH_CHAR_SPACE;
                dirty = 1;
                break;

            case FORTH_KEY_INSERT:
                pos = position | 63;
                if ((pos != position) && (FORTH_CHAR_SPACE == buffer[pos]))
                {
                    while (pos != position)
                    {
                        buffer[pos] = buffer[pos - 1];
                        pos -= 1;
                    }
                    buffer[pos] = FORTH_CHAR_SPACE;
                    dirty = 1;
                }
                break;

            case FORTH_KEY_CTRL_C:
            case FORTH_KEY_CTRL_Y:
                memcpy(line_buffer, buffer + (y * 64), 64);
                forth_edit_at_xy(ctx, x0 + 0, y0 + 16);
                forth_PUSH(ctx, (forth_cell_t)line_buffer);
                forth_PUSH(ctx, 64);
                forth_type(ctx);
                break;

            case FORTH_KEY_CTRL_E:  // Insert Empty line.
                if (y < 15)
                {
                    f = 0;

                    for (pos = 0; pos < 64; pos++)
                    {
                        if (FORTH_CHAR_SPACE != buffer[(15*64) + pos])
                        {
                            f = 1;
                            break;
                        }
                    }

                    if (!f)
                    {
                        memmove(&(buffer[64*(y+1)]), &(buffer[64*y]), 1024 - (64 * y));
                        memset(&(buffer[64*y]), FORTH_CHAR_SPACE, 64);
                        position = 64*y;
                        dirty = 1;
                    }
                }
                break;

            case FORTH_KEY_CTRL_X:  // Cut Line.
                
                memcpy(line_buffer, buffer + (y * 64), 64);
                forth_edit_at_xy(ctx, x0 + 0, y0 + 16);
                forth_PUSH(ctx, (forth_cell_t)line_buffer);
                forth_PUSH(ctx, 64);
                forth_type(ctx);

                if (y < 15)
                {
                    memmove(&(buffer[64*y]), &(buffer[64*(y+1)]), 1024 - (64 * y));
                }

                memset(&(buffer[64*15]), FORTH_CHAR_SPACE, 64);
                position = 64*y;
                dirty = 1;
                break;

            case FORTH_KEY_CTRL_W: // OverWrite Line
                memcpy(&(buffer[64*y]), line_buffer, 64);
                position = 64*(y + 1);
                dirty = 1;
                break;

            case FORTH_KEY_CTRL_R: // Replace Line.
                for (pos = 0; pos < 64; pos++)
                {
                    c = line_buffer[pos];
                    line_buffer[pos] = buffer[(y * 64) + pos];
                    buffer[(y * 64) + pos] = c;
                }

                dirty = 1;

                forth_edit_at_xy(ctx, x0 + 0, y0 + 16);
                forth_PUSH(ctx, (forth_cell_t)line_buffer);
                forth_PUSH(ctx, 64);
                forth_type(ctx);
                break;

            default:
                if ((32 <= event) && (255 >= event))
                {
                    buffer[position++] = (uint8_t)event;
                    dirty = 1;
                }
                break;
        }
    }
    forth_page(ctx);
    if (dirty)
    {
        forth_update(ctx);
    }

}
#endif
