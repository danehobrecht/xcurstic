#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <X11/Xlib.h>
#include <linux/joystick.h>
#include <X11/extensions/XTest.h>

const int MAX_SPEED = 20;
const double MAX_VELOCITY = 50.0;
const double MAX_JOYSTICK = 32767;
const int CURSOR_UPDATE_FREQUENCY = 60;
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
		std::cerr << "Failed to open joystick device." << std::endl;
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
				}
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

		// Calculate cursor movement based on right joystick input
		int rx = 0, ry = 0;
		bytes = read(joy_fd, &js, sizeof(js_event));
		if (bytes == sizeof(js_event)) {
			switch (js.type & ~JS_EVENT_INIT) {
				case JS_EVENT_AXIS:
					if (js.number == 2) {
						// Right joystick X-axis
						rx = js.value;
					} else if (js.number == 3 ) {
						// Right joystick Y-axis
						ry = js.value;
					}
					break;

				default:
					break;
			}

			// Warp cursor based on right joystick input
			if (rx != 0 || ry != 0) {
				double x_scale = rx / MAX_JOYSTICK;
				double y_scale = ry / MAX_JOYSTICK;

				// Calculate the velocity based on how far the joystick is from the center
				double magnitude = sqrt(rx * rx + ry * ry);
				double velocity = magnitude / MAX_JOYSTICK * MAX_VELOCITY;

				// Warp cursor with a scaled velocity
				XWarpPointer(display, None, root_window, 0, 0, 0, 0, x_scale * velocity, y_scale * velocity);
				XFlush(display);
			}
		}
		usleep(1000000 / CURSOR_UPDATE_FREQUENCY);
	}

	// Close joystick device and X11 display
	close(joy_fd);
	XCloseDisplay(display);
	return 0;
}