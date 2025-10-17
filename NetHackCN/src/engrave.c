/* NetHack 3.7	engrave.c	$NHDT-Date: 1713657576 2024/04/20 23:59:36 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.157 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2012. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* doengrave() data */
struct _doengrave_ctx {
    boolean dengr;    /* TRUE if we wipe out the current engraving */
    boolean doblind;  /* TRUE if engraving blinds the player */
    boolean doknown;  /* TRUE if we identify the stylus */
    boolean eow;      /* TRUE if we are overwriting oep */
    boolean jello;    /* TRUE if we are engraving in slime */
    boolean ptext;    /* TRUE if we must prompt for engrave text */
    boolean teleengr; /* TRUE if we move the old engraving */
    boolean zapwand;  /* TRUE if we remove a wand charge */
    boolean disprefresh; /* TRUE if the display needs a refresh */
    boolean frosted;  /* TRUE if engraving on ice */
    boolean adding;   /* TRUE if adding to existing engraving */

    int ret;          /* doengrave return value */
    int type;         /* Type of engraving made */
    int oetype;       /* will be set to type of current engraving */

    struct obj *otmp; /* Object selected with which to engrave */
    struct engr* oep; /* The current engraving */

    char buf[BUFSZ];          /* Buffer for final/poly engraving text */
    char ebuf[BUFSZ];         /* Buffer for initial engraving text */
    char fbuf[BUFSZ];         /* Buffer for "your fingers" */
    char qbuf[QBUFSZ];        /* Buffer for query text */
    char post_engr_text[BUFSZ]; /* Text displayed after engraving prompt */
    char *writer;      /* text of item used for writing */
    const char *everb; /* Present tense of engraving type */
    const char *eloc;  /* Where the engraving is (ie dust/floor/...) */

    size_t len;          /* # of nonspace chars of new engraving text */
};

staticfn int stylus_ok(struct obj *);
staticfn boolean u_can_engrave(void);
staticfn void doengrave_ctx_init(struct _doengrave_ctx *);
staticfn void doengrave_sfx_item_WAN(struct _doengrave_ctx *);
staticfn boolean doengrave_sfx_item(struct _doengrave_ctx *);
staticfn void doengrave_ctx_verb(struct _doengrave_ctx *);
staticfn int engrave(void);
staticfn const char *blengr(void);

char *
random_engraving(char *outbuf)
{
    const char *rumor;

    /* a random engraving may come from the "rumors" file,
       or from the "engrave" file (formerly in an array here) */
    if (!rn2(4) || !(rumor = getrumor(0, outbuf, TRUE)) || !*rumor)
        (void) get_rnd_text(ENGRAVEFILE, outbuf, rn2, MD_PAD_RUMORS);

    wipeout_text(outbuf, (int) (strlen(outbuf) / 4), 0);
    return outbuf;
}

/* Partial rubouts for engraving characters. -3. */
static const struct {
    char wipefrom;
    const char *wipeto;
} rubouts[] = { { 'A', "^" },
                { 'B', "Pb[" },
                { 'C', "(" },
                { 'D', "|)[" },
                { 'E', "|FL[_" },
                { 'F', "|-" },
                { 'G', "C(" },
                { 'H', "|-" },
                { 'I', "|" },
                { 'K', "|<" },
                { 'L', "|_" },
                { 'M', "|" },
                { 'N', "|\\" },
                { 'O', "C(" },
                { 'P', "F" },
                { 'Q', "C(" },
                { 'R', "PF" },
                { 'T', "|" },
                { 'U', "J" },
                { 'V', "/\\" },
                { 'W', "V/\\" },
                { 'Z', "/" },
                { 'b', "|" },
                { 'd', "c|" },
                { 'e', "c" },
                { 'g', "c" },
                { 'h', "n" },
                { 'j', "i" },
                { 'k', "|" },
                { 'l', "|" },
                { 'm', "nr" },
                { 'n', "r" },
                { 'o', "c" },
                { 'q', "c" },
                { 'w', "v" },
                { 'y', "v" },
                { ':', "." },
                { ';', ",:" },
                { ',', "." },
                { '=', "-" },
                { '+', "-|" },
                { '*', "+" },
                { '@', "0" },
                { '0', "C(" },
                { '1', "|" },
                { '6', "o" },
                { '7', "/" },
                { '8', "3o" } };

/* degrade some of the characters in a string */
void
wipeout_text(
    char *engr,    /* engraving text */
    int cnt,       /* number of chars to degrade */
    unsigned seed) /* for semi-controlled randomization */
{
    char *s;
    int i, j, nxt, use_rubout;
    unsigned lth = (unsigned) strlen(engr);

    if (lth && cnt > 0) {
        while (cnt--) {
            /* pick next character */
            if (!seed) {
                /* random */
                nxt = rn2((int) lth);
                use_rubout = rn2(4);
            } else {
                /* predictable; caller can reproduce the same sequence by
                   supplying the same arguments later, or a pseudo-random
                   sequence by varying any of them */
                nxt = seed % lth;
                seed *= 31, seed %= (BUFSZ - 1);
                use_rubout = seed & 3;
            }
            s = &engr[nxt];
            if (*s == ' ')
                continue;

            /* rub out unreadable & small punctuation marks */
            if (strchr("?.,'`-|_", *s)) {
                *s = ' ';
                continue;
            }

            if (!use_rubout) {
                i = SIZE(rubouts);
            } else {
                for (i = 0; i < SIZE(rubouts); i++)
                    if (*s == rubouts[i].wipefrom) {
                        unsigned ln = (unsigned) strlen(rubouts[i].wipeto);
                        /*
                         * Pick one of the substitutes at random.
                         */
                        if (!seed) {
                            j = rn2((int) ln);
                        } else {
                            seed *= 31, seed %= (BUFSZ - 1);
                            j = seed % ln;
                        }
                        *s = rubouts[i].wipeto[j];
                        break;
                    }
            }

            /* didn't pick rubout; use '?' for unreadable character */
            if (i == SIZE(rubouts))
                *s = '?';
        }
    }

    /* trim trailing spaces */
    while (lth && engr[lth - 1] == ' ')
        engr[--lth] = '\0';
}

/* check whether hero can reach something at ground level */
boolean
can_reach_floor(boolean check_pit)
{
    struct trap *t;

    if (u.uswallow
        || (u.ustuck && !sticks(gy.youmonst.data)
            /* assume that arms are pinned rather than that the hero
               has been lifted up above the floor [doesn't explain
               how hero can attack the creature holding him or her;
               that's life in nethack...] */
            && attacktype(u.ustuck->data, AT_HUGS))
        || (Levitation && !(Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))))
        return FALSE;
    /* Restricted/unskilled riders can't reach the floor */
    if (u.usteed && P_SKILL(P_RIDING) < P_BASIC)
        return FALSE;
    if (u.uundetected && ceiling_hider(gy.youmonst.data))
        return FALSE;

    if (Flying || gy.youmonst.data->msize >= MZ_HUGE)
        return TRUE;

    if (check_pit && (t = t_at(u.ux, u.uy)) != 0
        && (uteetering_at_seen_pit(t) || uescaped_shaft(t)))
        return FALSE;

    return TRUE;
}

