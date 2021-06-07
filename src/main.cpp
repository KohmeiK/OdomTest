#include "main.h"
#include "singleton.hpp"
#include "okapi/impl/util/timer.hpp"

//Initialize pointer to zero so that it can be initialized in first call to getInstance
Singleton *Singleton::instance = 0;
auto imuZ = IMU(19, IMUAxes::z);
Controller controller;

int numberOfLoops = 0;
bool settled = false;
float wrappedIMU = 0;
float startingOffsetDeg = 0;

float sumError = 0;
float lastError = 0;
float deltaT = 0;

MotorGroup Conveyor = MotorGroup({-1,9});
MotorGroup Intake = MotorGroup({18,-16});
MotorGroup Indexer = MotorGroup({4,-8});

void pidTurnToAngle(float targetDegree){
	Singleton *s = s->getInstance();
	shared_ptr<OdomChassisController> chassis = s->getChassis();

	// float kP = 0.0055; //0.0056
	// float kI = 0.002;
	float kI = 0.00054;//0.005
	// float kD = 0.00015;
	// float kD = 0.00024;//hh

	float kP = 0.0048; //0.0056
	// float kI = 0;
	float kD = 0;
	float targetAngle = targetDegree;

	float error = targetAngle - wrappedIMU;

	unsigned long lastTime = pros::millis();
	lastError = 0;
	sumError = 0;


	SettledUtil settledUtil( //5 deg, 5 deg /sec, hold for 250ms
	std::make_unique<Timer>(), 2, 3, 100_ms);

	while(!settledUtil.isSettled(error)){
		unsigned long now = pros::millis();
		deltaT = (now - lastTime)/1000.0; //ms to s

		error = targetAngle - wrappedIMU;

		if((lastError < 0) != (error < 0)){
			// sumError = -sumError*0.4;
			sumError = 0;
		}

		sumError += (error * deltaT);

		float deriError = (error-lastError)/deltaT;

		lastTime = now;
		lastError = error;

		float turnPower = kP*error + kI*sumError + kD*deriError;

		chassis->getModel()->left(turnPower);
		chassis->getModel()->right(-turnPower);

		pros::delay(20);
	}

}

void IMUWrapperUpdate(){
	float currentAngle = 0;
	float lastAngle = 0;
	while(true){

		currentAngle = imuZ.get();
		if (currentAngle == 0){
			currentAngle = 0.0001;
		}
		if(currentAngle > 0 && lastAngle < 0 && abs(currentAngle) > 90.0){
			numberOfLoops--;
		}
		else if(currentAngle < 0 && lastAngle > 0 && abs(currentAngle) > 90.0){
			numberOfLoops++;
		}

		wrappedIMU = currentAngle + (360.0 * numberOfLoops) + startingOffsetDeg;
		lastAngle = currentAngle;

		pros::delay(20);
	}
}

void displayTaskFnc(){
	Singleton *s = s->getInstance();
	shared_ptr<OdomChassisController> chassis = s->getChassis();

	controller.clear();

	while(true){

		char buff[100];
		char buff2[100];
		snprintf(buff, sizeof(buff), "(%3.1f,%3.1f)",chassis->getState().x.convert(inch),chassis->getState().y.convert(inch));
		// snprintf(buff2, sizeof(buff2), "T:%3.2f",chassis->getState().theta.convert(degree));
		snprintf(buff2, sizeof(buff2), "wIMU:%3.2f (%d)",wrappedIMU, numberOfLoops);

		std::string buffAsStdStr = buff;
		std::string buff2AsStdStr = buff2;
		controller.setText(1,3, buff2);

		pros::lcd::print(0, "Dt: %f", deltaT);
		pros::lcd::print(1, "Sum: %f", sumError);

		// pros::lcd::print(4, "L: %d", chassis->getModel()->getSensorVals()[0]);
		// pros::lcd::print(5, "R: %d", chassis->getModel()->getSensorVals()[1]);
		// pros::lcd::print(6, "B: %d", chassis->getModel()->getSensorVals()[2]);

		pros::lcd::print(4, "X: %f", chassis->getState().x.convert(inch));
		pros::lcd::print(5, "Y: %f", chassis->getState().y.convert(inch));
		pros::lcd::print(6, "T: %f", chassis->getState().theta.convert(degree));

		pros::delay(20);
	}
}

