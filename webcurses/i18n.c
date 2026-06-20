/* i18n.c — English -> Korean message translation + 조사(particle) engine.
 * See i18n.h for the contract. Build (native self-test):
 *     gcc -DI18N_DEMO i18n.c -o i18n_demo && ./i18n_demo
 */
#include "i18n.h"
#include <string.h>
#include <ctype.h>

/* ---- 조사(particle) selection by final consonant (받침) -------------------
 * A precomposed Hangul syllable occupies U+AC00..U+D7A3. Its final consonant
 * index is (cp - 0xAC00) % 28; index 0 means "no 받침". Mirrors the prototype's
 * hasBatchim() (rogue-touch-prototype.jsx), but decodes UTF-8 in C. */

static unsigned last_codepoint(const char *s)
{
    size_t len = strlen(s);
    if (len == 0)
        return 0;

    /* step back over UTF-8 continuation bytes (10xxxxxx) to the lead byte */
    size_t i = len;
    do { i--; } while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80);

    unsigned char c = (unsigned char)s[i];
    unsigned cp;
    int trailing;
    if (c < 0x80)              { cp = c;          trailing = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; trailing = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; trailing = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; trailing = 3; }
    else return 0;             /* stray continuation byte */

    for (int k = 1; k <= trailing; k++)
        cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
    return cp;
}

int kr_has_batchim(const char *word)
{
    unsigned cp = last_codepoint(word);
    if (cp < 0xAC00 || cp > 0xD7A3)
        return 0;
    return (cp - 0xAC00) % 28 != 0;
}

/* ㄹ-final is special for 으로/로: a 받침 of ㄹ (jong index 8) takes 로, not 으로 */
static int ends_in_rieul(const char *word)
{
    unsigned cp = last_codepoint(word);
    if (cp < 0xAC00 || cp > 0xD7A3)
        return 0;
    return (cp - 0xAC00) % 28 == 8;
}

const char *kr_eul_reul(const char *w) { return kr_has_batchim(w) ? "을" : "를"; }
const char *kr_i_ga(const char *w)     { return kr_has_batchim(w) ? "이" : "가"; }
const char *kr_eun_neun(const char *w) { return kr_has_batchim(w) ? "은" : "는"; }
const char *kr_gwa_wa(const char *w)   { return kr_has_batchim(w) ? "과" : "와"; }
const char *kr_euro_ro(const char *w)
{
    if (ends_in_rieul(w))      return "로";   /* 칼로, not 칼으로 */
    return kr_has_batchim(w) ? "으로" : "로";
}

/* ---- message table (English -> Korean) ------------------------------------
 * Only fully-assembled, standalone messages live here; fragment-built dynamic
 * lines (item/monster names) are handled in the keyed-template wave and pass
 * through for now. Keys are stored with a lower-case first character; tr_msg()
 * normalizes the first char so endmsg()'s upper-casing doesn't cause misses. */
struct tr_entry { const char *en; const char *ko; };

