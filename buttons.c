#include "buttons.h"
#include "time.h"

#include <avr/io.h>

static volatile uint8_t state = 0x00;
static volatile uint8_t clicked = 0x00;
static volatile uint8_t released = 0x00;
static volatile uint8_t held = 0x00;

void BUTTONS_init()
{
	DDRD &= ~(_BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5) | _BV(PB6));
	PORTD |= _BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5) | _BV(PB6);
}

bool IO_get(uint8_t num)
{
	switch (num) {
	case 0:
		return PIND & _BV(PD5);
	case 1:
		return PIND & _BV(PD6);
	case 2:
		return PIND & _BV(PD4);
	case 3:
		return PIND & _BV(PD3);
	case 4:
		return PIND & _BV(PD2);
	default:
		return false;
	}
}

static volatile uint64_t last_pressed;
static volatile int8_t last_pressed_no = -1;

void BUTTONS_task()
{
	static uint64_t last_time = 0;
	const uint64_t time = TIME_get();
	if (time < last_time + DEBOUNCE_DELAY_MS)
		return;
	last_time = time;
	for (int i = 0; i < 5; ++i) {
		const bool newstate = !IO_get(i);
		if (!(state & (1 << i)) && newstate) {
			clicked |= 1 << i;
			last_pressed = time;
			last_pressed_no = i;
		} else if ((state & (1 << i)) && !newstate) {
			last_pressed_no = -1;
			released |= 1 << i;
		} else if (time > last_pressed + 1000 && last_pressed_no == i) {
			last_pressed_no = -1;
			held |= 1 << i;
		}
		if (newstate)
			state |= 1 << i;
		else
			state &= ~(1 << i);
	}
}

bool BUTTONS_get(uint8_t num)
{
	return state & (1 << num);
}

bool BUTTONS_has_been_clicked(uint8_t num)
{
	const bool c = clicked & (1 << num);
	clicked &= ~(1 << num);
	return c;
}

bool BUTTONS_has_been_released(uint8_t num)
{
	const bool r = released & (1 << num);
	released &= ~(1 << num);
	return r;
}

bool BUTTONS_has_been_held(uint8_t num)
{
	const bool h = held & (1 << num);
	held &= ~(1 << num);
	return h;
}
