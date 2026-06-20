/* i18n.c — English -> Korean message translation + 조사(particle) engine.
 * See i18n.h for the contract. Build (native self-test):
 *     gcc -DI18N_DEMO i18n.c -o i18n_demo && ./i18n_demo
 */
#include "i18n.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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

/* ---- small string helpers -------------------------------------------------*/
static int starts(const char *s, const char *pre)
{
    return strncmp(s, pre, strlen(pre)) == 0;
}
static int ends(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}
/* append noun + its object particle (을/를) into dst */
static void put_eul(char *dst, const char *noun)
{
    strcat(dst, noun);
    strcat(dst, kr_eul_reul(noun));
}
/* append noun + its subject particle (이/가) into dst */
static void put_iga(char *dst, const char *noun)
{
    strcat(dst, noun);
    strcat(dst, kr_i_ga(noun));
}

/* ---- noun dictionaries ----------------------------------------------------
 * Names are kept English in the engine; we translate them here so untranslated
 * frames fall back to clean English rather than a half-Korean mix. */
struct kv { const char *en; const char *ko; };

/* 26 monsters, indexed A..Z in monsters[] */
static const struct kv MONSTER[] = {
    { "aquator", "아쿠아터" },     { "bat", "박쥐" },
    { "centaur", "켄타우로스" },   { "dragon", "용" },
    { "emu", "에뮤" },             { "venus flytrap", "파리지옥" },
    { "griffin", "그리핀" },       { "hobgoblin", "홉고블린" },
    { "ice monster", "얼음 괴물" },{ "jabberwock", "재버워크" },
    { "kestrel", "황조롱이" },     { "leprechaun", "레프러콘" },
    { "medusa", "메두사" },        { "nymph", "님프" },
    { "orc", "오크" },             { "phantom", "팬텀" },
    { "quagga", "콰가" },          { "rattlesnake", "방울뱀" },
    { "snake", "뱀" },             { "troll", "트롤" },
    { "black unicorn", "검은 유니콘" }, { "vampire", "뱀파이어" },
    { "wraith", "망령" },          { "xeroc", "제록" },
    { "yeti", "예티" },            { "zombie", "좀비" },
};

/* item identity names (oi_name in the obj_info tables) */
static const struct kv IDENT[] = {
    /* potions */
    { "confusion", "혼란" }, { "hallucination", "환각" }, { "poison", "독" },
    { "gain strength", "힘 증가" }, { "see invisible", "투명체 감지" },
    { "healing", "치유" }, { "monster detection", "몬스터 감지" },
    { "magic detection", "마법 감지" }, { "raise level", "레벨 상승" },
    { "extra healing", "완전 치유" }, { "haste self", "가속" },
    { "restore strength", "힘 회복" }, { "blindness", "실명" },
    { "levitation", "공중부양" },
    /* scrolls */
    { "monster confusion", "몬스터 혼란" }, { "magic mapping", "마법 지도" },
    { "hold monster", "몬스터 묶기" }, { "sleep", "수면" },
    { "enchant armor", "갑옷 강화" }, { "identify potion", "물약 감정" },
    { "identify scroll", "두루마리 감정" }, { "identify weapon", "무기 감정" },
    { "identify armor", "갑옷 감정" },
    { "identify ring, wand or staff", "반지·마법봉·지팡이 감정" },
    { "scare monster", "몬스터 겁주기" }, { "food detection", "음식 감지" },
    { "teleportation", "순간이동" }, { "enchant weapon", "무기 강화" },
    { "create monster", "몬스터 생성" }, { "remove curse", "저주 해제" },
    { "aggravate monsters", "몬스터 자극" }, { "protect armor", "갑옷 보호" },
    /* rings */
    { "protection", "보호" }, { "add strength", "힘 추가" },
    { "sustain strength", "힘 유지" }, { "searching", "탐색" },
    { "adornment", "장식" }, { "aggravate monster", "몬스터 자극" },
    { "dexterity", "민첩" }, { "increase damage", "피해 증가" },
    { "regeneration", "재생" }, { "slow digestion", "소화 지연" },
    { "stealth", "은신" }, { "maintain armor", "갑옷 유지" },
    /* sticks */
    { "light", "빛" }, { "invisibility", "투명" }, { "lightning", "번개" },
    { "fire", "화염" }, { "cold", "냉기" }, { "polymorph", "변신" },
    { "magic missile", "마법 화살" }, { "haste monster", "몬스터 가속" },
    { "slow monster", "몬스터 둔화" }, { "drain life", "생명력 흡수" },
    { "nothing", "무효" }, { "teleport away", "추방 순간이동" },
    { "teleport to", "소환 순간이동" }, { "cancellation", "마법 무효화" },
};