/* give a message after caller has determined that hero can't reach */
void
cant_reach_floor(coordxy x, coordxy y, boolean up, boolean check_pit)
{
    You("无法够到%s.",
        up ? ceiling(x, y)
           : (check_pit && can_reach_floor(FALSE))
               ? "坑的底部"
               : surface(x, y));
}

struct engr *
engr_at(coordxy x, coordxy y)
{
    struct engr *ep = head_engr;

    while (ep) {
        if (x == ep->engr_x && y == ep->engr_y)
            return ep;
        ep = ep->nxt_engr;
    }
    return (struct engr *) 0;
}

/* Decide whether a particular string is engraved at a specified
 * location; a case-insensitive substring match is used.
 * Ignore headstones, in case the player names herself "Elbereth".
 *
 * If strict checking is requested, the word is only considered to be
 * present if it is intact and is the entire content of the engraving.
 */
struct engr *
sengr_at(const char *s, coordxy x, coordxy y, boolean strict)
{
    struct engr *ep = engr_at(x, y);

    if (ep && ep->engr_type != HEADSTONE && ep->engr_time <= svm.moves) {
        if (strict ? !strcmpi(ep->engr_txt[actual_text], s)
                   : (strstri(ep->engr_txt[actual_text], s) != 0))
            return ep;
    }
    return (struct engr *) NULL;
}

void
u_wipe_engr(int cnt)
{
    if (can_reach_floor(TRUE))
        wipe_engr_at(u.ux, u.uy, cnt, FALSE);
}

void
wipe_engr_at(coordxy x, coordxy y, xint16 cnt, boolean magical)
{
    struct engr *ep = engr_at(x, y);

    /* Headstones and some specially marked engravings are indelible */
    if (ep && ep->engr_type != HEADSTONE && !ep->nowipeout) {
        debugpline1("asked to erode %d characters", cnt);
        if (ep->engr_type != BURN || is_ice(x, y) || (magical && !rn2(2))) {
            if (ep->engr_type != DUST && ep->engr_type != ENGR_BLOOD) {
                cnt = rn2(1 + 50 / (cnt + 1)) ? 0 : 1;
                debugpline1("actually eroding %d characters", cnt);
            }
            wipeout_text(ep->engr_txt[actual_text], (int) cnt, 0);
            while (ep->engr_txt[actual_text][0] == ' ')
                ep->engr_txt[actual_text]++;
            if (!ep->engr_txt[actual_text][0])
                del_engr(ep);
        }
    }
}

void
read_engr_at(coordxy x, coordxy y)
{
    struct engr *ep = engr_at(x, y);
    const char *eloc = surface(x, y);
    int sensed = 0;

    /* Sensing an engraving does not require sight,
     * nor does it necessarily imply comprehension (literacy).
     */
    if (ep && ep->engr_txt[actual_text][0]) {
        switch (ep->engr_type) {
        case DUST:
            if (!Blind) {
                sensed = 1;
                pline("%s写在%s里.", Something,
                      is_ice(x, y) ? "霜" : "灰尘");
            }
            break;
        case ENGRAVE:
        case HEADSTONE:
            if (!Blind || can_reach_floor(TRUE)) {
                sensed = 1;
                pline("%s被刻在%s上.", Something, eloc);
            }
            break;
        case BURN:
            if (!Blind || can_reach_floor(TRUE)) {
                sensed = 1;
                pline("一些文本已经%s进这里的%s.",
                      is_ice(x, y) ? "熔" : "烧", eloc);
            }
            break;
        case MARK:
            if (!Blind) {
                sensed = 1;
                pline("这里的%s上有一些涂鸦.", surface(x, y));
            }
            break;
        case ENGR_BLOOD:
            /* "It's a message!  Scrawled in blood!"
             * "What's it say?"
             * "It says... `See you next Wednesday.'" -- Thriller
             */
            if (!Blind) {
                sensed = 1;
                You_see("血里面用手印写着一条消息.");
            }
            break;
        default:
            impossible("%s is written in a very strange way.", Something);
            sensed = 1;
        }

        if (sensed) {
            char *et, buf[BUFSZ];
            int maxelen = (int) (sizeof buf
                                 /* sizeof "literal" counts terminating \0 */
                                 - sizeof "You feel the words: \"\".");

            if ((int) strlen(ep->engr_txt[actual_text]) > maxelen) {
                (void) strncpy(buf, ep->engr_txt[actual_text], maxelen);
                buf[maxelen] = '\0';
                et = buf;
            } else {
                et = ep->engr_txt[actual_text];
            }
            You("%s: \"%s\".", (Blind) ? "感受这句话" : "读道", et);
            Strcpy(ep->engr_txt[remembered_text], ep->engr_txt[actual_text]);
            ep->eread = 1;
            if (svc.context.run > 0)
                nomul(0);
        }
    }
}

void
make_engr_at(
    coordxy x, coordxy y,
    const char *s,
    long e_time,
    int e_type)
{
    int i;
    struct engr *ep;
    unsigned smem = Strlen(s) + 1;

    if ((ep = engr_at(x, y)) != 0)
        del_engr(ep);

    ep = newengr(smem * 3);
    (void) memset((genericptr_t) ep, 0, (smem * 3) + sizeof (struct engr));
    ep->nxt_engr = head_engr;
    head_engr = ep;
    ep->engr_x = x;
    ep->engr_y = y;
    ep->engr_txt[actual_text] = (char *) (ep + 1);
    ep->engr_txt[remembered_text] = ep->engr_txt[actual_text] + smem;
    ep->engr_txt[pristine_text] = ep->engr_txt[remembered_text] + smem;
    for(i = 0; i < text_states; ++i)
        Strcpy(ep->engr_txt[i], s);
    if (!strcmp(s, "Elbereth")) {
        /* engraving "Elbereth":  if done when making a level, it creates
           an old-style Elbereth that deters monsters when any objects are
           present; otherwise (done by the player), exercises wisdom */
        if (gi.in_mklev)
            ep->guardobjects = 1;
        else
            exercise(A_WIS, TRUE);
    }
    ep->engr_time = e_time;
    ep->engr_type = (xint8) ((e_type > 0) ? e_type : rnd(N_ENGRAVE - 1));
    ep->engr_szeach = smem;
    ep->engr_alloc = smem * 3;
    /* we do not set ep->eread; the caller will need to if required */
}

/* delete any engraving at location <x,y> */
void
del_engr_at(coordxy x, coordxy y)
{
    struct engr *ep = engr_at(x, y);

    if (ep)
        del_engr(ep);
}

/*
 * freehand - returns true if player has a free hand
 */
int
freehand(void)
{
    return (!uwep || !welded(uwep)
            || (!bimanual(uwep) && (!uarms || !uarms->cursed)));
}

/* getobj callback for an object to engrave with */
staticfn int
stylus_ok(struct obj *obj)
{
    if (!obj)
        return GETOBJ_SUGGEST;

    /* Potential extension: exclude weapons that don't make any sense (such as
     * bullwhips) and downplay rings and gems that wouldn't be good to write
     * with (such as glass and non-gem rings) */
    if (obj->oclass == WEAPON_CLASS || obj->oclass == WAND_CLASS
        || obj->oclass == GEM_CLASS || obj->oclass == RING_CLASS)
        return GETOBJ_SUGGEST;

    /* Only markers and towels are recommended tools. */
    if (obj->oclass == TOOL_CLASS
        && (obj->otyp == TOWEL || obj->otyp == MAGIC_MARKER))
        return GETOBJ_SUGGEST;

    return GETOBJ_DOWNPLAY;
}

