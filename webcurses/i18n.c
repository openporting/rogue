/* i18n.c — English -> Korean translation for Rogue messages. See i18n.h.
 *
 * Strategy (matches the assembled sentence, so the C sources stay un-refactored):
 *   1. exact table   — fixed messages with no variable part (the big, easy win)
 *   2. frame rules   — "there is %s to pick up" etc.: match prefix+suffix,
 *                      pull the middle noun out, translate it, recompose in
 *                      Korean word order with the right 조사
 *   3. combat rules  — "<A> hit <B>" / "<A> misses <B>" built from fight.c's
 *                      h_names/m_names verb tables
 *   4. fallback      — return the original English untouched
 *
 * Build (web)    : compiled by build.sh alongside web_curses.c
 * Build (native) : cc -DI18N_TEST i18n.c -o t && ./t   (self-test, no curses)
 */
#include "i18n.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================
 *  Korean particle (조사) engine — 받침 detection on UTF-8 Hangul
 * ============================================================ */

/* Decode the last codepoint of a UTF-8 string (0 if empty). */
static unsigned last_cp(const char *s)
{
    size_t n = strlen(s);
    if (n == 0)
        return 0;
    size_t i = n - 1;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
        i--;
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80)
        return c;
    if ((c & 0xE0) == 0xC0 && i + 1 < n)
        return ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
    if ((c & 0xF0) == 0xE0 && i + 2 < n)
        return ((c & 0x0Fu) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) |
               ((unsigned char)s[i + 2] & 0x3Fu);
    return c;
}

/* Index of the final consonant (종성); 0 means no 받침. -1 if not Hangul. */
static int jongseong(const char *s)
{
    unsigned cp = last_cp(s);
    if (cp < 0xAC00 || cp > 0xD7A3)
        return -1;
    return (int)((cp - 0xAC00) % 28);
}

int i18n_has_batchim(const char *s)
{
    int j = jongseong(s);
    return j > 0; /* -1 (non-Hangul) and 0 (no 받침) both => no batchim */
}

/* noun + particle, choosing `with` when 받침 present else `without`. */
static void josa(char *out, int n, const char *noun, const char *with, const char *without)
{
    snprintf(out, n, "%s%s", noun, i18n_has_batchim(noun) ? with : without);
}

void i18n_eul(char *out, int n, const char *noun)     { josa(out, n, noun, "\xEC\x9D\x84", "\xEB\xA5\xBC"); } /* 을 / 를 */
void i18n_iga(char *out, int n, const char *noun)     { josa(out, n, noun, "\xEC\x9D\xB4", "\xEA\xB0\x80"); } /* 이 / 가 */
void i18n_eunneun(char *out, int n, const char *noun) { josa(out, n, noun, "\xEC\x9D\x80", "\xEB\x8A\x94"); } /* 은 / 는 */
void i18n_gwa(char *out, int n, const char *noun)     { josa(out, n, noun, "\xEA\xB3\xBC", "\xEC\x99\x80"); } /* 과 / 와 */

void i18n_ro(char *out, int n, const char *noun)
{
    /* (으)로: 받침 없거나 ㄹ받침(종성 8)이면 "로", 그 외엔 "으로" */
    int j = jongseong(noun);
    int simple = (j <= 0 || j == 8);
    snprintf(out, n, "%s%s", noun, simple ? "\xEB\xA1\x9C" : "\xEC\x9C\xBC\xEB\xA1\x9C");
}

/* ============================================================
 *  Noun translation — monsters, colors, common item base words
 * ============================================================ */

typedef struct { const char *en, *kr; } kv;

/* 26 monster names (extern.c monsters[]). */
static const kv MONSTERS[] = {
    {"aquator", "\xEC\x95\x84\xEC\xBF\xA0\xEC\x95\x84\xED\x84\xB0"},          /* 아쿠아터 */
    {"bat", "\xEB\xB0\x95\xEC\xA5\x90"},                                      /* 박쥐 */
    {"centaur", "\xEC\xBC\x84\xED\x83\x80\xEC\x9A\xB0\xEB\xA1\x9C\xEC\x8A\xA4"},/* 켄타우로스 */
    {"dragon", "\xEC\x9A\xA9"},                                              /* 용 */
    {"emu", "\xEC\x97\x90\xEB\xAE\xA4"},                                      /* 에뮤 */
    {"venus flytrap", "\xED\x8C\x8C\xEB\xA6\xAC\xEC\xA7\x80\xEC\x98\xA5"},    /* 파리지옥 */
    {"griffin", "\xEA\xB7\xB8\xEB\xA6\xAC\xED\x95\x80"},                      /* 그리핀 */
    {"hobgoblin", "\xED\x99\x89\xEA\xB3\xA0\xEB\xB8\x94\xEB\xA6\xB0"},        /* 홉고블린 */
    {"ice monster", "\xEC\x96\xBC\xEC\x9D\x8C \xEA\xB4\xB4\xEB\xAC\xBC"},     /* 얼음 괴물 */
    {"jabberwock", "\xEC\x9E\xAC\xEB\xB2\x84\xEC\x9B\x8C\xED\x81\xAC"},       /* 재버워크 */
    {"kestrel", "\xED\x99\xA9\xEC\xA1\xB0\xEB\xA1\xB1\xEC\x9D\xB4"},          /* 황조롱이 */
    {"leprechaun", "\xEB\xA0\x88\xED\x94\x84\xEB\xA0\x88\xEC\xB9\xB8"},       /* 레프레칸 */
    {"medusa", "\xEB\xA9\x94\xEB\x91\x90\xEC\x82\xAC"},                       /* 메두사 */
    {"nymph", "\xEC\x9A\x94\xEC\xA0\x95"},                                    /* 요정 */
    {"orc", "\xEC\x98\xA4\xED\x81\xAC"},                                      /* 오크 */
    {"phantom", "\xED\x8C\xAC\xED\x85\x80"},                                  /* 팬텀 */
    {"quagga", "\xEC\xBF\xBC\xEA\xB0\x80"},                                   /* 쿼가 */
    {"rattlesnake", "\xEB\xB0\xA9\xEC\x9A\xB8\xEB\xB1\x80"},                  /* 방울뱀 */
    {"snake", "\xEB\xB1\x80"},                                                /* 뱀 */
    {"troll", "\xED\x8A\xB8\xEB\xA1\xA4"},                                    /* 트롤 */
    {"black unicorn", "\xEA\xB2\x80\xEC\x9D\x80 \xEC\x9C\xA0\xEB\x8B\x88\xEC\xBD\x98"}, /* 검은 유니콘 */
    {"vampire", "\xED\x9D\xA1\xED\x98\x88\xEA\xB7\x80"},                      /* 흡혈귀 */
    {"wraith", "\xEB\xA7\x9D\xEB\xA0\xB9"},                                  /* 망령 */
    {"xeroc", "\xEC\xA0\x9C\xEB\xA1\x9D"},                                    /* 제록 */
    {"yeti", "\xEC\x98\x88\xED\x8B\xB0"},                                     /* 예티 */
    {"zombie", "\xEC\xA2\x80\xEB\xB9\x84"},                                   /* 좀비 */
    {0, 0}
};

