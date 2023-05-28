#define RED PB1
#define GREEN PB2
#define BLUE PB3
#define BUTTON PD4
#define JOY_BUTTON PD2
#define VRX A0
#define VRY A1

const byte fullBlock[8] = {
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111,
	B11111
};

const byte outlineBlock[8] = {
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