/* can hero engrave at all (at their location)? */
staticfn boolean
u_can_engrave(void)
{
    int levtyp = SURFACE_AT(u.ux, u.uy);

    if (u.uswallow) {
        if (is_animal(u.ustuck->data)) {
            pline("你想写什么?  \"Jonah was here\"?");
            return FALSE;
        } else if (is_whirly(u.ustuck->data)) {
            cant_reach_floor(u.ux, u.uy, FALSE, FALSE);
            return FALSE;
        }
        /* Note: for amorphous engulfers, writing attempt is allowed here
           but yields the 'jello' result in doengrave() */
    } else if (is_lava(u.ux, u.uy)) {
        You_cant("在%s上写东西!", surface(u.ux, u.uy));
        return FALSE;
    } else if (is_pool(u.ux, u.uy) || IS_FOUNTAIN(levtyp)) {
        You_cant("在%s上写东西!", surface(u.ux, u.uy));
        return FALSE;
    } else if (IS_AIR(levtyp)) {
        /* airlevel or inside bubble on waterlevel */
        You_cant("在%s写东西!",
                 (levtyp == CLOUD) ? "云里" : "稀薄的空气中");
        return FALSE;
    } else if (!ACCESSIBLE(levtyp)) {
        /* stone, tree, wall, secret corridor, pool, lava, bars */
        You_cant("在这里写东西.");
        return FALSE;
    }

    if (cantwield(gy.youmonst.data)) {
        You_cant("持握任何东西!");
        return FALSE;
    }
    if (check_capacity((char *) 0))
        return FALSE;
    return TRUE;
}

/* initialize the doengrave data */
staticfn void
doengrave_ctx_init(struct _doengrave_ctx *de)
{
    de->dengr = FALSE;
    de->doblind = FALSE;
    de->doknown = FALSE;
    de->eow = FALSE;
    de->ptext = TRUE;
    de->teleengr = FALSE;
    de->zapwand = FALSE;
    de->disprefresh = FALSE;
    de->adding = FALSE;

    de->ret = ECMD_OK;
    de->type = DUST;
    de->oetype = 0;

    de->otmp = (struct obj *) 0;
    de->oep = engr_at(u.ux, u.uy);

    de->buf[0] = (char) 0;
    de->ebuf[0] = (char) 0;
    de->fbuf[0] = (char) 0;
    de->qbuf[0] = (char) 0;
    de->post_engr_text[0] = (char) 0;
    de->writer = (char *) 0;

    if (de->oep)
        de->oetype = de->oep->engr_type;
    if (is_demon(gy.youmonst.data) || is_vampire(gy.youmonst.data))
        de->type = ENGR_BLOOD;

    de->jello = (u.uswallow && !(is_animal(u.ustuck->data)
                                 || is_whirly(u.ustuck->data)));
    de->frosted = is_ice(u.ux, u.uy);
}

/* special engraving effects for WAND objects */
staticfn void
doengrave_sfx_item_WAN(struct _doengrave_ctx *de)
{
    switch (de->otmp->otyp) {
        /* DUST wands */
    default:
        break;
        /* NODIR wands */
    case WAN_LIGHT:
    case WAN_SECRET_DOOR_DETECTION:
    case WAN_CREATE_MONSTER:
    case WAN_WISHING:
    case WAN_ENLIGHTENMENT:
        zapnodir(de->otmp);
        break;
        /* IMMEDIATE wands */
        /* If wand is "IMMEDIATE", remember to affect the
         * previous engraving even if turning to dust.
         */
    case WAN_STRIKING:
        Strcpy(de->post_engr_text,
               "魔杖没能反抗你的刻写!");
        break;
    case WAN_SLOW_MONSTER:
        if (!Blind) {
            Sprintf(de->post_engr_text, "%s上的臭虫速度变慢了!",
                    surface(u.ux, u.uy));
        }
        break;
    case WAN_SPEED_MONSTER:
        if (!Blind) {
            Sprintf(de->post_engr_text, "%s上的臭虫速度加快了!",
                    surface(u.ux, u.uy));
        }
        break;
    case WAN_POLYMORPH:
        if (de->oep) {
            if (!Blind) {
                de->type = (xint16) 0; /* random */
                (void) random_engraving(de->buf);
            } else {
                /* keep the same type so that feels don't
                   change and only the text is altered,
                   but you won't know anyway because
                   you're a _blind writer_ */
                if (de->oetype)
                    de->type = de->oetype;
                xcrypt(blengr(), de->buf);
            }
            de->dengr = TRUE;
        }
        break;
    case WAN_NOTHING:
    case WAN_UNDEAD_TURNING:
    case WAN_OPENING:
    case WAN_LOCKING:
    case WAN_PROBING:
        break;
        /* RAY wands */
    case WAN_MAGIC_MISSILE:
        de->ptext = TRUE;
        if (!Blind) {
            Sprintf(de->post_engr_text,
                    "%s上都是弹孔!",
                    surface(u.ux, u.uy));
        }
        break;
        /* can't tell sleep from death - Eric Backus */
    case WAN_SLEEP:
    case WAN_DEATH:
        if (!Blind) {
            Sprintf(de->post_engr_text,  "%s上的臭虫停止了移动!",
                    surface(u.ux, u.uy));
        }
        break;
    case WAN_COLD:
        if (!Blind)
            Strcpy(de->post_engr_text,
                   "一些冰块从魔杖上掉下来了.");
        if (!de->oep || (de->oep->engr_type != BURN))
            break;
        /*FALLTHRU*/
    case WAN_CANCELLATION:
    case WAN_MAKE_INVISIBLE:
        if (de->oep && de->oep->engr_type != HEADSTONE) {
            if (!Blind)
                pline_The("刻写在%s上的文字消失了!",
                          surface(u.ux, u.uy));
            de->dengr = TRUE;
        }
        break;
    case WAN_TELEPORTATION:
        if (de->oep && de->oep->engr_type != HEADSTONE) {
            if (!Blind)
                pline_The("刻写在%s上的文字消失了!",
                          surface(u.ux, u.uy));
            de->teleengr = TRUE;
        }
        break;
        /* type = ENGRAVE wands */
    case WAN_DIGGING:
        de->ptext = TRUE;
        de->type = ENGRAVE;
        if (!objects[de->otmp->otyp].oc_name_known) {
            if (flags.verbose)
                pline("这把%s是挖掘魔杖!", xname(de->otmp));
            de->doknown = TRUE;
        }
        Strcpy(de->post_engr_text,
               (Blind && !Deaf)
               ? "你听到电钻的声音!"    /* Deaf-aware */
               : Blind
                  ? "你感觉到震动."
                  : IS_GRAVE(levl[u.ux][u.uy].typ)
                     ? "碎片从墓碑中飞出."
                     : de->frosted
                        ? "冰碎片从冰面飞出!"
                        : (svl.level.locations[u.ux][u.uy].typ
                          == DRAWBRIDGE_DOWN)
                           ? "碎木片从桥上飞出."
                           : "砾石从地板上飞出.");
        break;
        /* type = BURN wands */
    case WAN_FIRE:
        de->ptext = TRUE;
        de->type = BURN;
        if (!objects[de->otmp->otyp].oc_name_known) {
            if (flags.verbose)
                pline("这把%s是火焰魔杖!", xname(de->otmp));
            de->doknown = TRUE;
        }
        Strcpy(de->post_engr_text, Blind ? "你感觉这根魔杖变热了."
                                         : "火焰从魔杖中飞出.");
        break;
    case WAN_LIGHTNING:
        de->ptext = TRUE;
        de->type = BURN;
        if (!objects[de->otmp->otyp].oc_name_known) {
            if (flags.verbose)
                pline("这把%s是闪电魔杖!", xname(de->otmp));
            de->doknown = TRUE;
        }
        if (!Blind) {
            Strcpy(de->post_engr_text, "魔杖释放出电弧.");
            de->doblind = TRUE;
        } else {
            Strcpy(de->post_engr_text, !Deaf
                   ? "你听见劈啪声!"     /* Deaf-aware */
                   : "你的头发竖起来了!");
        }
        break;
        /* type = MARK wands */
        /* type = ENGR_BLOOD wands */
    }
}