/* weapon / armor base names */
static const struct kv GEAR[] = {
    { "mace", "철퇴" }, { "long sword", "장검" }, { "short bow", "단궁" },
    { "arrow", "화살" }, { "dagger", "단검" }, { "two handed sword", "양손검" },
    { "dart", "다트" }, { "shuriken", "수리검" }, { "spear", "창" },
    { "leather armor", "가죽 갑옷" }, { "ring mail", "사슬 미늘 갑옷" },
    { "studded leather armor", "징 박은 가죽 갑옷" }, { "scale mail", "비늘 갑옷" },
    { "chain mail", "사슬 갑옷" }, { "splint mail", "판금 미늘 갑옷" },
    { "banded mail", "띠 갑옷" }, { "plate mail", "판금 갑옷" },
};

/* potion / ring / stick flavor colors (rainbow[] in init.c) */
static const struct kv COLOR[] = {
    { "amber", "호박색" }, { "aquamarine", "옥색" }, { "black", "검은" },
    { "blue", "파란" }, { "brown", "갈색" }, { "clear", "투명한" },
    { "crimson", "진홍색" }, { "cyan", "청록색" }, { "ecru", "베이지색" },
    { "gold", "금색" }, { "green", "초록" }, { "grey", "회색" },
    { "magenta", "자홍색" }, { "orange", "주황" }, { "pink", "분홍" },
    { "plaid", "격자무늬" }, { "purple", "보라" }, { "red", "빨간" },
    { "silver", "은색" }, { "tan", "황갈색" }, { "tangerine", "귤색" },
    { "topaz", "황옥색" }, { "turquoise", "청록" }, { "vermilion", "주홍" },
    { "violet", "제비꽃색" }, { "white", "하얀" }, { "yellow", "노란" },
};

static const char *lookup(const struct kv *t, int n, const char *en)
{
    for (int i = 0; i < n; i++)
        if (strcmp(t[i].en, en) == 0)
            return t[i].ko;
    return NULL;
}
#define DICT(t, en) lookup(t, (int)(sizeof(t) / sizeof(t[0])), en)

/* Find any monster name as a substring of `s`; return its Korean and the match
 * span via start/len. Longest English name wins (so 'venus flytrap' beats a
 * stray 'bat'). Returns NULL if none found. */
static const char *find_monster(const char *s, int *start, int *len)
{
    const char *best_ko = NULL;
    int best_at = -1, best_len = 0, n = (int)(sizeof(MONSTER) / sizeof(MONSTER[0]));
    for (int i = 0; i < n; i++) {
        const char *p = strstr(s, MONSTER[i].en);
        if (p) {
            int l = (int)strlen(MONSTER[i].en);
            if (l > best_len) { best_ko = MONSTER[i].ko; best_at = (int)(p - s); best_len = l; }
        }
    }
    if (!best_ko) return NULL;
    *start = best_at; *len = best_len;
    return best_ko;
}

/* ---- kr_item: inv_name() English -> Korean --------------------------------*/
static char item_buf[256];