/**
 * A callback function for LLEMU's center button.
 *
 * When this callback is fired, it will toggle line 2 of the LCD text between
 * "I was pressed!" and nothing.
 */
void on_center_button() {
	static bool pressed = false;
	pressed = !pressed;
	if (pressed) {
		pros::lcd::set_text(2, "I was pressed!");
	} else {
		pros::lcd::clear_line(2);
	}
}

/**
 * Runs initialization code. This occurs as soon as the program is started.
 *
 * All other competition modes are blocked by initialize; it is recommended
 * to keep execution time for this mode under a few seconds.
 */
void initialize() {
	pros::lcd::initialize();

	Singleton *s = s->getInstance(); //Make Chassis object (only happens on first call)

	pros::Task IMUTask(IMUWrapperUpdate); // Continous IMU readings use "wrappedIMU"
	pros::Task displayTask(displayTaskFnc);

}

/**
 * Runs while the robot is in the disabled state of Field Management System or
 * the VEX Competition Switch, following either autonomous or opcontrol. When
 * the robot is enabled, this task will exit.
 */
void disabled() {}

/**
 * Runs after initialize(), and before autonomous when connected to the Field
 * Management System or the VEX Competition Switch. This is intended for
 * competition-specific initialization routines, such as an autonomous selector
 * on the LCD.
 *
 * This task will exit when the robot is enabled and autonomous or opcontrol
 * starts.
 */
void competition_initialize() {}

/**
 * Runs the user autonomous code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the autonomous
 * mode. Alternatively, this function may be called in initialize or opcontrol
 * for non-competition testing purposes.
 *
 * If the robot is disabled or communications is lost, the autonomous task
 * will be stopped. Re-enabling the robot will restart the task, not re-start it
 * from where it left off.
 */
