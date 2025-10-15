/* NetHack 3.7	fountain.c	$NHDT-Date: 1699582923 2023/11/10 02:22:03 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.100 $ */
/*      Copyright Scott R. Turner, srt@ucla, 10/27/86 */
/* NetHack may be freely redistributed.  See license for details. */

/* Code for drinking from fountains. */

#include "hack.h"

staticfn void dowatersnakes(void);
staticfn void dowaterdemon(void);
staticfn void dowaternymph(void);
staticfn void gush(coordxy, coordxy, genericptr_t) NONNULLARG3;
staticfn void dofindgem(void);
staticfn boolean watchman_warn_fountain(struct monst *) NONNULLARG1;

DISABLE_WARNING_FORMAT_NONLITERAL

/* used when trying to dip in or drink from fountain or sink or pool while
   levitating above it, or when trying to move downwards in that state */
void
floating_above(const char *what)
{
    const char *umsg = "高高地飘浮在%s上空.";

    if (u.utrap && (u.utraptype == TT_INFLOOR || u.utraptype == TT_LAVA)) {
        /* when stuck in floor (not possible at fountain or sink location,
           so must be attempting to move down), override the usual message */
        umsg = "被困在%s里.";
        what = surface(u.ux, u.uy); /* probably redundant */
    }
    You(umsg, what);
}

RESTORE_WARNING_FORMAT_NONLITERAL

/* Fountain of snakes! */
staticfn void
dowatersnakes(void)
{
    int num = rn1(5, 2);
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_MOCCASIN].mvflags & G_GONE)) {
        if (!Blind) {
            pline("络绎不绝的%s涌流出来!",
                  Hallucination ? makeplural(rndmonnam(NULL)) : "蛇");
        } else {
            Soundeffect(se_snakes_hissing, 75);
            You_hear("%s 发出嘶嘶声!", something);
        }
        while (num-- > 0)
            if ((mtmp = makemon(&mons[PM_WATER_MOCCASIN], u.ux, u.uy,
                                MM_NOMSG)) != 0
                && t_at(mtmp->mx, mtmp->my))
                (void) mintrap(mtmp, NO_TRAP_FLAGS);
    } else {
        Soundeffect(se_furious_bubbling, 20);
        pline_The("喷泉剧烈地冒泡, 一小会后平静了.");
    }
}

/* Water demon */
staticfn void
dowaterdemon(void)
{
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_DEMON].mvflags & G_GONE)) {
        if ((mtmp = makemon(&mons[PM_WATER_DEMON], u.ux, u.uy,
                            MM_NOMSG)) != 0) {
            if (!Blind)
                You("解放了%s!", a_monnam(mtmp));
            else
                You_feel("到邪恶的存在.");

            /* Give those on low levels a (slightly) better chance of survival
             */
            if (rnd(100) > (80 + level_difficulty())) {
                pline("感激于%s解放, %s满足你一个愿望!",
                      mhis(mtmp), mhe(mtmp));
                /* give a wish and discard the monster (mtmp set to null) */
                mongrantswish(&mtmp);
            } else if (t_at(mtmp->mx, mtmp->my))
                (void) mintrap(mtmp, NO_TRAP_FLAGS);
        }
    } else {
        Soundeffect(se_furious_bubbling, 20);
        pline_The("喷泉片刻剧烈地冒泡, 然后平静了.");
    }
}

/* Water Nymph */
staticfn void
dowaternymph(void)
{
    struct monst *mtmp;

    if (!(svm.mvitals[PM_WATER_NYMPH].mvflags & G_GONE)
        && (mtmp = makemon(&mons[PM_WATER_NYMPH], u.ux, u.uy,
                           MM_NOMSG)) != 0) {
        if (!Blind)
            You("吸引了%s!", a_monnam(mtmp));
        else
            You_hear("一个性感的声音.");
        mtmp->msleeping = 0;
        if (t_at(mtmp->mx, mtmp->my))
            (void) mintrap(mtmp, NO_TRAP_FLAGS);
    } else if (!Blind) {
        Soundeffect(se_bubble_rising, 50);
        Soundeffect(se_loud_pop, 50);
        pline("一个大气泡上升到了水面然后破掉了.");
    } else {
        Soundeffect(se_loud_pop, 50);
        You_hear("啵的一声.");
    }
}