/* read a leading integer; return it (or 1) and advance *pp past it + space */
static int lead_count(const char **pp)
{
    const char *p = *pp;
    if (*p >= '0' && *p <= '9') {
        int n = 0;
        while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
        if (*p == ' ') p++;
        *pp = p;
        return n;
    }
    return 1;
}

char *kr_item(const char *en)
{
    char work[256];
    strncpy(work, en, sizeof work - 1);
    work[sizeof work - 1] = '\0';

    /* trailing inv_describe marker -> Korean, stripped from `work` */
    const char *tail = "";
    struct kv marks[] = {
        { " (being worn)", " (착용 중)" },
        { " (weapon in hand)", " (장착 중)" },
        { " (on left hand)", " (왼손)" },
        { " (on right hand)", " (오른손)" },
    };
    for (int i = 0; i < 4; i++)
        if (ends(work, marks[i].en)) {
            work[strlen(work) - strlen(marks[i].en)] = '\0';
            tail = marks[i].ko;
            break;
        }

    char out[256];
    out[0] = '\0';
    const char *p = work;
    int handled = 1;

    /* normalize the leading article so matching is case-insensitive */
    if (starts(p, "A ") || starts(p, "a ")) p += 2;
    else if (starts(p, "An ") || starts(p, "an ")) p += 3;
    else if (starts(p, "Some ") || starts(p, "some ")) p += 5;
    else if (starts(p, "The ") || starts(p, "the ")) p += 4;

    int count = lead_count(&p);   /* "5 scrolls ..." -> 5, advances p */

    if (strstr(en, "Gold pieces") || strstr(en, "gold pieces")) {
        /* "N Gold pieces" */
        int g = 0; const char *q = en;
        while (*q && !(*q >= '0' && *q <= '9')) q++;
        while (*q >= '0' && *q <= '9') g = g * 10 + (*q++ - '0');
        sprintf(out, "금화 %d닢", g);
    }
    else if (strstr(work, "Amulet of Yendor")) {
        strcpy(out, "옌도르의 부적");
    }
    else if (strstr(work, "potion")) {
        const char *of = strstr(work, "of ");
        if (of) {                                   /* known: "...of NAME(color)" */
            char name[128]; int k = 0; of += 3;
            while (*of && *of != '(' && k < 127) name[k++] = *of++;
            while (k > 0 && name[k - 1] == ' ') k--;
            name[k] = '\0';
            const char *ko = DICT(IDENT, name);
            sprintf(out, "%s의 물약", ko ? ko : name);
        } else {                                    /* unknown: "COLOR potion" */
            char col[64]; int k = 0;
            while (p[k] && p[k] != ' ' && k < 63) { col[k] = p[k]; k++; }
            col[k] = '\0';
            const char *ko = DICT(COLOR, col);
            sprintf(out, "%s 물약", ko ? ko : col);
        }
    }
    else if (strstr(work, "ring") && !strstr(work, "ring mail")) {
        const char *of = strstr(work, "of ");
        if (of) {
            char name[128]; int k = 0; of += 3;
            while (*of && *of != '(' && *of != '[' && k < 127) name[k++] = *of++;
            while (k > 0 && name[k - 1] == ' ') k--;
            name[k] = '\0';
            const char *ko = DICT(IDENT, name);
            sprintf(out, "%s의 반지", ko ? ko : name);
        } else {
            char col[64]; int k = 0;
            while (p[k] && p[k] != ' ' && k < 63) { col[k] = p[k]; k++; }
            col[k] = '\0';
            sprintf(out, "%s 반지", col);
        }
    }
    else if (strstr(work, "wand") || strstr(work, "staff")) {
        const char *base = strstr(work, "wand") ? "마법봉" : "지팡이";
        const char *of = strstr(work, "of ");
        if (of) {
            char name[128]; int k = 0; of += 3;
            while (*of && *of != '(' && k < 127) name[k++] = *of++;
            while (k > 0 && name[k - 1] == ' ') k--;
            name[k] = '\0';
            const char *ko = DICT(IDENT, name);
            sprintf(out, "%s의 %s", ko ? ko : name, base);
        } else {
            sprintf(out, "%s", base);   /* "MATERIAL wand" -> just 마법봉 */
        }
    }
    else if (strstr(work, "scroll")) {
        const char *of = strstr(work, "of ");
        const char *ti = strstr(work, "titled");
        const char *cl = strstr(work, "called ");
        if (of) {
            char name[128]; int k = 0; of += 3;
            while (*of && *of != '(' && k < 127) name[k++] = *of++;
            while (k > 0 && name[k - 1] == ' ') k--;
            name[k] = '\0';
            const char *ko = DICT(IDENT, name);
            sprintf(out, "%s의 두루마리", ko ? ko : name);
        } else if (cl) {
            char g[128]; int k = 0; cl += 7;
            while (*cl && *cl != '(' && k < 127) g[k++] = *cl++;
            g[k] = '\0';
            sprintf(out, "'%s'라 불리는 두루마리", g);
        } else if (ti) {
            const char *q = strchr(ti, '\'');
            char t[128]; int k = 0;
            if (q) { q++; while (*q && *q != '\'' && k < 127) t[k++] = *q++; }
            t[k] = '\0';
            sprintf(out, "'%s'라 적힌 두루마리", t);
        } else {
            strcpy(out, "두루마리");
        }
    }
    else if (strstr(work, "food") || strstr(work, "ration")) {
        strcpy(out, "음식");
    }
    else {
        /* weapon / armor: find a known base name anywhere in the string */
        const char *ko = NULL; int n = (int)(sizeof(GEAR) / sizeof(GEAR[0]));
        int best = -1;
        for (int i = 0; i < n; i++) {
            const char *q = strstr(work, GEAR[i].en);
            if (q && (best < 0 || (int)strlen(GEAR[i].en) > best)) {
                ko = GEAR[i].ko; best = (int)strlen(GEAR[i].en);
            }
        }
        if (ko) {
            const char *ench = strchr(work, '+');         /* enchantment +N,+N */
            char e[32] = "";
            if (ench) { int k = 0; while (ench[k] && ench[k] != ' ' && ench[k] != '[' && k < 31) { e[k] = ench[k]; k++; } e[k] = '\0'; }
            const char *prot = strstr(work, "protection ");
            if (e[0] && prot) {
                int pv = atoi(prot + 11);
                sprintf(out, "%s %s [방어 %d]", e, ko, pv);
            } else if (e[0]) {
                sprintf(out, "%s %s", e, ko);
            } else {
                strcpy(out, ko);
            }
        } else {
            handled = 0;
        }
    }

    if (!handled) {
        strncpy(item_buf, en, sizeof item_buf - 1);
        item_buf[sizeof item_buf - 1] = '\0';
        return item_buf;            /* unrecognized: pass English through */
    }

    /* assemble: <name> [N개] [tail] */
    item_buf[0] = '\0';
    strncat(item_buf, out, sizeof item_buf - 1);
    if (count > 1 && !strstr(out, "금화")) {
        char c[24]; sprintf(c, " %d개", count);
        strncat(item_buf, c, sizeof item_buf - strlen(item_buf) - 1);
    }
    if (tail[0])
        strncat(item_buf, tail, sizeof item_buf - strlen(item_buf) - 1);
    return item_buf;
}

