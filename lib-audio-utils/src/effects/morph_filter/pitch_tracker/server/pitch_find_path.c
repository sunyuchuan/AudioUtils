#include "pitch_find_path.h"
#include <math.h>
#include <stdlib.h>
#include "pitch_type.h"
#define NUMlog2e 1.442695041f
#define NUMlog2(x) (log(x) * NUMlog2e)

int Pitch_getMaxnCandidates(PitchFrame **frame_list, int frame_num) {
    int result = 0, cand_num, i;
    for (i = 0; i < frame_num; i++) {
        cand_num = frame_list[i]->nCandidates;
        if (cand_num > result) {
            result = cand_num;
        }
    }
    return result;
}

int PitchPathFinder(PitchFrame **frame_list, float silenceThreshold,
                    float voicingThreshold, float octaveCost,
                    float octave_jump_cost, float voiced_unvoiced_cost,
                    float ceiling, int frame_num) {
    int max_cand_num = Pitch_getMaxnCandidates(frame_list, frame_num);
    int place, i, icand, icand1, icand2, iframe, **psi, voiceless, *cur_psi;
    float maximum, value, **delta, unvoicedStrength, f1, f2, transitionCost,
        octaveJumpCost, voicedUnvoicedCost;  // volatile
    float *prev_delta, *cur_delta;
    float timeStepCorrection = 0.01f * 75.0f;  // 75 is minimum frequency
    int previousVoiceless, currentVoiceless;
    PitchFrame *frame, *prev_frame, *cur_frame;
    PitchCandidate *candidate, help;

    octaveJumpCost = octave_jump_cost * timeStepCorrection;
    voicedUnvoicedCost = voiced_unvoiced_cost * timeStepCorrection;

    delta = (float **)malloc(sizeof(float *) * frame_num);
    if (delta == NULL) {
        return -1;
    }

    psi = (int **)malloc(sizeof(int *) * frame_num);
    if (psi == NULL) {
        return -1;
    }
    for (i = 0; i < frame_num; i++) {
        delta[i] = (float *)malloc(sizeof(float) * max_cand_num);
        if (delta[i] == NULL) {
            return -1;
        }
        psi[i] = (int *)malloc(sizeof(int) * max_cand_num);
        if (psi[i] == NULL) {
            return -1;
        }
    }

    for (iframe = 0; iframe < frame_num; iframe++) {
        if (iframe == 711) {
            iframe = 711;
        }
        frame = frame_list[iframe];
        unvoicedStrength =
            silenceThreshold <= 0.0f
                ? 0.0f
                : (2.0f - ((1.0f + voicingThreshold) * frame->intensity) /
                              silenceThreshold);
        unvoicedStrength = voicingThreshold +
                           (unvoicedStrength > 0.0f ? unvoicedStrength : 0.0f);
        for (icand = 0; icand < frame->nCandidates; icand++) {
            candidate = &frame->candidate[icand];
            voiceless = (candidate->frequency == 0.0f ||
                         candidate->frequency > ceiling);
            delta[iframe][icand] =
                voiceless
                    ? unvoicedStrength
                    : (candidate->strength +
                       octaveCost * NUMlog2(candidate->frequency / ceiling));
        }
    }

    /* Look for the most probable path through the maxima.
       There is a cost for the voiced/unvoiced transition,
       and a cost for a frequency jump. */
    for (iframe = 1; iframe < frame_num; iframe++) {
        prev_frame = frame_list[iframe - 1];
        cur_frame = frame_list[iframe];
        prev_delta = delta[iframe - 1];
        cur_delta = delta[iframe];
        cur_psi = psi[iframe];

        for (icand2 = 0; icand2 < cur_frame->nCandidates; icand2++) {
            f2 = cur_frame->candidate[icand2].frequency;
            maximum = -1e10f;
            place = 0;
            for (icand1 = 0; icand1 < prev_frame->nCandidates; icand1++) {
                f1 = prev_frame->candidate[icand1].frequency;
                previousVoiceless = ((f1 <= 0.0f) || (f1 >= ceiling));
                currentVoiceless = ((f2 <= 0.0f) || (f2 >= ceiling));
                if (currentVoiceless) {
                    if (previousVoiceless) {
                        transitionCost = 0.0f;  // both voiceless
                    } else {
                        transitionCost =
                            voicedUnvoicedCost;  // voiced-to-unvoiced
                                                 // transition
                    }
                } else {
                    if (previousVoiceless) {
                        transitionCost =
                            voicedUnvoicedCost;  // unvoiced-to-voiced
                                                 // transition
                    } else {
                        transitionCost = octaveJumpCost *
                                         fabs(NUMlog2(f1 / f2));  // both voiced
                    }
                }
                value = prev_delta[icand1] - transitionCost + cur_delta[icand2];

                if (value > maximum) {
                    maximum = value;
                    place = icand1;
                }
            }
            cur_delta[icand2] = maximum;
            cur_psi[icand2] = place;
        }
    }

    /* Find the end of the most probable path. */
    place = 0;
    maximum = delta[frame_num - 1][place];
    for (icand = 1; icand < frame_list[frame_num - 1]->nCandidates; icand++) {
        if (delta[frame_num - 1][icand] > maximum) {
            place = icand;
            maximum = delta[frame_num - 1][icand];
        }
    }

    /* Backtracking: follow the path backwards. */
    for (iframe = frame_num - 1; iframe >= 0; iframe--) {
        frame = frame_list[iframe];
        help = frame->candidate[0];
        frame->candidate[0] = frame->candidate[place];
        frame->candidate[place] = help;
        place = psi[iframe][place];
    }

    for (i = 0; i < frame_num; i++) {
        free(delta[i]);
        free(psi[i]);
    }

    free(delta);
    free(psi);

    return 0;
}