/* Gushing forth along LOS from (u.ux, u.uy) */
void
dogushforth(int drinking)
{
    int madepool = 0;

    do_clear_area(u.ux, u.uy, 7, gush, (genericptr_t) &madepool);
    if (!madepool) {
        if (drinking)
            Your("口渴缓和了.");
        else
            pline("水洒到你全身.");
    }
}

staticfn void
gush(coordxy x, coordxy y, genericptr_t poolcnt)
{
    struct monst *mtmp;
    struct trap *ttmp;

    if (((x + y) % 2) || u_at(x, y)
        || (rn2(1 + distmin(u.ux, u.uy, x, y))) || (levl[x][y].typ != ROOM)
        || (sobj_at(BOULDER, x, y)) || nexttodoor(x, y))
        return;

    if ((ttmp = t_at(x, y)) != 0 && !delfloortrap(ttmp))
        return;

    if (!((*(int *) poolcnt)++))
        pline("水从满溢的喷泉里喷出!");

    /* Put a pool at x, y */
    set_levltyp(x, y, POOL);
    levl[x][y].flags = 0;
    /* No kelp! */
    del_engr_at(x, y);
    water_damage_chain(svl.level.objects[x][y], TRUE);

    if ((mtmp = m_at(x, y)) != 0)
        (void) minliquid(mtmp);
    else
        newsym(x, y);
}

/* Find a gem in the sparkling waters. */
staticfn void
dofindgem(void)
{
    if (!Blind)
        You("在水的粼粼波光中发现了一颗宝石!");
    else
        You_feel("这里有一颗宝石!");
    (void) mksobj_at(rnd_class(DILITHIUM_CRYSTAL, LUCKSTONE - 1), u.ux, u.uy,
                     FALSE, FALSE);
    SET_FOUNTAIN_LOOTED(u.ux, u.uy);
    newsym(u.ux, u.uy);
    exercise(A_WIS, TRUE); /* a discovery! */
}

staticfn boolean
watchman_warn_fountain(struct monst *mtmp)
{
    if (is_watch(mtmp->data) && couldsee(mtmp->mx, mtmp->my)
        && mtmp->mpeaceful) {
        if (!Deaf) {
            pline("%s 大叫:", Amonnam(mtmp));
            verbalize("喂, 停止使用那个喷泉!");
        } else {
            pline("%s诚挚地%s%s%s!",
                  Amonnam(mtmp),
                  nolimbs(mtmp->data) ? "摇" : "挥舞",
                  mhis(mtmp),
                  nolimbs(mtmp->data)
                  ? mbodypart(mtmp, HEAD)
                  : makeplural(mbodypart(mtmp, ARM)));
        }
        return TRUE;
    }
    return FALSE;
}

void
dryup(coordxy x, coordxy y, boolean isyou)
{
    if (IS_FOUNTAIN(levl[x][y].typ)
        && (!rn2(3) || FOUNTAIN_IS_WARNED(x, y))) {
        if (isyou && in_town(x, y) && !FOUNTAIN_IS_WARNED(x, y)) {
            struct monst *mtmp;

            SET_FOUNTAIN_WARNED(x, y);
            /* Warn about future fountain use. */
            mtmp = get_iter_mons(watchman_warn_fountain);
            /* You can see or hear this effect */
            if (!mtmp)
                pline_The("涌流减少为涓涓细流.");
            return;
        }
        if (isyou && wizard) {
            if (yn("让喷泉干涸?") == 'n')
                return;
        }
        /* FIXME: sight-blocking clouds should use block_point() when
           being created and unblock_point() when going away, then this
           glyph hackery wouldn't be necessary */
        if (cansee(x, y)) {
            int glyph = glyph_at(x, y);

            if (!glyph_is_cmap(glyph) || glyph_to_cmap(glyph) != S_cloud)
                pline_The("喷泉干涸了!");
        }
        /* replace the fountain with ordinary floor */
        set_levltyp(x, y, ROOM); /* updates level.flags.nfountains */
        levl[x][y].flags = 0;
        levl[x][y].blessedftn = 0;
        /* The location is seen if the hero/monster is invisible
           or felt if the hero is blind. */
        newsym(x, y);
        if (isyou && in_town(x, y))
            (void) angry_guards(FALSE);
    }
}