/* ---- tr_msg frame engine --------------------------------------------------
 * Operates on the fully-assembled buffer. Item nouns are already Korean (the
 * inv_name wrapper ran first); monster nouns are still English and resolved via
 * find_monster(). Each rule writes Korean to `frame_buf` and returns 1. */
static char frame_buf[512];

/* combat: subject + verb + object, English word order, monster name English. */
static int frame_combat(const char *s, char *out)
{
    /* monster hits player: "The X <hitverb> you"  /  "The X <missverb> you" */
    if (ends(s, " you") || ends(s, " you.")) {
        int at, len; const char *mko = find_monster(s, &at, &len);
        if (mko && (at == 0 || at == 4) /* leading "the " */) {
            if (strstr(s, "miss") || strstr(s, "doesn't hit") || strstr(s, "don't hit")) {
                out[0] = '\0'; put_iga(out, mko); strcat(out, " 당신을 공격했으나 빗나갔다"); return 1;
            }
            if (strstr(s, "hit") || strstr(s, "injure")) {
                out[0] = '\0'; put_iga(out, mko); strcat(out, " 당신을 공격했다"); return 1;
            }
        }
    }
    /* player hits monster: "You <hitverb> the X"  /  "You <missverb> the X" */
    if (starts(s, "You ") || starts(s, "you ")) {
        int at, len; const char *mko = find_monster(s, &at, &len);
        if (mko) {
            if (strstr(s, "miss") || strstr(s, "don't hit")) {
                out[0] = '\0'; put_eul(out, mko); strcat(out, " 공격했으나 빗나갔다"); return 1;
            }
            if (strstr(s, "hit") || strstr(s, "injure")) {
                out[0] = '\0';
                if (strstr(s, "excellent")) { put_eul(out, mko); strcat(out, " 멋지게 명중시켰다"); }
                else { put_eul(out, mko); strcat(out, " 공격했다"); }
                return 1;
            }
        }
    }
    return 0;
}

