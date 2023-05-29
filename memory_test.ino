#include <LiquidCrystal_I2C.h>
#include "memory_test.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

pos positions[10]; // pozitii numere pe LCD

pos currPos; // pozitia curenta
pos prevPos; // pozitia anterioara

int currNums; // numarul de cifre afisate
volatile int nextNum; // urmatorul numar ce trebuie ghicit

volatile int mistakes; // greseli per runda (maxim 3)
int strikes; // runde gresite (maxim 3)

volatile bool newRound;
bool inGame;
bool gameEnded;

bool roundEndedCorrectly;
bool roundEndedWrongly;

volatile bool buttonPressed;

unsigned long lastButtonPressTime = 0;
unsigned long changeLEDduration = 200; // durata noii culori a LED-ului dupa ghicirea unui numar

unsigned long debounceDelay = 200;

volatile unsigned long timerCount; // timpul petrecut intr-o runda

float avgResponseTime; // timpul mediu de raspuns dupa incheierea jocului (calculat luand in considerare doar rundele terminate corect)
unsigned long totalResponseTime;
unsigned int roundCount;

/* seteaza culoarea data a LED-ului */
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

/* obtine urmatoarea miscare in functie de pozitia joystick-ului */
pos getMove(int x, int y) {

	if (x <= 100) {
		return pos(-1, 0); // stanga
	}
	if (x >= 1000) {
		return pos(1, 0); // dreapta
	}
	if (y <= 100) {
		return pos(0, -1); // sus
	}
	if (y >= 1000) {
		return pos(0, 1); // jos
	}

	return pos(0, 0);
}

/* actualizeaza pozitia cursorului dupa miscarea efectuata, daca aceasta este valida */
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

/* seteaza pozitii aleatorii pentru numere, diferite intre ele */
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

/* verifica daca un numar exista deja la pozitia data */
bool existsPos(pos posToCheck) {
	for (int i = 0; i < currNums; i++) {
		if ((positions[i].x == posToCheck.x) && (positions[i].y == posToCheck.y)) {
			return true;
		}
	}
	return false;
}

/* afiseaza numarul dat la pozitia data */
void printNum(pos atPos, int x) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.print(String(x));
}

/* afiseaza un numar acoperit la pozitia data */
void printSquare(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.write(1);
}

/* afiseaza cursorul la pozitia data */
void printBlock(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.write(0);
}

/* sterge caracterul de la pozitia data */
void clearBlock(pos atPos) {
	lcd.setCursor(atPos.x, atPos.y);
	lcd.print(" ");
}

/* afiseaza mesajul corespunzator dupa terminarea jocului in functie de corectitudine
si timpul mediu de raspuns */
void printGameEnded(bool correct) {
    if (correct) {
        lcd.setCursor(4, 0);
		lcd.print("VICTORY");
    } else {
		lcd.setCursor(3, 0);
		lcd.print("GAME OVER");
    }
    lcd.setCursor(0, 1);
    lcd.print("AVG. TIME:");
    lcd.print(avgResponseTime);
    lcd.print("s");
}

/* afiseaza un mesaj corespunzator dupa terminarea rundei, in functie de corectitudine
si numarul de strike-uri curente */
void printNextRound(bool correct) {
    lcd.setCursor(0, 0);
    if (correct) {
		lcd.print("Press for next");
    } else {
        lcd.setCursor(0, 0);
		lcd.print("Press to restart");
	}
    lcd.setCursor(0, 1);
	lcd.print("round. Strikes=");
    lcd.print(strikes);
}

/* afiseaza numerele, incepand cu urmatorul numar ce trebuie ghicit, drept numere in sine
sau acoperite in functie de parametrul dat */
void printNums(bool revealed) {
	for (int i = (nextNum - 1); i < currNums; i++) {
        if (positions[i].x == currPos.x && positions[i].y == currPos.y) {
            continue; // daca numarul de afisat se afla la pozitia curenta a cursorului nu se afiseaza pentru a evita overlap-ul
        }
		if (revealed) {
			printNum(positions[i], (i + 1));
		} else {
			printSquare(positions[i]);
		}
	}
}

/* obtine numarul de la pozitia data */
int getNumAtPos(pos atPos) {
	for (int i = 0; i < currNums; i++) {
		if ((positions[i].x == atPos.x) && (positions[i].y == atPos.y)) {
			return (i + 1);
		}
	}
	return -1;
}

/* intrerupere care trateaza apasarea butonului joystick-ului, cu semnificatia
ghicirii pozitiei unui numar; nu are efect in afara unei runde */
ISR(INT0_vect)
{
    if (inGame == false) {
        return;
    }

    /* trateaza o singura apasare de buton */
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
  
    if (millis() - lastInterruptTime < debounceDelay) {
        lastInterruptTime = interruptTime;
        return;
    }
    lastInterruptTime = interruptTime;

    buttonPressed = true;

    lastButtonPressTime = millis();

    if (getNumAtPos(currPos) == nextNum) {
        setColor(false, true, false);
        nextNum += 1;
    } else {
        if (existsPos(currPos)) {
            setColor(true, false, false);
            mistakes += 1;
        }
    }
}

