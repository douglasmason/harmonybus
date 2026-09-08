#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/harmony_core.h"

static void fail(const char *label, const char *message) {
    fprintf(stderr, "FAIL %s: %s\n", label, message);
    exit(1);
}

static void expect_harmony(
    const char *label,
    const uint8_t *notes,
    int note_count,
    int expected_root,
    int expected_bass,
    int expected_chord_index
) {
    hb_harmony_t harmony = hb_infer_harmony(notes, note_count);
    if (!harmony.valid) fail(label, "invalid harmony");
    if (harmony.root_pc != expected_root) {
        fprintf(stderr, "FAIL %s: root=%d expected=%d name=%s\n", label, harmony.root_pc, expected_root, harmony.name);
        exit(1);
    }
    if (harmony.bass_pc != expected_bass) {
        fprintf(stderr, "FAIL %s: bass=%d expected=%d name=%s\n", label, harmony.bass_pc, expected_bass, harmony.name);
        exit(1);
    }
    if (harmony.chord_index != expected_chord_index) {
        fprintf(stderr, "FAIL %s: chord_index=%d expected=%d name=%s\n", label, harmony.chord_index, expected_chord_index, harmony.name);
        exit(1);
    }
}

static void expect_pitch_class_in_mask(const char *label, uint16_t mask, int pitch_class) {
    if ((mask & (uint16_t)(1u << (pitch_class % 12))) == 0) {
        fprintf(stderr, "FAIL %s: pitch class %d absent from mask 0x%03x\n", label, pitch_class, mask);
        exit(1);
    }
}