static const struct tr_entry TABLE[] = {
    /* movement / navigation */
    { "i see no way down",                          "내려가는 길이 보이지 않는다" },
    { "i see no way up",                            "올라가는 길이 보이지 않는다" },
    { "a secret door",                              "비밀 문" },
    { "your way is magically blocked",              "마법으로 길이 막혀 있다" },
    { "the room is lit",                            "방에 불이 밝혀진다" },
    { "the corridor glows and then fades",          "복도가 빛나다 사그라든다" },

    /* traps / combat hazards */
    { "you fell into a trap!",                      "함정에 빠졌다!" },
    { "you are caught in a bear trap",              "곰덫에 걸렸다" },
    { "you are still stuck in the bear trap",       "아직 곰덫에 걸려 있다" },
    { "a gush of water hits you on the head",       "물줄기가 머리를 강타한다" },
    { "a small dart just hit you in the shoulder",  "작은 다트가 어깨에 꽂혔다" },
    { "a small dart whizzes by your ear and vanishes", "작은 다트가 귓가를 스치며 사라진다" },
    { "a spike shoots past your ear!",              "가시가 귓가를 스쳐 날아간다!" },
    { "a poisoned dart killed you",                 "독 다트에 맞아 죽었다" },
    { "an arrow shoots past you",                   "화살이 당신을 스쳐 날아간다" },
    { "an arrow killed you",                        "화살에 맞아 죽었다" },
    { "oh no! an arrow shot you",                   "이런! 화살에 맞았다" },
    { "a strange white mist envelops you and you fall asleep",
                                                    "기이한 흰 안개가 당신을 휘감자 잠에 빠진다" },

    /* status afflictions */
    { "you fall asleep",                            "잠에 빠진다" },
    { "you are frozen",                             "얼어붙었다" },
    { "you are being held",                         "붙잡혀 있다" },
    { "you can move again",                         "다시 움직일 수 있다" },
    { "you faint from exhaustion",                  "탈진하여 기절한다" },
    { "you feel momentarily sick",                  "잠시 메스꺼운 느낌이 든다" },
    { "you feel very sick now",                     "이제 몹시 메스껍다" },
    { "you feel a wrenching sensation in your gut", "뱃속이 뒤틀리는 느낌이 든다" },
    { "you feel a strange sense of loss",           "기이한 상실감이 느껴진다" },
    { "you feel a sting in the side of your neck",  "목덜미가 따끔한 느낌이 든다" },
    { "you feel a bite in your leg and now feel weaker",
                                                    "다리를 물려 더 약해진 기분이다" },
    { "a bite has weakened you",                    "물려서 약해졌다" },
    { "a bite momentarily weakens you",             "물려서 잠시 약해졌다" },
    { "bite has no effect",                         "물어도 효과가 없다" },
    { "you have a tingling feeling",                "찌릿한 느낌이 든다" },
    { "your nose tingles",                          "코가 찡한 느낌이 든다" },

    /* stat / speed changes */
    { "you suddenly feel weaker",                   "갑자기 약해진 기분이 든다" },
    { "you feel stronger, now.  what bulging muscles!",
                                                    "이제 더 강해진 기분이다. 근육이 불끈!" },
    { "you suddenly feel much more skillful",       "갑자기 훨씬 능숙해진 기분이 든다" },
    { "you suddenly feel very thirsty",             "갑자기 몹시 목이 마르다" },
    { "you feel yourself moving much faster",       "몸이 훨씬 빠르게 움직이는 것이 느껴진다" },
    { "you feel yourself slowing down",             "몸이 느려지는 것이 느껴진다" },
    { "you feel time speed up suddenly",            "갑자기 시간이 빨라지는 느낌이 든다" },
    { "time now seems to be going slower",          "이제 시간이 더 느리게 흐르는 것 같다" },
    { "you begin to feel better",                   "기분이 나아지기 시작한다" },
    { "you begin to feel much better",              "기분이 훨씬 나아지기 시작한다" },

    /* armor / rings / equipment */
    { "your armor weakens",                         "갑옷이 약해진다" },
    { "your armor appears to be weaker now. oh my!","갑옷이 이제 더 약해 보인다. 이런!" },
    { "you can't wear that",                        "그건 입을 수 없다" },
    { "you can't wield armor",                      "갑옷은 장착할 수 없다" },
    { "not wearing armor",                          "갑옷을 입고 있지 않다" },
    { "you aren't wearing any armor",               "갑옷을 하나도 입고 있지 않다" },
    { "you already have a ring on each hand",       "양손에 이미 반지를 끼고 있다" },
    { "not wearing such a ring",                    "그런 반지를 끼고 있지 않다" },
    { "you aren't wearing any rings",               "반지를 하나도 끼고 있지 않다" },
    { "not a ring",                                 "반지가 아니다" },
    { "it would be difficult to wrap that around a finger",
                                                    "그걸 손가락에 끼우긴 어려울 것 같다" },

    /* scrolls / potions / sticks */
    { "nothing to read",                            "읽을 것이 없다" },
    { "there is nothing on it to read",             "거기엔 읽을 것이 아무것도 없다" },
    { "the scroll turns to dust as you pick it up", "두루마리가 집어 드는 순간 먼지로 변한다" },
    { "oh, now this scroll has a map on it",        "오, 이제 이 두루마리에 지도가 그려져 있다" },
    { "what a puzzling scroll!",                    "정말 알 수 없는 두루마리군!" },
    { "what an odd tasting potion!",                "정말 묘한 맛의 물약이군!" },
    { "what a bizarre schtick!",                    "정말 괴상한 물건이군!" },
    { "multi-colored lines swirl around you, then fade",
                                                    "형형색색의 선들이 주위를 맴돌다 사라진다" },
    { "that's undrinkable",                         "마실 수 없는 것이다" },
    { "that's inedible!",                           "먹을 수 없는 것이다!" },
    { "yuk! why would you want to drink that?",     "윽! 왜 그런 걸 마시려 하는가?" },
    { "ugh, you would get ill if you ate that",     "윽, 그걸 먹으면 탈이 날 것이다" },
    { "hey, this tastes great.  it make you feel warm all over",
                                                    "오, 정말 맛있다. 온몸이 따뜻해지는 기분이다" },
    { "you can't zap with that!",                   "그걸로는 지팡이를 쓸 수 없다!" },
    { "the flame bounces",                          "불길이 튕겨 나간다" },
    { "the missle vanishes with a puff of smoke",   "화살이 연기와 함께 사라진다" },
    { "missle vanishes",                            "화살이 사라진다" },
    { "the rust vanishes instantly",               "녹이 순식간에 사라진다" },

    /* identification / naming */
    { "that has already been identified",           "그것은 이미 감정되었다" },
    { "you must identify something",                "무언가를 감정해야 한다" },
    { "you don't have anything in your pack to identify",
                                                    "배낭에 감정할 것이 없다" },
    { "you can't call that anything",               "그건 이름을 붙일 수 없다" },

    /* pack / inventory */
    { "you aren't carrying anything",               "아무것도 들고 있지 않다" },
    { "nothing here",                               "여기엔 아무것도 없다" },
    { "you are too weak to use it",                 "너무 약해서 그것을 쓸 수 없다" },
    { "you can't.  it appears to be cursed",        "그럴 수 없다. 저주받은 것 같다" },
    { "you can't.  you're floating off the ground!","그럴 수 없다. 당신은 땅에서 떠 있다!" },
    { "your purse feels lighter",                   "지갑이 가벼워진 느낌이 든다" },

    /* monsters / sounds */
    { "no monster there",                           "거기엔 몬스터가 없다" },
    { "no trap there",                              "거기엔 함정이 없다" },
    { "there is something there already",           "거기엔 이미 무언가가 있다" },
    { "you hear a faint cry of anguish in the distance",
                                                    "멀리서 희미한 비명이 들린다" },
    { "you hear a high pitched humming noise",      "높은 음의 웅웅거리는 소리가 들린다" },
    { "you hear maniacal laughter in the distance", "멀리서 광기 어린 웃음소리가 들린다" },

    /* misc / flavor */
    { "nothing happens",                            "아무 일도 일어나지 않는다" },
    { "everything looks so boring now.",            "이제 모든 게 너무 지루해 보인다." },
    { "you are suddenly in a parallel dimension",   "갑자기 평행 차원에 들어섰다" },
    { "you haven't typed a command yet",            "아직 명령을 입력하지 않았다" },
    { "warning, connectivity problem on this level","경고: 이 층의 연결에 문제가 있다" },

    /* prompts / confirmations */
    { "really quit?",                               "정말 종료하겠는가?" },
    { "please answer y or n",                       "Y 또는 N으로 답하라" },
    { "please type l or r",                         "L 또는 R을 입력하라" },
    { "how much?",                                  "얼마나?" },
    { "file exists.  do you wish to overwrite it?", "파일이 이미 있다. 덮어쓰겠는가?" },
    { "the load is rather high, o exaulted one",    "부하가 다소 높습니다, 오 존귀하신 분이여" },
    { "the load has dropped back down.  you have a reprieve",
                                                    "부하가 다시 내려갔다. 한숨 돌렸다" },
};

