#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <X11/Xlib.h>
#include <linux/joystick.h>
#include <X11/extensions/XTest.h>

const int MAX_SPEED = 8;
const int CLICK_DELAY = 50;
const int SCROLL_MULTIPLIER = 1;
const int CURSOR_UPDATE_FREQUENCY = 160;
const double MAX_VELOCITY = 50.0;
const double MAX_JOYSTICK = 32676;
const double JOYSTICK_CENTER_THRESHOLD = 0.1;

int main() {
	int joy_fd, num_of_axis;
	char const *device = "/dev/input/js0";
	Display *display;
	Window root_window;
	int dx = 0, dy = 0;
	double magnitude = 0;
	double joystick_center_x = 0, joystick_center_y = 0;

	struct js_event js;

	// Initialize X11 display and root window
	display = XOpenDisplay(NULL);
	root_window = XRootWindow(display, 0);

	joy_fd = open(device, O_RDONLY);
	if (joy_fd < 0) {
		std::cerr << "Error: Failed to detect Xbox One Controller (is it on?)." << std::endl;
		return 1;
	}

	// Set joystick to non-blocking mode
	fcntl(joy_fd, F_SETFL, O_NONBLOCK);

	while (true) {
		ssize_t bytes = read(joy_fd, &js, sizeof(js_event));
		if (bytes == -1 && errno == EAGAIN) {
			// No data available, continue loop
			continue;
		} else if (bytes == -1) {
			// Handle error
			continue;
		} else if (bytes != sizeof(js_event)) {
			// Handle incomplete or partial structure
			continue;
		}

		switch (js.type & ~JS_EVENT_INIT) {
			case JS_EVENT_AXIS:
				if (js.number == 0) {
					// Left joystick X-axis
					dx = js.value;
				} else if (js.number == 1 ) {
					// Left joystick Y-axis
					dy = js.value;
				} else if (js.number == 3) {
					// Right joystick Y-axis
					static int scroll_accum = 0;
					int scroll_speed = -js.value / (MAX_JOYSTICK / 2);
					scroll_accum += scroll_speed;
					if (scroll_accum >= 1 || scroll_accum <= -1) {
						int scroll_steps = scroll_accum / 2 * SCROLL_MULTIPLIER;
						for (int i = 0; i < abs(scroll_steps); i++) {
							XTestFakeButtonEvent(display, scroll_steps > 0 ? Button4 : Button5, True, CurrentTime);
							XTestFakeButtonEvent(display, scroll_steps > 0 ? Button4 : Button5, False, CurrentTime);
							XFlush(display);
							usleep(1000);
						}
						scroll_accum -= scroll_steps * 1.2;
					}
				}
				break;
			case JS_EVENT_BUTTON:
				// Left click
				if (js.number == 0 && js.value == 1) {
					XTestFakeButtonEvent(display, 1, True, CurrentTime);
					XFlush(display);
					usleep(CLICK_DELAY * 1000);
					XTestFakeButtonEvent(display, 1, False, CurrentTime);
					XFlush(display);
				// Right click
				} else if (js.number == 1 && js.value == 1) {
					XTestFakeButtonEvent(display, 3, True, CurrentTime);
					XFlush(display);
					usleep(CLICK_DELAY * 1000);
					XTestFakeButtonEvent(display, 3, False, CurrentTime);
					XFlush(display);
				// Super key
				} else if (js.number == 12 && js.value == 1) {
					XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Super_L), True, CurrentTime);
					XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Super_L), False, CurrentTime);
					XFlush(display);
				}
				std::cout << "Button " << static_cast<int>(js.number) << " " << (js.value ? "pressed" : "released") << std::endl;
				break;
			default:
				break;
		}

		// Calculate cursor movement based on left joystick input
		magnitude = sqrt(dx * dx + dy * dy);
		if (magnitude > MAX_JOYSTICK) {
			magnitude = MAX_JOYSTICK;
		}

		if (magnitude > JOYSTICK_CENTER_THRESHOLD * MAX_JOYSTICK) {
			double scale = (magnitude - JOYSTICK_CENTER_THRESHOLD * MAX_JOYSTICK) / (MAX_JOYSTICK - JOYSTICK_CENTER_THRESHOLD * MAX_JOYSTICK);
			int speed = round(MAX_SPEED * scale);
			int x_speed = round(speed * dx / magnitude);
			int y_speed = round(speed * dy / magnitude);
			XTestFakeRelativeMotionEvent(display, x_speed, y_speed, 0);
			XFlush(display);
		}
		usleep(1000000 / CURSOR_UPDATE_FREQUENCY);
	}

	// Close joystick device and X11 display
	close(joy_fd);
	XCloseDisplay(display);
	return 0;
}