/* quaff from a fountain when standing on its location */
void
drinkfountain(void)
{
    /* What happens when you drink from a fountain? */
    boolean mgkftn = (levl[u.ux][u.uy].blessedftn == 1);
    int fate = rnd(30);

    if (Levitation) {
        floating_above("fountain");
        return;
    }

    if (mgkftn && u.uluck >= 0 && fate >= 10) {
        int i, ii, littleluck = (u.uluck < 4);

        pline("哇!  这让你感觉很棒!");
        /* blessed restore ability */
        for (ii = 0; ii < A_MAX; ii++)
            if (ABASE(ii) < AMAX(ii)) {
                ABASE(ii) = AMAX(ii);
                disp.botl = TRUE;
            }
        /* gain ability, blessed if "natural" luck is high */
        i = rn2(A_MAX); /* start at a random attribute */
        for (ii = 0; ii < A_MAX; ii++) {
            if (adjattrib(i, 1, littleluck ? -1 : 0) && littleluck)
                break;
            if (++i >= A_MAX)
                i = 0;
        }
        display_nhwindow(WIN_MESSAGE, FALSE);
        pline("一缕蒸汽从喷泉中逸出...");
        exercise(A_WIS, TRUE);
        levl[u.ux][u.uy].blessedftn = 0;
        return;
    }

    if (fate < 10) {
        pline_The("凉爽的让你神清气爽.");
        u.uhunger += rnd(10); /* don't choke on water */
        newuhs(FALSE);
        if (mgkftn)
            return;
    } else {
        switch (fate) {
        case 19: /* Self-knowledge */
            You_feel("自知的...");
            display_nhwindow(WIN_MESSAGE, FALSE);
            enlightenment(MAGICENLIGHTENMENT, ENL_GAMEINPROGRESS);
            exercise(A_WIS, TRUE);
            pline_The("感觉消退了.");
            break;
        case 20: /* Foul water */
            pline_The("水很脏!  你恶心并呕吐.");
            morehungry(rn1(20, 11));
            vomit();
            break;
        case 21: /* Poisonous */
            pline_The("水被污染了!");
            if (Poison_resistance) {
                pline("也许它是从附近的%s农场流过来的.",
                      fruitname(FALSE));
                losehp(rnd(4), "未冷藏的果汁", KILLED_BY_AN);
                break;
            }
            poison_strdmg(rn1(4, 3), rnd(10), "被污染的水",
                          KILLED_BY);
            exercise(A_CON, FALSE);
            break;
        case 22: /* Fountain of snakes! */
            dowatersnakes();
            break;
        case 23: /* Water demon */
            dowaterdemon();
            break;
        case 24: { /* Maybe curse some items */
            struct obj *obj;
            int buc_changed = 0;

            pline("这个水不好!");
            morehungry(rn1(20, 11));
            exercise(A_CON, FALSE);
            /* this is more severe than rndcurse() */
            for (obj = gi.invent; obj; obj = obj->nobj)
                if (obj->oclass != COIN_CLASS && !obj->cursed && !rn2(5)) {
                    curse(obj);
                    ++buc_changed;
                }
            if (buc_changed)
                update_inventory();
            break;
        }
        case 25: /* See invisible */
            if (Blind) {
                if (Invisible) {
                    You("感觉自己是透明的.");
                } else {
                    You("感觉能意识到自己的存在.");
                    pline("然后这种感觉消失了.");
                }
            } else {
                You_see("某人的影像在偷偷接近你.");
                pline("但它消失了.");
            }
            HSee_invisible |= FROMOUTSIDE;
            newsym(u.ux, u.uy);
            exercise(A_WIS, TRUE);
            break;
        case 26: /* See Monsters */
            if (monster_detect((struct obj *) 0, 0))
                pline_The("%s 尝起来像是什么也没有.", hliquid("water"));
            exercise(A_WIS, TRUE);
            break;
        case 27: /* Find a gem in the sparkling waters. */
            if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
                dofindgem();
                break;
            }
            /*FALLTHRU*/
        case 28: /* Water Nymph */
            dowaternymph();
            break;
        case 29: /* Scare */
        {
            struct monst *mtmp;

            pline("这%s让你口臭!",
                  hliquid("水"));
            for (mtmp = fmon; mtmp; mtmp = mtmp->nmon) {
                if (DEADMONSTER(mtmp))
                    continue;
                monflee(mtmp, 0, FALSE, FALSE);
            }
            break;
        }
        case 30: /* Gushing forth in this room */
            dogushforth(TRUE);
            break;
        default:
            pline("这温热的%s没什么味道.",
                  hliquid("水"));
            break;
        }
    }
    dryup(u.ux, u.uy, TRUE);
}

