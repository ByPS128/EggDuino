

void initHardware(){
	// enable eeprom wait in avr/eeprom.h functions
	SPMCSR &= ~SELFPRGEN;

	loadPenPosFromEE();
// SM,10000,1000,1000
#ifdef BOARD_ULN2003
	pinMode(engraverPin, OUTPUT);
	rotMotor.setMaxSpeed(800.0);
	rotMotor.setAcceleration(300.0);
	penMotor.setMaxSpeed(800.0);
	penMotor.setAcceleration(300.0);
#else
	pinMode(enableRotMotor, OUTPUT);
	pinMode(enablePenMotor, OUTPUT);
	pinMode(engraverPin, OUTPUT);
	rotMotor.setMaxSpeed(2000.0);
	rotMotor.setAcceleration(10000.0);
	penMotor.setMaxSpeed(2000.0);
	penMotor.setAcceleration(10000.0);
#endif
	motorsOff();
	penServo.attach(servoPin);
	penServo.write(penState);
}

void loadPenPosFromEE() {
	penUpPos = eeprom_read_word(penUpPosEEAddress);
	penDownPos = eeprom_read_word(penDownPosEEAddress);
	servoRateUp = eeprom_read_word(penUpRateEEAddress);
	servoRateDown = eeprom_read_word(penDownRateEEAddress);
	penState = penUpPos;
}

void storePenUpPosInEE() {
	eeprom_update_word(penUpPosEEAddress, penUpPos);
}

void storePenDownPosInEE() {
	eeprom_update_word(penDownPosEEAddress, penDownPos);
}

void sendAck(){
	Serial.print("OK\r\n");
}

void sendError(const char *command){
    Serial.print("!8 Err: Unknown command");
    if (command != nullptr) {
        Serial.print(": ");
        Serial.print(command); // Přidá command na konec zprávy pouze pokud není nullptr
    }
    Serial.print("\r\n");
}

void sendRetry(const char *command){
    Serial.print("RETRY: ");
    if (command != nullptr) {
        Serial.print(command); // Přidá command na konec zprávy pouze pokud není nullptr
    }
    Serial.print("\r\n");
}

void motorsOff() {
#ifdef BOARD_ULN2003
	for (byte i= 2; i < 10; i++)
		digitalWrite(i, LOW);
#else
	digitalWrite(enableRotMotor, HIGH);
	digitalWrite(enablePenMotor, HIGH);
#endif
	motorsEnabled = 0;
}

void motorsOn() {
#ifndef BOARD_ULN2003
	digitalWrite(enableRotMotor, LOW) ;
	digitalWrite(enablePenMotor, LOW) ;
#endif
	motorsEnabled = 1;
}

void toggleMotors() {
	if (motorsEnabled) {
		motorsOff();
	} else {
		motorsOn();
	}
}

bool parseSMArgs(uint16_t *duration, int *penStepsEBB, int *rotStepsEBB) {
	char *arg1;
	char *arg2;
	char *arg3;
	arg1 = SCmd.next();
	if (arg1 != NULL) {
		*duration = atoi(arg1);
		arg2 = SCmd.next();
	}
	if (arg2 != NULL) {
		*penStepsEBB = atoi(arg2);
		arg3 = SCmd.next();
	}
	if (arg3 != NULL) {
		*rotStepsEBB = atoi(arg3);

		return true;
	}

	return false;
}

void prepareMove(uint16_t duration, int penStepsEBB, int rotStepsEBB) {
	if (!motorsEnabled) {
		motorsOn();
	}
	if( (1 == rotStepCorrection) && (1 == penStepCorrection) ){ // if coordinatessystems are identical
		//set Coordinates and Speed

#ifdef BOARD_ULN2003
        // map 3200x800 eggbot corrdinates to our 28BYJ-48's penStepsPerRev and rotStepsUseable

/*
 * original
        rotStepsEBB = map(rotStepsEBB, 0, 3200, 0, penStepsPerRev);
        penStepsEBB = map(penStepsEBB, 0, 800, 0, rotStepsUseable);
*/

    long rotStepsX16 = (long)(rotStepsEBB * 16L);
    long penStepsX16 = (long)(penStepsEBB * 16L);

    // Compare regular solution against 16x magnified solution
    long rotSteps = (long)((rotStepsEBB) * rotScale) + (rotStepError / 16);
    long penSteps = (long)((penStepsEBB) * penScale) + (penStepError / 16);

    rotStepsX16 = (long)((rotStepsX16 * rotScale) + rotStepError);
    penStepsX16 = (long)((penStepsX16 * penScale) + penStepError);

    // Compute new error terms
    rotStepError = rotStepsX16 - (rotSteps * 16L);
    penStepError = penStepsX16 - (penSteps * 16L);

    rotStepsEBB = rotSteps;
    penStepsEBB = penSteps;

#endif

		rotMotor.move(rotStepsEBB);
		rotMotor.setSpeed( abs( (float)rotStepsEBB * (float)1000 / (float)duration ) );
		penMotor.move(penStepsEBB);
		penMotor.setSpeed( abs( (float)penStepsEBB * (float)1000 / (float)duration ) );
	} else {
		//incoming EBB-Steps will be multiplied by 16, then Integer-maths is done, result will be divided by 16
		// This make thinks here really complicated, but floating point-math kills performance and memory, believe me... I tried...
		long rotSteps =   (  (long)rotStepsEBB * 16 / rotStepCorrection) + (long)rotStepError;	//correct incoming EBB-Steps to our microstep-Setting and multiply  by 16 to avoid floatingpoint...
		long penSteps =   (  (long)penStepsEBB * 16 / penStepCorrection) + (long)penStepError;

		int rotStepsToGo = (int) (rotSteps/16);		//Calc Steps to go, which are possible on our machine
		int penStepsToGo = (int) (penSteps/16);

		rotStepError = (long)rotSteps - ((long) rotStepsToGo * (long)16);	// calc Position-Error, if there is one
		penStepError = (long)penSteps - ((long) penStepsToGo * (long)16);

		long temp_rotSpeed =  ((long)rotStepsToGo * (long)1000 / (long)duration );	// calc Speed in Integer Math
		long temp_penSpeed =  ((long)penStepsToGo * (long)1000 / (long)duration ) ;

		float rotSpeed= (float) abs(temp_rotSpeed);	// type cast
		float penSpeed= (float) abs(temp_penSpeed);

		//set Coordinates and Speed
		rotMotor.move(rotStepsToGo);		// finally, let us set the target position...
		rotMotor.setSpeed(rotSpeed);		// and the Speed!
		penMotor.move(penStepsToGo);
		penMotor.setSpeed( penSpeed );
	}
}

void moveOneStep() {
	if ( penMotor.distanceToGo() || rotMotor.distanceToGo() ) {
		penMotor.runSpeedToPosition(); // Moving.... moving... moving....
		rotMotor.runSpeedToPosition();
	}
}

void moveToDestination() {
	while ( penMotor.distanceToGo() || rotMotor.distanceToGo() ) {
		penMotor.runSpeedToPosition(); // Moving.... moving... moving....
		rotMotor.runSpeedToPosition();
	}
}

void setprgButtonState(){
	prgButtonState = 1;
}
