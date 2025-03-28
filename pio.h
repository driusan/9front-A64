enum {
	PioInput	= 0,
	PioOutput	= 1,
	PioDisable	= 7,

	/* pin specific values, verify with datasheet before using */
	PioInterrupt	= 6,
};

enum {
	PioEIntPos = 0,
	PioEIntNeg = 1,
	PioEIntHigh = 2,
	PioEIntLow = 3,
	PioEIntDouble = 4
};