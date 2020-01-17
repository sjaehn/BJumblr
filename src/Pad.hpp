#ifndef PAD_HPP_
#define PAD_HPP_

struct Pad
{
	Pad () : Pad (0) {}
	Pad (float level) :
		level (level) {}
	bool operator== (Pad& that) {return (level == that.level);}
	bool operator!= (Pad& that) {return (!operator== (that));}

	float level;
};

#endif /* PAD_HPP_ */