/* intrerupere care trateaza apasarea butonului, cu semnificatia inceperii 
unei noi runde; nu are efect in timpul unei runde */
ISR(PCINT2_vect) {
    if ((PIND & (1 << BUTTON)) == 0) {
        if (inGame == false) {
	        newRound = true;
        }
    }
}

/* intrerupere corespunzatoare timer-ului */
ISR(TIMER1_OVF_vect)
{
    timerCount += 1;
}

/* configureaza si activeaza intretuperile: buton joystick: INT0 si buton: PCINT20 */
void setup_interrupts() {
    cli();
 
    /* seteaza input-ul drept INPUT_PULLUP */
    DDRD &= ~(1 << JOY_BUTTON) & ~(1 << BUTTON);
    PORTD |= (1 << JOY_BUTTON) | (1 << BUTTON);
 
    /* configureaza intreruperile externe */
    EICRA &= ~(1 << ISC00);
    EICRA |= (1 << ISC01);
 
    /* configureaza intreruperile de tip pin change */
    PCICR |= (1 << PCIE2);
 
    /* activeaza intreruperile externe */
    EIMSK |= (1 << INT0);
 
    /* activeaza intreruperile de tip pin change */
    PCMSK2 |= (1 << PCINT20);

    sei();
}

/* configureaza timer-ul in modul normal pentru masurarea timpului */
void setup_timer() {

    cli();

    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    TCCR1B |= (1 << CS12); // seteaza prescaler-ul la 256
    TIMSK1 |= (1 << TOIE1);

    sei();
}

void setup() {

    setup_interrupts();
    setup_timer();

    roundCount = 0;
    timerCount = 0;
    totalResponseTime = 0;

    DDRB |= (1 << RED) | (1 << GREEN) | (1 << BLUE); // seteaza LED-ul drept output
    setColor(false, false, true); // initial se va afisa culoarea albastra

	for (int i = 0; i < 10; i++) {
		positions[i] = pos(0, 0);
	}

    currPos = pos(0, 0);
    prevPos = pos(-1, -1);

    currNums = 4;
	nextNum = 1;

    mistakes = 0;
    strikes = 0;

	newRound = true;
    inGame = true;
    gameEnded = false;

    roundEndedCorrectly = false;
    roundEndedWrongly = false;

    buttonPressed = false;

	lcd.init();
	lcd.backlight();

	lcd.createChar(0, fullBlock);
	lcd.createChar(1, outlineBlock);

	lcd.clear();
}

void loop() {

    /* afiseaza un mesaj corespunzator in functie de starea curenta a jocului, respectiv
    afiseaza numerele la pozitiile corespunzatoare */

    if (roundEndedCorrectly) {
        if (gameEnded) {
            printGameEnded(true);
        } else {
            printNextRound(true);
        }
    } else if (roundEndedWrongly) {
        if (gameEnded) {
            printGameEnded(false);
        } else {
            printNextRound(false);
        }   
    } else {
        if (nextNum >= 2) {
            printNums(false);
        } else {
            printNums(true);
        }
    }

	if (newRound) {
        lcd.clear();

        timerCount = 0;

        if (roundEndedCorrectly) {
            currNums += 1;
        }
	    nextNum = 1;

        if (gameEnded) {
            gameEnded = false;
            currNums = 4;
            strikes = 0;
        }

        mistakes = 0;

	    currPos = pos(0, 0);
        prevPos = pos(-1, -1);

		setRandomPositions();

		newRound = false;
        inGame = true;
        roundEndedCorrectly = false;
        roundEndedWrongly = false;
	}

    if (inGame) {

        /* sterge vechea pozitie a cursorului */
        if (prevPos.x != -1 && prevPos.y != -1) {
            if (!(prevPos.x == currPos.x && prevPos.y == currPos.y)) {
                clearBlock(prevPos);
            }
        }

        prevPos = currPos;

        /* citeste pozitia joystick-ului */
        int xVal = analogRead(VRX);
        int yVal = analogRead(VRY);

        /* muta cursorul conform joystick-ului */
        pos nextMove = getMove(xVal, yVal);
        pos nextPos = updatePos(currPos, nextMove);

        currPos = nextPos;

        printBlock(currPos);

        /* daca de la apasarea butonului joystick-ului a trecut timpul definit reface
        culoarea LED-ului albastra */
        if (buttonPressed && (millis() - lastButtonPressTime >= changeLEDduration)) {
            buttonPressed = false;
            setColor(false, false, true);
        }

        /* trateaza cazurile de terminare a rundei, respectiv a jocului */

        if (mistakes == 3) {
            strikes += 1;
            if (strikes == 3) {
                gameEnded = true;
            }
            roundEndedWrongly = true;
            inGame = false;
            lcd.clear();
        }

        if (nextNum > currNums) {
            if (currNums == 9) {
                gameEnded = true;
            }
            roundEndedCorrectly = true;
            inGame = false;
            totalResponseTime += timerCount;
            roundCount += 1;
            lcd.clear();
        }

        if (gameEnded) {
            if (roundCount == 0) { // nicio runda terminata corect
                avgResponseTime = 0;
            } else {
                avgResponseTime = (float)totalResponseTime / roundCount;
            }
            totalResponseTime = 0;
            roundCount = 0;
        }
    }

    delay(100);
}