/* "defeated X" / "you have defeated X" / ".  Defeated X" -> "X 처치" */
static int frame_defeat(const char *s, char *out)
{
    if (strstr(s, "efeated ")) {
        int at, len; const char *mko = find_monster(s, &at, &len);
        const char *who = mko ? mko : "그것";
        out[0] = '\0';
        if (s[0] == '.') strcat(out, "그리고 ");
        put_eul(out, who); strcat(out, " 처치했다");
        return 1;
    }
    return 0;
}

/* a handful of "<frame> %s" item/monster lines with the noun already filled. */
struct frame_fix { const char *pre; const char *post; const char *fmt; int monster; };
static const struct frame_fix FIX[] = {
    /* %N = noun (with object particle 을/를 unless fmt handles it) */
    { "dropped ",        "", "{N을} 떨어뜨렸다", 0 },
    { "you now have ",   "", "이제 {N을} 가지고 있다", 0 },
    { "you found ",      "", "{N을} 발견했다", 0 },
    { "moved onto ",     "", "{N0} 위로 이동했다", 0 },
    { "wielding ",       "", "{N을} 들었다", 0 },
    { "wearing ",        "", "{N을} 착용했다", 0 },
    { "she stole ",      "!", "{N을} 훔쳐 갔다!", 0 },
    { "started a wandering ", "", "{N가} 배회하기 시작했다", 1 },
};

/* extract the noun between pre/post, substitute into fmt with josa */
static int frame_fix_apply(const char *s, char *out)
{
    for (int i = 0; i < (int)(sizeof(FIX) / sizeof(FIX[0])); i++) {
        if (!starts(s, FIX[i].pre)) continue;
        const char *mid = s + strlen(FIX[i].pre);
        size_t ml = strlen(mid);
        size_t pl = strlen(FIX[i].post);
        if (pl && (ml < pl || strcmp(mid + ml - pl, FIX[i].post) != 0)) continue;
        char noun[256];
        size_t take = ml - pl;
        if (take >= sizeof noun) take = sizeof noun - 1;
        memcpy(noun, mid, take); noun[take] = '\0';
        /* trim a trailing " (c)" pack letter, e.g. "단검 (a)" -> "단검" */
        size_t nl = strlen(noun);
        if (nl >= 4 && noun[nl - 1] == ')' && noun[nl - 3] == '(' && noun[nl - 4] == ' ')
            noun[nl - 4] = '\0';
        /* "N gold pieces" -> "금화 N닢" (gold isn't an inv_name item) */
        if (strstr(noun, "gold pieces")) {
            int g = atoi(noun);
            char gb[32]; sprintf(gb, "금화 %d닢", g); strcpy(noun, gb);
        }
        if (FIX[i].monster) {
            int at, len; const char *mko = find_monster(noun, &at, &len);
            if (mko) strcpy(noun, mko); else continue;
        }
        /* render fmt: {N을} -> noun+을/를, {N0} -> noun */
        out[0] = '\0';
        const char *f = FIX[i].fmt;
        while (*f) {
            if (starts(f, "{N을}")) { put_eul(out, noun); f += strlen("{N을}"); }
            else if (starts(f, "{N가}")) { put_iga(out, noun); f += strlen("{N가}"); }
            else if (starts(f, "{N0}")) { strcat(out, noun); f += strlen("{N0}"); }
            else { size_t L = strlen(out); out[L] = *f++; out[L + 1] = '\0'; }
        }
        return 1;
    }
    return 0;
}