void autonomous() {

	controller.clear();
	controller.setText(1, 0, "The IMU Calibrates...");
	pros::delay(100);
	imuZ.calibrate();
	pros::delay(3000);
	controller.clear();

	Singleton *s = s->getInstance();
	shared_ptr<OdomChassisController> chassis = s->getChassis();
	shared_ptr<OdomChassisController> chassisRev = s->getChassisRev();
	shared_ptr<ChassisController> chassisOld = s->getChassisOld();

	std::shared_ptr<AsyncMotionProfileController> profileController =
	AsyncMotionProfileControllerBuilder()
		.withLimits({
			2.0, // Maximum linear velocity of the Chassis in m/s
			1.3, // Maximum linear acceleration of the Chassis in m/s/s
			4.0 // Maximum linear jerk of the Chassis in m/s/s/s
		})
		.withOutput(chassis)
		.buildMotionProfileController();

	std::shared_ptr<AsyncMotionProfileController> revProfileController =
	AsyncMotionProfileControllerBuilder()
		.withLimits({
			2.0, // Maximum linear velocity of the Chassis in m/s
			1.3, // Maximum linear acceleration of the Chassis in m/s/s
			4.0 // Maximum linear jerk of the Chassis in m/s/s/s
		})
		.withOutput(chassisRev)
		.buildMotionProfileController();

	std::shared_ptr<AsyncMotionProfileController> turnProfileController =
	AsyncMotionProfileControllerBuilder()
		.withLimits({
			1.0, // Maximum linear velocity of the Chassis in m/s
			0.6, // Maximum linear acceleration of the Chassis in m/s/s
			2.0 // Maximum linear jerk of the Chassis in m/s/s/s
		})
		.withOutput(chassis)
		.buildMotionProfileController();

		startingOffsetDeg = -57;

		//Start here!;
		profileController->generatePath(
    {{0_ft, 0_ft, 0_deg}, {30.5_in, 0_ft, 0_deg}}, "FirstStraight");
		profileController->setTarget("FirstStraight");
		profileController->waitUntilSettled();

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {8_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(-135);

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {9_in, 0_ft, 0_deg}}, "TowardsGoal1");
		profileController->setTarget("TowardsGoal1");
		profileController->waitUntilSettled();\

		pros::delay(1000); //Score Goal 1

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {9_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(68+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {31_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pidTurnToAngle(185+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {15_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {5_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		chassis->getModel()->left(0);
		chassis->getModel()->right(0);

		pros::delay(1000); //Score Goal 2

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {4_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(88+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {18_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pidTurnToAngle(82+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {15_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {8_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(137+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {15_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pros::delay(1000); //Score Goal 3

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {4_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(-21+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {37_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pidTurnToAngle(93+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {15_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pros::delay(1000); //Score Goal 4

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {4_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(-10+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {25_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pidTurnToAngle(110+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {15_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {8_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();

		pidTurnToAngle(45+(360*-1));

		profileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {8_in, 0_ft, 0_deg}}, "LongShotToBall");
		profileController->setTarget("LongShotToBall");
		profileController->waitUntilSettled();

		pros::delay(1000); //Score Goal 5

		revProfileController->generatePath(
		{{0_ft, 0_ft, 0_deg}, {4_in, 0_ft, 0_deg}}, "REVToGoal1");
		revProfileController->setTarget("REVToGoal1");
		revProfileController->waitUntilSettled();


}

/**
 * Runs the operator control code. This function will be started in its own task
 * with the default priority and stack size whenever the robot is enabled via
 * the Field Management System or the VEX Competition Switch in the operator
 * control mode.
 *
 * If no competition control is connected, this function will run immediately
 * following initialize().
 *
 * If the robot is disabled or communications is lost, the
 * operator control task will be stopped. Re-enabling the robot will restart the
 * task, not resume it from where it left off.
 */
void opcontrol() {

		Controller controller;
		controller.clear();
		ControllerButton XButton(ControllerDigital::X);
		ControllerButton AButton(ControllerDigital::A);
		ControllerButton BButton(ControllerDigital::B);
		ControllerButton CalButton(ControllerDigital::up);
		ControllerButton L1Button(ControllerDigital::L1);
		ControllerButton L2Button(ControllerDigital::L2);
		ControllerButton R1Button(ControllerDigital::R1);
		ControllerButton R2Button(ControllerDigital::R2);
		Singleton *s = s->getInstance();
		shared_ptr<OdomChassisController> chassis = s->getChassis();




		// chassis->moveDistance(24_in); // Drive forward 12 inches

		// profileController->generatePath(
    // {{0_ft, 0_ft, 0_deg}, {2.5_ft, 0_ft, 0_deg}}, "A");
		//
		// turnProfileController->generatePath(
		// {{0_ft, 0_ft, 0_deg}, {2_ft, 2_ft, 0_deg}}, "B");


		while (true) {
		// Arcade drive with the left stick
		chassis->getModel()->arcade(controller.getAnalog(ControllerAnalog::leftY),
															controller.getAnalog(ControllerAnalog::rightX));

		if (L1Button.isPressed()){
			Intake.moveVoltage(127*100);
			Conveyor.moveVoltage(127*100);
			Indexer.moveVoltage(-40*100);
		}

		else if (L2Button.isPressed()){
			Intake.moveVoltage(-127*100);
			Conveyor.moveVoltage(-127*100);
			Indexer.moveVoltage(-127*100);
		}

		else if (R1Button.isPressed()){
			Conveyor.moveVoltage(127*100);
			Indexer.moveVoltage(127*100);
			Intake.moveVoltage(0);
		}
		else{
			Intake.moveVoltage(0);
			Conveyor.moveVoltage(0);
			Indexer.moveVoltage(0);
		}
		// Run the test autonomous routine if we press the button
		if (XButton.changedToPressed()) {
				// Drive the robot in a square pattern using closed-loop control
				// chassis->turnAngle(90_deg);   // Turn in place 90 degrees
				// chassis->getModel()->resetSensors();
				// auto imuZ = IMU(19, IMUAxes::z);

		}

		if (CalButton.changedToPressed()) {
			imuZ.calibrate();
			controller.clear();
			controller.setText(1, 0, "The IMU Calibrates...");
			pros::delay(3000);
			controller.clear();
		}
		if (AButton.changedToPressed()) {
					// chassis->moveDistance(24_in); // Drive forward 12 inches
					// // profileController->setTarget("A");
					// // profileController->waitUntilSettled();
					// // turnProfileController->setTarget("B");
					// // turnProfileController->waitUntilSettled();
					pidTurnToAngle(80);
		}

		if (BButton.changedToPressed()) {
			autonomous();
		}
	}
}