/* Rainbow colors (init.c) — used in scroll/potion/glow flavor text. */
static const kv COLORS[] = {
    {"amber", "\xED\x98\xB8\xEB\xB0\x95\xEC\x83\x89"},        /* 호박색 */
    {"aquamarine", "\xEC\x95\x84\xED\x80\xB8\xEB\xA7\x88\xEB\xA6\xB0"}, /* 아쿠아마린 */
    {"black", "\xEA\xB2\x80\xEC\x9D\x80\xEC\x83\x89"},        /* 검은색 */
    {"blue", "\xED\x8C\x8C\xEB\x9E\x80\xEC\x83\x89"},         /* 파란색 */
    {"brown", "\xEA\xB0\x88\xEC\x83\x89"},                    /* 갈색 */
    {"clear", "\xED\x88\xAC\xEB\xAA\x85"},                    /* 투명 */
    {"crimson", "\xEC\xA7\x84\xED\x99\x8D\xEC\x83\x89"},      /* 진홍색 */
    {"cyan", "\xEC\x82\xAC\xEC\x9D\xB4\xEC\x95\x88"},         /* 사이안 */
    {"ecru", "\xEC\x97\x90\xED\x81\xAC\xEB\xA3\xA8\xEC\x83\x89"}, /* 에크루색 */
    {"gold", "\xEA\xB8\x88\xEB\xB9\x9B"},                     /* 금빛 */
    {"green", "\xEC\xB4\x88\xEB\xA1\x9D\xEC\x83\x89"},        /* 초록색 */
    {"grey", "\xED\x9A\x8C\xEC\x83\x89"},                     /* 회색 */
    {"magenta", "\xEC\x9E\x90\xEB\xA3\xA8\xEB\xB9\x9B"},      /* 자루빛 (magenta) */
    {"orange", "\xEC\xA3\xBC\xED\x99\xA9\xEC\x83\x89"},       /* 주황색 */
    {"pink", "\xEB\xB6\x84\xED\x99\x8D\xEC\x83\x89"},         /* 분홍색 */
    {"plaid", "\xEA\xB2\xA9\xEC\x9E\x90\xEB\xAC\xB4\xEB\x8A\x8C"}, /* 격자무늬 */
    {"purple", "\xEB\xB3\xB4\xEB\x9D\xBC\xEC\x83\x89"},       /* 보라색 */
    {"red", "\xEB\xB9\xA8\xEA\xB0\x84\xEC\x83\x89"},          /* 빨간색 */
    {"silver", "\xEC\x9D\x80\xEB\xB9\x9B"},                   /* 은빛 */
    {"tan", "\xED\x99\xA9\xEA\xB0\x88\xEC\x83\x89"},          /* 황갈색 */
    {"tangerine", "\xEA\xB7\xA4\xEC\x83\x89"},                /* 귤색 */
    {"topaz", "\xED\x86\xA0\xED\x8C\x8C\xEC\x8A\xA4\xEC\x83\x89"}, /* 토파스색 */
    {"turquoise", "\xEC\xB2\xAD\xEB\xA1\x9D\xEC\x83\x89"},    /* 청록색 */
    {"vermilion", "\xEC\x84\xA0\xED\x99\x8D\xEC\x83\x89"},    /* 선홍색 */
    {"violet", "\xEC\x97\xB0\xEB\xB3\xB4\xEB\x9D\xBC\xEC\x83\x89"}, /* 연보라색 */
    {"white", "\xED\x9D\xB0\xEC\x83\x89"},                    /* 흰색 */
    {"yellow", "\xEB\x85\xB8\xEB\x9E\x80\xEC\x83\x89"},       /* 노란색 */
    {0, 0}
};

/* Common item base nouns (best-effort; full inv_name() Korean is future work). */
static const kv ITEMS[] = {
    {"gold", "\xEA\xB8\x88\xED\x99\x94"},                     /* 금화 */
    {"food ration", "\xEC\x8B\x9D\xEB\x9F\x89 \xEB\xB0\xB0\xEA\xB8\x89"}, /* 식량 배급 */
    {"mango", "\xEB\xA7\x9D\xEA\xB3\xA0"},                    /* 망고 */
    {"dagger", "\xEB\x8B\xA8\xEA\xB2\x80"},                   /* 단검 */
    {"mace", "\xEB\xA9\x94\xEC\x9D\xB4\xEC\x8A\xA4"},         /* 메이스 */
    {"short bow", "\xEB\x8B\xA8\xEA\xB6\x81"},                /* 단궁 */
    {"long sword", "\xEC\x9E\xA5\xEA\xB2\x80"},               /* 장검 */
    {"two handed sword", "\xEC\x96\x91\xEC\x86\x90\xEA\xB2\x80"}, /* 양손검 */
    {"spear", "\xEC\xB0\xBD"},                                /* 창 */
    {"arrow", "\xED\x99\x94\xEC\x82\xB4"},                    /* 화살 */
    {"dart", "\xED\x91\x9C\xEC\xB0\xBD"},                     /* 표창 */
    {"leather armor", "\xEA\xB0\x80\xEC\xA3\xBD \xEA\xB0\x91\xEC\x98\xB7"}, /* 가죽 갑옷 */
    {"ring mail", "\xEC\x82\xAC\xEC\x8A\xAC \xEA\xB0\x91\xEC\x98\xB7"}, /* 사슬 갑옷 */
    {"scale mail", "\xEB\xB9\x84\xEB\x8A\x98 \xEA\xB0\x91\xEC\x98\xB7"}, /* 비늘 갑옷 */
    {"chain mail", "\xEC\x82\xAC\xEC\x8A\xAC\xEA\xB0\x91\xEC\x98\xB7"}, /* 사슬갑옷 */
    {"splint mail", "\xED\x8C\x90\xEA\xB8\x88 \xEA\xB0\x91\xEC\x98\xB7"}, /* 판금 갑옷 */
    {"plate mail", "\xED\x8C\x90\xEA\xB0\x91\xEC\x98\xB7"},   /* 판갑옷 */
    {"amulet", "\xEB\xB6\x80\xEC\xA0\x81"},                   /* 부적 */
    {0, 0}
};

static const char *lookup(const kv *t, const char *s)
{
    for (; t->en; t++)
        if (strcmp(t->en, s) == 0)
            return t->kr;
    return 0;
}


/* Translate one noun phrase into `out`. Strips an English article
 * (a/an/the/some), then tries monsters -> items -> colors -> special words;
 * unknown nouns are copied through in English (so 조사 falls back to 받침-less). */
static void tr_noun(char *out, int n, const char *en)
{
    char buf[128];
    /* trim leading spaces */
    while (*en == ' ')
        en++;
    snprintf(buf, sizeof buf, "%s", en);
    /* trim trailing spaces */
    size_t L = strlen(buf);
    while (L > 0 && buf[L - 1] == ' ')
        buf[--L] = '\0';

    const char *p = buf;
    if      (strncmp(p, "the ", 4) == 0) p += 4;
    else if (strncmp(p, "an ", 3) == 0)  p += 3;
    else if (strncmp(p, "a ", 2) == 0)   p += 2;
    else if (strncmp(p, "some ", 5) == 0) p += 5;

    if (strcmp(p, "you") == 0)        { snprintf(out, n, "\xEB\x8B\xB9\xEC\x8B\xA0"); return; }      /* 당신 */
    if (strcmp(p, "it") == 0)         { snprintf(out, n, "\xEA\xB7\xB8\xEA\xB2\x83"); return; }      /* 그것 */
    if (strcmp(p, "something") == 0)  { snprintf(out, n, "\xEB\xAC\xB4\xEC\x96\xB8\xEA\xB0\x80"); return; } /* 무언가 */

    const char *kr = lookup(MONSTERS, p);
    if (!kr) kr = lookup(ITEMS, p);
    if (!kr) kr = lookup(COLORS, p);
    snprintf(out, n, "%s", kr ? kr : p);
}

/* ============================================================
 *  Exact-match table — fixed messages (no variable part)
 *  Keys are compared with the first letter forced to lowercase, because
 *  endmsg() upper-cases msgbuf[0] before this runs.
 * ============================================================ */