int main(void) {
    /* Exact inversions must beat partial bass-root interpretations. */
    const uint8_t gsharp_major_over_dsharp[] = {51, 56, 60}; /* D#3 G#3 C4(B#) */
    expect_harmony("G# major / D#", gsharp_major_over_dsharp, 3, 8, 3, 0);

    const uint8_t f_minor7_over_eb[] = {51, 53, 56, 60}; /* Eb3 F3 Ab3 C4 */
    expect_harmony("Fm7 / Eb", f_minor7_over_eb, 4, 5, 3, 11);

    const uint8_t f_minor7_root[] = {53, 56, 60, 63};
    expect_harmony("Fm7 root", f_minor7_root, 4, 5, 5, 11);

    /* Triad inversions. */
    const uint8_t c_major_root[] = {60, 64, 67};
    const uint8_t c_major_first[] = {52, 55, 60};
    const uint8_t c_major_second[] = {55, 60, 64};
    expect_harmony("C major root", c_major_root, 3, 0, 0, 0);
    expect_harmony("C major first", c_major_first, 3, 0, 4, 0);
    expect_harmony("C major second", c_major_second, 3, 0, 7, 0);

    /* Rooted shell voicings should retain seventh quality even without P5. */
    const uint8_t c7_shell[] = {48, 52, 58}; /* C E Bb */
    expect_harmony("C7 shell", c7_shell, 3, 0, 0, 10);

    const uint8_t cm7_shell[] = {48, 51, 58}; /* C Eb Bb */
    expect_harmony("Cm7 shell", cm7_shell, 3, 0, 0, 11);

    /* Ordinary shell: root + minor third + minor seventh implies the
       unaltered fifth. Never hallucinate b5 when it was not played. */
    const uint8_t fm7_shell[] = {53, 56, 63}; /* F Ab Eb */
    expect_harmony("Fm7 shell", fm7_shell, 3, 5, 5, 11);

    const uint8_t f_minor_third[] = {53, 56}; /* F Ab */
    expect_harmony("F minor dyad", f_minor_third, 2, 5, 5, 1);

    /* min6 is enabled; major6 remains disabled. */
    const uint8_t cm6[] = {48, 51, 55, 57};
    expect_harmony("Cm6", cm6, 4, 0, 0, 8);

    /* Canonical follower mask must recover implied chord tones from shells. */
    hb_harmony_t c7 = hb_infer_harmony(c7_shell, 3);
    uint16_t c7_mask = hb_harmony_chord_mask(c7);
    expect_pitch_class_in_mask("C7 canonical root", c7_mask, 0);
    expect_pitch_class_in_mask("C7 canonical third", c7_mask, 4);
    expect_pitch_class_in_mask("C7 canonical fifth", c7_mask, 7);
    expect_pitch_class_in_mask("C7 canonical seventh", c7_mask, 10);

    /* Inversion should not change canonical chord-tone availability. */
    hb_harmony_t c_major_inv = hb_infer_harmony(c_major_first, 3);
    uint16_t c_major_mask = hb_harmony_chord_mask(c_major_inv);
    expect_pitch_class_in_mask("C/E canonical C", c_major_mask, 0);
    expect_pitch_class_in_mask("C/E canonical E", c_major_mask, 4);
    expect_pitch_class_in_mask("C/E canonical G", c_major_mask, 7);

    /* Follower chord mapping should always land on canonical chord tones. */
    for (int midi_note = 36; midi_note <= 84; ++midi_note) {
        int mapped = hb_map_note(midi_note, 0, c7, HB_MAP_CHORD);
        if ((c7_mask & (uint16_t)(1u << (mapped % 12))) == 0) {
            fprintf(stderr, "FAIL follower map: input=%d output=%d not in C7 mask\n", midi_note, mapped);
            return 1;
        }
    }

    /* Rich colors may be recognized only after the root is established. */
    hb_harmony_t established_c = hb_infer_harmony(c_major_root, 3);
    const uint8_t c6_notes[] = {48, 52, 55, 57};
    hb_harmony_t c6_color = hb_refine_harmony_with_root(c6_notes, 4, established_c);
    if (c6_color.root_pc != 0 || c6_color.chord_index != 7) fail("C6 color", c6_color.name);

    const uint8_t c11_notes[] = {48, 50, 52, 53, 55, 58};
    hb_harmony_t c11_color = hb_refine_harmony_with_root(c11_notes, 6, established_c);
    if (c11_color.root_pc != 0 || c11_color.chord_index != 20) fail("C11 color", c11_color.name);

    const uint8_t c13_notes[] = {48, 50, 52, 55, 57, 58};
    hb_harmony_t c13_color = hb_refine_harmony_with_root(c13_notes, 6, established_c);
    if (c13_color.root_pc != 0 || c13_color.chord_index != 22) fail("C13 color", c13_color.name);

    /* Polyphonic follower assignment: outputs stay legal, ordered, and
       avoid gratuitous duplicate voices when alternatives exist. */
    const uint8_t follower_sources[] = {60, 64, 67};
    const int follower_previous[] = {60, 64, 67};
    int follower_outputs[] = {-1, -1, -1};
    hb_map_held_voices(follower_sources, 3, 0, c7, HB_MAP_CHORD,
                       follower_previous, follower_outputs);
    for (int voice = 0; voice < 3; ++voice) {
        if ((c7_mask & (uint16_t)(1u << (follower_outputs[voice] % 12))) == 0)
            fail("poly follower legal tone", "mapped outside chord");
        if (voice > 0 && follower_outputs[voice] < follower_outputs[voice - 1])
            fail("poly follower crossing", "voice order reversed");
    }
    /* User's concrete follower semantics: the clip remains F/Ab while
       conductor Fm -> Bbm makes it SOUND Bb/Db. */
    const uint8_t f_minor_notes[] = {53, 56, 60};
    const uint8_t bb_minor_notes[] = {58, 61, 65};
    hb_harmony_t follower_fm = hb_infer_harmony(f_minor_notes, 3);
    hb_harmony_t follower_bbm = hb_infer_harmony(bb_minor_notes, 3);
    const uint8_t follower_f_ab[] = {53, 56};
    int follower_over_fm[] = {-1, -1};
    int follower_over_bbm[] = {-1, -1};
    hb_map_held_voices_by_role(follower_f_ab, 2, follower_fm, follower_fm, 0, NULL,
                               follower_over_fm);
    hb_map_held_voices_by_role(follower_f_ab, 2, follower_fm, follower_bbm, 0,
                               follower_over_fm, follower_over_bbm);
    if (follower_over_fm[0] != 53 || follower_over_fm[1] != 56) {
        fprintf(stderr, "FAIL Fm follower baseline: got %d,%d expected 53,56\n",
                follower_over_fm[0], follower_over_fm[1]);
        return 1;
    }
    if (follower_over_bbm[0] != 58 || follower_over_bbm[1] != 61) {
        fprintf(stderr, "FAIL Fm->Bbm follower: got %d,%d expected 58,61\n",
                follower_over_bbm[0], follower_over_bbm[1]);
        return 1;
    }

    /* Harmonic roles, not raw semitone offsets: a source minor third maps
       to the target chord's third quality. Fm -> Bb major maps Ab -> D. */
    const uint8_t bb_major_notes[] = {58, 62, 65};
    hb_harmony_t follower_bb_major = hb_infer_harmony(bb_major_notes, 3);
    if (hb_map_note_by_role(56, follower_fm, follower_bb_major, 0) != 62)
        fail("minor-third to major-third role", "Ab should map to D");

    /* Smooth mode trades role preservation for minimum movement. */
    int follower_smooth_bbm[] = {-1, -1};
    hb_map_held_voices_by_role(follower_f_ab, 2, follower_fm, follower_bbm, 1,
                               follower_over_fm, follower_smooth_bbm);
    if (follower_smooth_bbm[0] != 53 || follower_smooth_bbm[1] != 58) {
        fprintf(stderr, "FAIL smooth Fm->Bbm: got %d,%d expected 53,58\n",
                follower_smooth_bbm[0], follower_smooth_bbm[1]);
        return 1;
    }

    /* Exact regressions from Move: ordinary minor shell and sixth colors. */
    const uint8_t move_fm7_shell[] = {53, 56, 63}; /* F Ab Eb */
    expect_harmony("Move Fm7 shell", move_fm7_shell, 3, 5, 5, 11);
    const uint8_t move_fmin6[] = {53, 56, 62}; /* F Ab D */
    /* Standalone this is exactly D diminished in first inversion. */
    expect_harmony("Move Ddim/F standalone", move_fmin6, 3, 2, 5, 5);
    /* But once F minor is established, D is a 6th color and must not re-root. */
    hb_harmony_t established_fm = hb_infer_harmony(f_minor_notes, 3);
    hb_harmony_t move_fmin6_stable = hb_refine_harmony_with_root(move_fmin6, 3, established_fm);
    if (move_fmin6_stable.root_pc != 5 || move_fmin6_stable.chord_index != 8)
        fail("Move Fm6 established root", move_fmin6_stable.name);

    const uint8_t move_f_db_ab[] = {53, 56, 61}; /* F Ab Db = Db/F */
    expect_harmony("Move Db/F standalone", move_f_db_ab, 3, 1, 5, 0);
    /* Likewise, established F minor treats Db as the b6 color rather than
       hallucinating F major. We currently preserve Fm because no dedicated
       minor-b6 template exists. */
    hb_harmony_t move_f_db_stable = hb_refine_harmony_with_root(move_f_db_ab, 3, established_fm);
    if (move_f_db_stable.root_pc != 5 || move_f_db_stable.chord_index != 1)
        fail("Move Fm stable with Db color", move_f_db_stable.name);

    /* Realtime state-machine regressions: committed harmony is consulted
       before allowing transient subsets/extensions to reinterpret the root. */
    hb_harmony_t committed_fm = hb_infer_harmony(f_minor_notes, 3);

    const uint8_t realtime_f_ab[] = {53, 56};
    hb_harmony_t realtime_f_ab_result = hb_infer_harmony_contextual(realtime_f_ab, 2, committed_fm);
    if (realtime_f_ab_result.root_pc != 5 || realtime_f_ab_result.chord_index != 1)
        fail("context F-Ab stays Fm", realtime_f_ab_result.name);

    const uint8_t realtime_f_ab_eb[] = {53, 56, 63};
    hb_harmony_t realtime_fm7_result = hb_infer_harmony_contextual(realtime_f_ab_eb, 3, committed_fm);
    if (realtime_fm7_result.root_pc != 5 || realtime_fm7_result.chord_index != 11)
        fail("context F-Ab-Eb -> Fm7", realtime_fm7_result.name);

    const uint8_t realtime_f_ab_d[] = {53, 56, 62};
    hb_harmony_t realtime_fm6_result = hb_infer_harmony_contextual(realtime_f_ab_d, 3, committed_fm);
    if (realtime_fm6_result.root_pc != 5 || realtime_fm6_result.chord_index != 8)
        fail("context F-Ab-D -> Fm6", realtime_fm6_result.name);

    const uint8_t realtime_f_ab_db[] = {53, 56, 61};
    hb_harmony_t realtime_f_db_result = hb_infer_harmony_contextual(realtime_f_ab_db, 3, committed_fm);
    if (realtime_f_db_result.root_pc != 5 || realtime_f_db_result.chord_index != 1)
        fail("context F-Ab-Db keeps Fm", realtime_f_db_result.name);

    const uint8_t realtime_f_a[] = {53, 57};
    hb_harmony_t realtime_f_major_result = hb_infer_harmony_contextual(realtime_f_a, 2, committed_fm);
    if (realtime_f_major_result.root_pc != 5 || realtime_f_major_result.chord_index != 0)
        fail("context F-A changes to F major", realtime_f_major_result.name);

    const uint8_t realtime_f_only[] = {53};
    hb_harmony_t realtime_f_only_result = hb_infer_harmony_contextual(realtime_f_only, 1, committed_fm);
    if (realtime_f_only_result.root_pc != 5 || realtime_f_only_result.chord_index != 1)
        fail("single F preserves committed Fm", realtime_f_only_result.name);

    printf("Harmony Bus core tests passed.\n");
    return 0;
}
