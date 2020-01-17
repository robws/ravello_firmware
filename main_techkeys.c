#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "usb.h"
#include "main.h"
#include "auxiliary.h"
#include "gfx.h"
#include "time.h"
#include "buttons.h"
#include "macro.h"

const uint8_t PROGMEM robot[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0F, 0xD0, 0x00, 0x1F, 0xB0,
	0x00, 0x3F, 0x70, 0x00, 0x7E, 0xD0, 0x00, 0xFD,
	0x90, 0x01, 0xFB, 0x30, 0x00, 0x02, 0x60, 0x01,
	0xFA, 0xC0, 0x01, 0xFB, 0x80, 0x01, 0xFB, 0x00,
	0x01, 0xFA, 0x00, 0x00, 0x00, 0x80, 0x00, 0xBB,
	0xC0, 0x01, 0xBB, 0xE0, 0x03, 0xBB, 0xF0, 0x07,
	0xC7, 0xE0, 0x07, 0xFF, 0xD0, 0x03, 0xFF, 0xB0,
	0x05, 0xFF, 0x70, 0x06, 0x7E, 0xF0, 0x07, 0x99,
	0xF0, 0x07, 0xE7, 0xF0, 0x07, 0xEF, 0xF0, 0x07,
	0xEF, 0xE0, 0x07, 0xEF, 0x00, 0x07, 0xEC, 0x00,
	0x07, 0x80, 0x00, 0x06, 0x38, 0x00, 0x00, 0x38,
	0x00, 0x00, 0xBA, 0x00, 0x00, 0xC6, 0x00, 0x00,
	0xFE, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x38, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};

const uint8_t PROGMEM techkeys_scroll[] = {
	0x00, 0x00, 0x00, 0x90, 0xB3, 0x27, 0xAA, 0xA2,
	0x52, 0xA8, 0xB3, 0x72, 0xAA, 0x91, 0x52, 0x90,
	0xB3, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0x44,
	0xC9, 0xFE, 0x29, 0x29, 0xFE, 0x11, 0x29, 0xFE,
	0x11, 0x29, 0x7C, 0x11, 0x29, 0x38, 0x11, 0x29,
	0x10, 0x10, 0xC6
};


const uint8_t PROGMEM question[] = {
	0x0C, 0x00, 0x30, 0x12, 0x1C, 0x48, 0x02, 0x14,
	0x08, 0x0C, 0x7F, 0x30, 0x08, 0x55, 0x20, 0x00,
	0x7F, 0x00, 0x08, 0x00, 0x20
};

/* whole screen rectangle, for basic text drawing */
const rect_t screen_r = {0, 0, 24, 7};

uint8_t EEMEM ee_strings[4][MACRO_MAX_LEN+1] = {
	"Coraline❤",
	"Samuel❤",
	"Veronica❤",
	"Nellie❤"
};

static uint8_t next_symbol(uint8_t symbol)
{
	if (symbol == 'z')
		return ' ';
	else if (symbol == ' ')
		return 'a';
	else if (symbol == 'Z')
		return 'A';

	else if (symbol == '9')
		return '!';
	else if (symbol == '/')
		return ':';
	else if (symbol == '@')
		return '[';
	else if (symbol == '`')
		return '{';
	else if (symbol == '~')
		return '0';

	else if (symbol == 13)
		return 0x19;
	else if (symbol == 0x1d)
		return 1;

	return symbol+1;
}

static inline uint8_t prev_symbol(uint8_t symbol)
{
	if (symbol == 'a')
		return ' ';
	else if (symbol == ' ')
		return 'z';
	else if (symbol == 'A')
		return 'Z';

	else if (symbol == '{')
		return '`';
	else if (symbol == '[')
		return '@';
	else if (symbol == ':')
		return '/';
	else if (symbol == '!')
		return '9';
	else if (symbol == '0')
		return '~';

	else if (symbol == 1)
		return 0x1d;
	else if (symbol == 0x19)
		return 13;

	return symbol-1;
}

