#include "GyverButton.h"

GButton::GButton(uint8_t pin) {
	_PIN = pin;
	init();
}

GButton::GButton(uint8_t pin, boolean type, boolean dir) {
	_PIN = pin;
	flags.type = type;
	flags.mode = dir;
	init();
}

void GButton::init() {
	if (flags.type) pinMode(_PIN, INPUT);
	else pinMode(_PIN, INPUT_PULLUP);
	flags.tickMode = AUTO;
	_debounce = 80;
	_timeout = 300;
	_click_timeout = 500;
	_step_timeout = 400;
	flags.btn_state = !flags.mode;
	flags.btn_flag = false;
	flags.hold_flag = false;
	flags.counter_flag = false;
	flags.isHolded_f = false;
	flags.isRelease_f = false;
	flags.isPress_f = false;
	flags.step_flag = false;
	flags.oneClick_f = false;
	flags.isOne_f = false;
	flags.inv_state = flags.mode;
	btn_counter = 0;
	last_counter = 0;
	btn_timer = 0;
}

void GButton::setDebounce(uint16_t debounce) {
	_debounce = debounce;
}

void GButton::setTimeout(uint16_t timeout) {
	_timeout = timeout;
}

void GButton::setClickTimeout(uint16_t timeout) {
	_click_timeout = timeout;
}

void GButton::setStepTimeout(uint16_t step_timeout) {
	_step_timeout = step_timeout;
}

void GButton::setType(boolean type) {
	flags.type = type;
	if (flags.type) pinMode(_PIN, INPUT);
	else pinMode(_PIN, INPUT_PULLUP);
}

void GButton::setDirection(boolean dir) {
	flags.mode = dir;
	flags.inv_state = flags.mode;
}

void GButton::setTickMode(boolean tickMode) {
	flags.tickMode = tickMode;
}

void GButton::tick() {
	boolean pinState = digitalRead(_PIN);
	if (flags.type == HIGH_PULL) {
		pinState = !pinState;  // Инвертируем для HIGH_PULL (кнопка подключена к GND)
	}
	tick(pinState);
}

void GButton::tick(boolean state) {
	flags.btn_flag = state;
	boolean btnState = flags.btn_flag;
	
	uint32_t now = millis();
	
	if (btnState && !flags.btn_state) {
		if ((now - btn_timer) >= _debounce || btn_timer == 0) {
			flags.btn_state = true;
			btn_timer = now;
			flags.isPress_f = true;
			flags.oneClick_f = true;
			flags.counter_flag = true;
			btn_counter++;
		}
	} else {
		flags.isPress_f = false;
	}
	
	if (!btnState && flags.btn_state) {
		if ((now - btn_timer) >= _debounce || btn_timer == 0) {
			flags.btn_state = false;
			btn_timer = now;
			flags.isRelease_f = true;
			flags.hold_flag = false;
			flags.step_flag = false;
		}
	}
	
	if (flags.btn_state && ((now - btn_timer) >= _timeout) && !flags.hold_flag) {
		flags.hold_flag = true;
		flags.isHolded_f = true;
		flags.step_flag = true;
		flags.counter_flag = false;
		btn_counter = 0;
		last_counter = 0;
	} else {
		flags.isHolded_f = false;
	}
	
	if (flags.btn_state && flags.hold_flag && ((now - btn_timer) >= _step_timeout)) {
		flags.step_flag = true;
		btn_timer = now;
	} else {
		flags.step_flag = false;
	}
	
	if (!flags.btn_state) {
		flags.hold_flag = false;
	}
	
	if ((now - btn_timer) >= _click_timeout) {
		if (btn_counter != 0) {
			last_counter = btn_counter;
			btn_counter = 0;
		}
		flags.counter_flag = false;
	}
}

boolean GButton::isPress() {
	if (flags.isPress_f) {
		flags.isPress_f = false;
		return true;
	}
	return false;
}

boolean GButton::isRelease() {
	if (flags.isRelease_f) {
		flags.isRelease_f = false;
		return true;
	}
	return false;
}

boolean GButton::isClick() {
	if (flags.tickMode) tick();
	if (flags.oneClick_f && !flags.btn_state && !flags.hold_flag) {
		flags.oneClick_f = false;
		return true;
	}
	return false;
}

boolean GButton::isHolded() {
	if (flags.tickMode) tick();
	if (flags.isHolded_f) {
		flags.isHolded_f = false;
		return true;
	}
	return false;
}

boolean GButton::isHold() {
	if (flags.tickMode) tick();
	return flags.btn_state;
}

boolean GButton::state() {
	if (flags.tickMode) tick();
	return flags.btn_state;
}

boolean GButton::isSingle() {
	if (flags.tickMode) tick();
	if (flags.counter_flag && last_counter == 1) {
		flags.counter_flag = false;
		last_counter = 0;
		return true;
	}
	return false;
}

boolean GButton::isDouble() {
	if (flags.tickMode) tick();
	if (flags.counter_flag && last_counter == 2) {
		flags.counter_flag = false;
		last_counter = 0;
		return true;
	}
	return false;
}

boolean GButton::isTriple() {
	if (flags.tickMode) tick();
	if (flags.counter_flag && last_counter == 3) {
		flags.counter_flag = false;
		last_counter = 0;
		return true;
	}
	return false;
}

boolean GButton::hasClicks() {
	if (flags.tickMode) tick();
	if (flags.counter_flag) {
		flags.counter_flag = false;
		return true;
	}
	return false;
}

uint8_t GButton::getClicks() {
	if (flags.tickMode) tick();
	if (flags.counter_flag) {
		uint8_t clicks = last_counter;
		flags.counter_flag = false;
		last_counter = 0;
		return clicks;
	}
	return 0;
}

boolean GButton::isStep() {
	if (flags.tickMode) tick();
	if (flags.step_flag) {
		return true;
	}
	return false;
}