/* special engraving effects for all objects */
staticfn boolean
doengrave_sfx_item(struct _doengrave_ctx *de)
{
    switch (de->otmp->oclass) {
    default:
    case AMULET_CLASS:
    case CHAIN_CLASS:
    case POTION_CLASS:
    case COIN_CLASS:
        break;
    case RING_CLASS:
        /* "diamond" rings and others should work */
    case GEM_CLASS:
        /* diamonds & other hard gems should work */
        if (objects[de->otmp->otyp].oc_tough) {
            de->type = ENGRAVE;
            break;
        }
        break;
    case ARMOR_CLASS:
        if (is_boots(de->otmp)) {
            de->type = DUST;
            break;
        }
        /*FALLTHRU*/
    /* Objects too large to engrave with */
    case BALL_CLASS:
    case ROCK_CLASS:
        You_cant("用这么大的东西来写字!");
        de->ptext = FALSE;
        break;
    /* Objects too silly to engrave with */
    case FOOD_CLASS:
    case SCROLL_CLASS:
    case SPBOOK_CLASS:
        pline("不要. %s会%s的.", Yname2(de->otmp),
              de->frosted ? "盖上一层霜" : "变得太脏");
        de->ptext = FALSE;
        break;
    case RANDOM_CLASS: /* This should mean fingers */
        break;

    /* The charge is removed from the wand before prompting for
     * the engraving text, because all kinds of setup decisions
     * and pre-engraving messages are based upon knowing what type
     * of engraving the wand is going to do.  Also, the player
     * will have potentially seen "You wrest .." message, and
     * therefore will know they are using a charge.
     */
    case WAND_CLASS:
        if (zappable(de->otmp)) {
            check_unpaid(de->otmp);
            if (de->otmp->cursed && !rn2(WAND_BACKFIRE_CHANCE)) {
                wand_explode(de->otmp, 0);
                de->ret = ECMD_TIME;
                return FALSE;
            }
            de->zapwand = TRUE;
            if (!can_reach_floor(TRUE))
                de->ptext = FALSE;
            doengrave_sfx_item_WAN(de);
        } else { /* end if zappable */
            /* failing to wrest one last charge takes time */
            de->ptext = FALSE; /* use "early exit" below, return 1 */
            /* give feedback here if we won't be getting the
               "can't reach floor" message below */
            if (can_reach_floor(TRUE)) {
                /* cancelled wand turns to dust */
                if (de->otmp->spe < 0)
                    de->zapwand = TRUE;
                /* empty wand just doesn't write */
                else
                    pline_The("魔杖太破旧了不能刻写.");
            }
        }
        break;

    case WEAPON_CLASS:
        if (is_art(de->otmp, ART_FIRE_BRAND)) {
            de->type = BURN; /* doesn't dull weapon */
        } else if (is_blade(de->otmp)) {
            /* if non-blade or welded or too dull, engraving type stays set
               to DUST; feedback for that is only given for bladed weapons */
            if (welded(de->otmp))
                pline("%s只能刮花%s.",
                      Yname2(de->otmp), surface(u.ux, u.uy));
            else if ((int) de->otmp->spe <= -3)
                pline("%s 太钝了不能刻写.",
                      Yobjnam2(de->otmp, "已经"));
            else
                de->type = ENGRAVE;
        }
        break;

    case TOOL_CLASS:
        if (de->otmp == ublindf) {
            pline(
                "那个有点难用来刻写, 你不这样认为吗?");
            de->ret = ECMD_FAIL;
            return FALSE;
        }
        switch (de->otmp->otyp) {
        case MAGIC_MARKER:
            if (de->otmp->spe <= 0)
                Your("魔笔已经干了.");
            else
                de->type = MARK;
            break;
        case TOWEL:
            /* Can't really engrave with a towel */
            de->ptext = FALSE;
            if (de->oep) {
                if (de->oep->engr_type == DUST
                    || de->oep->engr_type == ENGR_BLOOD
                    || de->oep->engr_type == MARK) {
                    if (is_wet_towel(de->otmp))
                        dry_a_towel(de->otmp, -1, TRUE);
                    if (!Blind)
                        You("清除了这里的信息.");
                    else
                        pline("%s %s.", Yobjnam2(de->otmp, "变得"),
                              de->frosted ? "上霜了" : "灰扑扑的");
                    de->dengr = TRUE;
                } else {
                    pline("%s不能清除刻在这里的文字.",
                          Yname2(de->otmp));
                }
            } else {
                pline("%s %s.", Yobjnam2(de->otmp, "变得"),
                      de->frosted ? "上霜了" : "灰扑扑的");
            }
            break;
        default:
            break;
        }
        break;

    case VENOM_CLASS:
        /* this used to be ``if (wizard)'' and fall through to ILLOBJ_CLASS
           for normal play, but splash of venom isn't "illegal" because it
           could occur in normal play via wizard mode bones */
        pline("写一封带毒邮件?");
        break;

    case ILLOBJ_CLASS:
        impossible("You're engraving with an illegal object!");
        break;
    }

    return TRUE;
}

/* which verb phrasing to use for engraving */
staticfn void
doengrave_ctx_verb(struct _doengrave_ctx *de)
{
    switch (de->type) {
    default:
        de->everb = de->adding ? "添加到奇怪文字上"
                               : "奇怪地写在";
        break;
    case DUST:
        de->everb = de->adding ? "添加到书写内容上" : "写在";
        de->eloc = de->frosted ? "霜" : "灰尘";
        break;
    case HEADSTONE:
        de->everb = de->adding ? "添加到墓志铭上" : "刻在墓碑上";
        break;
    case ENGRAVE:
        de->everb = de->adding ? "添加到雕刻上" : "雕刻在";
        break;
    case BURN:
        de->everb = de->adding ? (de->frosted ? "添加到烧熔的文字上"
                                  : "添加到烧灼的文字上")
                       : (de->frosted ? "烧熔" : "烧进");
        break;
    case MARK:
        de->everb = de->adding ? "添加到涂鸦上" : "涂写在";
        break;
    case ENGR_BLOOD:
        de->everb = de->adding ? "添加到潦草字迹上" : "潦草写在";
        break;
    }
}

