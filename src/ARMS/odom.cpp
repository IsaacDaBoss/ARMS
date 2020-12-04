#include "ARMS/odom.h"
#include "ARMS/chassis.h"
#include "ARMS/pid.h"
#include "api.h"

using namespace pros;

namespace odom {

bool debug;
double left_right_distance;
double middle_distance;
double left_right_tpi;
double middle_tpi;
double exit_error;

// odom tracking values
double global_x;
double global_y;
double heading;
double heading_degrees;
double prev_heading = 0;
double prev_left_pos = 0;
double prev_right_pos = 0;
double prev_middle_pos = 0;

int odomTask() {

	global_x = 0;
	global_y = 0;

	while (true) {
		double left_pos;
		double right_pos;
		double middle_pos;

		// get positions of each encoder
		if (chassis::leftEncoder) {
			left_pos = chassis::leftEncoder->get_value();
			right_pos = chassis::rightEncoder->get_value();
		} else {
			left_pos = chassis::leftMotors->getPosition();
			right_pos = chassis::rightMotors->getPosition();
		}

		if (chassis::middleEncoder)
			middle_pos = chassis::middleEncoder->get_value();

		// calculate change in each encoder
		double delta_left = (left_pos - prev_left_pos) / left_right_tpi;
		double delta_right = (right_pos - prev_right_pos) / left_right_tpi;
		double delta_middle = (middle_pos - prev_middle_pos) / middle_tpi;

		// calculate new heading
		double delta_angle;
		if (chassis::imu) {
			heading_degrees = chassis::imu->get_rotation();
			heading = heading_degrees * M_PI / 180.0;
			delta_angle = heading - prev_heading;
		} else {
			delta_angle = (delta_left - delta_right) / (left_right_distance * 2.0);
			heading += delta_angle;
			heading_degrees = heading * 180.0 / M_PI;
		}

		// store previous positions
		prev_left_pos = left_pos;
		prev_right_pos = right_pos;
		prev_middle_pos = middle_pos;
		prev_heading = heading;

		// calculate local displacement
		double local_x;
		double local_y;

		if (delta_angle) {
			double i = sin(delta_angle / 2.0) * 2.0;
			local_y = (delta_right / delta_angle + left_right_distance) * i;
			local_x = (delta_middle / delta_angle + middle_distance) * i;
		} else {
			local_y = delta_right;
			local_x = delta_middle;
		}

		double p = heading - delta_angle / 2.0; // global angle

		// convert to absolute displacement
		global_y += cos(p) * local_y - sin(p) * local_x;
		global_x += sin(p) * local_y + cos(p) * local_x;

		if (debug)
			printf("%.2f, %.2f, %.2f \n", global_x, global_y, heading_degrees);

		delay(10);
	}
}

double getAngleError(std::array<double, 2> point) {
	double y = point[0];
	double x = point[1];

	y -= global_y;
	x -= global_x;

	double delta_theta = heading - atan2(x, y);

	while (fabs(delta_theta) > M_PI) {
		delta_theta -= 2 * M_PI * delta_theta / fabs(delta_theta);
	}

	return delta_theta;
}

double getDistanceError(std::array<double, 2> point) {
	double y = point[0];
	double x = point[1];

	y -= global_y;
	x -= global_x;
	return sqrt(x * x + y * y);
}

void moveAsync(std::array<double, 2> point, double max) {
	chassis::reset();
	chassis::maxSpeed = max;
	pid::pointTarget = point;
	pid::mode = ODOM;
}

void holoAsync(std::array<double, 2> point, double angle, double max) {
	chassis::reset();
	chassis::maxSpeed = max;
	pid::pointTarget = point;
	pid::angularTarget = angle;
	pid::mode = ODOM_HOLO;
}

void move(std::array<double, 2> point, double max) {
	moveAsync(point, max);
	delay(450);
	chassis::waitUntilSettled();
}

void holo(std::array<double, 2> point, double angle, double max) {
	holoAsync(point, angle, max);
	delay(450);
	chassis::waitUntilSettled();
}

void init(bool debug, double left_right_distance, double middle_distance,
          double left_right_tpi, double middle_tpi) {
	odom::debug = debug;
	odom::left_right_distance = left_right_distance;
	odom::middle_distance = middle_distance;
	odom::left_right_tpi = left_right_tpi;
	odom::middle_tpi = middle_tpi;
	delay(1500);
	Task odom_task(odomTask);
}

} // namespace odom
