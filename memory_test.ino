#include <LiquidCrystal_I2C.h>

#define RED PB1
#define GREEN PB2
#define BLUE PB3
#define BUTTON PD4
#define JOY_BUTTON PD2
#define VRX A0
#define VRY A1

byte fullBlock[8] = {
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111
};

byte outlineBlock[8] = {
	B11111,
	B10001,
	B10001,
	B10001,
	B10001,
	B10001,
	B10001,
	B11111
};

struct pos {
	int x;
	int y;

	pos() {
		x = 0;
		y = 0;
	}

	pos(int xVal, int yVal) {
		x = xVal;
		y = yVal;
	}
};

pos getMove(int x, int y) {

	if (x <= 100) {
		return pos(-1, 0);
	}
	if (x >= 1000) {
		return pos(1, 0);
	}
	if (y <= 100) {
		return pos(0, -1);
	}
	if (y >= 1000) {
		return pos(0, 1);
	}

	return pos(0, 0);
}

pos updatePos(pos currPos, pos nextMove) {

	pos nextPos = currPos;

	if ((currPos.x + nextMove.x) >= 0 && (currPos.x + nextMove.x) <= 15) {
		nextPos.x += nextMove.x;
	}
	if ((currPos.y + nextMove.y) >= 0 && (currPos.y + nextMove.y) <= 1) {
		nextPos.y += nextMove.y;
	}

	return nextPos;
}

void setColor(bool R, bool G, bool B) {

    if (R) {
        PORTB |= (1 << RED);
    } else {
        PORTB &= ~(1 << RED);
    }
    if (G) {
        PORTB |= (1 << GREEN);
    } else {
        PORTB &= ~(1 << GREEN);
    }
    if (B) {
        PORTB |= (1 << BLUE);
    } else {
        PORTB &= ~(1 << BLUE);
    }
}

LiquidCrystal_I2C lcd(0x27, 16, 2);

pos currPos = pos(0, 0);
pos prevPos = pos(-1, -1);

pos positions[10];

volatile int wrong;

int currNums;

volatile int nextNum;

volatile bool newRound;

volatile bool buttonPressed;

unsigned long lastButtonPressTime = 0;
unsigned long changeLEDduration = 200;

unsigned long debounceDelay = 200;

void setRandomPositions() {
	randomSeed(analogRead(0));

	for (int i = 0; i < currNums; i++) {

		pos newPos;

		while (true) {
			int x = random(1, 16);
			int y = random(0, 2);

			newPos = pos(x, y);

			if (existsPos(newPos) == false) {
				break;
			}
		}
		positions[i] = newPos;
	}
}

bool existsPos(pos posToCheck) {
	for (int i = 0; i < currNums; i++) {
		if ((positions[i].x == posToCheck.x) && (positions[i].y == posToCheck.y)) {
			return true;
		}
	}
	return false;
}

void printNum(pos atPos, int x) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.print(String(x));
}

void printSquare(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.write(1);
}

void printBlock(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.write(0);
}

void clearBlock(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.print(" ");
}

void printNextRound(bool correct) {
    if (correct) {
        lcd.setCursor(0, 0);
		lcd.print("Press button");
		lcd.setCursor(0, 1);
		lcd.print("for next round");
    } else {
        lcd.setCursor(0, 0);
		lcd.print("Press button");
		lcd.setCursor(0, 1);
		lcd.print("to restart round");
	}
}

void printNums(bool revealed) {

	for (int i = (nextNum - 1); i < currNums; i++) {
		if (revealed) {
			printNum(positions[i], (i + 1));
		} else {
			printSquare(positions[i]);
		}
	}
}

int getNumAtPos(pos atPos) {
	for (int i = 0; i < currNums; i++) {
		if ((positions[i].x == atPos.x) && (positions[i].y == atPos.y)) {
			return (i + 1);
		}
	}
	return -1;
}