int main(void)
{
	clock_prescale_set(clock_div_1);

	sei();
	USB_init();

	BUTTONS_init();

	GFX_init();
	uint8_t macro_len = 1;

	/* program edit mode */
	int8_t prog_mode = 0;
	/* after holding "PROGRAM", waiting for key choice to reprogram */
	bool prog_mode_select = false;

	TIME_delay_ms(200);
	for (uint8_t i = 0; i < 40; ++i)
	{
		TIME_delay_ms(30);
		GFX_draw_bitmap(screen_r, 4, 0, robot, 3, 0, i);
		GFX_swap();
	}
	TIME_delay_ms(150);

	int8_t scroll = 0;
	int trans_phase = 0;
	uint64_t transition_start = 0;
	/* the letter the current last letter should morph to or 0 when no
	 * morphing occurs */
	char morphing_to_letter = 0;
	bool once_released = false;
	while (true) {
		/* render to screen */
		int scroll_px = 0;
		if (scroll == 1) {
			MACRO_set(macro_len, 'a');
			MACRO_set(macro_len + 1, 0);
			scroll_px = -(int)(TIME_get() - transition_start)*6/150;
			if (scroll_px <= -6) {
				scroll_px = 0;
				++macro_len;
				scroll = 0;
			}
		} else if (scroll == -1) {
			scroll_px = (TIME_get() - transition_start)*6/150;
			if (scroll_px >= 6) {
				scroll_px = 0;
				MACRO_set(macro_len - 1, 0);
				--macro_len;
				scroll = 0;
			}
		} else if (morphing_to_letter) {
			trans_phase = (TIME_get() - transition_start)*5/130;
			if (trans_phase >= 5) {
				trans_phase = 0;
				MACRO_set(macro_len-1, morphing_to_letter);
				morphing_to_letter = 0;
			}
		}

		if (prog_mode_select) {
			GFX_draw_bitmap(screen_r, 4, 0, question, 3, 0, 0);
		} else if (prog_mode == 0) {
			const int t = TIME_get() % 7000;
			if (t < 3000)
				GFX_draw_bitmap(screen_r, 2, 0,
						techkeys_scroll, 3, 0, 0);
			else if (t < 3500)
				GFX_draw_bitmap(screen_r, 2, 0,
						techkeys_scroll, 3, 0, (t-3000) / 50);
			else if (t < 6500)
				GFX_draw_bitmap(screen_r, 2, 0,
						techkeys_scroll, 3, 0, 10);
			else
				GFX_draw_bitmap(screen_r, 2, 0,
						techkeys_scroll, 3, 0, (7000-t) / 50);
		} else {
			uint8_t brightness;
			int t = TIME_get() % 300;
			if (t < 220)
				brightness = min(99, t)/20;
			else
				brightness = min(49, (300-t))/10;
			int position;
			if (macro_len <= 4)
				position = scroll_px;
			else
				position = scroll_px - 6*(macro_len - 4);
			GFX_put_text(screen_r, position, 0,
					TEXT_EEP, MACRO_get_ptr(), macro_len - 1, 4, 0);
			if (!morphing_to_letter) {
				GFX_put_text(screen_r, position + 6*(macro_len-1), 0,
						TEXT_EEP, MACRO_get_ptr() + macro_len - 1, 1, brightness, 0);
			} else {
				GFX_put_text(screen_r, position + 6*(macro_len-1), 0,
						TEXT_EEP, MACRO_get_ptr() + macro_len - 1, 1, 4 - trans_phase, 0);
				uint8_t tmp[2];
				tmp[0] = morphing_to_letter;
				tmp[1] = 0;
				GFX_put_text(screen_r, position + 6*(macro_len-1), 0,
						TEXT_RAM, tmp, 1, trans_phase, 0);
			}
		}
		GFX_swap();

		if (prog_mode > 0) {
			/* check key clicks */
			if (BUTTONS_has_been_clicked(K_UP)) {
				if (!morphing_to_letter && !scroll) {
					morphing_to_letter = MACRO_get(macro_len - 1);
					if (macro_len > 1 && is_sticky_mod(MACRO_get(macro_len - 2))) {
						do
							morphing_to_letter = prev_symbol(morphing_to_letter);
						while (is_sticky_mod(morphing_to_letter));
					} else {
						morphing_to_letter = prev_symbol(morphing_to_letter);
					}
					transition_start = TIME_get();
				}
			} else if (BUTTONS_has_been_clicked(K_DOWN)) {
				if (!morphing_to_letter && !scroll) {
					morphing_to_letter = MACRO_get(macro_len - 1);
					if (macro_len > 1 && is_sticky_mod(MACRO_get(macro_len - 2))) {
						do
							morphing_to_letter = next_symbol(morphing_to_letter);
						while (is_sticky_mod(morphing_to_letter));
					} else {
						morphing_to_letter = next_symbol(morphing_to_letter);
					}
					transition_start = TIME_get();
				}
			} else if (BUTTONS_has_been_clicked(K_LEFT)) {
				if (!morphing_to_letter && !scroll && macro_len > 1) {
					if (macro_len <= 4) {
						MACRO_set(--macro_len, 0);
					} else {
						scroll = -1;
						transition_start = TIME_get();
					}
				}
			} else if (BUTTONS_has_been_clicked(K_RIGHT)) {
				if (!morphing_to_letter && !scroll && macro_len < MACRO_MAX_LEN) {
					if (macro_len < 4) {
						MACRO_set(macro_len, 'a');
						MACRO_set(++macro_len, 0);
					} else {
						scroll = 1;
						transition_start = TIME_get();
					}
				}
			}
			/* check key holds */
			if (BUTTONS_has_been_held(K_PROG)) {
				prog_mode = 0;
			} else if (once_released && BUTTONS_has_been_released(K_PROG)) {
				if (islower(MACRO_get(macro_len-1)))
					morphing_to_letter = 'A';
				else if (isupper(MACRO_get(macro_len-1)))
					morphing_to_letter ='0';
				else if (MACRO_get(macro_len-1) >= 32)
					morphing_to_letter = 1;
				else
					morphing_to_letter = 'a';
				transition_start = TIME_get();
			} else if (BUTTONS_has_been_released(K_PROG)) {
				once_released = true;
			}
		} else if (prog_mode_select) {
			for (int i = 0; i < 5; ++i) {
				if (!BUTTONS_has_been_clicked(i))
					continue;
				int8_t clicked = i;
				if (0 <= clicked && clicked <= 3) {
					//UP, DOWN, LEFT, RIGHT ARROW
					prog_mode = clicked+1;
					prog_mode_select = false;
					macro_len = MACRO_init(&ee_strings[clicked][0]);
				} else if (clicked == K_PROG) {
					prog_mode = 0;
					prog_mode_select = false;
				}
			}
		} else { /* regular mode */
			if (BUTTONS_has_been_held(K_PROG)) {
				prog_mode_select = true;
				once_released = false;
			} else {
				for (int i = 0; i < 5; ++i) {
					if (!BUTTONS_has_been_clicked(i))
						continue;
					int8_t clicked = i;
					if (clicked == K_PROG) {
						//TODO
					} else if (clicked >= 0) {
						if (MACRO_init(&ee_strings[clicked][0]) == 1) {
							MACRO_write(true, false);
						} else {
							MACRO_write(true, true);
						}
					}
				}
				for (int i = 0; i < 5; ++i) {
					if (!BUTTONS_has_been_released(i))
						continue;
					int8_t released = i;
					if (released >= 0 && MACRO_init(&ee_strings[released][0]) == 1)
						MACRO_write(false, true);
				}
			}
		}
	}
}


void MAIN_handle_sof()
{
	TIME_update_1ms();
	BUTTONS_task();
}