/* dip an object into a fountain when standing on its location */
void
dipfountain(struct obj *obj)
{
    int er = ER_NOTHING;
    boolean is_hands = (obj == &hands_obj);

    if (Levitation) {
        floating_above("喷泉");
        return;
    }

    if (obj->otyp == LONG_SWORD && u.ulevel >= 5
        && !rn2(Role_if(PM_KNIGHT) ? 6 : 30)
        /* once upon a time it was possible to poly N daggers into N swords */
        && obj->quan == 1L && !obj->oartifact
        && !exist_artifact(LONG_SWORD, artiname(ART_EXCALIBUR))) {
        static const char lady[] = "Lady of the Lake";

        if (u.ualign.type != A_LAWFUL) {
            /* Ha!  Trying to cheat her. */
            pline("冰冷的薄雾从%s中升起并包裹住了剑.",
                  hliquid("水"));
            pline_The("喷泉消失了!");
            curse(obj);
            if (obj->spe > -6 && !rn2(3))
                obj->spe--;
            obj->oerodeproof = FALSE;
            exercise(A_WIS, FALSE);
            livelog_printf(LL_ARTIFACT,
                           "无权成为%s!  那位%s认为%s配不上.",
                           artiname(ART_EXCALIBUR), lady, uhim());
        } else {
            /* The lady of the lake acts! - Eric Backus */
            /* Be *REAL* nice */
            pline(
              "从黑暗深处, 一只手伸上来祝福那把剑.");
            pline("当手撤离的时候, 喷泉消失了!");
            obj = oname(obj, artiname(ART_EXCALIBUR),
                        ONAME_VIA_DIP | ONAME_KNOW_ARTI);
            discover_artifact(ART_EXCALIBUR);
            bless(obj);
            obj->oeroded = obj->oeroded2 = 0;
            obj->oerodeproof = TRUE;
            exercise(A_WIS, TRUE);
            livelog_printf(LL_ARTIFACT, "被冠名%s因为%s的祝福",
                           artiname(ART_EXCALIBUR), lady);
        }
        update_inventory();
        set_levltyp(u.ux, u.uy, ROOM); /* updates level.flags.nfountains */
        levl[u.ux][u.uy].flags = 0;
        newsym(u.ux, u.uy);
        if (in_town(u.ux, u.uy))
            (void) angry_guards(FALSE);
        return;
    } else if (is_hands || obj == uarmg) {
        er = wash_hands();
    } else {
        er = water_damage(obj, NULL, TRUE);
    }

    if (er == ER_DESTROYED || (er != ER_NOTHING && !rn2(2))) {
        return; /* no further effect */
    }

    switch (rnd(30)) {
    case 16: /* Curse the item */
        if (!is_hands && obj->oclass != COIN_CLASS && !obj->cursed) {
            curse(obj);
        }
        break;
    case 17:
    case 18:
    case 19:
    case 20: /* Uncurse the item */
        if (!is_hands && obj->cursed) {
            if (!Blind)
                pline_The("%s发光了片刻.", hliquid("水"));
            uncurse(obj);
        } else {
            pline("一种失落感来袭.");
        }
        break;
    case 21: /* Water Demon */
        dowaterdemon();
        break;
    case 22: /* Water Nymph */
        dowaternymph();
        break;
    case 23: /* an Endless Stream of Snakes */
        dowatersnakes();
        break;
    case 24: /* Find a gem */
        if (!FOUNTAIN_IS_LOOTED(u.ux, u.uy)) {
            dofindgem();
            break;
        }
        /*FALLTHRU*/
    case 25: /* Water gushes forth */
        dogushforth(FALSE);
        break;
    case 26: /* Strange feeling */
        pline("一种奇怪的刺痛感沿着你的%s爬上脊背.", body_part(ARM));
        break;
    case 27: /* Strange feeling */
        You_feel("突然的寒意.");
        break;
    case 28: /* Strange feeling */
        pline("想要洗澡的冲动淹没了你.");
        {
            long money = money_cnt(gi.invent);
            struct obj *otmp;

            if (money > 10) {
                /* Amount to lose.  Might get rounded up as fountains don't
                 * pay change... */
                money = somegold(money) / 10;
                for (otmp = gi.invent; otmp && money > 0; otmp = otmp->nobj)
                    if (otmp->oclass == COIN_CLASS) {
                        int denomination = objects[otmp->otyp].oc_cost;
                        long coin_loss =
                            (money + denomination - 1) / denomination;
                        coin_loss = min(coin_loss, otmp->quan);
                        otmp->quan -= coin_loss;
                        money -= coin_loss * denomination;
                        if (!otmp->quan)
                            delobj(otmp);
                    }
                You("在喷泉中丢失了一些钱!");
                CLEAR_FOUNTAIN_LOOTED(u.ux, u.uy);
                exercise(A_WIS, FALSE);
            }
        }
        break;
    case 29: /* You see coins */
        /* We make fountains have more coins the closer you are to the
         * surface.  After all, there will have been more people going
         * by.  Just like a shopping mall!  Chris Woodbury  */

        if (FOUNTAIN_IS_LOOTED(u.ux, u.uy))
            break;
        SET_FOUNTAIN_LOOTED(u.ux, u.uy);
        (void) mkgold((long) (rnd((dunlevs_in_dungeon(&u.uz) - dunlev(&u.uz)
                                   + 1) * 2) + 5),
                      u.ux, u.uy);
        if (!Blind)
            pline("下面远处, 你看见金币在%s里闪闪发光.",
                  hliquid("水"));
        exercise(A_WIS, TRUE);
        newsym(u.ux, u.uy);
        break;
    default:
        if (er == ER_NOTHING)
            pline1(nothing_seems_to_happen);
        break;
    }
    update_inventory();
    dryup(u.ux, u.uy, TRUE);
}