/* get_item prompt: "which object do you want to <ACTION>? (* for list): " */
static int frame_prompt(const char *s, char *out)
{
    const char *pre = "which object do you want to ";
    const char *post = "? (* for list): ";
    if (!starts(s, pre) || !ends(s, post)) return 0;
    char act[64]; int k = 0;
    const char *a = s + strlen(pre);
    while (*a && *a != '?' && k < 63) act[k++] = *a++;
    act[k] = '\0';
    struct kv verbs[] = {
        { "drop", "떨어뜨릴" }, { "eat", "먹을" }, { "wield", "들" },
        { "wear", "입을" }, { "put on", "낄" }, { "quaff", "마실" },
        { "read", "읽을" }, { "throw", "던질" }, { "identify", "감정할" },
        { "call", "이름 붙일" }, { "charge", "충전할" }, { "zap with", "사용할" },
    };
    const char *ko = NULL;
    for (int i = 0; i < (int)(sizeof(verbs)/sizeof(verbs[0])); i++)
        if (strcmp(verbs[i].en, act) == 0) { ko = verbs[i].ko; break; }
    if (!ko) return 0;
    sprintf(out, "어느 것을 %s까? (* = 목록): ", ko);
    return 1;
}

/* eat() flavor: "<interj>, this food tastes awful" / "<interj>, that tasted
 * good" / "my, that was a yummy <fruit>" */