static const kv EXACT[] = {
    /* movement / general */
    {"nothing happens", "\xEC\x95\x84\xEB\xAC\xB4 \xEC\x9D\xBC\xEB\x8F\x84 \xEC\x9D\xBC\xEC\x96\xB4\xEB\x82\x98\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8A\x94\xEB\x8B\xA4."}, /* 아무 일도 일어나지 않는다. */
    {"nothing here", "\xEC\x97\xAC\xEA\xB8\xB0\xEC\x97\x94 \xEC\x95\x84\xEB\xAC\xB4\xEA\xB2\x83\xEB\x8F\x84 \xEC\x97\x86\xEB\x8B\xA4."}, /* 여기엔 아무것도 없다. */
    /* verbose form of the "," pickup-miss (command.c) — must precede the
     * "there is %s to pick up" frame, hence an exact entry. */
    {"there is nothing here to pick up", "\xEC\x97\xAC\xEA\xB8\xB0\xEC\x97\x94 \xEC\xA3\xBC\xEC\x9A\xB8 \xEA\xB2\x83\xEC\x9D\xB4 \xEC\x97\x86\xEB\x8B\xA4."}, /* 여기엔 주울 것이 없다. */
    {"there is something there already", "\xEA\xB7\xB8\xEA\xB3\xB3\xEC\x97\x94 \xEC\x9D\xB4\xEB\xAF\xB8 \xEB\xAC\xB4\xEC\x96\xB8\xEA\xB0\x80\xEA\xB0\x80 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 그곳엔 이미 무언가가 있다. */
    {"no trap there", "\xEA\xB1\xB0\xEA\xB8\xB0\xEC\x97\x94 \xED\x95\xA8\xEC\xA0\x95\xEC\x9D\xB4 \xEC\x97\x86\xEB\x8B\xA4."}, /* 거기엔 함정이 없다. */
    {"no monster there", "\xEA\xB1\xB0\xEA\xB8\xB0\xEC\x97\x94 \xEB\xAA\xAC\xEC\x8A\xA4\xED\x84\xB0\xEA\xB0\x80 \xEC\x97\x86\xEB\x8B\xA4."}, /* 거기엔 몬스터가 없다. */
    {"i see no way down", "\xEB\x82\xB4\xEB\xA0\xA4\xEA\xB0\x80\xEB\x8A\x94 \xEA\xB8\xB8\xEC\x9D\xB4 \xEB\xB3\xB4\xEC\x9D\xB4\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8A\x94\xEB\x8B\xA4."}, /* 내려가는 길이 보이지 않는다. */
    {"i see no way up", "\xEC\x98\xAC\xEB\x9D\xBC\xEA\xB0\x80\xEB\x8A\x94 \xEA\xB8\xB8\xEC\x9D\xB4 \xEB\xB3\xB4\xEC\x9D\xB4\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8A\x94\xEB\x8B\xA4."}, /* 올라가는 길이 보이지 않는다. */
    {"really quit?", "\xEC\xA0\x95\xEB\xA7\x90 \xEA\xB7\xB8\xEB\xA7\x8C\xEB\x91\x90\xEA\xB2\xA0\xEB\x8A\x94\xEA\xB0\x80?"}, /* 정말 그만두겠는가? */
    {"you haven't typed a command yet", "\xEC\x95\x84\xEC\xA7\x81 \xEC\x95\x84\xEB\xAC\xB4 \xEB\xAA\x85\xEB\xA0\xB9\xEB\x8F\x84 \xEC\x9E\x85\xEB\xA0\xA5\xED\x95\x98\xEC\xA7\x80 \xEC\x95\x8A\xEC\x95\x98\xEB\x8B\xA4."}, /* 아직 아무 명령도 입력하지 않았다. */
    {"you aren't carrying anything", "\xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x80 \xEC\x95\x84\xEB\xAC\xB4\xEA\xB2\x83\xEB\x8F\x84 \xEB\x93\xA4\xEA\xB3\xA0 \xEC\x9E\x88\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8B\xA4."}, /* 당신은 아무것도 들고 있지 않다. */

    /* hold / freeze / movement state */
    {"you are frozen", "\xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x80 \xEC\x96\xBC\xEC\x96\xB4\xEB\xB6\x99\xEC\x97\x88\xEB\x8B\xA4."}, /* 당신은 얼어붙었다. */
    {"you can move again", "\xEB\x8B\xA4\xEC\x8B\x9C \xEC\x9B\x80\xEC\xA7\x81\xEC\x9D\xBC \xEC\x88\x98 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 다시 움직일 수 있다. */
    {"you are being held", "\xEB\xAC\xB4\xEC\x96\xB8\xEA\xB0\x80\xEC\x97\x90\xEA\xB2\x8C \xEB\xB6\x99\xEC\x9E\xA1\xED\x98\x80 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 무언가에게 붙잡혀 있다. */
    {"you can't.  you're floating off the ground!", "\xEA\xB7\xB8\xEB\x9F\xB4 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4. \xEB\x95\x85\xEC\x97\x90\xEC\x84\x9C \xEB\x9C\xA8 \xEC\x9E\x88\xEB\x8B\xA4!"}, /* 그럴 수 없다. 땅에서 떠 있다! */
    {"your way is magically blocked", "\xEB\xA7\x88\xEB\xB2\x95\xEC\x9C\xBC\xEB\xA1\x9C \xEA\xB8\xB8\xEC\x9D\xB4 \xEB\xA7\x89\xED\x98\x80 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 마법으로 길이 막혀 있다. */

    /* hunger / fatigue */
    {"you faint from exhaustion", "\xED\x83\x88\xEC\xA7\x84\xED\x95\x98\xEC\x97\xAC \xEC\x93\xB0\xEB\x9F\xAC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 탈진하여 쓰러진다. */
    {"you feel very weak now.", "\xEC\x9D\xB4\xEC\xA0\x9C \xEB\xAA\xB9\xEC\x8B\x9C \xEA\xB8\xB0\xEC\x9A\xB4\xEC\x9D\xB4 \xEC\x97\x86\xEB\x8B\xA4."}, /* 이제 몹시 기운이 없다. */
    {"you are starting to feel weak", "\xEA\xB8\xB0\xEC\x9A\xB4\xEC\x9D\xB4 \xEB\xB9\xA0\xEC\xA7\x80\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x95\x9C\xEB\x8B\xA4."}, /* 기운이 빠지기 시작한다. */

    /* potion / scroll generic flavor */
    {"you feel stronger, now.  what bulging muscles!", "\xED\x9E\x98\xEC\x9D\xB4 \xEC\x86\x9F\xEB\x8A\x94\xEB\x8B\xA4! \xEA\xB7\xBC\xEC\x9C\xA1\xEC\x9D\xB4 \xEB\xB6\x88\xEB\x81\x88\xEA\xB1\xB0\xEB\xA6\xB0\xEB\x8B\xA4!"}, /* 힘이 솟는다! 근육이 불끈거린다! */
    {"you suddenly feel weaker", "\xEA\xB0\x91\xEC\x9E\x90\xEA\xB8\xB0 \xED\x9E\x98\xEC\x9D\xB4 \xEB\xB9\xA0\xEC\xA7\x80\xEB\x8A\x94 \xEB\x8A\x90\xEB\x82\x8C\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 갑자기 힘이 빠지는 느낌이다. */
    {"you begin to feel better", "\xEB\xAA\xB8\xEC\x9D\xB4 \xEB\x82\x98\xEC\x95\x84\xEC\xA7\x80\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x95\x9C\xEB\x8B\xA4."}, /* 몸이 나아지기 시작한다. */
    {"you begin to feel much better", "\xEB\xAA\xB8\xEC\x9D\xB4 \xED\x95\x9C\xEA\xB2\xB0 \xEB\x82\x98\xEC\x95\x84\xEC\xA7\x80\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x95\x9C\xEB\x8B\xA4."}, /* 몸이 한결 나아지기 시작한다. */
    {"you feel a strange sense of loss", "\xEB\xAC\x98\xED\x95\x9C \xEC\x83\x81\xEC\x8B\xA4\xEA\xB0\x90\xEC\x9D\xB4 \xEB\x93\xA0\xEB\x8B\xA4."}, /* 묘한 상실감이 든다. */
    {"you feel a wrenching sensation in your gut", "\xEB\xB1\x83\xEC\x86\x8D\xEC\x9D\xB4 \xEB\x92\xA4\xED\x8B\x80\xEB\xA6\xAC\xEB\x8A\x94 \xEB\x8A\x90\xEB\x82\x8C\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 뱃속이 뒤틀리는 느낌이다. */
    {"you feel yourself moving much faster", "\xEB\xAA\xB8\xEC\x9D\xB4 \xED\x9B\xA8\xEC\x94\xAC \xEB\xB9\xA8\xEB\x9D\xBC\xEC\xA7\x84 \xEA\xB2\x83\xEC\x9D\x84 \xEB\x8A\x90\xEB\x82\x80\xEB\x8B\xA4."}, /* 몸이 훨씬 빨라진 것을 느낀다. */
    {"you feel yourself slowing down", "\xEB\xAA\xB8\xEC\x9D\xB4 \xEB\x8A\x90\xEB\xA0\xA4\xEC\xA7\x80\xEB\x8A\x94 \xEA\xB2\x83\xEC\x9D\x84 \xEB\x8A\x90\xEB\x82\x80\xEB\x8B\xA4."}, /* 몸이 느려지는 것을 느낀다. */
    {"you feel time speed up suddenly", "\xEA\xB0\x91\xEC\x9E\x90\xEA\xB8\xB0 \xEC\x8B\x9C\xEA\xB0\x84\xEC\x9D\xB4 \xEB\xB9\xA8\xEB\x9D\xBC\xEC\xA7\x80\xEB\x8A\x94 \xEB\x8A\x90\xEB\x82\x8C\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 갑자기 시간이 빨라지는 느낌이다. */
    {"time now seems to be going slower", "\xEC\x9D\xB4\xEC\xA0\x9C \xEC\x8B\x9C\xEA\xB0\x84\xEC\x9D\xB4 \xEB\x8D\x94 \xEB\x8A\x90\xEB\xA6\xAC\xEA\xB2\x8C \xED\x9D\x90\xEB\xA5\xB4\xEB\x8A\x94 \xEA\xB2\x83 \xEA\xB0\x99\xEB\x8B\xA4."}, /* 이제 시간이 더 느리게 흐르는 것 같다. */
    {"you suddenly feel much more skillful", "\xEA\xB0\x91\xEC\x9E\x90\xEA\xB8\xB0 \xED\x9B\xA8\xEC\x94\xAC \xEB\x8A\xA5\xEC\x88\x99\xED\x95\xB4\xEC\xA7\x84 \xEB\x8A\x90\xEB\x82\x8C\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 갑자기 훨씬 능숙해진 느낌이다. */
    {"you suddenly feel very thirsty", "\xEA\xB0\x91\xEC\x9E\x90\xEA\xB8\xB0 \xEB\xAA\xB9\xEC\x8B\x9C \xEB\xAA\xA9\xEC\x9D\xB4 \xEB\xA7\x88\xEB\xA5\xB4\xEB\x8B\xA4."}, /* 갑자기 몹시 목이 마르다. */
    {"you have a tingling feeling", "\xEC\xB0\x8C\xEB\xA6\xBF\xED\x95\x9C \xEB\x8A\x90\xEB\x82\x8C\xEC\x9D\xB4 \xEB\x93\xA0\xEB\x8B\xA4."}, /* 찌릿한 느낌이 든다. */
    {"your nose tingles", "\xEC\xBD\x94\xEA\xB0\x80 \xEA\xB0\x84\xEC\xA7\x88\xEA\xB1\xB0\xEB\xA6\xB0\xEB\x8B\xA4."}, /* 코가 간질거린다. */
    {"you feel momentarily sick", "\xEC\x9E\xA0\xEC\x8B\x9C \xEB\xA9\x94\xEC\x8A\xA4\xEA\xBA\x88\xEC\x9B\x80\xEC\x9D\x84 \xEB\x8A\x90\xEB\x82\x9C\xEB\x8B\xA4."}, /* 잠시 메스꺼움을 느낀다. */
    {"you feel very sick now", "\xEC\x9D\xB4\xEC\xA0\x9C \xEB\xAA\xB9\xEC\x8B\x9C \xEB\xA9\x94\xEC\x8A\xA4\xEA\xBA\x99\xEB\x8B\xA4."}, /* 이제 몹시 메스껍다. */
    {"multi-colored lines swirl around you, then fade", "\xED\x98\x95\xED\x98\x95\xEC\x83\x89\xEC\x83\x89\xEC\x9D\x98 \xEC\x84\xA0\xEB\x93\xA4\xEC\x9D\xB4 \xEC\xA3\xBC\xEC\x9C\x84\xEB\xA5\xBC \xEB\xA7\xB4\xEB\x8F\x8C\xEB\x8B\xA4 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 형형색색의 선들이 주위를 맴돌다 사라진다. */
    {"everything looks so boring now.", "\xEC\x9D\xB4\xEC\xA0\x9C \xEB\xAA\xA8\xEB\x93\xA0 \xEA\xB2\x83\xEC\x9D\xB4 \xEB\x84\x88\xEB\xAC\xB4\xEB\x82\x98 \xEC\xA7\x80\xEB\xA3\xA8\xED\x95\xB4 \xEB\xB3\xB4\xEC\x9D\xB8\xEB\x8B\xA4."}, /* 이제 모든 것이 너무나 지루해 보인다. */

    /* traps */
    {"you fell into a trap!", "\xED\x95\xA8\xEC\xA0\x95\xEC\x97\x90 \xEB\xB9\xA0\xEC\xA1\x8C\xEB\x8B\xA4!"}, /* 함정에 빠졌다! */
    {"you are caught in a bear trap", "\xEA\xB3\xB0\xEB\x8D\xAB\xEC\x97\x90 \xEA\xB1\xB8\xEB\xA0\xB8\xEB\x8B\xA4."}, /* 곰덫에 걸렸다. */
    {"you are still stuck in the bear trap", "\xEC\x95\x84\xEC\xA7\x81 \xEA\xB3\xB0\xEB\x8D\xAB\xEC\x97\x90 \xEA\xB1\xB8\xEB\xA0\xA4 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 아직 곰덫에 걸려 있다. */
    {"a small dart whizzes by your ear and vanishes", "\xEC\x9E\x91\xEC\x9D\x80 \xED\x91\x9C\xEC\xB0\xBD\xEC\x9D\xB4 \xEA\xB7\x93\xEA\xB0\x80\xEB\xA5\xBC \xEC\x8A\xA4\xEC\xB9\x98\xEA\xB3\xA0 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 작은 표창이 귓가를 스치고 사라진다. */
    {"a small dart just hit you in the shoulder", "\xEC\x9E\x91\xEC\x9D\x80 \xED\x91\x9C\xEC\xB0\xBD\xEC\x9D\xB4 \xEC\x96\xB4\xEA\xB9\xA8\xEC\x97\x90 \xEB\xB0\x95\xED\x98\x94\xEB\x8B\xA4."}, /* 작은 표창이 어깨에 박혔다. */
    {"a strange white mist envelops you and you fall asleep", "\xEA\xB8\xB0\xEC\x9D\xB4\xED\x95\x9C \xED\x9D\xB0 \xEC\x95\x88\xEA\xB0\x9C\xEA\xB0\x80 \xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x84 \xEA\xB0\x90\xEC\x8B\xB8\xEA\xB3\xA0 \xEC\x9E\xA0\xEC\x97\x90 \xEB\xB9\xA0\xEC\xA7\x84\xEB\x8B\xA4."}, /* 기이한 흰 안개가 당신을 감싸고 잠에 빠진다. */
    {"a gush of water hits you on the head", "\xEB\xAC\xBC\xEC\xA4\x84\xEA\xB8\xB0\xEA\xB0\x80 \xEB\xA8\xB8\xEB\xA6\xAC\xEB\xA5\xBC \xEA\xB0\x95\xED\x83\x80\xED\x95\x9C\xEB\x8B\xA4."}, /* 물줄기가 머리를 강타한다. */
    {"a spike shoots past your ear!", "\xEC\x87\xA0\xEC\xB0\xBD\xEC\x9D\xB4 \xEA\xB7\x93\xEA\xB0\x80\xEB\xA5\xBC \xEC\x8A\xA4\xEC\xB3\x90 \xEC\xA7\x80\xEB\x82\x98\xEA\xB0\x84\xEB\x8B\xA4!"}, /* 쇠창이 귓가를 스쳐 지나간다! */
    {"the corridor glows and then fades", "\xEB\xB3\xB5\xEB\x8F\x84\xEA\xB0\x80 \xEB\xB9\x9B\xEB\x82\xAC\xEB\x8B\xA4\xEA\xB0\x80 \xEC\x82\xAC\xEA\xB7\xB8\xEB\x9D\xBC\xEB\x93\xA0\xEB\x8B\xA4."}, /* 복도가 빛났다가 사그라든다. */
    {"you fall asleep", "\xEC\x9E\xA0\xEC\x97\x90 \xEB\xB9\xA0\xEC\xA7\x84\xEB\x8B\xA4."}, /* 잠에 빠진다. */

    /* combat side-effects */
    {"a bite has weakened you", "\xEB\xAC\xBC\xEB\xA6\xB0 \xEC\x83\x81\xEC\xB2\x98\xEA\xB0\x80 \xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x84 \xEC\x95\xBD\xED\x95\x98\xEA\xB2\x8C \xED\x96\x88\xEB\x8B\xA4."}, /* 물린 상처가 당신을 약하게 했다. */
    {"a bite momentarily weakens you", "\xEB\xAC\xBC\xEB\xA6\xB0 \xEC\x83\x81\xEC\xB2\x98\xEA\xB0\x80 \xEC\x9E\xA0\xEC\x8B\x9C \xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x84 \xEC\x95\xBD\xED\x95\x98\xEA\xB2\x8C \xED\x95\x9C\xEB\x8B\xA4."}, /* 물린 상처가 잠시 당신을 약하게 한다. */
    {"bite has no effect", "\xEB\xAC\xBC\xEB\xA0\xB8\xEC\xA7\x80\xEB\xA7\x8C \xEC\x95\x84\xEB\xAC\xB4 \xED\x9A\xA8\xEA\xB3\xBC\xEA\xB0\x80 \xEC\x97\x86\xEB\x8B\xA4."}, /* 물렸지만 아무 효과가 없다. */
    {"you feel a bite in your leg and now feel weaker", "\xEB\x8B\xA4\xEB\xA6\xAC\xEA\xB0\x80 \xEB\xAC\xBC\xEB\xA0\xA4 \xEA\xB8\xB0\xEC\x9A\xB4\xEC\x9D\xB4 \xEB\xB9\xA0\xEC\xA7\x84\xEB\x8B\xA4."}, /* 다리가 물려 기운이 빠진다. */
    {"you feel a sting in the side of your neck", "\xEB\xAA\xA9\xEB\x8D\xA7\xEB\xAF\xB8\xEC\x97\x90 \xEB\x94\xB0\xEA\xB0\x80\xEC\x9A\xB4 \xED\x86\xB5\xEC\xA6\x9D\xEC\x9D\xB4 \xEB\x8A\x90\xEA\xBB\xB4\xEC\xA7\x84\xEB\x8B\xA4."}, /* 목덜미에 따가운 통증이 느껴진다. */

    /* arrows / sounds */
    {"oh no! an arrow shot you", "\xEC\x9D\xB4\xEB\x9F\xB0! \xED\x99\x94\xEC\x82\xB4\xEC\x9D\xB4 \xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x84 \xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4."}, /* 이런! 화살이 당신을 맞혔다. */
    {"an arrow shoots past you", "\xED\x99\x94\xEC\x82\xB4\xEC\x9D\xB4 \xEA\xB3\x81\xEC\x9D\x84 \xEC\x8A\xA4\xEC\xB3\x90 \xEC\xA7\x80\xEB\x82\x98\xEA\xB0\x84\xEB\x8B\xA4."}, /* 화살이 곁을 스쳐 지나간다. */
    {"you hear a faint cry of anguish in the distance", "\xEB\xA9\x80\xEB\xA6\xAC\xEC\x84\x9C \xED\x9D\xAC\xEB\xAF\xB8\xED\x95\x9C \xEB\xB9\x84\xEB\xAA\x85\xEC\x9D\xB4 \xEB\x93\xA4\xEB\xA6\xB0\xEB\x8B\xA4."}, /* 멀리서 희미한 비명이 들린다. */
    {"you hear a high pitched humming noise", "\xEB\x86\x92\xEC\x9D\x80 \xEC\x9C\x99\xEC\x9C\x99\xEA\xB1\xB0\xEB\xA6\xBC\xEC\x9D\xB4 \xEB\x93\xA4\xEB\xA6\xB0\xEB\x8B\xA4."}, /* 높은 윙윙거림이 들린다. */
    {"you hear maniacal laughter in the distance", "\xEB\xA9\x80\xEB\xA6\xAC\xEC\x84\x9C \xEA\xB4\x91\xEA\xB8\xB0 \xEC\x96\xB4\xEB\xA6\xB0 \xEC\x9B\x83\xEC\x9D\x8C\xEC\x86\x8C\xEB\xA6\xAC\xEA\xB0\x80 \xEB\x93\xA4\xEB\xA6\xB0\xEB\x8B\xA4."}, /* 멀리서 광기 어린 웃음소리가 들린다. */

    /* eat / drink / inedible */
    {"that's inedible!", "\xEB\xA8\xB9\xEC\x9D\x84 \xEC\x88\x98 \xEC\x97\x86\xEB\x8A\x94 \xEA\xB2\x83\xEC\x9D\xB4\xEB\x8B\xA4!"}, /* 먹을 수 없는 것이다! */
    {"that's undrinkable", "\xEB\xA7\x88\xEC\x8B\xA4 \xEC\x88\x98 \xEC\x97\x86\xEB\x8A\x94 \xEA\xB2\x83\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 마실 수 없는 것이다. */
    {"ugh, you would get ill if you ate that", "\xEC\x9C\x99, \xEA\xB7\xB8\xEA\xB1\xB8 \xEB\xA8\xB9\xEC\x9C\xBC\xEB\xA9\xB4 \xED\x83\x88\xEC\x9D\xB4 \xEB\x82\xA0 \xEA\xB2\x83\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 윽, 그걸 먹으면 탈이 날 것이다. */
    {"yuk! why would you want to drink that?", "\xEC\x9C\x99! \xEA\xB7\xB8\xEA\xB1\xB8 \xEC\x99\x9C \xEB\xA7\x88\xEC\x8B\x9C\xEB\xA0\xA4 \xED\x95\x98\xEB\x8A\x94\xEA\xB0\x80?"}, /* 윽! 그걸 왜 마시려 하는가? */
    {"hey, this tastes great.  it make you feel warm all over", "\xEC\x96\xB4, \xEB\xA7\x9B\xEC\x9D\xB4 \xEC\xA2\x8B\xEB\x8B\xA4. \xEC\x98\xA8\xEB\xAA\xB8\xEC\x9D\xB4 \xEB\x94\xB0\xEC\x8A\xA4\xED\x95\xB4\xEC\xA7\x84\xEB\x8B\xA4."}, /* 어, 맛이 좋다. 온몸이 따스해진다. */

    /* pack / wield / wear / rings */
    {"you can't.  it appears to be cursed", "\xEA\xB7\xB8\xEB\x9F\xB4 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4. \xEC\xA0\x80\xEC\xA3\xBC\xEB\xB0\x9B\xEC\x9D\x80 \xEA\xB2\x83 \xEA\xB0\x99\xEB\x8B\xA4."}, /* 그럴 수 없다. 저주받은 것 같다. */
    {"you are too weak to use it", "\xEA\xB7\xB8\xEA\xB2\x83\xEC\x9D\x84 \xEC\x93\xB0\xEA\xB8\xB0\xEC\x97\x94 \xEB\x84\x88\xEB\xAC\xB4 \xEC\x95\xBD\xED\x95\x98\xEB\x8B\xA4."}, /* 그것을 쓰기엔 너무 약하다. */
    {"nothing to read", "\xEC\x9D\xBD\xEC\x9D\x84 \xEA\xB2\x83\xEC\x9D\xB4 \xEC\x97\x86\xEB\x8B\xA4."}, /* 읽을 것이 없다. */
    {"there is nothing on it to read", "\xEA\xB1\xB0\xEA\xB8\xB0\xEC\x97\x94 \xEC\x9D\xBD\xEC\x9D\x84 \xEA\xB2\x83\xEC\x9D\xB4 \xEC\xA0\x81\xED\x98\x80 \xEC\x9E\x88\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8B\xA4."}, /* 거기엔 읽을 것이 적혀 있지 않다. */
    {"you can't wield armor", "\xEA\xB0\x91\xEC\x98\xB7\xEC\x9D\x80 \xEB\xAC\xB4\xEA\xB8\xB0\xEB\xA1\x9C \xEB\x93\xA4 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4."}, /* 갑옷은 무기로 들 수 없다. */
    {"you can't wear that", "\xEA\xB7\xB8\xEA\xB2\x83\xEC\x9D\x80 \xEC\x9E\x85\xEC\x9D\x84 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4."}, /* 그것은 입을 수 없다. */
    {"you can't zap with that!", "\xEA\xB7\xB8\xEA\xB2\x83\xEC\x9C\xBC\xEB\xA1\x9C\xEB\x8A\x94 \xEB\xA7\x88\xEB\xB2\x95\xEB\xB4\x89\xEC\x9D\x84 \xEC\x93\xB8 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4!"}, /* 그것으로는 마법봉을 쓸 수 없다! */
    {"not wearing armor", "\xEA\xB0\x91\xEC\x98\xB7\xEC\x9D\x84 \xEC\x9E\x85\xEA\xB3\xA0 \xEC\x9E\x88\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8B\xA4."}, /* 갑옷을 입고 있지 않다. */
    {"you aren't wearing any armor", "\xEA\xB0\x91\xEC\x98\xB7\xEC\x9D\x84 \xEC\x9E\x85\xEA\xB3\xA0 \xEC\x9E\x88\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8B\xA4."}, /* 갑옷을 입고 있지 않다. */
    {"you aren't wearing any rings", "\xEB\xB0\x98\xEC\xA7\x80\xEB\xA5\xBC \xEB\x81\xBC\xEA\xB3\xA0 \xEC\x9E\x88\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8B\xA4."}, /* 반지를 끼고 있지 않다. */
    {"you already have a ring on each hand", "\xEC\x96\x91\xEC\x86\x90\xEC\x97\x90 \xEC\x9D\xB4\xEB\xAF\xB8 \xEB\xB0\x98\xEC\xA7\x80\xEB\xA5\xBC \xEB\x81\xBC\xEA\xB3\xA0 \xEC\x9E\x88\xEB\x8B\xA4."}, /* 양손에 이미 반지를 끼고 있다. */
    {"it would be difficult to wrap that around a finger", "\xEA\xB7\xB8\xEA\xB2\x83\xEC\x9D\x84 \xEC\x86\x90\xEA\xB0\x80\xEB\x9D\xBD\xEC\x97\x90 \xEB\x81\xBC\xEC\x9A\xB0\xEA\xB8\xB4 \xEC\x96\xB4\xEB\xA0\xB5\xEB\x8B\xA4."}, /* 그것을 손가락에 끼우긴 어렵다. */
    {"not a ring", "\xEB\xB0\x98\xEC\xA7\x80\xEA\xB0\x80 \xEC\x95\x84\xEB\x8B\x88\xEB\x8B\xA4."}, /* 반지가 아니다. */
    {"no rings", "\xEB\xB0\x98\xEC\xA7\x80\xEA\xB0\x80 \xEC\x97\x86\xEB\x8B\xA4."}, /* 반지가 없다. */
    {"you don't have anything in your pack to identify", "\xEA\xB0\x80\xEB\xB0\xA9\xEC\x97\x90 \xEC\x8B\x9D\xEB\xB3\x84\xED\x95\xA0 \xEB\xAC\xBC\xEA\xB1\xB4\xEC\x9D\xB4 \xEC\x97\x86\xEB\x8B\xA4."}, /* 가방에 식별할 물건이 없다. */
    {"that has already been identified", "\xEC\x9D\xB4\xEB\xAF\xB8 \xEC\x8B\x9D\xEB\xB3\x84\xEB\x90\x9C \xEB\xAC\xBC\xEA\xB1\xB4\xEC\x9D\xB4\xEB\x8B\xA4."}, /* 이미 식별된 물건이다. */
    {"you can't call that anything", "\xEA\xB1\xB0\xEA\xB8\xB0\xEC\x97\x94 \xEC\x9D\xB4\xEB\xA6\x84\xEC\x9D\x84 \xEB\xB6\x99\xEC\x9D\xB4 \xEC\x88\x98 \xEC\x97\x86\xEB\x8B\xA4."}, /* 거기엔 이름을 붙일 수 없다. */
    {"the scroll turns to dust as you pick it up", "\xEB\x91\x90\xEB\xA3\xA8\xEB\xA7\x88\xEB\xA6\xAC\xEA\xB0\x80 \xEC\xA7\x91\xEC\x96\xB4 \xEB\x93\xA4\xEB\x8A\x94 \xEC\x88\x9C\xEA\xB0\x84 \xEB\xA8\xBC\xEC\xA7\x80\xEA\xB0\x80 \xEB\x90\x98\xEC\x96\xB4 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 두루마리가 집어 드는 순간 먼지가 되어 사라진다. */
    {"oh, now this scroll has a map on it", "\xEC\x98\xA4, \xEC\x9D\xB4 \xEB\x91\x90\xEB\xA3\xA8\xEB\xA7\x88\xEB\xA6\xAC\xEC\x97\x90 \xEC\xA7\x80\xEB\x8F\x84\xEA\xB0\x80 \xEB\x82\x98\xED\x83\x80\xEB\x82\xAC\xEB\x8B\xA4."}, /* 오, 이 두루마리에 지도가 나타났다. */
    {"the room is lit", "\xEB\xB0\xA9\xEC\x9D\xB4 \xED\x99\x98\xED\x95\xB4\xEC\xA7\x84\xEB\x8B\xA4."}, /* 방이 환해진다. */
    {"the rust vanishes instantly", "\xEB\x85\xB9\xEC\x9D\xB4 \xEC\x88\x9C\xEC\x8B\x9D\xEA\xB0\x84\xEC\x97\x90 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 녹이 순식간에 사라진다. */
    {"your armor weakens", "\xEA\xB0\x91\xEC\x98\xB7\xEC\x9D\xB4 \xEC\x95\xBD\xED\x95\xB4\xEC\xA7\x84\xEB\x8B\xA4."}, /* 갑옷이 약해진다. */
    {"your armor appears to be weaker now. oh my!", "\xEA\xB0\x91\xEC\x98\xB7\xEC\x9D\xB4 \xEB\x8D\x94 \xEC\x95\xBD\xED\x95\xB4\xEC\xA7\x84 \xEA\xB2\x83 \xEA\xB0\x99\xEB\x8B\xA4. \xEC\x9D\xB4\xEB\x9F\xB0!"}, /* 갑옷이 더 약해진 것 같다. 이런! */
    {"the missle vanishes with a puff of smoke", "\xED\x88\xAC\xEC\x82\xAC\xEC\xB2\xB4\xEA\xB0\x80 \xEC\x97\xB0\xEA\xB8\xB0\xEB\xA5\xBC \xEB\x82\xB4\xEB\xA9\xB0 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 투사체가 연기를 내며 사라진다. */
    {"missle vanishes", "\xED\x88\xAC\xEC\x82\xAC\xEC\xB2\xB4\xEA\xB0\x80 \xEC\x82\xAC\xEB\x9D\xBC\xEC\xA7\x84\xEB\x8B\xA4."}, /* 투사체가 사라진다. */

    {0, 0}
};

/* ============================================================
 *  Buffers + small helpers
 * ============================================================ */
#define NBUF 4
static char ring[NBUF][256];
static int ring_i = 0;
static char *take_buf(void) { char *b = ring[ring_i]; ring_i = (ring_i + 1) % NBUF; return b; }

/* Lowercase the whole of `en` into `low` (cap n). All matching is done on this
 * normalized copy: endmsg() upper-cases msgbuf[0], and some source fragments
 * carry internal capitals ("What", ".  Defeated ", "I see"), so the table keys,
 * frame affixes and combat phrases are all stored lowercase. The Korean output
 * is composed from the tables, never echoed from `low`, so this is lossless;
 * only an *untranslated* English noun would show up lowercased (acceptable). */
static void lower_all(const char *en, char *low, int n)
{
    int i = 0;
    for (; en[i] && i < n - 1; i++)
        low[i] = (char)tolower((unsigned char)en[i]);
    low[i] = '\0';
}

/* match prefix+suffix frame; on success copy the middle into `mid`. */
static int frame(const char *s, const char *pre, const char *suf, char *mid, int midsz)
{
    size_t ls = strlen(s), lp = strlen(pre), lf = strlen(suf);
    if (ls < lp + lf)
        return 0;
    if (strncmp(s, pre, lp) != 0)
        return 0;
    if (strcmp(s + ls - lf, suf) != 0)
        return 0;
    int mlen = (int)(ls - lp - lf);
    if (mlen >= midsz)
        mlen = midsz - 1;
    memcpy(mid, s + lp, mlen);
    mid[mlen] = '\0';
    return 1;
}

/* ============================================================
 *  Combat — "<A> <verb> <B>" assembled from fight.c h_names/m_names
 * ============================================================ */
/* fight.c verb tables (h_names hit / m_names miss), ordered LONGEST FIRST so the
 * generic " hit " / " miss" can't pre-empt a more specific phrase — crucially the
 * negatives " doesn't hit" / " don't hit" must be tested before " hit ". */
static const struct { const char *ph; int hit; } CVERB[] = {
    {" scored an excellent hit on ", 1},
    {" swings and misses", 0},
    {" swings and hits ", 1},
    {" swing and miss", 0},
    {" swing and hit ", 1},
    {" have injured ", 1},
    {" barely misses", 0},
    {" has injured ", 1},
    {" barely miss", 0},
    {" doesn't hit", 0},
    {" don't hit", 0},
    {" missed ", 0},
    {" misses", 0},
    {" hit ", 1},
    {" miss", 0},
    {0, 0}
};

static int try_combat(const char *low, char **out)
{
    const char *pos = 0, *hit_ph = 0;
    int is_hit = 0, i;

    for (i = 0; CVERB[i].ph; i++)
        if ((pos = strstr(low, CVERB[i].ph))) { hit_ph = CVERB[i].ph; is_hit = CVERB[i].hit; break; }
    if (!pos)
        return 0;

    char atk_en[96], tgt_en[96], atk[64], tgt[64];
    int alen = (int)(pos - low);
    if (alen >= (int)sizeof atk_en) alen = (int)sizeof atk_en - 1;
    memcpy(atk_en, low, alen); atk_en[alen] = '\0';
    snprintf(tgt_en, sizeof tgt_en, "%s", pos + strlen(hit_ph));

    /* trim */
    char *t = tgt_en; while (*t == ' ') t++;
    size_t tl = strlen(t); while (tl && t[tl - 1] == ' ') t[--tl] = '\0';

    tr_noun(atk, sizeof atk, atk_en);
    char *b = take_buf();
    if (*t == '\0') {
        /* no target (terse): "<A>의 공격이 적중/빗나갔다." */
        snprintf(b, 256, "%s\xEC\x9D\x98 \xEA\xB3\xB5\xEA\xB2\xA9\xEC\x9D\xB4 %s", atk,
                 is_hit ? "\xEC\xA0\x81\xEC\xA4\x91\xED\x96\x88\xEB\x8B\xA4." /* 적중했다. */
                        : "\xEB\xB9\x97\xEB\x82\x98\xEA\xB0\x94\xEB\x8B\xA4."); /* 빗나갔다. */
    } else {
        char atk_p[80], tgt_p[80];
        tr_noun(tgt, sizeof tgt, t);
        i18n_iga(atk_p, sizeof atk_p, atk);
        i18n_eul(tgt_p, sizeof tgt_p, tgt);
        snprintf(b, 256, "%s %s %s", atk_p, tgt_p,
                 is_hit ? "\xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4." /* 맞혔다. */
                        : "\xEB\xB9\x97\xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4."); /* 빗맞혔다. */
    }
    *out = b;
    return 1;
}

/* ============================================================
 *  tr_msg — the entry point
 * ============================================================ */
const char *tr_msg(const char *en)
{
    if (en == 0)
        return "";

    char low[256];
    lower_all(en, low, sizeof low);

    /* 1. exact fixed messages */
    const char *kr = lookup(EXACT, low);
    if (kr)
        return kr;

    /* 2. parametrized frames */
    char mid[160], noun[160], *b;

    if (frame(low, "there is ", " to pick up", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_eul(part, sizeof part, noun);
        snprintf(b, 256, "\xEC\x97\xAC\xEA\xB8\xB0 %s \xEC\xA3\xBC\xEC\x9A\xB8 \xEC\x88\x98 \xEC\x9E\x88\xEB\x8B\xA4.", part); /* 여기 %s 주울 수 있다. */
        return b;
    }
    if (frame(low, "you found ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_eul(part, sizeof part, noun);
        snprintf(b, 256, "%s \xEB\xB0\x9C\xEA\xB2\xAC\xED\x96\x88\xEB\x8B\xA4.", part); /* %s 발견했다. */
        return b;
    }
    if (frame(low, "dropped ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_eul(part, sizeof part, noun);
        snprintf(b, 256, "%s \xEB\x82\xB4\xEB\xA0\xA4\xEB\x86\x93\xEC\x95\x98\xEB\x8B\xA4.", part); /* %s 내려놓았다. */
        return b;
    }
    if (frame(low, "moved onto ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        snprintf(b, 256, "%s \xEC\x9C\x84\xEB\xA1\x9C \xEC\x98\xAC\xEB\x9D\xBC\xEC\x84\xB0\xEB\x8B\xA4.", noun); /* %s 위로 올라섰다. */
        return b;
    }
    if (frame(low, "started a wandering ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_iga(part, sizeof part, noun);
        snprintf(b, 256, "%s \xEB\x96\xA0\xEB\x8F\x8C\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x96\x88\xEB\x8B\xA4.", part); /* %s 떠돌기 시작했다. */
        return b;
    }
    if (frame(low, "", " appears confused", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_iga(part, sizeof part, noun);
        snprintf(b, 256, "%s \xED\x98\xBC\xEB\x9E\x80\xEC\x97\x90 \xEB\xB9\xA0\xEC\xA7\x84 \xEB\x93\xAF\xED\x95\x98\xEB\x8B\xA4.", part); /* %s 혼란에 빠진 듯하다. */
        return b;
    }
    /* "welcome to level %d" -> "N층에 온 것을 환영한다." */
    if (frame(low, "welcome to level ", "", mid, sizeof mid)) {
        b = take_buf();
        snprintf(b, 256, "%s\xEC\xB8\xB5\xEC\x97\x90 \xEC\x98\xA8 \xEA\xB2\x83\xEC\x9D\x84 \xED\x99\x98\xEC\x98\x81\xED\x95\x9C\xEB\x8B\xA4.", mid); /* %s층에 온 것을 환영한다. */
        return b;
    }
    /* "%d gold pieces" -> "금화 N닢." */
    if (frame(low, "", " gold pieces", mid, sizeof mid)) {
        b = take_buf();
        snprintf(b, 256, "\xEA\xB8\x88\xED\x99\x94 %s\xEB\x8B\xA2.", mid); /* 금화 %s닢. */
        return b;
    }
    /* "your hands begin to glow %s" -> "두 손이 <색>으로 빛나기 시작한다." */
    if (frame(low, "your hands begin to glow ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        char part[200]; i18n_ro(part, sizeof part, noun);
        snprintf(b, 256, "\xEB\x91\x90 \xEC\x86\x90\xEC\x9D\xB4 %s \xEB\xB9\x9B\xEB\x82\x98\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x95\x9C\xEB\x8B\xA4.", part); /* 두 손이 %s 빛나기 시작한다. */
        return b;
    }
    /* "your hands stop glowing %s" -> "두 손의 <색> 빛이 사그라든다." */
    if (frame(low, "your hands stop glowing ", "", mid, sizeof mid)) {
        tr_noun(noun, sizeof noun, mid);
        b = take_buf();
        snprintf(b, 256, "\xEB\x91\x90 \xEC\x86\x90\xEC\x9D\x98 %s \xEB\xB9\x9B\xEC\x9D\xB4 \xEC\x82\xAC\xEA\xB7\xB8\xEB\x9D\xBC\xEB\x93\xA0\xEB\x8B\xA4.", noun); /* 두 손의 %s 빛이 사그라든다. */
        return b;
    }

    /* defeat / kill (fight.c killed()) — "you have defeated X", terse "defeated X",
     * and the running-kill combo "...<hit>.  Defeated X". Checked before combat
     * so the combo's embedded " hit " doesn't get grabbed by try_combat. */
    {
        const char *d = strstr(low, ".  defeated ");
        const char *dm = 0;
        if (d)
            dm = d + strlen(".  defeated ");
        else if (frame(low, "you have defeated ", "", mid, sizeof mid))
            dm = mid;
        else if (frame(low, "defeated ", "", mid, sizeof mid))
            dm = mid;
        if (dm) {
            tr_noun(noun, sizeof noun, dm);
            b = take_buf();
            char part[200]; i18n_eul(part, sizeof part, noun);
            snprintf(b, 256, "%s \xEC\xB2\x98\xEC\xB9\x98\xED\x96\x88\xEB\x8B\xA4.", part); /* %s 처치했다. */
            return b;
        }
    }

    /* 3. combat */
    if (try_combat(low, &b))
        return b;

    /* 4. fallback: original English, untouched */
    return en;
}

/* ============================================================
 *  Self-test (native): cc -DI18N_TEST i18n.c -o t && ./t
 * ============================================================ */
#ifdef I18N_TEST
static int fails = 0;
static void chk(const char *label, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL %-22s got [%s] want [%s]\n", label, got, want);
        fails++;
    } else {
        printf("ok   %-22s %s\n", label, got);
    }
}
int main(void)
{
    char buf[128];

    /* 받침 detection */
    chk("batchim 단검", i18n_has_batchim("\xEB\x8B\xA8\xEA\xB2\x80") ? "Y" : "N", "Y");  /* 검: 받침 O */
    chk("batchim 박쥐", i18n_has_batchim("\xEB\xB0\x95\xEC\xA5\x90") ? "Y" : "N", "N");  /* 쥐: 받침 X */

    /* 조사 */
    i18n_eul(buf, sizeof buf, "\xEB\x8B\xA8\xEA\xB2\x80"); chk("eul 단검", buf, "\xEB\x8B\xA8\xEA\xB2\x80\xEC\x9D\x84"); /* 단검을 */
    i18n_eul(buf, sizeof buf, "\xEB\xB0\x95\xEC\xA5\x90"); chk("eul 박쥐", buf, "\xEB\xB0\x95\xEC\xA5\x90\xEB\xA5\xBC"); /* 박쥐를 */
    i18n_iga(buf, sizeof buf, "\xEB\xB0\x95\xEC\xA5\x90"); chk("iga 박쥐", buf, "\xEB\xB0\x95\xEC\xA5\x90\xEA\xB0\x80"); /* 박쥐가 */

    /* exact */
    chk("exact nothing", tr_msg("Nothing happens"),
        "\xEC\x95\x84\xEB\xAC\xB4 \xEC\x9D\xBC\xEB\x8F\x84 \xEC\x9D\xBC\xEC\x96\xB4\xEB\x82\x98\xEC\xA7\x80 \xEC\x95\x8A\xEB\x8A\x94\xEB\x8B\xA4."); /* 아무 일도 일어나지 않는다. */

    /* frames */
    chk("welcome", tr_msg("Welcome to level 3"),
        "3\xEC\xB8\xB5\xEC\x97\x90 \xEC\x98\xA8 \xEA\xB2\x83\xEC\x9D\x84 \xED\x99\x98\xEC\x98\x81\xED\x95\x9C\xEB\x8B\xA4."); /* 3층에 온 것을 환영한다. */
    chk("wander", tr_msg("Started a wandering hobgoblin"),
        "\xED\x99\x89\xEA\xB3\xA0\xEB\xB8\x94\xEB\xA6\xB0\xEC\x9D\xB4 \xEB\x96\xA0\xEB\x8F\x8C\xEA\xB8\xB0 \xEC\x8B\x9C\xEC\x9E\x91\xED\x96\x88\xEB\x8B\xA4."); /* 홉고블린이 떠돌기 시작했다. */

    /* combat */
    chk("you hit kobold", tr_msg("You hit the hobgoblin"),
        "\xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\xB4 \xED\x99\x89\xEA\xB3\xA0\xEB\xB8\x94\xEB\xA6\xB0\xEC\x9D\x84 \xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4."); /* 당신이 홉고블린을 맞혔다. */
    chk("bat hits you", tr_msg("The bat has injured you"),
        "\xEB\xB0\x95\xEC\xA5\x90\xEA\xB0\x80 \xEB\x8B\xB9\xEC\x8B\xA0\xEC\x9D\x84 \xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4."); /* 박쥐가 당신을 맞혔다. */

    /* fallback (untranslated stays English) */
    chk("fallback", tr_msg("zorptstring xyzzy"), "zorptstring xyzzy");

    printf("\n%s (%d failure(s))\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
#endif
