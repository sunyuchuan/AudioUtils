#ifndef _LOW_PASS_H_
#define _LOW_PASS_H_

void LowPassIIR(float *in, short in_len, float *out, short *out_len);

#define MWSPT_NSEC 5
static const int NL[MWSPT_NSEC] = { 1,3,1,2,1 };
static const float IIR_B[MWSPT_NSEC][3] = {
  {
  0.001751076314,              0,              0
  },
  {
				1,              2,              1
  },
  {
	0.04101465642,              0,              0
  },
  {
				1,              1,              0
  },
  {
				1,              0,              0
  }
};
static const int DL[MWSPT_NSEC] = { 1,3,1,2,1 };
static const float IIR_A[MWSPT_NSEC][3] = {
  {
				1,              0,              0
  },
  {
				1,   -1.911110044,   0.9181143045
  },
  {
				1,              0,              0
  },
  {
				1,  -0.9179706573,              0
  },
  {
				1,              0,              0
  }
};

#endif