/* dipping '-' in fountain, pool, or sink */
int
wash_hands(void)
{
    const char *hands = makeplural(body_part(HAND));
    int res = ER_NOTHING;
    boolean was_glib = !!Glib;

    You("在%s里面洗你的%s%s ",hliquid("water"), uarmg ? "戴着手套的 " : "", hands);
    if (Glib) {
        make_glib(0);
        Your("%s现在不滑了.", fingers_or_gloves(TRUE));
    }
    if (uarmg)
        res = water_damage(uarmg, (const char *) 0, TRUE);
    /* not what ER_GREASED is for, but the checks in dipfountain just
       compare the result to ER_DESTROYED and ER_NOTHING, so it works */
    if (was_glib && res == ER_NOTHING)
        res = ER_GREASED;
    return res;
}

/* convert a sink into a fountain */
void
breaksink(coordxy x, coordxy y)
{
    if (cansee(x, y) || u_at(x, y))
        pline_The("水管破裂!  水喷出来了!");
    /* updates level.flags.nsinks and level.flags.nfountains */
    set_levltyp(x, y, FOUNTAIN);
    levl[x][y].looted = 0;
    levl[x][y].blessedftn = 0;
    SET_FOUNTAIN_LOOTED(x, y);
    newsym(x, y);
}

