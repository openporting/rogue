/* i18n.c — English -> Korean translation of Rogue's assembled messages.
 *
 * tr_msg() is called from the patched endmsg() (io.c) with the fully assembled
 * English sentence. It matches that sentence against TRANSLATIONS[] (a table of
 * English patterns -> Korean templates) and returns the Korean rendering, or the
 * original English when nothing matches (so untranslated text still shows up).
 *
 * Pattern syntax (English side):
 *   - printf-style specs (%s %d %c %ld %*d %-5d %.5s ...) act as wildcards and
 *     are captured left-to-right into $1..$9. %% matches a literal '%'.
 *   - matching is full-string and ASCII-case-insensitive (endmsg() upper-cases
 *     the first letter, so "you hit" must match "You hit").
 *
 * Template syntax (Korean side):
 *   - $1..$9 insert the corresponding capture.
 *   - $N{을} appends a 조사 chosen by whether capture N ends in a 받침. Supported
 *     pairs: 을/를, 이/가, 은/는, 과/와, 으로/로, 아/야 (write the 받침 form).
 *
 * Native test:  gcc -DI18N_TEST i18n.c -o i18n_test && ./i18n_test
 */
#include "i18n.h"
#include <string.h>
#include <stdio.h>

/* ---- Hangul 받침 detection (UTF-8) ---- */