/* 莫氏硬度标度：
 *  1 - 滑石             6 - 正长石
 *  2 - 石膏             7 - 石英
 *  3 - 方解石           8 - 黄玉
 *  4 - 萤石             9 - 刚玉
 *  5 - 磷灰石          10 - 钻石
 *
 * 由于花岗岩是火成岩, 硬度约7, 任何硬度>=8的矿物
 * 应该能够刮擦岩石. 
 * 较软宝石的减值不易实现, 因为obj结构
 * 目前不包含单独的oc_cost. 7/91
 *
 * 钢        - 5-8.5   (通常武器)
 * 钻石      - 10                    * 玉        -  5-6      (软玉)
 * 红宝石    -  9      (刚玉)        * 绿松石    -  5-6
 * 蓝宝石    -  9      (刚玉)        * 蛋白石    -  5-6
 * 黄玉      -  8                    * 玻璃      - ~5.5
 * 祖母绿    -  7.5-8  (绿柱石)      * 双锂晶    -  4-5??
 * 海蓝宝石  -  7.5-8  (绿柱石)      * 铁        -  4-5
 * 石榴石    -  7.25   (变化 6.5-8)  * 萤石      -  4
 * 玛瑙      -  7      (石英)        * 黄铜      -  3-4
 * 紫水晶    -  7      (石英)        * 金        -  2.5-3
 * 碧玉      -  7      (石英)        * 银        -  2.5-3
 * 缟玛瑙    -  7      (石英)        * 铜        -  2.5-3
 * 月长石    -  6      (正长石)      * 琥珀      -  2-2.5
 */

/* #engrave 命令 */
int
doengrave(void)
{
    char *sp;         /* 雕刻文本空位占位符 */
    struct _doengrave_ctx *de;
    int retval;

    /* 冒险者能雕刻吗?  */
    if (!u_can_engrave())
        return ECMD_FAIL;

    de = (struct _doengrave_ctx *) alloc(sizeof (struct _doengrave_ctx));
    doengrave_ctx_init(de);

    gm.multi = 0;              /* 消耗的移动次数 */
    gn.nomovemsg = (char *) 0; /* 占用结束消息 */

    /* 可以用手指、武器、魔杖等书写...
     * 由GAN于10/20/86编辑, 不改变已挥舞的武器. 
     */

    de->otmp = getobj("用来书写的工具", stylus_ok, GETOBJ_PROMPT);
    if (!de->otmp) {/* otmp == &hands_obj 如果使用手指 */
        de->ret = ECMD_CANCEL;
        goto doengr_exit;
    }

    if (de->otmp == &hands_obj) {
        Strcat(strcpy(de->fbuf, "你的"), body_part(FINGERTIP));
        de->writer = de->fbuf;
    } else {
        de->writer = yname(de->otmp);
    }

    /* 没有理由在你双手被绑时还能用魔杖书写. 
     */
    if (!freehand() && de->otmp != uwep && !de->otmp->owornmask) {
        You("没有空闲的%s来书写! ", body_part(HAND));
        goto doengr_exit;
    }

    if (de->jello) {
        You("用%s挠了挠%s. ", de->writer, mon_nam(u.ustuck));
        Your("信息溶解了...");
        goto doengr_exit;
    }
    if (de->otmp->oclass != WAND_CLASS && !can_reach_floor(TRUE)) {
        cant_reach_floor(u.ux, u.uy, FALSE, TRUE);
        goto doengr_exit;
    }
    if (IS_ALTAR(levl[u.ux][u.uy].typ)) {
        You("朝着祭坛用%s比划. ", de->writer);
        altar_wrath(u.ux, u.uy);
        goto doengr_exit;
    }
    if (IS_GRAVE(levl[u.ux][u.uy].typ)) {
        if (de->otmp == &hands_obj) { /* 仅使用手指 */
            You("只会在%s上留下一个手指印. ",
                surface(u.ux, u.uy));
            goto doengr_exit;
        } else if (!levl[u.ux][u.uy].disturbed) {
            /* 打扰坟墓：召唤食尸鬼, 类似于有时踢坟墓时发生的情况；
               设置levl[ux][uy]->disturbed, 因此只会发生一次 */
            disturb_grave(u.ux, u.uy);
            goto doengr_exit;
        }
    }

    /* 物品的特殊效果 */
    if (!doengrave_sfx_item(de))
        goto doengr_exit;

    if (IS_GRAVE(levl[u.ux][u.uy].typ)) {
        if (de->type == ENGRAVE || de->type == 0) {
            de->type = HEADSTONE;
        } else {
            /* 确保"无法擦除"的情况 */
            de->type = DUST;
            de->dengr = FALSE;
            de->teleengr = FALSE;
            de->buf[0] = '\0';
        }
    }

    /*
     * 工具设置结束
     */

    /* 识别铁笔 */
    if (de->doknown) {
        learnwand(de->otmp);
        if (objects[de->otmp->otyp].oc_name_known)
            more_experienced(0, 10);
    }
    if (de->teleengr) {
        rloc_engr(de->oep);
        de->oep->eread = 0;
        de->disprefresh = TRUE;
        de->oep = (struct engr *) 0;
    }
    if (de->dengr) {
        del_engr(de->oep);
        de->oep = (struct engr *) 0;
        de->disprefresh = TRUE;
    }
    /* 某些东西改变了这里的雕刻 */
    if (*de->buf) {
        struct engr *tmp_ep;

        make_engr_at(u.ux, u.uy, de->buf, svm.moves, de->type);
        tmp_ep = engr_at(u.ux, u.uy);
        if (!Blind) {
            if (tmp_ep != 0) {
                pline_The("雕刻现在显示为：\"%s\". ", de->buf);
                tmp_ep->eread = 1;
                de->disprefresh = TRUE;
            }
        }
        de->ptext = FALSE;
    }
    if (de->zapwand && (de->otmp->spe < 0)) {
        pline("%s %s化为灰尘. ", The(xname(de->otmp)),
              Blind ? "" : "剧烈发光, 然后");
        if (!IS_GRAVE(levl[u.ux][u.uy].typ))
            You(
    "试图用你的灰尘在%s上书写是行不通的. ",
                de->frosted ? "霜" : "灰尘");
        useup(de->otmp);
        de->otmp = 0; /* 魔杖现已消失 */
        de->ptext = FALSE;
    }
    /* Early exit for some implements. */
    if (!de->ptext) {
        if (de->otmp && de->otmp->oclass == WAND_CLASS
            && !can_reach_floor(TRUE))
            cant_reach_floor(u.ux, u.uy, FALSE, TRUE);
        de->ret = ECMD_TIME;
        goto doengr_exit;
    }
    /*
     * Special effects should have deleted the current engraving (if
     * possible) by now.
     */
    if (de->oep) {
        char c = 'n';

        /* Give player the choice to add to engraving. */
        if (de->type == HEADSTONE) {
            /* no choice, only append */
            c = 'y';
        } else if (de->type == de->oep->engr_type
                   && (!Blind || de->oep->engr_type == BURN
                       || de->oep->engr_type == ENGRAVE)) {
            c = yn_function("你想在当前的刻写上添加吗?",
                            ynqchars, 'y', TRUE);
            if (c == 'q') {
                pline1(Never_mind);
                goto doengr_exit;
            }
        }

        if (c == 'n' || Blind) {
            if (de->oep->engr_type == DUST
                || de->oep->engr_type == ENGR_BLOOD
                || de->oep->engr_type == MARK) {
                if (!Blind) {
                    You("清除了%s的信息.",
                        (de->oep->engr_type == DUST)
                            ? (de->frosted
                                ? "写在霜里"
                                : "写在灰尘里")
                            : (de->oep->engr_type == ENGR_BLOOD)
                                ? "涂抹在血液里"
                                : "写在这里");
                    del_engr(de->oep);
                    de->oep = (struct engr *) 0;
                    de->disprefresh = TRUE;
                } else {
                    /* defer deletion until after we *know* we're engraving */
                    de->eow = TRUE;
                }
            } else if (de->type == DUST || de->type == MARK
                       || de->type == ENGR_BLOOD) {
                You("不能清除%s%s.",
                    (de->oep->engr_type == BURN)
                        ? (de->frosted ? "熔进" : "烧进")
                        : "刻在",
                    surface(u.ux, u.uy));
                de->ret = ECMD_TIME;
                goto doengr_exit;
            } else if (de->type != de->oep->engr_type || c == 'n') {
                if (!Blind || can_reach_floor(TRUE))
                    You("会覆盖当前的信息.");
                de->eow = TRUE;
            }
        } else if (de->oep
                   && Strlen(de->oep->engr_txt[actual_text]) >= BUFSZ - 1) {
            There("没有空间添加其他的字了.");
            de->ret = ECMD_TIME;
            goto doengr_exit;
        }
    }

    de->eloc = surface(u.ux, u.uy);
    de->adding = (de->oep && !de->eow);
    doengrave_ctx_verb(de);

    /* Tell adventurer what is going on */
    if (de->otmp != &hands_obj)
        You("%s the %s with %s%s.", de->everb, de->eloc,
            /* since doname() yields "N items" when quantity is more than
               one, match that by using "1 of" rather than "one of" when
               informing the player that the stack will be split */
            (de->type == ENGRAVE && de->otmp->quan > 1L) ? "1 of " : "",
            doname(de->otmp));
    else
        You("用你的%s%s到%s上.", body_part(FINGERTIP),
            de->everb, de->eloc);

    /* Prompt for engraving! */
    Sprintf(de->qbuf, "你想在%s上%s什么?", de->eloc,
            de->everb);
    getlin(de->qbuf, de->ebuf);
    /* convert tabs to spaces and condense consecutive spaces to one */
    mungspaces(de->ebuf);

    /* Count the actual # of chars engraved not including spaces */
    de->len = strlen(de->ebuf);
    for (sp = de->ebuf; *sp; sp++)
        if (*sp == ' ')
            de->len -= 1;

    if (de->len == 0 || strchr(de->ebuf, '\033')) {
        if (de->zapwand) {
            if (!Blind)
                pline("%s, 然后%s了.", Tobjnam(de->otmp, "发光"),
                      otense(de->otmp, "暗淡"));
            de->ret = ECMD_TIME;
            goto doengr_exit;
        } else {
            pline1(Never_mind);
            goto doengr_exit;
        }
    }

    /* A single `x' is the traditional signature of an illiterate person */
    if (de->len != 1 || (!strchr(de->ebuf, 'x') && !strchr(de->ebuf, 'X')))
        if (!u.uconduct.literate++)
            livelog_printf(LL_CONDUCT, "识字了, 因为你刻写了\"%s\"",
                           de->ebuf);

    /* Mix up engraving if surface or state of mind is unsound.
       Note: this won't add or remove any spaces. */
    for (sp = de->ebuf; *sp; sp++) {
        if (*sp == ' ')
            continue;
        if (((de->type == DUST || de->type == ENGR_BLOOD) && !rn2(25))
            || (Blind && !rn2(11)) || (Confusion && !rn2(7))
            || (Stunned && !rn2(4)) || (Hallucination && !rn2(2)))
            *sp = ' ' + rnd(96 - 2); /* ASCII '!' thru '~'
                                        (excludes ' ' and DEL) */
    }

    /* Previous engraving is overwritten */
    if (de->eow) {
        del_engr(de->oep);
        de->oep = (struct engr *) 0;
        de->disprefresh = TRUE;
    }

    Strcpy(svc.context.engraving.text, de->ebuf);
    svc.context.engraving.nextc = svc.context.engraving.text;
    svc.context.engraving.stylus = de->otmp;
    svc.context.engraving.type = de->type;
    svc.context.engraving.pos.x = u.ux;
    svc.context.engraving.pos.y = u.uy;
    svc.context.engraving.actionct = 0;
    set_occupation(engrave, "engraving", 0);

    if (de->post_engr_text[0])
        pline("%s", de->post_engr_text);
    if (de->doblind && !resists_blnd(&gy.youmonst)) {
        You("被强烈的闪光导致失明!");
        make_blinded((long) rnd(50), FALSE);
        if (!Blind)
            Your1(vision_clears);
    }

    /* Engraving will always take at least one action via being run as an
       occupation, so do not count this setup as taking time. */
 doengr_exit:
    if (de->disprefresh)
        newsym(u.ux, u.uy);
    retval = de->ret;
    free(de);
    return retval;
}