/* quaff from a sink while standing on its location */
void
drinksink(void)
{
    struct obj *otmp;
    struct monst *mtmp;

    if (Levitation) {
        floating_above("水槽");
        return;
    }
    switch (rn2(20)) {
    case 0:
        You("喝了一小口非常冷的%s.", hliquid("水"));
        break;
    case 1:
        You("喝了一小口非常温暖的%s.", hliquid("水"));
        break;
    case 2:
        You("喝了一小口滚烫的%s.", hliquid("water"));
        if (Fire_resistance) {
            pline("这似乎相当可口.");
            monstseesu(M_SEEN_FIRE);
        } else {
            losehp(rnd(6), "喝沸腾的开水", KILLED_BY);
            monstunseesu(M_SEEN_FIRE);
        }
        /* boiling water burns considered fire damage */
        break;
    case 3:
        if (svm.mvitals[PM_SEWER_RAT].mvflags & G_GONE)
            pline_The("水槽似乎相当肮脏.");
        else {
            mtmp = makemon(&mons[PM_SEWER_RAT], u.ux, u.uy, MM_NOMSG);
            if (mtmp)
                pline("呀!  那里有%s在水槽里!",
                      (Blind || !canspotmon(mtmp)) ? "蠕动的什么东西"
                                                   : a_monnam(mtmp));
        }
        break;
    case 4:
        for (;;) {
            otmp = mkobj(POTION_CLASS, FALSE);
            if (otmp->otyp != POT_WATER)
                break;
            /* reject water and try again */
            obfree(otmp, (struct obj *) 0);
        }
        otmp->cursed = otmp->blessed = 0;
        pline("一些%s 液体从水龙头流出.",
              Blind ? "古怪的" : hcolor(OBJ_DESCR(objects[otmp->otyp])));
        otmp->dknown = !(Blind || Hallucination);
        otmp->quan++;       /* Avoid panic upon useup() */
        otmp->fromsink = 1; /* kludge for docall() */
        (void) dopotion(otmp);
        obfree(otmp, (struct obj *) 0);
        break;
    case 5:
        if (!(levl[u.ux][u.uy].looted & S_LRING)) {
            You("在水槽里找到一枚戒指!");
            (void) mkobj_at(RING_CLASS, u.ux, u.uy, TRUE);
            levl[u.ux][u.uy].looted |= S_LRING;
            exercise(A_WIS, TRUE);
            newsym(u.ux, u.uy);
        } else
            pline("一些肮脏的%s淤积到下水道中.", hliquid("水"));
        break;
    case 6:
        breaksink(u.ux, u.uy);
        break;
    case 7:
        pline_The("%s就像是有着自己的意志一样移动了起来!", hliquid("water"));
        if ((svm.mvitals[PM_WATER_ELEMENTAL].mvflags & G_GONE)
            || !makemon(&mons[PM_WATER_ELEMENTAL], u.ux, u.uy, MM_NOMSG))
            pline("不过它安静下来了.");
        break;
    case 8:
        pline("恶心, 这%s味道很糟.", hliquid("水"));
        more_experienced(1, 0);
        newexplevel();
        break;
    case 9:
        pline("呕...  这尝起来像下水道里的污水!  你呕吐了.");
        morehungry(rn1(30 - ACURR(A_CON), 11));
        vomit();
        break;
    case 10:
        pline("这%s含有有毒废物!", hliquid("水"));
        if (!Unchanging) {
            You("经历奇特的变形!");
            polyself(POLY_NOFLAGS);
        }
        break;
    /* more odd messages --JJB */
    case 11:
        Soundeffect(se_clanking_pipe, 50);
        You_hear("水管里传来的叮当声...");
        break;
    case 12:
        Soundeffect(se_sewer_song, 100);
        You_hear("下水道里的片段歌声...");
        break;
    case 13:
        pline("Ew, what a stench!");
        create_gas_cloud(u.ux, u.uy, 1, 4);
        break;
    case 19:
        if (Hallucination) {
            pline("从黑暗深处, 一只手伸上来... --哎呦！--");
            losehp(rnd(6), "幻觉中击打你的手", KILLED_BY);
            break;
        }
        /*FALLTHRU*/
    default:
        You("喝了一小口%s%s.",
            rn2(3) ? (rn2(2) ? "冷" : "温") : "热",
            hliquid("水"));
    }
}

