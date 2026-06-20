/* i18n.h — English -> Korean message translation for the web port.
 *
 * The single chokepoint is endmsg() in io.c: once Rogue has assembled the full
 * English message in `msgbuf`, the patched endmsg() calls
 *     web_emit_msg(tr_msg(msgbuf));
 * and skips the curses message-line draw (see build.sh patch #2, HANDOFF §8).
 *
 * tr_msg() looks the assembled English sentence up in a pattern table
 * (TRANSLATIONS[] in i18n.c). Patterns may contain printf-style wildcards
 * (%s/%d/%c/%ld/...) which are captured from the input and re-inserted into the
 * Korean template via $1..$9. Korean particles (조사) that depend on whether the
 * preceding noun ends in a 받침 are written as $N{을}/$N{이}/... and resolved at
 * runtime. Unmatched messages fall back to the original English so the game
 * stays playable while the table is filled in.
 */
#ifndef WEBCURSES_I18N_H
#define WEBCURSES_I18N_H

/* Translate an assembled English message to Korean. Returns a pointer to a
 * static buffer (overwritten on each call); copy if you need to keep it.
 * Never returns NULL — falls back to the input string when no rule matches. */
const char *tr_msg(const char *en);

/* True if the last Korean (Hangul) syllable of `word` carries a 받침 (final
 * consonant). Non-Hangul tails are treated as having no 받침. Exposed for tests
 * and for any caller that needs to pick a 조사 directly. */
int han_has_batchim(const char *word);

#endif /* WEBCURSES_I18N_H */