static int frame_food(const char *s, char *out)
{
    if (ends(s, "this food tastes awful")) { strcpy(out, "윽, 맛없는 음식이다"); return 1; }
    if (ends(s, "that tasted good"))       { strcpy(out, "음, 맛있다"); return 1; }
    if (starts(s, "my, that was a yummy ")) {
        const char *fruit = s + strlen("my, that was a yummy ");
        sprintf(out, "와, %s%s 맛있었다", fruit, kr_i_ga(fruit));
        return 1;
    }
    return 0;
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

    /* hunger (non-terse variants; terse strings differ but default is verbose) */
    { "you are starting to get hungry",             "슬슬 배가 고파지기 시작한다" },
    { "you are starting to feel weak",              "기운이 빠지기 시작한다" },
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

    /* 1) exact static table (normalize first char for endmsg's upper-casing) */
    char norm[256];
    const char *key = normalize_first(en, norm, sizeof norm);
    for (int i = 0; i < TABLE_LEN; i++)
        if (strcmp(TABLE[i].en, key) == 0)
            return TABLE[i].ko;

    /* 2) dynamic frame rules — run on the first-char-normalized copy so that
     *    endmsg()'s upper-casing of the leading letter doesn't defeat matching.
     *    Only the first ASCII char differs; embedded Korean nouns are intact. */
    if (frame_combat(key, frame_buf)) return frame_buf;
    if (frame_defeat(key, frame_buf)) return frame_buf;
    if (frame_prompt(key, frame_buf)) return frame_buf;
    if (frame_food(key, frame_buf)) return frame_buf;
    if (frame_fix_apply(key, frame_buf)) return frame_buf;

    /* 3) untranslated: pass English (possibly with an embedded Korean noun) */
    return en;
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

    /* item names (kr_item): real inv_name() output forms */
    struct { const char *en; const char *ko; } items[] = {
        { "a dagger",                         "단검" },
        { "5 daggers",                        "단검 5개" },
        { "A +1,+0 long sword",               "+1,+0 장검" },
        { "leather armor",                    "가죽 갑옷" },
        { "+1 ring mail [protection 6]",      "+1 사슬 미늘 갑옷 [방어 6]" },
        { "A potion of healing(blue)",        "치유의 물약" },
        { "A blue potion",                    "파란 물약" },
        { "2 blue potions",                   "파란 물약 2개" },
        { "A scroll of identify scroll",      "두루마리 감정의 두루마리" },
        { "A scroll titled 'foo bar'",        "'foo bar'라 적힌 두루마리" },
        { "A ring of protection(ruby)",       "보호의 반지" },
        { "Some food",                        "음식" },
        { "2 rations of food",                "음식 2개" },
        { "42 Gold pieces",                   "금화 42닢" },
        { "The Amulet of Yendor",             "옌도르의 부적" },
        { "a mace (weapon in hand)",          "철퇴 (장착 중)" },
    };
    printf("\n--- kr_item ---\n");
    for (int i = 0; i < (int)(sizeof(items)/sizeof(items[0])); i++) {
        char *got = kr_item(items[i].en);
        int bad = strcmp(got, items[i].ko) != 0;
        printf("%-34s -> %s%s\n", items[i].en, got, bad ? "   <-- FAIL" : "");
        if (bad) { fails++; printf("    expected: %s\n", items[i].ko); }
    }

    /* message frames (tr_msg): assembled buffers from the engine */
    struct { const char *en; const char *ko; } frames[] = {
        { "You hit the hobgoblin",            "홉고블린을 공격했다" },
        { "You scored an excellent hit on the kobold the orc", NULL }, /* sanity: longest-name */
        { "The hobgoblin hit you",            "홉고블린이 당신을 공격했다" },
        { "The hobgoblin misses you",         "홉고블린이 당신을 공격했으나 빗나갔다" },
        { "You miss the snake",               "뱀을 공격했으나 빗나갔다" },
        { "you have defeated the troll",      "트롤을 처치했다" },
        { ".  Defeated the troll",            "그리고 트롤을 처치했다" },
        { "Dropped 단검",                     "단검을 떨어뜨렸다" },
        { "you now have 단검 (a)",            "이제 단검을 가지고 있다" },
        { "you found 5 gold pieces",          "금화 5닢을 발견했다" },
        { "moved onto 철퇴",                  "철퇴 위로 이동했다" },
        { "started a wandering bat",          "박쥐가 배회하기 시작했다" },
        { "Which object do you want to drop? (* for list): ",
                                              "어느 것을 떨어뜨릴까? (* = 목록): " },
        { "Yuk, this food tastes awful",      "윽, 맛없는 음식이다" },
        { "My, that was a yummy slime-mold",  "와, slime-mold가 맛있었다" },
    };
    printf("\n--- tr_msg frames ---\n");
    for (int i = 0; i < (int)(sizeof(frames)/sizeof(frames[0])); i++) {
        if (!frames[i].ko) continue;  /* sanity-only entries */
        const char *got = tr_msg(frames[i].en);
        int bad = strcmp(got, frames[i].ko) != 0;
        printf("%-36s -> %s%s\n", frames[i].en, got, bad ? "   <-- FAIL" : "");
        if (bad) { fails++; printf("    expected: %s\n", frames[i].ko); }
    }

    printf(fails ? "\n%d FAILURES\n" : "\nALL OK\n", fails);
    return fails ? 1 : 0;
}
#endif