#define TABLE_LEN ((int)(sizeof(TABLE) / sizeof(TABLE[0])))

/* normalized copy of `en` with a lower-case first character (ASCII only). */
static const char *normalize_first(const char *en, char *buf, size_t cap)
{
    size_t n = strlen(en);
    if (n == 0 || n >= cap)
        return en;                 /* too long to normalize: match as-is */
    memcpy(buf, en, n + 1);
    buf[0] = (char)tolower((unsigned char)buf[0]);
    return buf;
}

const char *tr_msg(const char *en)
{
    if (en == NULL)
        return "";

    char norm[256];
    const char *key = normalize_first(en, norm, sizeof norm);

    for (int i = 0; i < TABLE_LEN; i++)
        if (strcmp(TABLE[i].en, key) == 0)
            return TABLE[i].ko;

    return en;                      /* untranslated: pass English through */
}

#ifdef I18N_DEMO
#include <stdio.h>
int main(void)
{
    /* particle engine */
    struct { const char *w; const char *eul; const char *iga; } cases[] = {
        { "단검", "을", "이" },   /* 검: 받침 ㅁ */
        { "물약", "을", "이" },   /* 약: 받침 ㄱ */
        { "두루마리", "를", "가" },/* 리: 받침 없음 */
        { "사과", "를", "가" },   /* 과: 받침 없음 */
    };
    int fails = 0;
    for (int i = 0; i < 4; i++) {
        const char *e = kr_eul_reul(cases[i].w), *g = kr_i_ga(cases[i].w);
        printf("%-10s -> %s / %s\n", cases[i].w, e, g);
        if (strcmp(e, cases[i].eul) || strcmp(g, cases[i].iga)) { fails++; printf("  FAIL\n"); }
    }
    printf("으로/로: 칼=%s 방패=%s 활=%s\n",
           kr_euro_ro("칼"), kr_euro_ro("방패"), kr_euro_ro("활"));

    /* translation table */
    const char *t1 = tr_msg("you fell into a trap!");
    const char *t2 = tr_msg("You fell into a trap!");   /* upper-cased first char */
    const char *t3 = tr_msg("a dagger you do not have");/* untranslated passthrough */
    printf("tr: [%s]\ntr(Upper): [%s]\ntr(passthru): [%s]\n", t1, t2, t3);
    if (strcmp(t1, t2) != 0) { fails++; printf("  FAIL: case-normalize\n"); }
    if (strcmp(t3, "a dagger you do not have") != 0) { fails++; printf("  FAIL: passthrough\n"); }

    printf(fails ? "\n%d FAILURES\n" : "\nOK (%d entries)\n", fails ? fails : TABLE_LEN);
    return fails ? 1 : 0;
}
#endif