/* for #dip(potion.c) when standing on a sink */
void
dipsink(struct obj *obj)
{
    boolean try_call = FALSE,
            not_looted_yet = (levl[u.ux][u.uy].looted & S_LRING) == 0,
            is_hands = (obj == &hands_obj || (uarmg && obj == uarmg));

    if (!rn2(not_looted_yet ? 25 : 15)) {
        /* can't rely on using sink for unlimited scroll blanking; however,
           since sink will be converted into a fountain, hero can dip again */
        breaksink(u.ux, u.uy); /* "The pipes break!  Water spurts out!" */
        if (Glib && is_hands)
            Your("%s仍然很光滑.", fingers_or_gloves(TRUE));
        return;
    } else if (is_hands) {
        (void) wash_hands();
        return;
    } else if (obj->oclass != POTION_CLASS) {
        You("将 %s放在水龙头底下.", the(xname(obj)));
        if (water_damage(obj, (const char *) 0, TRUE) == ER_NOTHING)
            pline1(nothing_seems_to_happen);
        return;
    }

    /* at this point the object must be a potion */
    You("将%s%s倒入下水道.", (obj->quan > 1L ? "一个 " : ""),
        the(xname(obj)));
    switch (obj->otyp) {
    case POT_POLYMORPH:
        polymorph_sink();
        try_call = TRUE;
        break;
    case POT_OIL:
        if (!Blind) {
            pline("这在水槽中留下了一个油性的涂层.");
            try_call = TRUE;
        } else {
            pline1(nothing_seems_to_happen);
        }
        break;
    case POT_ACID:
        /* acts like a drain cleaner product */
        try_call = TRUE;
        if (!Blind) {
            pline_The("下水道似乎没有那么堵塞的.");
        } else if (!Deaf) {
            You_hear("哗啦啦的声音.");
        } else {
            pline1(nothing_seems_to_happen);
            try_call = FALSE;
        }
        break;
    case POT_LEVITATION:
        sink_backs_up(u.ux, u.uy);
        try_call = TRUE;
        break;
    case POT_OBJECT_DETECTION:
        if (!(levl[u.ux][u.uy].looted & S_LRING)) {
            You("感觉到有一枚戒指被谁丢在水槽里.");
            try_call = TRUE;
            break;
        }
        /* FALLTHRU */
    case POT_GAIN_LEVEL:
    case POT_GAIN_ENERGY:
    case POT_MONSTER_DETECTION:
    case POT_FRUIT_JUICE:
    case POT_WATER:
        /* potions with no potionbreathe() effects, plus water.  if effects
           are added to potionbreathe these should go to that instead (except
           for water). */
        pline1(nothing_seems_to_happen);
        break;
    default:
        /* hero can feel the vapor on her skin, so no need to check Blind or
           breathless for this message */
        pline("一缕蒸汽逸散出来...");
        /* NB: potionbreathe calls trycall or makeknown as appropriate */
        if (!breathless(gy.youmonst.data) || haseyes(gy.youmonst.data))
            potionbreathe(obj);
        break;
    }
    if (try_call && obj->dknown)
        trycall(obj);
    useup(obj);
}

/* find a ring in a sink */
void
sink_backs_up(coordxy x, coordxy y)
{
    char buf[BUFSZ];

    if (!Blind)
        Strcpy(buf, "泥泞的垃圾从下水道涌出");
    else if (!Deaf)
        Strcpy(buf, "你听见一阵泥泞的哗啦声"); /* Deaf-aware */
    else
        Sprintf(buf, "有什么东西糊在了你的%s上", body_part(FACE));
    pline("%s%s.", !Deaf ? "噗啦!  " : "", buf);

    if (!(levl[x][y].looted & S_LRING)) { /* once per sink */
        if (!Blind)
            You_see("一枚戒指在水槽的中间发光.");
        (void) mkobj_at(RING_CLASS, x, y, TRUE);
        newsym(x, y);
        exercise(A_DEX, TRUE);
        exercise(A_WIS, TRUE); /* a discovery! */
        levl[x][y].looted |= S_LRING;
    }
}

/*fountain.c*/
