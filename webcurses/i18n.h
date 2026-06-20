/* i18n.h — Korean localization seam for Rogue messages.
 *
 * The message chokepoint is io.c's endmsg(): once `msgbuf` holds the fully
 * assembled English line, the build patches in
 *     web_emit_msg(tr_msg(msgbuf));
 * (see build.sh patch #2). tr_msg() looks the line up in a table and returns
 * Korean; anything not yet in the table passes through unchanged (English), so
 * the game never breaks while the table is filled in.
 *
 * Korean particle (조사) selection depends on whether the preceding noun ends
 * in a final consonant (받침). The kr_* helpers below are UTF-8 aware and are
 * meant for the next wave, where fragment-concatenated messages (io.c's
 * msg("there is ")+...+msg(" to pick up")) get collapsed into keyed templates
 * with Korean item/monster names substituted in.
 */
#ifndef ROGUE_I18N_H
#define ROGUE_I18N_H

/* Translate a fully-assembled English message to Korean. Returns a pointer
 * valid until the next tr_msg() call (matched entries are static literals;
 * unmatched input is returned as-is). Case of the first character is ignored
 * so it works whether or not endmsg() has upper-cased msgbuf[0]. */
const char *tr_msg(const char *en);

/* Translate inv_name()'s English output (item as shown in inventory / msgs) to
 * Korean. things.c wraps inv_name around this. Unrecognized forms pass through.
 * Returns a static buffer valid until the next kr_item() call. */
char *kr_item(const char *en);

/* Translate a string drawn straight to the curses screen (help screen, death
 * tombstone, fixed prompts) — these bypass the msg() path. web_curses.c routes
 * waddstr() through this. Exact full-string match only; everything else
 * (status line, already-Korean item names, the map) passes through unchanged. */
const char *tr_screen(const char *s);

/* 1 if the last Unicode syllable of `word` carries a 받침 (final consonant),
 * 0 otherwise (including non-Hangul tails). UTF-8 input. */
int kr_has_batchim(const char *word);

/* Return the correct particle for `word` (just the particle, no noun), so
 * callers can build "%s%s" templates: noun + kr_eul_reul(noun). */
const char *kr_eul_reul(const char *word);  /* 을 / 를  (object)        */
const char *kr_i_ga(const char *word);      /* 이 / 가  (subject)       */
const char *kr_eun_neun(const char *word);  /* 은 / 는  (topic)         */
const char *kr_euro_ro(const char *word);   /* 으로 / 로 (direction)    */
const char *kr_gwa_wa(const char *word);    /* 과 / 와  (conjunction)   */

#endif /* ROGUE_I18N_H */