/* Decode the last Unicode code point of a UTF-8 string. -1 if empty. */
static long last_codepoint(const char *s)
{
    size_t len = strlen(s);
    if (len == 0)
        return -1;
    size_t i = len;
    do { i--; } while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80);
    const unsigned char *p = (const unsigned char *)s + i;
    int n = (int)(len - i);
    if (p[0] < 0x80)                  return p[0];
    if ((p[0] & 0xE0) == 0xC0 && n >= 2)
        return ((long)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    if ((p[0] & 0xF0) == 0xE0 && n >= 3)
        return ((long)(p[0] & 0x0F) << 12) | ((long)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    if ((p[0] & 0xF8) == 0xF0 && n >= 4)
        return ((long)(p[0] & 0x07) << 18) | ((long)(p[1] & 0x3F) << 12)
             | ((long)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    return p[0];
}

/* 종성(받침) index 0..27 for a Hangul syllable, or -1 if the tail isn't Hangul.
 * 0 = no 받침, 8 = ㄹ (needed for the 으로/로 rule). */
static int han_jong(const char *word)
{
    long cp = last_codepoint(word);
    if (cp < 0xAC00 || cp > 0xD7A3)
        return -1;
    return (int)((cp - 0xAC00) % 28);
}

int han_has_batchim(const char *word)
{
    int j = han_jong(word);
    return j > 0;
}

/* Resolve a 조사 written in its 받침 form against `word`. Returns a static
 * particle string, or NULL if `spec` isn't a 조사 we manage (caller emits it
 * verbatim). */
static const char *josa(const char *word, const char *spec)
{
    int b = han_has_batchim(word);
    if (!strcmp(spec, "\xEC\x9D\x84") || !strcmp(spec, "\xEB\xA5\xBC"))  /* 을 / 를 */
        return b ? "\xEC\x9D\x84" : "\xEB\xA5\xBC";
    if (!strcmp(spec, "\xEC\x9D\xB4") || !strcmp(spec, "\xEA\xB0\x80"))  /* 이 / 가 */
        return b ? "\xEC\x9D\xB4" : "\xEA\xB0\x80";
    if (!strcmp(spec, "\xEC\x9D\x80") || !strcmp(spec, "\xEB\x8A\x94"))  /* 은 / 는 */
        return b ? "\xEC\x9D\x80" : "\xEB\x8A\x94";
    if (!strcmp(spec, "\xEA\xB3\xBC") || !strcmp(spec, "\xEC\x99\x80"))  /* 과 / 와 */
        return b ? "\xEA\xB3\xBC" : "\xEC\x99\x80";
    if (!strcmp(spec, "\xEC\x95\x84") || !strcmp(spec, "\xEC\x95\xBC"))  /* 아 / 야 */
        return b ? "\xEC\x95\x84" : "\xEC\x95\xBC";
    if (!strcmp(spec, "\xEC\x9C\xBC\xEB\xA1\x9C") || !strcmp(spec, "\xEB\xA1\x9C")) { /* 으로 / 로 */
        int j = han_jong(word);
        return (j <= 0 || j == 8) ? "\xEB\xA1\x9C" : "\xEC\x9C\xBC\xEB\xA1\x9C";
    }
    return NULL;
}

/* ---- pattern matching ---- */

#define MAX_CAPS 9
#define CAP_LEN  192

static int ci_eq(char a, char b)
{
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    return a == b;
}

/* case-insensitive substring search; returns pointer into hay or NULL */
static const char *ci_find(const char *hay, const char *needle, size_t nlen)
{
    if (nlen == 0)
        return hay;
    for (; *hay; hay++) {
        size_t k = 0;
        while (k < nlen && hay[k] && ci_eq(hay[k], needle[k]))
            k++;
        if (k == nlen)
            return hay;
    }
    return NULL;
}

/* Match `pat` against the whole of `in`, filling caps[]. Returns 1 on success. */
static int match(const char *pat, const char *in,
                 char caps[MAX_CAPS][CAP_LEN], int *ncap)
{
    const char *p = pat, *q = in;
    *ncap = 0;
    while (*p) {
        if (*p == '%' && p[1] == '%') {       /* literal percent */
            if (*q != '%') return 0;
            p += 2; q++;
            continue;
        }
        if (*p == '%') {                      /* wildcard: skip the spec */
            char conv = 0;
            p++;
            while (*p && !strchr("diouxXeEfgGaAcspn", *p))
                p++;
            if (*p) conv = *p++;

            /* literal that delimits the end of this capture */
            const char *delim = p;
            size_t dlen = 0;
            while (delim[dlen] && delim[dlen] != '%')
                dlen++;

            const char *cap_end;
            if (dlen == 0) {
                /* no following literal: rest of string, except adjacent %c */
                if (*p == '%' && conv == 'c')
                    cap_end = (*q) ? q + 1 : q;   /* one char per %c */
                else
                    cap_end = q + strlen(q);
            } else {
                cap_end = ci_find(q, delim, dlen);
                if (!cap_end)
                    return 0;
            }

            size_t clen = (size_t)(cap_end - q);
            if (*ncap < MAX_CAPS) {
                size_t n = clen < CAP_LEN - 1 ? clen : CAP_LEN - 1;
                memcpy(caps[*ncap], q, n);
                caps[*ncap][n] = '\0';
                (*ncap)++;
            }
            q = cap_end;
            continue;
        }
        if (!*q || !ci_eq(*p, *q))            /* literal char */
            return 0;
        p++; q++;
    }
    return *q == '\0';
}

/* Expand a Korean template into `out` (size `cap`). */
static void expand(const char *tmpl, char caps[MAX_CAPS][CAP_LEN], int ncap,
                   char *out, size_t cap)
{
    size_t o = 0;
    const char *t = tmpl;
    const char *last = "";                    /* most recently inserted capture */
    while (*t && o + 1 < cap) {
        if (*t == '$' && t[1] >= '1' && t[1] <= '9') {
            int idx = t[1] - '1';
            t += 2;
            const char *val = (idx < ncap) ? caps[idx] : "";
            last = val;
            for (; *val && o + 1 < cap; val++)
                out[o++] = *val;
            if (*t == '{') {                  /* attached 조사 */
                const char *e = strchr(t, '}');
                if (e) {
                    char spec[16];
                    size_t sl = (size_t)(e - (t + 1));
                    if (sl >= sizeof spec) sl = sizeof spec - 1;
                    memcpy(spec, t + 1, sl);
                    spec[sl] = '\0';
                    const char *j = josa(last, spec);
                    const char *w = j ? j : spec;
                    for (; *w && o + 1 < cap; w++)
                        out[o++] = *w;
                    t = e + 1;
                }
            }
            continue;
        }
        out[o++] = *t++;
    }
    out[o] = '\0';
}

/* ---- translation table ----
 * More specific patterns (more literals / more args) must precede the patterns
 * they could otherwise be shadowed by; the first full match wins. */
static const struct { const char *en, *ko; } TRANSLATIONS[] = {
    /* combat (assembled in fight.c) */
    { "the %s hits %s",   "$1{이} $2{을} 맞혔다." },
    { "the %s misses %s", "$1{이} $2{을} 빗맞혔다." },
    { "you hit %s",       "$1{을} 맞혔다." },
    { "you missed %s",    "$1{을} 빗맞혔다." },
    { "the %s hits",      "$1{이} 명중한다." },
    { "the %s misses ",   "$1{이} 빗나간다." },
    { "the %s bounces",   "$1{이} 튕겨 나간다." },
    { "the %s vanishes as it hits the ground", "$1{이} 땅에 떨어지며 사라진다." },
    { "the %s whizzes past %s", "$1{이} $2{을} 스쳐 지나간다." },
    { "the %s whizzes by you",  "$1{이} 당신을 스쳐 지나간다." },
    { "%s appears confused", "$1{이} 혼란에 빠진 듯하다." },
    { "%s misses",        "$1{이} 빗나간다." },
    { "you are hit by the %s", "$1에 맞았다." },
    { "she stole %s!",    "$1{을} 훔쳐 갔다!" },

    /* pickup / pack (assembled in pack.c, command.c) */
    { "you found %d gold pieces", "금화 $1닢을 발견했다." },
    { "you now have %s (%c)",     "이제 $1{을} 가지고 있다. ($2)" },
    { "you found a trapdoor",         "함정문을 발견했다." },
    { "you found an arrow trap",      "화살 함정을 발견했다." },
    { "you found a sleeping gas trap","수면 가스 함정을 발견했다." },
    { "you found a beartrap",         "곰덫을 발견했다." },
    { "you found a teleport trap",    "순간이동 함정을 발견했다." },
    { "you found a poison dart trap", "독 다트 함정을 발견했다." },
    { "you found a rust trap",        "녹 함정을 발견했다." },
    { "you found a mysterious trap",  "정체불명의 함정을 발견했다." },
    { "you found a secret door",      "비밀 문을 발견했다." },
    { "you found %s",     "$1{을} 발견했다." },
    { "dropped %s",       "$1{을} 내려놓았다." },
    { "moved onto %s",    "$1 위로 이동했다." },

    /* traps / hazards */
    { "you fell into a trap!",        "함정에 빠졌다!" },
    { "you are caught in a bear trap","곰덫에 걸렸다." },
    { "you are still stuck in the bear trap", "여전히 곰덫에 걸려 있다." },
    { "a small dart just hit you in the shoulder", "작은 다트가 어깨에 꽂혔다." },
    { "a small dart whizzes by your ear and vanishes", "작은 다트가 귓가를 스치고 사라진다." },
    { "a spike shoots past your ear!", "쇠못이 귓가를 스쳐 날아간다!" },
    { "a gush of water hits you on the head", "물줄기가 머리를 강타한다." },
    { "a strange white mist envelops you and you fall asleep", "기이한 흰 안개가 당신을 감싸고, 잠에 빠져든다." },
    { "an arrow shoots past you",     "화살이 당신을 스쳐 날아간다." },
    { "oh no! An arrow shot you",     "이런! 화살에 맞았다." },

    /* deaths */
    { "a poisoned dart killed you",   "독 다트에 목숨을 잃었다." },
    { "an arrow killed you",          "화살에 목숨을 잃었다." },

    /* status / sensations */
    { "you begin to feel much better","몸이 훨씬 나아지기 시작한다." },
    { "you begin to feel better",     "몸이 나아지기 시작한다." },
    { "you feel a bite in your leg and now feel weaker", "다리를 물렸고, 약해진 느낌이 든다." },
    { "you feel a sting in the side of your neck", "목덜미가 따끔하다." },
    { "you feel a strange sense of loss", "기이한 상실감이 든다." },
    { "you feel a wrenching sensation in your gut", "뱃속이 뒤틀리는 느낌이 든다." },
    { "you feel momentarily sick",    "잠시 메스껍다." },
    { "you feel very sick now",       "이제 몹시 메스껍다." },
    { "you feel stronger, now.  What bulging muscles!", "힘이 세진 느낌이다. 근육이 불끈!" },
    { "you feel time speed up suddenly", "갑자기 시간이 빨라지는 느낌이 든다." },
    { "you feel yourself moving much faster", "몸이 훨씬 빨라지는 게 느껴진다." },
    { "you feel yourself slowing down", "몸이 느려지는 게 느껴진다." },
    { "you feel less %s now",         "이제 덜 $1 느낌이다." },
    { "you suddenly feel much more skillful", "갑자기 훨씬 능숙해진 느낌이다." },
    { "you suddenly feel very thirsty", "갑자기 몹시 목이 마르다." },
    { "you suddenly feel weaker",     "갑자기 약해진 느낌이다." },
    { "you faint from exhaustion",    "탈진하여 쓰러진다." },
    { "you fall asleep",              "잠에 빠져든다." },
    { "you can move again",           "다시 움직일 수 있다." },
    { "you are frozen",               "당신은 얼어붙었다." },
    { "you are being held",           "당신은 붙잡혀 있다." },
    { "you have a tingling feeling",  "찌릿한 느낌이 든다." },
    { "you have a %s feeling for a moment, then it passes", "잠시 $1 느낌이 들다가 사라진다." },
    { "a bite has weakened you",      "물려서 약해졌다." },
    { "a bite momentarily weakens you", "잠시 물려 약해졌다." },
    { "bite has no effect",           "물렸지만 아무렇지 않다." },
    { "your nose tingles",            "코가 간질거린다." },
    { "your purse feels lighter",     "지갑이 가벼워진 느낌이다." },
    { "your way is magically blocked","앞길이 마법으로 막혀 있다." },
    { "you hear a faint cry of anguish in the distance", "멀리서 희미한 비명이 들린다." },
    { "you hear a high pitched humming noise", "높은 윙윙거리는 소리가 들린다." },
    { "you hear maniacal laughter in the distance", "멀리서 광기 어린 웃음소리가 들린다." },

    /* potions / scrolls / wands effects */
    { "this scroll is an %s scroll",  "이 두루마리는 $1 두루마리다." },
    { "the light in here suddenly seems %s", "이곳의 빛이 갑자기 $1 듯하다." },
    { "the room is lit",              "방이 밝혀졌다." },
    { "the corridor glows and then fades", "복도가 빛났다가 사라진다." },
    { "the scroll turns to dust as you pick it up", "두루마리가 집어 들자 먼지로 변한다." },
    { "oh, now this scroll has a map on it", "오, 이 두루마리에 지도가 나타났다." },
    { "multi-colored lines swirl around you, then fade", "여러 빛깔의 선이 주위를 맴돌다 사라진다." },
    { "a %s light flashes in your eyes", "$1 빛이 눈앞에서 번쩍인다." },
    { "your hands begin to glow %s",  "양손이 $1 빛으로 빛나기 시작한다." },
    { "your hands stop glowing %s",   "양손이 $1 빛을 멈춘다." },
    { "your %s glows %s for a moment","$1{이} 잠시 $2 빛난다." },
    { "your armor glows %s for a moment", "갑옷이 잠시 $1 빛난다." },
    { "your armor is covered by a shimmering %s shield", "갑옷이 일렁이는 $1 보호막에 뒤덮인다." },
    { "your armor appears to be weaker now. Oh my!", "갑옷이 약해진 듯하다. 이런!" },
    { "your armor weakens",           "갑옷이 약해진다." },
    { "the rust vanishes instantly",  "녹이 순식간에 사라진다." },
    { "the flame bounces",            "불꽃이 튕겨 나간다." },
    { "the missle vanishes with a puff of smoke", "발사체가 연기를 내며 사라진다." },
    { "missle vanishes",              "발사체가 사라진다." },
    { "time now seems to be going slower", "시간이 더 느리게 흐르는 듯하다." },
    { "you pack turns %s!",           "가방이 $1 색으로 변한다!" },
    { "what a puzzling scroll!",      "참 알쏭달쏭한 두루마리다!" },
    { "what a bizarre schtick!",      "참 별난 재주로군!" },
    { "what an odd tasting potion!",  "참 묘한 맛의 물약이다!" },
    { "Everything looks SO boring now.", "이제 모든 게 너무 지루해 보인다." },

    /* food */
    { "%s, that tasted good",         "$1, 맛이 좋았다." },
    { "%s, this food tastes awful",   "$1, 음식 맛이 형편없다." },
    { "hey, this tastes great.  It make you feel warm all over", "이야, 맛이 끝내준다. 온몸이 따뜻해진다." },
    { "ugh, you would get ill if you ate that", "윽, 그걸 먹으면 탈이 날 것이다." },
    { "that's Inedible!",             "먹을 수 없다!" },
    { "that's undrinkable",           "마실 수 없다." },
    { "yuk! Why would you want to drink that?", "윽! 그걸 왜 마시려 하는가?" },
    { "food left: %d",                "남은 식량: $1" },

    /* errors / refusals */
    { "you can't.  It appears to be cursed", "그럴 수 없다. 저주받은 것 같다." },
    { "you can't.  You're floating off the ground!", "그럴 수 없다. 당신은 공중에 떠 있다!" },
    { "you can't call that anything", "그것엔 이름을 붙일 수 없다." },
    { "you can't wear that",          "그것은 입을 수 없다." },
    { "you can't wield armor",        "갑옷은 무기로 들 수 없다." },
    { "you can't zap with that!",     "그것으로는 마법봉을 쓸 수 없다!" },
    { "you are too weak to use it",   "그것을 쓰기엔 너무 약하다." },
    { "you are already wearing some", "이미 착용하고 있다." },
    { "you already have a ring on each hand", "양손에 이미 반지를 끼고 있다." },
    { "you aren't carrying anything", "아무것도 지니고 있지 않다." },
    { "you aren't wearing any armor", "갑옷을 입고 있지 않다." },
    { "you aren't wearing any rings", "반지를 끼고 있지 않다." },
    { "you don't have anything in your pack to identify", "가방에 감정할 물건이 없다." },
    { "you must identify a %s",       "$1{을} 감정해야 한다." },
    { "you must identify something",  "무언가를 감정해야 한다." },
    { "you haven't typed a command yet", "아직 아무 명령도 입력하지 않았다." },
    { "you ran out",                  "다 떨어졌다." },
    { "it would be difficult to wrap that around a finger", "그건 손가락에 끼기 어렵겠다." },
    { "that has already been identified", "이미 감정된 물건이다." },
    { "not wearing armor",            "갑옷을 입고 있지 않다." },
    { "not wizard any more",          "더 이상 마법사가 아니다." },
    { "nothing to read",              "읽을 것이 없다." },
    { "nothing here",                 "여기엔 아무것도 없다." },
    { "nothing happens",              "아무 일도 일어나지 않는다." },
    { "no monster there",             "거기엔 몬스터가 없다." },
    { "no trap there",                "거기엔 함정이 없다." },
    { "no rings",                     "반지가 없다." },
    { "no room",                      "공간이 없다." },
    { "not a ring",                   "반지가 아니다." },
    { "there is nothing on it to read", "거기엔 읽을 게 아무것도 없다." },
    { "there is something there already", "거기엔 이미 무언가가 있다." },
    { "Not a type",                   "그런 종류가 아니다." },
    { "sorry",                        "미안합니다." },
    { "illegal command '%s'",         "잘못된 명령 '$1'." },
    { "unknown character '%s'",       "알 수 없는 문자 '$1'." },
    { "'%s' is not a valid item",     "'$1'{은} 올바른 물건이 아니다." },
    { "'%s' not in pack",             "'$1'{이} 가방에 없다." },
    { "warning, connectivity problem on this level", "경고: 이 층의 연결에 문제가 있다." },

    /* prompts */
    { "really quit?",                 "정말 종료하시겠습니까?" },
    { "Please answer Y or N",         "Y 또는 N으로 답해 주세요." },
    { "please answer Y or N",         "Y 또는 N으로 답해 주세요." },
    { "please type L or R",           "L 또는 R을 입력하세요." },
    { "Please type one of %c%c%c%c (ESCAPE to quit)", "$1$2$3$4 중 하나를 입력하세요 (ESCAPE로 종료)." },
    { "L or R",                       "L 또는 R" },
    { "how much?",                    "얼마나?" },
    { "blessing? (+,-,n)",            "축복? (+,-,n)" },
    { "call it: ",                    "이름: " },
    { "left hand or right hand? ",    "왼손인가 오른손인가? " },
    { "left or right ring? ",         "왼쪽 반지인가 오른쪽 반지인가? " },
    { "which %c do you want? (0-f)",  "어느 $1{을} 원하는가? (0-f)" },
    { "what do you want identified? ","무엇을 감정하시겠습니까? " },
    { "what do you want to call it? ","무엇이라 부르시겠습니까? " },
    { "File exists.  Do you wish to overwrite it?", "파일이 이미 있습니다. 덮어쓰시겠습니까?" },
    { "save file (%s)? ",             "세이브 파일 ($1)? " },
    { "file name: %s",                "파일 이름: $1" },

    /* level / misc */
    { "welcome to level %d",          "$1층에 온 것을 환영한다." },
    { "version %s. (mctesq was here)","버전 $1. (mctesq가 다녀감)" },
};

const char *tr_msg(const char *en)
{
    static char out[1024];
    static char caps[MAX_CAPS][CAP_LEN];
    int ncap;
    size_t i;

    if (en == NULL)
        return "";
    for (i = 0; i < sizeof TRANSLATIONS / sizeof TRANSLATIONS[0]; i++) {
        if (match(TRANSLATIONS[i].en, en, caps, &ncap)) {
            expand(TRANSLATIONS[i].ko, caps, ncap, out, sizeof out);
            return out;
        }
    }
    return en;   /* graceful fallback: untranslated text passes through */
}

#ifdef I18N_TEST
#include <stdlib.h>
static int fails;
static void check(const char *en, const char *want)
{
    const char *got = tr_msg(en);
    if (strcmp(got, want) != 0) {
        printf("FAIL  in : %s\n      got: %s\n      want: %s\n", en, got, want);
        fails++;
    } else {
        printf("ok    %s -> %s\n", en, got);
    }
}
int main(void)
{
    /* 받침 detection */
    if (!han_has_batchim("\xEB\x8B\xA8\xEA\xB2\x80")) { puts("FAIL batchim 단검"); fails++; } /* 단검 has 받침 */
    if (han_has_batchim("\xEC\x87\xA0")) { puts("FAIL batchim 쇠"); fails++; }             /* 쇠 no 받침 */

    /* 조사 selection */
    check("you found a beartrap", "\xEA\xB3\xB0\xEB\x8D\xAB\xEC\x9D\x84 \xEB\xB0\x9C\xEA\xB2\xAC\xED\x96\x88\xEB\x8B\xA4."); /* 곰덫을 발견했다. */
    /* capture + josa: monster name with 받침 (코볼드 ends in 드 -> no 받침 -> 를) */
    check("you hit \xEC\xBD\x94\xEB\xB3\xBC\xEB\x93\x9C", "\xEC\xBD\x94\xEB\xB3\xBC\xEB\x93\x9C\xEB\xA5\xBC \xEB\xA7\x9E\xED\x98\x94\xEB\x8B\xA4."); /* 코볼드 -> 코볼드를 맞혔다. */

    check("welcome to level 3", "3\xEC\xB8\xB5\xEC\x97\x90 \xEC\x98\xA8 \xEA\xB2\x83\xEC\x9D\x84 \xED\x99\x98\xEC\x98\x81\xED\x95\x9C\xEB\x8B\xA4."); /* 3층에 온 것을 환영한다. */
    check("you found 50 gold pieces", "\xEA\xB8\x88\xED\x99\x94 50\xEB\x8B\xA2\xEC\x9D\x84 \xEB\xB0\x9C\xEA\xB2\xAC\xED\x96\x88\xEB\x8B\xA4."); /* 금화 50닢을 발견했다. */
    check("nothing here", "\xEC\x97\xAC\xEA\xB8\xB0\xEC\x97\x94 \xEC\x95\x84\xEB\xAC\xB4\xEA\xB2\x83\xEB\x8F\x84 \xEC\x97\x86\xEB\x8B\xA4."); /* 여기엔 아무것도 없다. */

    /* untranslated falls back to English */
    check("xyzzy unmapped", "xyzzy unmapped");

    if (fails) { printf("\n%d FAILURES\n", fails); return 1; }
    puts("\nall tests passed");
    return 0;
}
#endif