ISR(INT0_vect)
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
  
    if (millis() - lastInterruptTime < debounceDelay) {
        lastInterruptTime = interruptTime;
        return;
    }

    lastInterruptTime = interruptTime;

    // cod întrerupere externă
    buttonPressed = true;

    lastButtonPressTime = millis();

    if (getNumAtPos(currPos) == nextNum) {
        setColor(false, true, false);
        nextNum += 1;
    } else {
        setColor(true, false, false);
        wrong += 1;
    }

    //PORTD ^= (1 << PD7);
}
 
ISR(PCINT2_vect) {
    // cod intrerupere de tip pin change
    if ((PIND & (1 << BUTTON)) == 0){
        // intreruperea a fost generata de pinul BUTTON / PCINT20
        // verificam nivelul logic al pinului
        //PORTD ^= (1 << PD7);
        //buttonPressed = true;

	    newRound = true;
    }
}

void setup_interrupts() {
    // buton joystick: JOY_BUTTON / INT0
    // buton : BUTTON / PCINT20
    cli();
 
    // input
    DDRD &= ~(1 << JOY_BUTTON) & ~(1 << BUTTON);
    // input pullup
    PORTD |= (1 << JOY_BUTTON) | (1 << BUTTON);
 
    // configurare intreruperi
    // intreruperi externe
    EICRA &= ~(1 << ISC00);
    EICRA |= (1 << ISC01);
 
    // intreruperi de tip pin change (activare vector de intreruperi)
    PCICR |= (1 << PCIE2);
 
    // activare intreruperi
    // intrerupere externa
    EIMSK |= (1 << INT0);
 
    // intrerupere de tip pin change
    PCMSK2 |= (1 << PCINT20);

    sei();
}

void setup() {

    Serial.begin(9600);

    setup_interrupts();

    DDRB |= (1 << RED) | (1 << GREEN) | (1 << BLUE);

    buttonPressed = false;

    wrong = 0;

	currNums = 4;
	nextNum = 1;

	newRound = true;

	for (int i = 0; i < 10; i++) {
		positions[i] = pos(0, 0);
	}

	lcd.init();
	lcd.backlight();

	lcd.createChar(0, fullBlock);
	lcd.createChar(1, outlineBlock);

	lcd.clear();

    setColor(false, false, true);
}

bool roundEndedCorrectly = false;
bool roundEndedWrongly = false;
bool inGame = true;

void loop() {

	if (newRound) {
        lcd.clear();
        inGame = true;
        wrong = 0;
        if (roundEndedCorrectly) {
            currNums += 1;
        }
	    nextNum = 1;
	    currPos = pos(0, 0);
		setRandomPositions();
		newRound = false;
        roundEndedCorrectly = false;
        roundEndedWrongly = false;
	}

    if (roundEndedCorrectly) {
        printNextRound(true);
    } else if (roundEndedWrongly) {
        printNextRound(false);
    } else {
        if (nextNum > 2) {
            printNums(false);
        } else {
            printNums(true);
        }
    }

	if (prevPos.x != -1 && prevPos.y != -1) {
        if (!(prevPos.x == currPos.x && prevPos.y == currPos.y)) {
		    clearBlock(prevPos);
        }
	}

    if (inGame) {

        if (wrong == 3) {
            roundEndedWrongly = true;
            inGame = false;
            lcd.clear();
        }

        if (nextNum > currNums) {
            roundEndedCorrectly = true;
            inGame = false;
            lcd.clear();
        }

        prevPos = currPos;

        int xVal = analogRead(VRX);
        int yVal = analogRead(VRY);

        pos nextMove = getMove(xVal, yVal);
        pos nextPos = updatePos(currPos, nextMove);

        if (existsPos(nextPos)) {
            clearBlock(nextPos);
        }

        currPos = nextPos;

        printBlock(currPos);

        if (buttonPressed && (millis() - lastButtonPressTime >= changeLEDduration)) {
            buttonPressed = false;  // Reset button press flag
            setColor(false, false, true);  // Set LED color back to blue
        }
    }

    delay(100);
}