/* occupation callback for engraving some text */
staticfn int
engrave(void)
{
    struct engr *oep;
    char buf[BUFSZ]; /* holds the post-this-action engr text, including
                      * anything already there */
    const char *finishverb; /* "You finish [foo]." */
    struct obj * stylus; /* shorthand for svc.context.engraving.stylus */
    boolean firsttime = (svc.context.engraving.actionct == 0);
    int rate = 10; /* # characters that can be engraved in this action */
    boolean truncate = FALSE;
    boolean neweng = (svc.context.engraving.actionct == 0);

    boolean carving = (svc.context.engraving.type == ENGRAVE
                       || svc.context.engraving.type == HEADSTONE);
    boolean dulling_wep, marker;
    char *endc; /* points at character 1 beyond the last character to engrave
                 * this action */
    int i, space_left;

    if (svc.context.engraving.pos.x != u.ux
        || svc.context.engraving.pos.y != u.uy) { /* teleported? */
        You("不能够继续刻写.");
        return 0;
    }
    /* Stylus might have been taken out of inventory and destroyed somehow.
     * Not safe to dereference stylus until after this. */
    if (svc.context.engraving.stylus == &hands_obj) { /* bare finger */
        stylus = (struct obj *) 0;
    } else {
        for (stylus = gi.invent; stylus; stylus = stylus->nobj) {
            if (stylus == svc.context.engraving.stylus)
                break;
        }
        if (!stylus) {
            You("不能够继续刻写.");
            return 0;
        }
    }

    dulling_wep = (carving && stylus && stylus->oclass == WEAPON_CLASS
                   && (stylus->otyp != ATHAME || stylus->cursed));
    marker = (stylus && stylus->otyp == MAGIC_MARKER
              && svc.context.engraving.type == MARK);

    svc.context.engraving.actionct++;

    /* sanity checks */
    if (dulling_wep && !is_blade(stylus)) {
        impossible("carving with non-bladed weapon");
    } else if (svc.context.engraving.type == MARK && !marker) {
        impossible("making graffiti with non-marker stylus");
    }

    /* Step 1: Compute rate. */
    if (carving && stylus
        && (dulling_wep || stylus->oclass == RING_CLASS
            || stylus->oclass == GEM_CLASS)) {
        /* slow engraving methods */
        rate = 1;
    } else if (marker) {
        /* one charge / 2 letters */
        rate = min(rate, stylus->spe * 2);
    }

    /* Step 2: Compute last character that can be engraved this action. */
    i = rate;
    for (endc = svc.context.engraving.nextc; *endc && i > 0; endc++) {
        if (*endc != ' ') {
            i--;
        }
    }

    /* Step 3: affect stylus from engraving - it might wear out. */
    if (dulling_wep) {
        boolean splitstack = FALSE, dulled = FALSE;

        /* 'dulling_wep' guarantees that 'stylus' is a weapon which is
           not welded to the hero's hand(s) */
        if (stylus->quan > 1L) {
            if (firsttime)
                pline("一把%s变钝了.", yname(stylus));
            stylus = svc.context.engraving.stylus = splitobj(stylus, 1L);
            /* if stack is wielded or quivered, the split-off one isn't */
            stylus->owornmask = 0L;
            splitstack = TRUE;
        } else {
            /* normal case: stylus->quan==1 */
            if (firsttime)
                pline("%s变钝了.", Yname2(stylus));
        }
        /* Dull the weapon at a rate of -1 enchantment per 2 characters,
         * rounding down.
         * The number of characters obtainable given starting enchantment:
         * -2 => 3, -1 => 5, 0 => 7, +1 => 9, +2 => 11
         * Note: this does not allow a +0 anything (except an athame) to
         * engrave "Elbereth" all at once.
         * However, you can engrave "Elb", then "ere", then "th", by taking
         * advantage of the rounding down. */
        if (svc.context.engraving.actionct % 2 == 1) { /* 1st,3rd,... action */
            /* deduct a point on 1st, 3rd, 5th, ... turns, unless this is the
             * last character being engraved (a rather convoluted way to round
             * down), but always deduct a point on the 1st turn to prevent
             * zero-cost engravings.
             * Check for truncation *before* deducting a point - otherwise,
             * attempting to e.g. engrave 3 characters with a -2 weapon will
             * stop at the 1st. */
            if (stylus->spe <= -3) {
                if (firsttime) {
                    impossible("<= -3 weapon valid for engraving");
                }
                truncate = TRUE;
            } else if (*endc || svc.context.engraving.actionct == 1) {
                stylus->spe -= 1;
                dulled = TRUE;
            }
        }
        if (splitstack) {
            obj_extract_self(stylus);
            stylus = hold_another_object(stylus, "你丢下一个%s!",
                                         doname(stylus), (char *) NULL);
        } else if (dulled && stylus->known) {
            /* reflect change in stylus->spe; not needed for splitstack
               since hold_another_object() does this */
            prinv((char *) NULL, stylus, 1L);
            update_inventory();
        }
    } else if (marker) {
        int ink_cost = max(rate / 2, 1); /* Prevent infinite graffiti */

        if (stylus->spe < ink_cost) {
            impossible("overly dry marker valid for graffiti?");
            ink_cost = stylus->spe;
            truncate = TRUE;
        }
        stylus->spe -= ink_cost;
        update_inventory();
        if (stylus->spe == 0) {
            /* can't engrave any further; truncate the string */
            Your("marker dries out.");
            truncate = TRUE;
        }
    }

    switch (svc.context.engraving.type) {
    default:
        finishverb = "完成了你的奇怪的刻写";
        break;
    case DUST:
        finishverb = is_ice(u.ux, u.uy) ? "写进霜里"
                     : "写进灰尘里";
        break;
    case HEADSTONE:
    case ENGRAVE:
        finishverb = "刻字进地板";
        break;
    case BURN:
        finishverb = is_ice(u.ux, u.uy) ? "将信息熔入冰面"
                     : "将信息烧进地板";
        break;
    case MARK:
        finishverb = "对地牢乱涂乱画";
        break;
    case ENGR_BLOOD:
        finishverb = "在血里涂出字";
    }

    /* actions that happen at the end of every engraving action go here */

    buf[0] = '\0';
    oep = engr_at(u.ux, u.uy);
    if (oep) /* add to existing engraving */
        Strcpy(buf, oep->engr_txt[actual_text]);

    space_left = (int) (sizeof buf - strlen(buf) - 1U);
    if (endc - svc.context.engraving.nextc > space_left) {
        You("没有空间继续写了.");
        endc = svc.context.engraving.nextc + space_left;
        truncate = TRUE;
    }

    /* If the stylus did wear out mid-engraving, truncate the input so that we
     * can't go any further. */
    if (truncate && *endc != '\0') {
        *endc = '\0';
        You("只能写下\"%s\".", svc.context.engraving.text);
    } else {
        /* input was not truncated; stylus may still have worn out on the last
         * character, though */
        truncate = FALSE;
    }

    (void) strncat(buf, svc.context.engraving.nextc,
                   min(space_left, endc - svc.context.engraving.nextc));
    make_engr_at(u.ux, u.uy, buf, svm.moves - gm.multi,
                 svc.context.engraving.type);
    oep = engr_at(u.ux, u.uy);
    if (oep)
        oep->eread = 1;

    if (*endc) {
        svc.context.engraving.nextc = endc;
        if (neweng) {
            newsym(svc.context.engraving.pos.x, svc.context.engraving.pos.y);
        }
        return 1; /* not yet finished this turn */
    } else { /* finished engraving */
        /* actions that happen after the engraving is finished go here */

        if (truncate) {
            /* Now that "You are only able to write 'foo'" also prints at the
             * end of engraving, this might be redundant. */
            You("不能写更多了.");
        } else if (!firsttime) {
            /* only print this if engraving took multiple actions */
            You("成功%s.", finishverb);
        }
        svc.context.engraving.text[0] = '\0';
        svc.context.engraving.nextc = (char *) 0;
        svc.context.engraving.stylus = (struct obj *) 0;
    }
    if (neweng)
        newsym(svc.context.engraving.pos.x, svc.context.engraving.pos.y);
    return 0;
}

