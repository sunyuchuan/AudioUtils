#ifndef _PITCH_TYPE_H_
#define _PITCH_TYPE_H_

typedef struct _PitchCandidate {
    float frequency;
    float strength;
} PitchCandidate;

typedef struct _PitchFrame {
    int nCandidates;
    float intensity;
    PitchCandidate *candidate;
} PitchFrame;

#endif