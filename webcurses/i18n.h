/* i18n.h — English -> Korean message translation for the web Rogue port.
 *
 * The message seam is endmsg() in io.c (patch #2): once Rogue has assembled the
 * full English message in `msgbuf`, it calls
 *
 *     web_emit_msg(tr_msg(msgbuf));
 *
 * instead of drawing the message on curses row 0. tr_msg() turns the assembled
 * English sentence into Korean (with proper 조사/particles) and falls back to
 * the original English when no rule matches, so nothing ever breaks — untranslated
 * lines just show up in English and can be added to the table later.
 *
 * Korean particle selection is data-driven on the trailing syllable's 받침
 * (final consonant), the same logic proven in the touch UI prototype
 * (hasBatchim / eulReul / iGa) but generalised to all the particles Rogue needs.
 */
#ifndef ROGUE_I18N_H
#define ROGUE_I18N_H

/* Translate a fully-assembled English message to Korean. Returns a pointer into
 * a small rotating set of static buffers (safe for the single
 * web_emit_msg(tr_msg(...)) call per message). Never returns NULL: unmatched
 * input is echoed back unchanged. */
const char *tr_msg(const char *en);

/* 조사 helpers — append `noun` plus the correct particle into `out` (size n).
 * Selection looks at the last Hangul syllable of `noun`; a non-Hangul tail
 * (e.g. an untranslated English noun or a digit) is treated as 받침-less.
 *   eul : 을/를   iga : 이/가   eunneun : 은/는   gwa : 와/과   ro : (으)로
 * Exposed for the UI/status code and for the i18n self-test. */
void i18n_eul(char *out, int n, const char *noun);
void i18n_iga(char *out, int n, const char *noun);
void i18n_eunneun(char *out, int n, const char *noun);
void i18n_gwa(char *out, int n, const char *noun);
void i18n_ro(char *out, int n, const char *noun);

/* TRUE if the last Hangul syllable of `s` carries a 받침 (final consonant). */
int i18n_has_batchim(const char *s);

#endif /* ROGUE_I18N_H */