/* while loading bones, clean up text which might accidentally
   or maliciously disrupt player's terminal when displayed */
void
sanitize_engravings(void)
{
    struct engr *ep;

    for (ep = head_engr; ep; ep = ep->nxt_engr) {
        sanitize_name(ep->engr_txt[actual_text]);
    }
}

void
engraving_sanity_check(void)
{
    struct engr *ep;
    int levtyp;

    if (head_engr && (Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))) {
        impossible("engraving sanity: on plane of air/water");
        return;
    }

    for (ep = head_engr; ep; ep = ep->nxt_engr) {
        coordxy x = ep->engr_x, y = ep->engr_y;

        if (!isok(x, y)) {
            impossible("engraving sanity: !isok <%i,%i>", x, y);
            continue;
        }
        levtyp = SURFACE_AT(x, y);
        if (is_pool_or_lava(x, y) || IS_AIR(levtyp) || !ACCESSIBLE(levtyp)) {
            impossible("engraving sanity: illegal surface (%d: \"%s\")",
                       levtyp, surface(x, y));
            continue;
        }
    }
}

void
save_engravings(NHFILE *nhfp)
{
    struct engr *ep, *ep2;
    unsigned no_more_engr = 0;

    for (ep = head_engr; ep; ep = ep2) {
        ep2 = ep->nxt_engr;
        if (ep->engr_alloc
            && ep->engr_txt[actual_text][0] && perform_bwrite(nhfp)) {
            if (nhfp->structlevel) {
                bwrite(nhfp->fd, (genericptr_t) &(ep->engr_alloc),
                       sizeof ep->engr_alloc);
                bwrite(nhfp->fd, (genericptr_t) ep,
                       sizeof (struct engr) + ep->engr_alloc);
            }
        }
        if (release_data(nhfp))
            dealloc_engr(ep);
    }
    if (perform_bwrite(nhfp)) {
        if (nhfp->structlevel)
            bwrite(nhfp->fd, (genericptr_t) &no_more_engr,
                   sizeof no_more_engr);
    }
    if (release_data(nhfp))
        head_engr = 0;
}

void
rest_engravings(NHFILE *nhfp)
{
    struct engr *ep;
    unsigned lth = 0;

    head_engr = 0;
    while (1) {
        if (nhfp->structlevel)
            mread(nhfp->fd, (genericptr_t) &lth, sizeof (unsigned));

        if (lth == 0)
            return;
        ep = newengr(lth);
        if (nhfp->structlevel) {
            mread(nhfp->fd, (genericptr_t) ep, sizeof (struct engr) + lth);
        }
        ep->nxt_engr = head_engr;
        head_engr = ep;
        ep->engr_txt[actual_text] = (char *) (ep + 1); /* Andreas Bormann */
        ep->engr_txt[remembered_text] = ep->engr_txt[actual_text]
                                      + ep->engr_szeach;
        ep->engr_txt[pristine_text] = ep->engr_txt[remembered_text]
                                    + ep->engr_szeach;
        while (ep->engr_txt[actual_text][0] == ' ')
            ep->engr_txt[actual_text]++;
        while (ep->engr_txt[remembered_text][0] == ' ')
            ep->engr_txt[remembered_text]++;
        /* mark as finished for bones levels -- no problem for
         * normal levels as the player must have finished engraving
         * to be able to move again */
        ep->engr_time = svm.moves;
    }
}

DISABLE_WARNING_FORMAT_NONLITERAL

/* to support '#stats' wizard-mode command */
void
engr_stats(
    const char *hdrfmt,
    char *hdrbuf,
    long *count,
    long *size)
{
    struct engr *ep;

    Sprintf(hdrbuf, hdrfmt, (long) sizeof (struct engr));
    *count = *size = 0L;
    for (ep = head_engr; ep; ep = ep->nxt_engr) {
        ++*count;
        *size += (long) sizeof *ep + (long) ep->engr_alloc;
    }
}

RESTORE_WARNING_FORMAT_NONLITERAL

void
del_engr(struct engr *ep)
{
    if (ep == head_engr) {
        head_engr = ep->nxt_engr;
    } else {
        struct engr *ept;

        for (ept = head_engr; ept; ept = ept->nxt_engr)
            if (ept->nxt_engr == ep) {
                ept->nxt_engr = ep->nxt_engr;
                break;
            }
        if (!ept) {
            impossible("Error in del_engr?");
            return;
        }
    }
    dealloc_engr(ep);
}

/* randomly relocate an engraving */
void
rloc_engr(struct engr *ep)
{
    int tx, ty, tryct = 200;

    do {
        if (--tryct < 0)
            return;
        tx = rn1(COLNO - 3, 2);
        ty = rn2(ROWNO);
    } while (engr_at(tx, ty) || !goodpos(tx, ty, (struct monst *) 0, 0));

    ep->engr_x = tx;
    ep->engr_y = ty;
    newsym(tx, ty);  /* caller took care of the old location */
}

/* Create a headstone at the given location.
 * The caller is responsible for newsym(x, y).
 */
void
make_grave(coordxy x, coordxy y, const char *str)
{
    char buf[BUFSZ];

    /* Can we put a grave here? */
    if ((levl[x][y].typ != ROOM && levl[x][y].typ != GRAVE) || t_at(x, y))
        return;
    /* Make the grave */
    if (!set_levltyp(x, y, GRAVE))
        return;
    /* Engrave the headstone */
    del_engr_at(x, y);
    if (!str)
        str = get_rnd_text(EPITAPHFILE, buf, rn2, MD_PAD_RUMORS);
    make_engr_at(x, y, str, 0L, HEADSTONE);
    return;
}

/* called when kicking or engraving on a grave's headstone */
void
disturb_grave(coordxy x, coordxy y)
{
    struct rm *lev = &levl[x][y];

    if (!IS_GRAVE(lev->typ)) {
        impossible("Disturbing grave that isn't a grave? (%d)", lev->typ);
    } else if (lev->disturbed) {
        impossible("Disturbing already disturbed grave?");
    } else {
        You("disturb the undead!");
        lev->disturbed = 1;
        (void) makemon(&mons[PM_GHOUL], x, y, NO_MM_FLAGS);
        exercise(A_WIS, FALSE);
    }
}

void
see_engraving(struct engr *ep)
{
    newsym(ep->engr_x, ep->engr_y);
}

/* like see_engravings() but overrides vision, but
   only for some types of engravings that can be felt */
void
feel_engraving(struct engr *ep)
{
    ep->eread = 1;
    map_engraving(ep, 1);
    /* in case it's beneath something, redisplay the something */
    newsym(ep->engr_x, ep->engr_y);
}

static const char blind_writing[][21] = {
    {0x44, 0x66, 0x6d, 0x69, 0x62, 0x65, 0x22, 0x45, 0x7b, 0x71,
     0x65, 0x6d, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    {0x51, 0x67, 0x60, 0x7a, 0x7f, 0x21, 0x40, 0x71, 0x6b, 0x71,
     0x6f, 0x67, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x49, 0x6d, 0x73, 0x69, 0x62, 0x65, 0x22, 0x4c, 0x61, 0x7c,
     0x6d, 0x67, 0x24, 0x42, 0x7f, 0x69, 0x6c, 0x77, 0x67, 0x7e, 0x00},
    {0x4b, 0x6d, 0x6c, 0x66, 0x30, 0x4c, 0x6b, 0x68, 0x7c, 0x7f,
     0x6f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x51, 0x67, 0x70, 0x7a, 0x7f, 0x6f, 0x67, 0x68, 0x64, 0x71,
     0x21, 0x4f, 0x6b, 0x6d, 0x7e, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x4c, 0x63, 0x76, 0x61, 0x71, 0x21, 0x48, 0x6b, 0x7b, 0x75,
     0x67, 0x63, 0x24, 0x45, 0x65, 0x6b, 0x6b, 0x65, 0x00, 0x00, 0x00},
    {0x4c, 0x67, 0x68, 0x6b, 0x78, 0x68, 0x6d, 0x76, 0x7a, 0x75,
     0x21, 0x4f, 0x71, 0x7a, 0x75, 0x6f, 0x77, 0x00, 0x00, 0x00, 0x00},
    {0x44, 0x66, 0x6d, 0x7c, 0x78, 0x21, 0x50, 0x65, 0x66, 0x65,
     0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x44, 0x66, 0x73, 0x69, 0x62, 0x65, 0x22, 0x56, 0x7d, 0x63,
     0x69, 0x76, 0x6b, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

staticfn const char *
blengr(void)
{
    return ROLL_FROM(blind_writing);
}

/*engrave.c*/
