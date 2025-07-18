/* NetHack 3.7	potion.c	$NHDT-Date: 1737605675 2025/01/22 20:14:35 $  $NHDT-Branch: NetHack-3.7 $:$NHDT-Revision: 1.274 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

staticfn long itimeout(long);
staticfn long itimeout_incr(long, int);
staticfn void ghost_from_bottle(void);
staticfn int drink_ok(struct obj *);
staticfn void peffect_restore_ability(struct obj *);
staticfn void peffect_hallucination(struct obj *);
staticfn void peffect_water(struct obj *);
staticfn void peffect_booze(struct obj *);
staticfn void peffect_enlightenment(struct obj *);
staticfn void peffect_invisibility(struct obj *);
staticfn void peffect_see_invisible(struct obj *);
staticfn void peffect_paralysis(struct obj *);
staticfn void peffect_sleeping(struct obj *);
staticfn int peffect_monster_detection(struct obj *);
staticfn int peffect_object_detection(struct obj *);
staticfn void peffect_sickness(struct obj *);
staticfn void peffect_confusion(struct obj *);
staticfn void peffect_gain_ability(struct obj *);
staticfn void peffect_speed(struct obj *);
staticfn void peffect_blindness(struct obj *);
staticfn void peffect_gain_level(struct obj *);
staticfn void peffect_healing(struct obj *);
staticfn void peffect_extra_healing(struct obj *);
staticfn void peffect_full_healing(struct obj *);
staticfn void peffect_levitation(struct obj *);
staticfn void peffect_gain_energy(struct obj *);
staticfn void peffect_oil(struct obj *);
staticfn void peffect_acid(struct obj *);
staticfn void peffect_polymorph(struct obj *);
staticfn boolean H2Opotion_dip(struct obj *, struct obj *, boolean,
                             const char *);
staticfn short mixtype(struct obj *, struct obj *);
staticfn int dip_ok(struct obj *);
staticfn int dip_hands_ok(struct obj *);
staticfn void hold_potion(struct obj *, const char *, const char *,
                        const char *);
staticfn void poof(struct obj *);
staticfn int potion_dip(struct obj *obj, struct obj *potion);

/* used to indicate whether quaff or dip has skipped an opportunity to
   use a fountain or such, in order to vary the feedback if hero lacks
   any potions [reinitialized every time it's used so does not need to
   be placed in struct instance_globals gd] */
static int drink_ok_extra = 0;

/* force `val' to be within valid range for intrinsic timeout value */
staticfn long
itimeout(long val)
{
    if (val >= TIMEOUT)
        val = TIMEOUT;
    else if (val < 1L)
        val = 0L;

    return val;
}

/* increment `old' by `incr' and force result to be valid intrinsic timeout */
staticfn long
itimeout_incr(long old, int incr)
{
    return itimeout((old & TIMEOUT) + (long) incr);
}

/* set the timeout field of intrinsic `which' */
void
set_itimeout(long *which, long val)
{
    *which &= ~TIMEOUT;
    *which |= itimeout(val);
}

/* increment the timeout field of intrinsic `which' */
void
incr_itimeout(long *which, int incr)
{
    set_itimeout(which, itimeout_incr(*which, incr));
}

void
make_confused(long xtime, boolean talk)
{
    long old = HConfusion;

    if (Unaware)
        talk = FALSE;

    if (!xtime && old) {
        if (talk)
            You_feel("less %s now.", Hallucination ? "trippy" : "confused");
    }
    if ((xtime && !old) || (!xtime && old))
        disp.botl = TRUE;

    set_itimeout(&HConfusion, xtime);
}

void
make_stunned(long xtime, boolean talk)
{
    long old = HStun;

    if (Unaware)
        talk = FALSE;

    if (!xtime && old) {
        if (talk)
            You_feel("%s now.",
                     Hallucination ? "less wobbly" : "a bit steadier");
    }
    if (xtime && !old) {
        if (talk) {
            if (u.usteed)
                You("wobble in the saddle.");
            else
                You("%s...", stagger(gy.youmonst.data, "stagger"));
        }
    }
    if ((!xtime && old) || (xtime && !old))
        disp.botl = TRUE;

    set_itimeout(&HStun, xtime);
}

/* Sick is overloaded with both fatal illness and food poisoning (via
   u.usick_type bit mask), but delayed killer can only support one or
   the other at a time.  They should become separate intrinsics.... */
void
make_sick(long xtime,
          const char *cause, /* sickness cause */
          boolean talk,
          int type)
{
    struct kinfo *kptr;
    long old = Sick;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        talk = FALSE;
#endif
    if (xtime > 0L) {
        int copperarmor = 0;
        struct obj* otmp;
        if (Sick_resistance)
            return;

        /* Copper's anti-microbial properties make it effective in warding off
         * sickness. */
        for (otmp = gi.invent; otmp; otmp = otmp->nobj) {
            if ((otmp->owornmask & W_ARMOR) && otmp->material == COPPER) {
                copperarmor++;
            }
        }
        if (rn2(5) < copperarmor) {
            /* practially, someone could have copper helm, boots, body armor,
             * shield, gloves. If they're *all* copper, you're immune to
             * sickness. */
            You_feel("briefly ill.");
            return;
        }

        if (!old) {
            /* newly sick */
            You_feel("deathly sick.");
        } else {
            /* already sick */
            if (talk)
                You_feel("%s worse.", xtime <= Sick / 2L ? "much" : "even");
        }
        set_itimeout(&Sick, fiend_adversity(PM_BAALZEBUB) ? (xtime + 1) / 2
                                                          : xtime);
        u.usick_type |= type;
        disp.botl = TRUE;
    } else if (old && (type & u.usick_type)) {
        /* was sick, now not */
        u.usick_type &= ~type;
        if (u.usick_type) { /* only partly cured */
            if (talk)
                You_feel("somewhat better.");
            set_itimeout(&Sick, Sick * 2); /* approximation */
        } else {
            if (talk)
                You_feel("cured.  What a relief!");
            Sick = 0L; /* set_itimeout(&Sick, 0L) */
        }
        disp.botl = TRUE;
    }

    kptr = find_delayed_killer(SICK);
    if (Sick) {
        exercise(A_CON, FALSE);
        /* setting delayed_killer used to be unconditional, but that's
           not right when make_sick(0) is called to cure food poisoning
           if hero was also fatally ill; this is only approximate */
        if (xtime || !old || !kptr) {
            int kpfx = ((cause && !strcmp(cause, "#wizintrinsic"))
                        ? KILLED_BY : KILLED_BY_AN);

            delayed_killer(SICK, kpfx, cause);
        }
    } else
        dealloc_killer(kptr);
}

void
make_slimed(long xtime, const char *msg)
{
    long old = Slimed;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        msg = 0;
#endif
    set_itimeout(&Slimed, xtime);
    if ((xtime != 0L) ^ (old != 0L)) {
        disp.botl = TRUE;
        if (msg)
            pline("%s", msg);
    }
    if (!Slimed) {
        dealloc_killer(find_delayed_killer(SLIMED));
        /* fake appearance is set late in turn-to-slime countdown */
        if (U_AP_TYPE == M_AP_MONSTER
            && gy.youmonst.mappearance == PM_GREEN_SLIME) {
            gy.youmonst.m_ap_type = M_AP_NOTHING;
            gy.youmonst.mappearance = 0;
        }
    }
}

/* start or stop petrification */
void
make_stoned(long xtime, const char *msg, int killedby, const char *killername)
{
    long old = Stoned;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        msg = 0;
#endif
    set_itimeout(&Stoned, xtime);
    if ((xtime != 0L) ^ (old != 0L)) {
        disp.botl = TRUE;
        if (msg)
            pline("%s", msg);
    }
    if (!Stoned)
        dealloc_killer(find_delayed_killer(STONED));
    else if (!old)
        delayed_killer(STONED, killedby, killername);
}

void
make_vomiting(long xtime, boolean talk)
{
    long old = Vomiting;

    if (Unaware)
        talk = FALSE;

    set_itimeout(&Vomiting, xtime);
    disp.botl = TRUE;
    if (!xtime && old)
        if (talk)
            You_feel("much less nauseated now.");
}

static const char vismsg[] = "vision seems to %s for a moment but is %s now.";
static const char eyemsg[] = "%s momentarily %s.";

void
make_blinded(long xtime, boolean talk)
{
    long old = BlindedTimeout;
    boolean u_could_see, can_see_now;
    const char *eyes;

    /* we probe ahead in case the Eyes of the Overworld
       are or will be overriding blindness */
    u_could_see = !Blind;
    set_itimeout(&HBlinded, xtime ? 1L : 0L);
    can_see_now = !Blind;
    set_itimeout(&HBlinded, old);

    if (Unaware)
        talk = FALSE;

    if (can_see_now && !u_could_see) { /* regaining sight */
        if (talk) {
            if (Hallucination)
                pline("Far out!  Everything is all cosmic again!");
            else
                You("can see again.");
        }
    } else if (old && can_see_now && !xtime) {
        /* clearing temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(gy.youmonst.data) || PermaBlind) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blindfolded) {
                eyes = body_part(EYE);
                if (eyecount(gy.youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "itch"));
            } else { /* Eyes of the Overworld */
                Your(vismsg, "brighten", Hallucination ? "sadder" : "normal");
            }
        }
    }

    if (u_could_see && !can_see_now) { /* losing sight */
        if (talk) {
            if (Hallucination)
                pline("Oh, bummer!  Everything is dark!  Help!");
            else
                pline("A cloud of darkness falls upon you.");
        }
        /* Before the hero goes blind, set the ball&chain variables. */
        if (Punished)
            set_bc(0);
    } else if (!old && xtime) {
        /* setting temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(gy.youmonst.data) || PermaBlind) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blindfolded) {
                eyes = body_part(EYE);
                if (eyecount(gy.youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "twitch"));
            } else { /* Eyes of the Overworld */
                Your(vismsg, "dim", Hallucination ? "happier" : "normal");
            }
        }
    }

    set_itimeout(&HBlinded, xtime);

    if (u_could_see ^ can_see_now) { /* one or the other but not both */
        toggle_blindness();
    }
}

/* blindness has just started or just ended--caller enforces that;
   called by Blindf_on(), Blindf_off(), and make_blinded() */
void
toggle_blindness(void)
{
    boolean Stinging = (uwep && (EWarn_of_mon & W_WEP) != 0L);

    /* blindness has just been toggled */
    disp.botl = TRUE; /* status conditions need update */
    gv.vision_full_recalc = 1; /* vision has changed */
    /* this vision recalculation used to be deferred until moveloop(),
       but that made it possible for vision irregularities to occur
       (cited case was force bolt hitting an adjacent potion of blindness
       and then a secret door; hero was blinded by vapors but then got the
       message "a door appears in the wall" because wall spot was IN_SIGHT) */
    vision_recalc(0);
    if (Blind_telepat || Infravision || Stinging)
        see_monsters(); /* also counts EWarn_of_mon monsters */
    /*
     * Avoid either of the sequences
     * "Sting starts glowing", [become blind], "Sting stops quivering" or
     * "Sting starts quivering", [regain sight], "Sting stops glowing"
     * by giving "Sting is quivering" when becoming blind or
     * "Sting is glowing" when regaining sight so that the eventual
     * "stops" message matches the most recent "Sting is ..." one.
     */
    if (Stinging)
        Sting_effects(-1);
    /* update dknown flag for inventory picked up while blind */
    if (!Blind)
        learn_unseen_invent();
}

DISABLE_WARNING_FORMAT_NONLITERAL

boolean
make_hallucinated(
    long xtime,   /* nonzero if this is an attempt to turn on hallucination */
    boolean talk,
    long mask)    /* nonzero if resistance status should change by mask */
{
    long old = HHallucination;
    boolean changed = 0;
    const char *message, *verb;

    if (Unaware)
        talk = FALSE;

    message = (!xtime) ? "Everything %s SO boring now."
                       : "Oh wow!  Everything %s so cosmic!";
    verb = (!Blind) ? "looks" : "feels";

    if (mask) {
        if (HHallucination)
            changed = TRUE;

        if (!xtime)
            EHalluc_resistance |= mask;
        else
            EHalluc_resistance &= ~mask;
    } else {
        if (!EHalluc_resistance && (!!Hallucination != !!xtime)
            && !u.uroleplay.hallu)
            changed = TRUE;
        set_itimeout(&HHallucination, xtime);

        /* clearing temporary hallucination without toggling vision */
        if (!changed && !Hallucination && old && talk) {
            if (!haseyes(gy.youmonst.data)) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blind) {
                const char *eyes = body_part(EYE);

                if (eyecount(gy.youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "itch"));
            } else { /* Grayswandir */
                Your(vismsg, "flatten", "normal");
            }
        }
    }

    if (changed) {
        /* in case we're mimicking an orange (hallucinatory form
           of mimicking gold) update the mimicking's-over message */
        if (!Hallucination)
            eatmupdate();

        if (u.uswallow) {
            swallowed(0); /* redraw swallow display */
        } else {
            /* The see_* routines should be called *before* the pline. */
            see_monsters();
            see_objects();
            see_traps();
        }

        /* for perm_inv and anything similar
        (eg. Qt windowport's equipped items display) */
        update_inventory();

        disp.botl = TRUE;
        if (talk)
            pline(message, verb);

        /* Hallucination blocks stoning, so if it is being removed, check to see
         * if there is anything that should be causing stoning but wasn't
         * because of hallucination */
        if (!Hallucination || Halluc_resistance) {
            /* At the moment, wielding a c corpse should be the only such
             * case... */
            if (uwep && cant_wield_corpse(uwep)) {
                /* lifesaved; unwield the corpse if we can't handle it */
                uwepgone(); /* calls update_inventory() */
            }
        }
    }
    return changed;
}

RESTORE_WARNING_FORMAT_NONLITERAL

void
make_deaf(long xtime, boolean talk)
{
    long old = HDeaf;
    boolean can_hear_now;

    if (Unaware)
        talk = FALSE;

    set_itimeout(&HDeaf, xtime);
    can_hear_now = !Deaf;
    if ((xtime != 0L) ^ (old != 0L)) {
        disp.botl = TRUE;
        if (talk) {
            if (old) {
                if (can_hear_now)
                    You("can hear again.");
                /* else possibly permadeaf */
            }
            else {
                You("are unable to hear anything.");
            }
        }
    }
}

/* Set paralysis.
 * xtime is a positive number that will be passed to nomul.
 * talk is TRUE if this function should handle frozen-related messages.
 * explan will be passed to multi_reason.
 */
void
make_paralyzed(int xtime, boolean talk, const char *explan)
{
    if (Free_action) {
        You("stiffen momentarily.");
        return;
    }
    /* could be getting paralyzed while already helpless, e.g. by monster
     * attacks */
    if (talk && gm.multi >= 0) {
        /* special message for one-turn paralysis */
        if (xtime == 1) {
            You("stiffen briefly.");
        }
        else if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz)) {
            You("are motionlessly suspended.");
        }
        else if (u.usteed) {
            You("are frozen in place!");
        }
        else {
            Your("%s are frozen to the %s!", makeplural(body_part(FOOT)),
                surface(u.ux, u.uy));
        }
    }
    nomul(-xtime);
    if (explan) {
        gm.multi_reason = explan;
    }
    if (xtime == 1) {
        gn.nomovemsg = 0;
    }
    else {
        gn.nomovemsg = You_can_move_again;
    }
    exercise(A_DEX, FALSE);
}

/* set or clear "slippery fingers" */
void
make_glib(int xtime)
{
    disp.botl |= (!Glib ^ !!xtime);
    set_itimeout(&Glib, xtime);
    /* may change "(being worn)" to "(being worn; slippery)" or vice versa */
    if (uarmg)
        update_inventory();
}

void
make_withering(long xtime, boolean talk)
{
    long old = (HWithering & TIMEOUT);
    boolean was_withering = Withering, no_longer_withering;

    if (Unaware)
        talk = FALSE;

    set_itimeout(&HWithering, xtime);
    no_longer_withering = !Withering;
    if ((xtime != 0L) ^ (old != 0L)) {
        disp.botl = TRUE;
        if (talk) {
            if (old) {
                if (no_longer_withering) {
                    You("stop withering.");
                }
                else {
                    Your("withering slows, but only briefly.");
                }
            }
            else if (!was_withering) {
                You("are withering away!");
            }
        }
    }
    else if (xtime > 0L) {
        if (xtime > old) {
            Your("withering speeds up!");
        }
        else {
            Your("withering slows, but does not stop.");
        }
    }
}

void
self_invis_message(void)
{
    pline("%s %s.",
          Hallucination ? "Far out, man!  You"
                        : "Gee!  All of a sudden, you",
          See_invisible ? "can see right through yourself"
                        : "can't see yourself");
}

staticfn void
ghost_from_bottle(void)
{
    struct monst *mtmp = makemon(&mons[PM_GHOST], u.ux, u.uy, MM_NOMSG);

    if (!mtmp) {
        pline("This bottle turns out to be empty.");
        return;
    }
    if (!canspotmon(mtmp)) {
        pline("As you open the bottle, %s emerges.", something);
        return;
    }
    pline("As you open the bottle, an enormous %s emerges!",
          Hallucination ? rndmonnam(NULL) : (const char *) "ghost");
    scary_ghost(mtmp);
}

/* getobj callback for object to drink from, which also does double duty as
   the callback for dipping into (both just allow potions). */
staticfn int
drink_ok(struct obj *obj)
{
    /* getobj()'s callback to test whether hands/self is a valid "item" to
       pick is used here to communicate the fact that player has already
       passed up an opportunity to perform the action (drink or dip) on a
       non-inventory dungeon feature, so if there are no potions in invent
       the message will be "you have nothing /else/ to {drink | dip into}";
       if player used 'm' prefix to bypass dungeon features, drink_ok_extra
       will be 0 and the potential "else" will be omitted */
    if (!obj)
        return drink_ok_extra ? GETOBJ_EXCLUDE_NONINVENT : GETOBJ_EXCLUDE;

    if (obj->oclass == POTION_CLASS)
        return GETOBJ_SUGGEST;

    return GETOBJ_EXCLUDE;
}

/* "Quaffing is like drinking, except you spill more." - Terry Pratchett */
/* the #quaff command */
int
dodrink(void)
{
    struct obj *otmp;

    if (Strangled) {
        pline("If you can't breathe air, how can you drink liquid?");
        return ECMD_OK;
    }

    drink_ok_extra = 0;
    /* preceding 'q'/#quaff with 'm' skips the possibility of drinking
       from fountains, sinks, and surrounding water plus the prompting
       which those entail; optional for interactive use, essential for
       context-sensitive inventory item action 'quaff' */
    if (!iflags.menu_requested) {
        /* Is there a fountain to drink from here? */
        if (IS_FOUNTAIN(levl[u.ux][u.uy].typ)
            /* not as low as floor level but similar restrictions apply */
            && can_reach_floor(FALSE)) {
            if (y_n("Drink from the fountain?") == 'y') {
                drinkfountain();
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        }
        /* Or a kitchen sink? */
        if (IS_SINK(levl[u.ux][u.uy].typ)
            /* not as low as floor level but similar restrictions apply */
            && can_reach_floor(FALSE)) {
            if (y_n("Drink from the sink?") == 'y') {
                drinksink();
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        }
        /* Or are you surrounded by water? */
        if (Underwater && !u.uswallow) {
            if (y_n("Drink the water around you?") == 'y') {
                pline("Do you know what lives in this water?");
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        }
    }

    otmp = getobj("drink", drink_ok, GETOBJ_NOFLAGS);
    if (!otmp)
        return ECMD_CANCEL;

    /*
     * 3.6:  quan > 1 used to be left to useup(), but we need to
     * force the current potion to be unworn, and don't want to do
     * that for the entire stack when starting with more than 1.
     * [Drinking a wielded potion of polymorph can trigger a shape
     * change which causes hero's weapon to be dropped.  In 3.4.x,
     * that led to an "object lost" panic since subsequent useup()
     * was no longer dealing with an inventory item.  Unwearing
     * the current potion is intended to keep it in inventory.]
     *
     * 3.7: switch back to relying on useup() unless the object is
     * actually worn.  Otherwise drinking a stack of unpaid potions
     * one by one in a shop makes each one a separate used-up item
     * for 'Ix' invent display and for itemized shop billing instead
     * of having a single stack with quantity greater than 1.
     */
    if (otmp->owornmask) {
        if (otmp->quan > 1L) {
            otmp = splitobj(otmp, 1L);
            otmp->owornmask = 0L; /* rest of original stack is unaffected */
        } else {
            remove_worn_item(otmp, FALSE);
        }
    }
    otmp->in_use = TRUE; /* you've opened the stopper */

    if (objdescr_is(otmp, "milky")
        && !(svm.mvitals[PM_GHOST].mvflags & G_GONE)
        && !rn2(POTION_OCCUPANT_CHANCE(svm.mvitals[PM_GHOST].born))) {
        ghost_from_bottle();
        useup(otmp);
        return ECMD_TIME;
    } else if (objdescr_is(otmp, "smoky")
               && !(svm.mvitals[PM_DJINNI].mvflags & G_GONE)
               && !rn2(POTION_OCCUPANT_CHANCE(svm.mvitals[PM_DJINNI].born))) {
        djinni_from_bottle(otmp);
        useup(otmp);
        return ECMD_TIME;
    }
    return dopotion(otmp);
}

int
dopotion(struct obj *otmp)
{
    int retval;

    otmp->in_use = TRUE;
    gp.potion_nothing = gp.potion_unkn = 0;
    if ((retval = peffects(otmp)) >= 0)
        return retval ? ECMD_TIME : ECMD_OK;

    if (gp.potion_nothing) {
        gp.potion_unkn++;
        You("have a %s feeling for a moment, then it passes.",
            Hallucination ? "normal" : "peculiar");
    }
    if (otmp->dknown && !objects[otmp->otyp].oc_name_known) {
        if (!gp.potion_unkn) {
            makeknown(otmp->otyp);
            more_experienced(0, 10);
        } else
            trycall(otmp);
    }
    useup(otmp);
    return ECMD_TIME;
}

/* potion or spell of restore ability; for spell, otmp is a temporary
   spellbook object that will be blessed if hero is skilled in healing */
staticfn void
peffect_restore_ability(struct obj *otmp)
{
    gp.potion_unkn++;
    if (otmp->cursed) {
        pline("Ulch!  This makes you feel mediocre!");
        return;
    } else {
        int i, ii;

        /* unlike unicorn horn, overrides Fixed_abil;
           does not recover temporary strength loss due to hunger */
        pline("Wow!  This makes you feel %s!",
              (!otmp->blessed) ? "good"
              : unfixable_trouble_count(FALSE) ? "better"
                : "great");
        i = rn2(A_MAX); /* start at a random point */
        for (ii = 0; ii < A_MAX; ii++) {
            int lim = AMAX(i);

            /* this used to adjust 'lim' for A_STR when u.uhs was
               WEAK or worse, but that's handled via ATEMP(A_STR) now */
            if (ABASE(i) < lim) {
                ABASE(i) = lim;
                disp.botl = TRUE;
                /* only first found if not blessed */
                if (!otmp->blessed)
                    break;
            }
            if (++i >= A_MAX)
                i = 0;
        }

        /* when using the potion (not the spell) also restore lost levels,
           to make the potion more worth keeping around for players with
           the spell or with a unihorn; this is better than full healing
           in that it can restore all of them, not just half, and a
           blessed potion restores them all at once */
        if (otmp->otyp == POT_RESTORE_ABILITY && u.ulevel < u.ulevelmax) {
            do {
                pluslvl(FALSE);
            } while (u.ulevel < u.ulevelmax && otmp->blessed);
        }

        /* also heal wounded legs and most status ailments ("restoring your
         * faculties"), and even illness if blessed, but not sliming.
         * Note: spell of restore ability will also cure these (except illness),
         * but if you're confused or stunned you can't cast it in the first
         * place. */
        if (Wounded_legs)
            heal_legs(0);
        make_confused(0L, TRUE);
        make_stunned(0L, TRUE);
        make_hallucinated(0L, TRUE, 0L);
        if (otmp->otyp == POT_RESTORE_ABILITY && otmp->blessed)
            make_sick(0L, (char *) 0, TRUE, SICK_ALL);
    }
}

staticfn void
peffect_hallucination(struct obj *otmp)
{
    if (Halluc_resistance) {
        gp.potion_nothing++;
        return;
    } else if (Hallucination) {
        gp.potion_nothing++;
    }
    (void) make_hallucinated(itimeout_incr(HHallucination,
                                           rn1(200, 600 - 300 * bcsign(otmp))),
                             TRUE, 0L);
    if ((otmp->blessed && !rn2(3)) || (!otmp->cursed && !rn2(6))) {
        You("perceive yourself...");
        display_nhwindow(WIN_MESSAGE, FALSE);
        enlightenment(MAGICENLIGHTENMENT, ENL_GAMEINPROGRESS);
        Your("awareness re-normalizes.");
        exercise(A_WIS, TRUE);
    }
}

staticfn void
peffect_water(struct obj *otmp)
{
    if (!otmp->blessed && !otmp->cursed) {
        pline("This tastes like %s.", hliquid("water"));
        u.uhunger += rnd(10);
        newuhs(FALSE);
        return;
    }
    gp.potion_unkn++;
    if (mon_hates_blessings(&gy.youmonst) /* undead or demon */
        || u.ualign.type == A_CHAOTIC) {
        if (otmp->blessed) {
            pline("This burns like %s!", hliquid("acid"));
            exercise(A_CON, FALSE);
            if (ismnum(u.ulycn)) {
                Your("affinity to %s disappears!",
                     makeplural(mons[u.ulycn].pmnames[NEUTRAL]));
                if (gy.youmonst.data == &mons[u.ulycn])
                    you_unwere(FALSE);
                set_ulycn(NON_PM); /* cure lycanthropy */
            }
            losehp(Maybe_Half_Phys(d(2, 6)), "potion of holy water",
                   KILLED_BY_AN);
        } else if (otmp->cursed) {
            You_feel("quite proud of yourself.");
            healup(d(2, 6), 0, 0, 0);
            if (ismnum(u.ulycn) && !Upolyd)
                you_were();
            exercise(A_CON, TRUE);
        }
    } else {
        if (otmp->blessed) {
            You_feel("full of awe.");
            make_sick(0L, (char *) 0, TRUE, SICK_ALL);
            exercise(A_WIS, TRUE);
            exercise(A_CON, TRUE);
            if (ismnum(u.ulycn))
                you_unwere(TRUE); /* "Purified" */
            /* make_confused(0L, TRUE); */
        } else {
            if (u.ualign.type == A_LAWFUL) {
                pline("This burns like %s!", hliquid("acid"));
                losehp(Maybe_Half_Phys(d(2, 6)), "potion of unholy water",
                       KILLED_BY_AN);
            } else
                You_feel("full of dread.");
            if (ismnum(u.ulycn) && !Upolyd)
                you_were();
            exercise(A_CON, FALSE);
        }
    }
}

staticfn void
peffect_booze(struct obj *otmp)
{
    gp.potion_unkn++;
    pline("Ooph!  This tastes like %s%s!",
          otmp->odiluted ? "watered down " : "",
          Hallucination ? "dandelion wine" : "liquid fire");
    if (!otmp->blessed) {
        /* booze hits harder if drinking on an empty stomach */
        make_confused(itimeout_incr(HConfusion, d(2 + u.uhs, 8)), FALSE);
    }
    /* the whiskey makes us feel better */
    if (!otmp->odiluted)
        healup(1, 0, FALSE, FALSE);
    u.uhunger += 10 * (2 + bcsign(otmp));
    newuhs(FALSE);
    exercise(A_WIS, FALSE);
    if (otmp->cursed) {
        You("pass out.");
        gm.multi = -rnd(15);
        gn.nomovemsg = "You awake with a headache.";
    }
}

staticfn void
peffect_enlightenment(struct obj *otmp)
{
    if (otmp->cursed) {
        gp.potion_unkn++;
        You("have an uneasy feeling...");
        exercise(A_WIS, FALSE);
    } else {
        if (otmp->blessed) {
            (void) adjattrib(A_INT, 1, FALSE);
            (void) adjattrib(A_WIS, 1, FALSE);
        }
        do_enlightenment_effect();
    }
}

staticfn void
peffect_invisibility(struct obj *otmp)
{
    boolean is_spell = (otmp->oclass == SPBOOK_CLASS);

    /* spell cannot penetrate mummy wrapping */
    if (is_spell && BInvis && uarmc->otyp == MUMMY_WRAPPING) {
        You_feel("rather itchy under %s.", yname(uarmc));
        return;
    }
    if (Invis || Blind || BInvis) {
        gp.potion_nothing++;
    } else {
        self_invis_message();
    }
    incr_itimeout(&HInvis, rnd(200) + 200 * (bcsign(otmp) + 2));
    newsym(u.ux, u.uy); /* update position */
    if (otmp->cursed) {
        pline("For some reason, you feel your presence is known.");
        aggravate();
    }
}

staticfn void
peffect_see_invisible(struct obj *otmp)
{
    int msg = Invisible && !Blind;

    gp.potion_unkn++;
    if (otmp->cursed)
        pline("Yecch!  This tastes %s.",
              Hallucination ? "overripe" : "rotten");
    else
        pline(
              Hallucination
              ? "This tastes like 10%% real %s%s all-natural beverage."
              : "This tastes like %s%s.",
              otmp->odiluted ? "reconstituted " : "", fruitname(TRUE));
    if (otmp->otyp == POT_FRUIT_JUICE) {
        u.uhunger += (otmp->odiluted ? 5 : 10) * (2 + bcsign(otmp));
        newuhs(FALSE);
        return;
    }
    if (!otmp->cursed) {
        /* Tell them they can see again immediately, which
         * will help them identify the potion...
         */
        make_blinded(0L, TRUE);
    }
    incr_itimeout(&HSee_invisible, rnd(200) + 200 * (bcsign(otmp) + 2));
    set_mimic_blocking(); /* do special mimic handling */
    see_monsters();       /* see invisible monsters */
    newsym(u.ux, u.uy);   /* see yourself! */
    if (msg && !Blind) {  /* Blind possible if polymorphed */
        You("can see through yourself, but you are visible!");
        gp.potion_unkn--;
    }
}

staticfn void
peffect_paralysis(struct obj *otmp)
{
    make_paralyzed(rn1(10, 25 - 12 * bcsign(otmp)), TRUE,
                   "frozen by a potion");
}

staticfn void
peffect_sleeping(struct obj *otmp)
{
    if (Sleep_resistance || Free_action) {
        monstseesu(M_SEEN_SLEEP);
        You("yawn.");
    } else {
        You("suddenly fall asleep!");
        monstunseesu(M_SEEN_SLEEP);
        fall_asleep(-rn1(10, 25 - 12 * bcsign(otmp)), TRUE);
    }
}

staticfn int
peffect_monster_detection(struct obj *otmp)
{
    if (otmp->blessed) {
        int i, x, y;

        if (Detect_monsters)
            gp.potion_nothing++;
        gp.potion_unkn++;
        /* after a while, repeated uses become less effective */
        if ((HDetect_monsters & TIMEOUT) >= 300L)
            i = 1;
        else if (otmp->oclass == SPBOOK_CLASS)
            i = rn1(40, 21);
        else /* potion */
            i = rn2(100) + 100;
        incr_itimeout(&HDetect_monsters, i);
        for (x = 1; x < COLNO; x++) {
            for (y = 0; y < ROWNO; y++) {
                if (levl[x][y].glyph == GLYPH_INVISIBLE) {
                    unmap_object(x, y);
                    newsym(x, y);
                }
                if (MON_AT(x, y))
                    gp.potion_unkn = 0;
            }
        }
        /* if swallowed or underwater, fall through to uncursed case */
        if (!u.uswallow && !Underwater) {
            see_monsters();
            if (gp.potion_unkn)
                You_feel("lonely.");
            return 0;
        }
    }
    if (monster_detect(otmp, 0))
        return 1; /* nothing detected */
    exercise(A_WIS, TRUE);
    return 0;
}

staticfn int
peffect_object_detection(struct obj *otmp)
{
    if (object_detect(otmp, 0))
        return 1; /* nothing detected */
    exercise(A_WIS, TRUE);
    return 0;
}

staticfn void
peffect_sickness(struct obj *otmp)
{
    pline("Yecch!  This stuff tastes like poison.");
    if (otmp->blessed) {
        pline("(But in fact it was mildly stale %s.)", fruitname(TRUE));
        if (!Role_if(PM_HEALER)) {
            /* NB: blessed otmp->fromsink is not possible */
            losehp(1, "mildly contaminated potion", KILLED_BY_AN);
        }
    } else {
        if (Poison_resistance)
            pline("(But in fact it was biologically contaminated %s.)",
                  fruitname(TRUE));
        if (Role_if(PM_HEALER)) {
            pline("Fortunately, you have been immunized.");
        } else {
            char contaminant[BUFSZ];
            int typ = rn2(A_MAX);

            Sprintf(contaminant, "%s%s",
                    (Poison_resistance) ? "mildly " : "",
                    (otmp->fromsink) ? "contaminated tap water"
                    : "contaminated potion");
            if (!Fixed_abil) {
                poisontell(typ, FALSE);
                (void) adjattrib(typ, Poison_resistance ? -1 : -rn1(4, 3),
                                 AA_YESMSG);
            }
            if (!Poison_resistance) {
                if (otmp->fromsink)
                    losehp(rnd(10) + 5 * !!(otmp->cursed), contaminant,
                           KILLED_BY);
                else
                    losehp(rnd(10) + 5 * !!(otmp->cursed), contaminant,
                           KILLED_BY_AN);
            } else {
                /* rnd loss is so that unblessed poorer than blessed */
                losehp(1 + rn2(2), contaminant,
                       (otmp->fromsink) ? KILLED_BY : KILLED_BY_AN);
            }
            exercise(A_CON, FALSE);
        }
    }
    if (Hallucination) {
        if (u.uroleplay.hallu) {
            You_feel("less groovy for a harsh second.");
        }
        else {
            You("are shocked back to your senses!");
        }
        /* this is ok with permahallu conduct */
        (void) make_hallucinated(0L, FALSE, 0L);
    }
}

staticfn void
peffect_confusion(struct obj *otmp)
{
    if (!Confusion) {
        if (Hallucination) {
            pline("What a trippy feeling!");
            gp.potion_unkn++;
        } else
            pline("Huh, What?  Where am I?");
    } else
        gp.potion_nothing++;
    make_confused(itimeout_incr(HConfusion,
                                rn1(7, 16 - 8 * bcsign(otmp))),
                  FALSE);
}

staticfn void
peffect_gain_ability(struct obj *otmp)
{
    int i, ii;
    if (otmp->cursed) {
        pline("Ulch!  That potion tasted foul!");
        if (Fixed_abil) {
            gp.potion_unkn++; /* not potion_nothing because you got message */
            return;
        }
        /* gain a point in an ability by stealing it from another ability;
         * point will always be given to the lowest score and taken from
         * somewhere else, so it can be used somewhat reliably to increase the
         * worst scores */
        int lowest = A_STR; /* attrib_types with lowest known score */
        int nlowest = 0; /* number of attribs with the same lowest known score;
                            pick randomly if tie; start at 0 because we haven't
                            actually evaluated A_STR yet (note assumption that
                            A_STR is first in attrib_types) */
        int highest = A_STR;
        int nhighest = 0;
        for (i = 0; i < A_MAX; ++i) {
            if (ABASE(i) < ABASE(lowest)) {
                lowest = i;
                nlowest = 1;
            }
            else if (ABASE(i) == ABASE(lowest)) {
                nlowest++;
                if (!rn2(nlowest + 1))
                    lowest = i;
            }
            if (ABASE(i) > ABASE(highest)) {
                highest = i;
                nhighest = 1;
            }
            else if (ABASE(i) == ABASE(highest)) {
                nhighest++;
                if (!rn2(nhighest + 1))
                    highest = i;
            }
        }
        if (ABASE(lowest) == ABASE(highest)) {
            /* possible for this if the hero has the same in every attribute */
            gp.potion_unkn++;
            return;
        }
        adjattrib(highest, -1, AA_YESMSG);
        adjattrib(lowest, 1, AA_YESMSG);
    } else if (Fixed_abil) {
        gp.potion_nothing++;
    } else {      /* If blessed, increase all; if not, try up to */
        char response = '*'; /* random by default */
        xint8 attr_selected;
        /* Ability scores are defined in a different order than they're
            * displayed on the status line. The menu presents them in display
            * order, so we need to map them to their real order in the case of
            * a blessed potion that presented a menu. */
        int attribs[A_MAX] = {A_STR, A_DEX, A_CON, A_INT, A_WIS, A_CHA};
        if (otmp->blessed) {
            /* Allow the player to choose what to increase, and give a bit
                * more increase than an uncursed potion. */
            menu_item *choice = (menu_item *) 0;
            winid win = create_nhwindow(NHW_MENU);
            anything any;
            char attrname[BUFSZ];
            static const char* attrnames[A_MAX] = {
                "Strength", "Dexterity", "Constitution", "Intelligence",
                "Wisdom", "Charisma"
            };
            start_menu(win, MENU_BEHAVE_STANDARD);
            for (ii = 0; ii < A_MAX; ii++) {
                if (ABASE(attribs[ii]) < ATTRMAX(attribs[ii])) {
                    Strcpy(attrname, attrnames[ii]);
                    any.a_char = 'a' + ii;
                }
                else {
                    Sprintf(attrname, "    %s (MAX)", attrnames[ii]);
                    any.a_char = 0;
                }
                add_menu(win, &nul_glyphinfo, &any, any.a_char, '\0',
                         ATR_NONE, 0, attrname, MENU_ITEMFLAGS_NONE);
            }
            any.a_char = '*';
            add_menu(win, &nul_glyphinfo, &any, '*', '\0', ATR_NONE, 0,
		     "pick one randomly", MENU_ITEMFLAGS_NONE);
            end_menu(win, "What attribute do you want to increase?");
            if (select_menu(win, PICK_ONE, &choice) <= 0) {
                /* cancelled; pick one randomly */
                response = '*';
            }
            else {
                response = choice->item.a_char;
                free((genericptr_t) choice);
            }
            destroy_nhwindow(win);
        }

        if (response == '*') {
            /* Shuffle the attributes. (Fisher-Yates) */
            for (ii = A_MAX-1; ii >= 1; ii--) {
                int tmp = attribs[ii];
                i = rn2(ii + 1);
                attribs[ii] = attribs[i];
                attribs[i] = tmp;
            }
            for (ii = 0; ii < A_MAX; ii++) {
                /* only give "your X is already as high as it can get"
                    message on last attempt */
                if (adjattrib(attribs[ii], otmp->blessed ? rnd(2) : 1,
                                (ii == A_MAX - 1) ? AA_YESMSG : AA_CONDMSG)
                        != AA_NOCHNG) {
                    break;
                }
            }
        }
        else {
            /* non-* response should always mean blessed potion */
            attr_selected = attribs[response - 'a'];
            adjattrib(attr_selected, rnd(2), AA_YESMSG);
        }
    }
}

staticfn void
peffect_speed(struct obj *otmp)
{
    boolean is_speed = (otmp->otyp == POT_SPEED); /* haste self */

    /* skip when mounted; heal_legs() would heal steed's legs */
    if (is_speed && Wounded_legs && !otmp->cursed && !u.usteed) {
        heal_legs(0);
        gp.potion_unkn++;
        return;
    }

    speed_up(rn1(10, 100 + 60 * bcsign(otmp)));

    /* non-cursed potion grants intrinsic speed */
    if (is_speed && !otmp->cursed && !(HFast & INTRINSIC)) {
        Your("quickness feels very natural.");
        HFast |= FROMOUTSIDE;
    }
}

staticfn void
peffect_blindness(struct obj *otmp)
{
    if (Blind || ((HBlinded || EBlinded) && BBlinded))
        gp.potion_nothing++;
    make_blinded(itimeout_incr(BlindedTimeout,
                               rn1(200, 250 - 125 * bcsign(otmp))),
                 (boolean) !Blind);
}

staticfn void
peffect_gain_level(struct obj *otmp)
{
    if (otmp->cursed) {
        boolean on_lvl_1 = (ledger_no(&u.uz) == 1);

        gp.potion_unkn++;
        /* they went up a level */
        if ((on_lvl_1 ? u.uhave.amulet : Can_rise_up(u.ux, u.uy, &u.uz))
            && !fiend_adversity(PM_ASMODEUS)) {
            int newlev;
            d_level newlevel;

            /* This is allowed now to bypass Sokoban levels, but it's pretty
             * clearly cheating on the challenge. */
            sokoban_guilt();

            if (on_lvl_1) {
                assign_level(&newlevel, &earth_level);
            } else {
                newlev = depth(&u.uz) - 1;
                get_level(&newlevel, newlev);
                if (on_level(&newlevel, &u.uz)) {
                    pline("It tasted bad.");
                    return;
                }
            }
            You("rise up, through the %s!", ceiling(u.ux, u.uy));
            goto_level(&newlevel, FALSE, FALSE, FALSE);
        } else {
            You("have an uneasy feeling.");
        }
        return;
    }
    pluslvl(FALSE);
    /* blessed potions place you at a random spot in the
       middle of the new level instead of the low point */
    if (otmp->blessed)
        u.uexp = rndexp(TRUE);
}

staticfn void
peffect_healing(struct obj *otmp)
{
    You_feel("better.");
    healup(8 + d(4 + 2 * bcsign(otmp), 4), !otmp->cursed ? 1 : 0,
           !!otmp->blessed, !otmp->cursed);
    exercise(A_CON, TRUE);
}

staticfn void
peffect_extra_healing(struct obj *otmp)
{
    You_feel("much better.");
    healup(16 + d(4 + 2 * bcsign(otmp), 8),
           otmp->blessed ? 5 : !otmp->cursed ? 2 : 0, !otmp->cursed,
           TRUE);
    (void) make_hallucinated(0L, TRUE, 0L);
    exercise(A_CON, TRUE);
    exercise(A_STR, TRUE);
    /* blessed potion also heals wounded legs unless riding (where leg
       wounds apply to the steed rather than to the hero) */
    if (Wounded_legs && (otmp->blessed && !u.usteed))
        heal_legs(0);
}

staticfn void
peffect_full_healing(struct obj *otmp)
{
    You_feel("completely healed.");
    healup(400, 4 + 4 * bcsign(otmp), !otmp->cursed, TRUE);
    /* Restore one lost level if blessed */
    if (otmp->blessed && u.ulevel < u.ulevelmax) {
        /* when multiple levels have been lost, drinking
           multiple potions will only get half of them back */
        u.ulevelmax -= 1;
        pluslvl(FALSE);
    }
    (void) make_hallucinated(0L, TRUE, 0L);
    exercise(A_STR, TRUE);
    exercise(A_CON, TRUE);
    /* blessed potion heals wounded legs even when riding (so heals steed's
       legs--it's magic); uncursed potion heals hero's legs unless riding */
    if (Wounded_legs && (otmp->blessed || (!otmp->cursed && !u.usteed)))
        heal_legs(0);
}

staticfn void
peffect_levitation(struct obj *otmp)
{
    /*
     * BLevitation will be set if levitation is blocked due to being
     * inside rock (currently or formerly in phazing xorn form, perhaps)
     * but it doesn't prevent setting or incrementing Levitation timeout
     * (which will take effect after escaping from the rock if it hasn't
     * expired by then).
     */
    if (!Levitation && !BLevitation) {
        /* kludge to ensure proper operation of float_up() */
        set_itimeout(&HLevitation, 1L);
        float_up();
        /* This used to set timeout back to 0, then increment it below
           for blessed and uncursed effects.  But now we leave it so
           that cursed effect yields "you float down" on next turn.
           Blessed and uncursed get one extra turn duration. */
    } else /* already levitating, or can't levitate */
        gp.potion_nothing++;

    if (otmp->cursed) {
        stairway *stway;

        /* 'already levitating' used to block the cursed effect(s)
           aside from ~I_SPECIAL; it was not clear whether that was
           intentional; either way, it no longer does (as of 3.6.1) */
        HLevitation &= ~I_SPECIAL; /* can't descend upon demand */
        if (BLevitation) {
            ; /* rising via levitation is blocked */
        } else if ((stway = stairway_at(u.ux, u.uy)) != 0 && stway->up) {
            (void) doup();
            /* in case we're already Levitating, which would have
               resulted in incrementing 'nothing' */
            gp.potion_nothing = 0; /* not nothing after all */
        } else if (has_ceiling(&u.uz)) {
            int dmg = rnd(!uarmh ? 10 : !hard_helmet(uarmh) ? 6 : 3);

            You("hit your %s on the %s.", body_part(HEAD),
                ceiling(u.ux, u.uy));
            losehp(Maybe_Half_Phys(dmg), "colliding with the ceiling",
                   KILLED_BY);
            gp.potion_nothing = 0; /* not nothing after all */
        }
    } else if (otmp->blessed) {
        /* at this point, timeout is already at least 1 */
        incr_itimeout(&HLevitation, rn1(50, 250));
        /* can descend at will (stop levitating via '>') provided timeout
           is the only factor (ie, not also wearing Lev ring or boots) */
        HLevitation |= I_SPECIAL;
    } else /* timeout is already at least 1 */
        incr_itimeout(&HLevitation, rn1(140, 10));

    if (Levitation && IS_SINK(levl[u.ux][u.uy].typ))
        spoteffects(FALSE);
    /* levitating blocks flying */
    float_vs_flight();
}

staticfn void
peffect_gain_energy(struct obj *otmp)
{
    int max_change, current_change;

    if (otmp->cursed)
        You_feel("lackluster.");
    else
        pline("Magical energies course through your body.");

    /* blessed:  +3..18 max (+10.5 avg), +9..54 current (+31.5 avg) OR 40% uenmax
     * uncursed: +2..12 max (+ 7   avg), +6..36 current (+21   avg) OR 25% uenmax
     * cursed:   -1.. 6 max (- 3.5 avg), -3..18 current (-10.5 avg)
     */
    if (otmp->blessed) {
        max_change = d(3, 6);
        current_change = 2 * u.uenmax / 5;
    } else if(!otmp->cursed) {
        max_change = d(2, 6);
        current_change = u.uenmax / 4;
    } else {
        max_change = -1 * d(1, 6);
        current_change = 3 * max_change;
    }
    if (current_change < 3 * max_change) {
        current_change = 3 * max_change;
    }
    u.uenmax += max_change;
    if (u.uenmax > u.uenpeak)
        u.uenpeak = u.uenmax;
    else if (u.uenmax <= 0)
        u.uenmax = 0;
    u.uen += current_change;
    if (u.uen > u.uenmax)
        u.uen = u.uenmax;
    else if (u.uen <= 0)
        u.uen = 0;
    disp.botl = TRUE;
    exercise(A_WIS, TRUE);
}

staticfn void
peffect_oil(struct obj *otmp)
{
    boolean good_for_you = FALSE, vulnerable;

    if (otmp->lamplit) {
        if (likes_fire(gy.youmonst.data)) {
            pline("Ahh, a refreshing drink.");
            good_for_you = TRUE;
        } else {
            /*
             * Note: if poly'd into green slime, hero ought to take
             * extra damage, but drinking potions in that form isn't
             * possible so there's no need to try to handle that.
             */
            You("burn your %s.", body_part(FACE));
            /* fire damage */
            vulnerable = !Fire_resistance || Cold_resistance;
            losehp(d(vulnerable ? 4 : 2, 4),
                   "quaffing a burning potion of oil",
                   KILLED_BY);
        }
        /*
         * This is slightly iffy because the burning isn't being
         * spread across the body.  But the message is "the slime
         * that covers you burns away" and having that follow
         * "you burn your face" seems consistent enough.
         */
        burn_away_slime();
    } else if (otmp->cursed) {
        pline("This tastes like castor oil.");
    } else {
        pline("That was smooth!");
    }
    exercise(A_WIS, good_for_you);
}

staticfn void
peffect_acid(struct obj *otmp)
{
    if (Acid_resistance) {
        /* Not necessarily a creature who _likes_ acid */
        pline("This tastes %s.", Hallucination ? "tangy" : "sour");
    } else {
        int dmg;

        pline("This burns%s!",
              otmp->blessed ? " a little" : otmp->cursed ? " a lot"
                                                         : " like acid");
        dmg = d(otmp->cursed ? 2 : 1, otmp->blessed ? 4 : 8);
        losehp(Maybe_Half_Phys(dmg), "potion of acid", KILLED_BY_AN);
        exercise(A_CON, FALSE);
    }
    if (Stoned)
        fix_petrification();
    gp.potion_unkn++; /* holy/unholy water can burn like acid too */
}

staticfn void
peffect_polymorph(struct obj *otmp)
{
    You_feel("a little %s.", Hallucination ? "normal" : "strange");
    if (!Unchanging) {
        if (!otmp->blessed || (u.umonnum != u.umonster))
            polyself(POLY_NOFLAGS);
        else {
            polyself(POLY_CONTROLLED|POLY_LOW_CTRL);
            if (u.mtimedone && u.umonnum != u.umonster)
                u.mtimedone = min(u.mtimedone, rn2(15) + 10);
        }
    }
}

int
peffects(struct obj *otmp)
{
    switch (otmp->otyp) {
    case POT_RESTORE_ABILITY:
    case SPE_RESTORE_ABILITY:
        peffect_restore_ability(otmp);
        break;
    case POT_HALLUCINATION:
        peffect_hallucination(otmp);
        break;
    case POT_WATER:
        peffect_water(otmp);
        break;
    case POT_BOOZE:
        peffect_booze(otmp);
        break;
    case POT_ENLIGHTENMENT:
        peffect_enlightenment(otmp);
        break;
    case SPE_INVISIBILITY:
    case POT_INVISIBILITY:
        peffect_invisibility(otmp);
        break;
    case POT_SEE_INVISIBLE: /* tastes like fruit juice in Rogue */
    case POT_FRUIT_JUICE:
        peffect_see_invisible(otmp);
        break;
    case POT_PARALYSIS:
        peffect_paralysis(otmp);
        break;
    case POT_SLEEPING:
        peffect_sleeping(otmp);
        break;
    case POT_MONSTER_DETECTION:
    case SPE_DETECT_MONSTERS:
        if (peffect_monster_detection(otmp))
            return 1;
        break;
    case POT_OBJECT_DETECTION:
    case SPE_DETECT_TREASURE:
        if (peffect_object_detection(otmp))
            return 1;
        break;
    case POT_SICKNESS:
        peffect_sickness(otmp);
        break;
    case POT_CONFUSION:
        peffect_confusion(otmp);
        break;
    case POT_GAIN_ABILITY:
        peffect_gain_ability(otmp);
        break;
    case POT_SPEED:
    case SPE_HASTE_SELF:
        peffect_speed(otmp);
        break;
    case POT_BLINDNESS:
        peffect_blindness(otmp);
        break;
    case POT_GAIN_LEVEL:
        peffect_gain_level(otmp);
        break;
    case POT_HEALING:
        peffect_healing(otmp);
        break;
    case POT_EXTRA_HEALING:
        peffect_extra_healing(otmp);
        break;
    case POT_FULL_HEALING:
        peffect_full_healing(otmp);
        break;
    case POT_LEVITATION:
    case SPE_LEVITATION:
        peffect_levitation(otmp);
        break;
    case POT_GAIN_ENERGY: /* M. Stephenson */
        peffect_gain_energy(otmp);
        break;
    case POT_OIL: /* P. Winner */
        peffect_oil(otmp);
        break;
    case POT_ACID:
        peffect_acid(otmp);
        break;
    case POT_POLYMORPH:
        peffect_polymorph(otmp);
        break;
    default:
        impossible("What a funny potion! (%u)", otmp->otyp);
        return 0;
    }
    return -1;
}

void
healup(int nhp, int nxtra, boolean curesick, boolean cureblind)
{
    if (nhp) {
        if (Upolyd) {
            u.mh += nhp;
            if (u.mh > u.mhmax)
                u.mh = (u.mhmax += nxtra);
        } else {
            u.uhp += nhp;
            if (u.uhp > u.uhpmax) {
                u.uhp = (u.uhpmax += nxtra);
                if (u.uhpmax > u.uhppeak)
                    u.uhppeak = u.uhpmax;
            }
        }
    }
    if (cureblind) {
        /* 3.6.1: it's debatable whether healing magic should clean off
           mundane 'dirt', but if it doesn't, blindness isn't cured */
        u.ucreamed = 0;
        make_blinded(0L, TRUE);
        /* heal deafness too */
        make_deaf(0L, TRUE);
    }
    if (curesick) {
        make_vomiting(0L, TRUE);
        make_sick(0L, (char *) 0, TRUE, SICK_ALL);
    }
    disp.botl = TRUE;
    return;
}

void
strange_feeling(struct obj *obj, const char *txt)
{
    if (flags.beginner || !txt)
        You("have a %s feeling for a moment, then it passes.",
            Hallucination ? "normal" : "strange");
    else
        pline1(txt);

    if (!obj) /* e.g., crystal ball finds no traps */
        return;

    if (obj->dknown)
        trycall(obj);

    /* if otmp is in flight, whatever effect results in strange_feeling() is
     * probably from a potion breaking and hero breathing vapors.
     * useup() here will panic because it's not on any object lists.
     * The assumption being made here is that there will not be a way for an
     * in-flight object to call strange_feeling() *unless* it is breaking. */
    if (obj == gt.thrownobj || obj == gk.kickedobj)
        return;

    useup(obj);
}

static const char *bottlenames[] = { "bottle", "phial", "flagon", "carafe",
                              "flask",  "jar",   "vial" };
static const char *hbottlenames[] = {
    "jug", "pitcher", "barrel", "tin", "bag", "box", "glass", "beaker",
    "tumbler", "vase", "flowerpot", "pan", "thingy", "mug", "teacup",
    "teapot", "keg", "bucket", "thermos", "amphora", "wineskin", "parcel",
    "bowl", "ampoule"
};

const char *
bottlename(void)
{
    if (Hallucination)
        return ROLL_FROM(hbottlenames);
    else
        return ROLL_FROM(bottlenames);
}

/* handle item dipped into water potion or steed saddle splashed by same */
staticfn boolean
H2Opotion_dip(
    struct obj *potion,    /* water */
    struct obj *targobj,   /* item being dipped into the water */
    boolean useeit,        /* will hero see the glow/aura? */
    const char *objphrase) /* "Your widget glows" or "Steed's saddle glows" */
{
    void (*func)(struct obj *) = (void (*)(struct obj *)) 0;
    const char *glowcolor = 0;
#define COST_alter (-2)
#define COST_none (-1)
    int costchange = COST_none;
    boolean altfmt = FALSE, res = FALSE;

    if (!potion || potion->otyp != POT_WATER)
        return FALSE;

    if (potion->blessed) {
        if (targobj->cursed) {
            func = uncurse;
            glowcolor = NH_AMBER;
            costchange = COST_UNCURS;
        } else if (!targobj->blessed) {
            func = bless;
            glowcolor = NH_LIGHT_BLUE;
            costchange = COST_alter;
            altfmt = TRUE; /* "with a <color> aura" */
        }
    } else if (potion->cursed) {
        if (targobj->blessed) {
            func = unbless;
            glowcolor = "brown";
            costchange = COST_UNBLSS;
        } else if (!targobj->cursed) {
            func = curse;
            glowcolor = NH_BLACK;
            costchange = COST_alter;
            altfmt = TRUE;
        }
    } else {
        /* dipping into uncursed water; carried() check skips steed saddle */
        if (carried(targobj)) {
            gm.mentioned_water = FALSE; /* water_damage() might set this */
            if (water_damage(targobj, 0, TRUE) != ER_NOTHING)
                res = TRUE;
            if (gm.mentioned_water)
                makeknown(POT_WATER);
            gm.mentioned_water = FALSE;
        }
    }
    if (func) {
        /* give feedback before altering the target object;
           this used to set obj->bknown even when not seeing
           the effect; now hero has to see the glow, and bknown
           is cleared instead of set if perception is distorted */
        if (useeit) {
            glowcolor = hcolor(glowcolor);
            if (altfmt)
                pline("%s with %s aura.", objphrase, an(glowcolor));
            else
                pline("%s %s.", objphrase, glowcolor);
            iflags.last_msg = PLNMSG_OBJ_GLOWS;
            targobj->bknown = !Hallucination;
        } else {
            /* didn't see what happened:  forget the BUC state if that was
               known unless the bless/curse state of the water is known;
               without this, hero would know the new state even without
               seeing the glow; priest[ess] will immediately relearn it */
            if (!potion->bknown || !potion->dknown)
                targobj->bknown = 0;
            /* [should the bknown+dknown exception require that water
               be discovered or at least named?] */
        }
        /* potions of water are the only shop goods whose price depends
           on their curse/bless state */
        if (targobj->unpaid && targobj->otyp == POT_WATER) {
            if (costchange == COST_alter)
                /* added blessing or cursing; update shop
                   bill to reflect item's new higher price */
                alter_cost(targobj, 0L);
            else if (costchange != COST_none)
                /* removed blessing or cursing; you
                   degraded it, now you'll have to buy it... */
                costly_alteration(targobj, costchange);
        }
        /* finally, change curse/bless state */
        (*func)(targobj);
        res = TRUE;
    }
    return res;
#undef COST_alter
#undef COST_none
}

/* used when blessed or cursed scroll of light interacts with artifact light;
   if the lit object (Sunsword or gold dragon scales/mail) doesn't resist,
   treat like dipping it in holy or unholy water (BUC change, glow message) */
void
impact_arti_light(
    struct obj *obj, /* wielded Sunsword or worn gold dragon scales/mail */
    boolean worsen,  /* True: lower BUC state unless already cursed;
                      * False: raise BUC state unless already blessed */
    boolean seeit)   /* True: give "<obj> glows <color>" message */
{
    struct obj *otmp;

    /* if already worst/best BUC it can be, or if it resists, do nothing */
    if ((worsen ? obj->cursed : obj->blessed) || obj_resists(obj, 25, 75))
        return;

    /* curse() and bless() take care of maybe_adjust_light() */
    otmp = mksobj(POT_WATER, TRUE, FALSE);
    if (worsen)
        curse(otmp);
    else
        bless(otmp);
    H2Opotion_dip(otmp, obj, seeit, seeit ? Yobjnam2(obj, "glow") : "");
    dealloc_obj(otmp);
#if 0   /* defer this until caller has used up the scroll so it won't be
         * visible; player was told that it disappeared as hero read it */
    if (carried(obj)) /* carried() will always be True here */
        update_inventory();
#endif
    return;
}

/* potion obj hits monster mon, which might be youmonst; obj always used up */
void
potionhit(struct monst *mon, struct obj *obj, int how)
{
    const char *botlnam = bottlename();
    boolean isyou = (mon == &gy.youmonst);
    int distance, tx, ty;
    struct obj *saddle = (struct obj *) 0;
    boolean hit_saddle = FALSE, your_fault = (how <= POTHIT_HERO_THROW);

    if (isyou) {
        tx = u.ux, ty = u.uy;
        distance = 0;
        pline_The("%s crashes on your %s and breaks into shards.", botlnam,
                  body_part(HEAD));
        losehp(Maybe_Half_Phys(rnd(2)),
               (how == POTHIT_OTHER_THROW) ? "propelled potion" /* scatter */
                                           : "thrown potion",
               KILLED_BY_AN);
    } else {
        tx = mon->mx, ty = mon->my;
        /* sometimes it hits the saddle */
        if (((mon->misc_worn_check & W_SADDLE)
             && (saddle = which_armor(mon, W_SADDLE)))
            && (!rn2(10)
                || (obj->otyp == POT_WATER
                    && ((rnl(10) > 7 && obj->cursed)
                        || (rnl(10) < 4 && obj->blessed) || !rn2(3)))))
            hit_saddle = TRUE;
        distance = distu(tx, ty);
        if (!cansee(tx, ty)) {
            Soundeffect(se_potion_crash_and_break, 60);
            pline("Crash!");
        } else {
            char *mnam = mon_nam(mon);
            char buf[BUFSZ];

            if (hit_saddle && saddle) {
                Sprintf(buf, "%s saddle",
                        s_suffix(x_monnam(mon, ARTICLE_THE, (char *) 0,
                                          (SUPPRESS_IT | SUPPRESS_SADDLE),
                                          FALSE)));
            } else if (has_head(mon->data)) {
                Sprintf(buf, "%s %s", s_suffix(mnam),
                        (gn.notonhead ? "body" : "head"));
            } else {
                Strcpy(buf, mnam);
            }
            Soundeffect(se_potion_crash_and_break, 60);
            pline_The("%s crashes on %s and breaks into shards.", botlnam,
                      buf);
        }
        if (rn2(5) && mon->mhp > 1 && !hit_saddle)
            mon->mhp--;
    }

    /* oil doesn't instantly evaporate; Neither does a saddle hit */
    if (obj->otyp != POT_OIL && !hit_saddle && cansee(tx, ty))
        pline("%s.", Tobjnam(obj, "evaporate"));

    if (isyou) {
        switch (obj->otyp) {
        case POT_OIL:
            if (obj->lamplit)
                explode_oil(obj, u.ux, u.uy);
            else {
                pline("Yuck!  You're covered in oil!");
                if (!Glib) {
                    make_glib(rn1(5, 5));
                }
                /* possible future extension: if "vulnerability to fire" or
                 * "flammability" is ever added, this should make the hero
                 * flammable :-) */
                if (obj->dknown) {
                    makeknown(POT_OIL);
                }
            }
            break;
        case POT_POLYMORPH:
            You_feel("a little %s.", Hallucination ? "normal" : "strange");
            if (!Unchanging && !Antimagic)
                polyself(POLY_NOFLAGS);
            break;
        case POT_ACID:
            if (!Acid_resistance) {
                int dmg;

                pline("This burns%s!",
                      obj->blessed ? " a little"
                                   : obj->cursed ? " a lot" : "");
                dmg = d(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                losehp(Maybe_Half_Phys(dmg), "potion of acid", KILLED_BY_AN);
            }
            break;
        }
    } else if (hit_saddle && saddle) {
        char *mnam, buf[BUFSZ], saddle_glows[BUFSZ];
        boolean affected = FALSE;
        boolean useeit = !Blind && canseemon(mon) && cansee(tx, ty);

        mnam = x_monnam(mon, ARTICLE_THE, (char *) 0,
                        (SUPPRESS_IT | SUPPRESS_SADDLE), FALSE);
        Sprintf(buf, "%s", upstart(s_suffix(mnam)));

        switch (obj->otyp) {
        case POT_WATER:
            Snprintf(saddle_glows, sizeof(saddle_glows), "%s %s",
                     buf, aobjnam(saddle, "glow"));
            affected = H2Opotion_dip(obj, saddle, useeit, saddle_glows);
            break;
        case POT_POLYMORPH:
            /* Do we allow the saddle to polymorph? */
            break;
        }
        if (useeit && !affected)
            pline("%s %s wet.", buf, aobjnam(saddle, "get"));
    } else {
        boolean angermon = your_fault, cureblind = FALSE;

        switch (obj->otyp) {
        case POT_FULL_HEALING:
            cureblind = TRUE;
            FALLTHROUGH;
            /*FALLTHRU*/
        case POT_EXTRA_HEALING:
            if (!obj->cursed)
                cureblind = TRUE;
            FALLTHROUGH;
            /*FALLTHRU*/
        case POT_HEALING:
            if (obj->blessed)
                cureblind = TRUE;
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_illness;
 do_healing:
            angermon = FALSE;
            if (mon->mhp < mon->mhpmax) {
                healmon(mon, mon->mhpmax, 0);
                if (canseemon(mon))
                    pline("%s looks sound and hale again.", Monnam(mon));
            }
            if (cureblind)
                mcureblindness(mon, canseemon(mon));
            break;
        case POT_RESTORE_ABILITY:
        case POT_GAIN_ABILITY:
            mcureblindness(mon, canseemon(mon));
            if (mon->mconf || mon->mstun) {
                mon->mconf = 0;
                mon->mstun = 0;
                if (canseemon(mon))
                    pline("%s looks steady again.", Monnam(mon));
            }
            break;
        case POT_SICKNESS:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_healing;
            if (dmgtype(mon->data, AD_DISE)
                /* won't happen, see prior goto */
                || dmgtype(mon->data, AD_PEST)
                /* most common case */
                || resists_poison(mon)) {
                if (canseemon(mon))
                    pline("%s looks unharmed.", Monnam(mon));
                break;
            }
 do_illness:
            if (mon->mhp > 2) {
                mon->mhp /= 2;
                if (canseemon(mon))
                    pline("%s looks rather ill.", Monnam(mon));
            }
            break;
        case POT_CONFUSION:
        case POT_BOOZE:
            if (!resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mconf = TRUE;
            break;
        case POT_INVISIBILITY: {
            boolean sawit = canspotmon(mon);

            angermon = FALSE;
            mon_set_minvis(mon);
            if (sawit && !canspotmon(mon) && cansee(mon->mx, mon->my))
                map_invisible(mon->mx, mon->my);
            break;
        }
        case POT_SLEEPING:
            /* wakeup() doesn't rouse victims of temporary sleep */
            if (sleep_monst(mon, rnd(12), POTION_CLASS)) {
                pline("%s falls asleep.", Monnam(mon));
                slept_monst(mon);
            }
            break;
        case POT_PARALYSIS:
            if (mon->mcanmove) {
                /* really should be rnd(5) for consistency with players
                 * breathing potions, but...
                 */
                paralyze_monst(mon, rnd(25));
            }
            break;
        case POT_SPEED:
            angermon = FALSE;
            mon_adjust_speed(mon, 1, obj);
            break;
        case POT_BLINDNESS:
            if (haseyes(mon->data) && !mon_perma_blind(mon)) {
                int btmp = 64 + rn2(32)
                            + rn2(32) * !resist(mon, POTION_CLASS, 0, NOTELL);

                btmp += mon->mblinded;
                mon->mblinded = min(btmp, 127);
                mon->mcansee = 0;
            }
            break;
        case POT_WATER:
            if (mon_hates_blessings(mon) /* undead or demon */
                || is_were(mon->data) || is_vampshifter(mon)) {
                if (obj->blessed) {
                    pline("%s %s in pain!", Monnam(mon),
                          is_silent(mon->data) ? "writhes" : "shrieks");
                    if (!is_silent(mon->data))
                        wake_nearto(tx, ty, mon->data->mlevel * 10);
                    mon->mhp -= d(2, 6);
                    /* should only be by you */
                    if (DEADMONSTER(mon))
                        killed(mon);
                    else if (is_were(mon->data) && !is_human(mon->data))
                        new_were(mon); /* revert to human */
                } else if (obj->cursed) {
                    angermon = FALSE;
                    if (canseemon(mon))
                        pline("%s looks healthier.", Monnam(mon));
                    healmon(mon, d(2, 6), 0);
                    if (is_were(mon->data) && is_human(mon->data)
                        && !Protection_from_shape_changers)
                        new_were(mon); /* transform into beast */
                }
            } else if (mon->data == &mons[PM_GREMLIN]) {
                angermon = FALSE;
                (void) split_mon(mon, (struct monst *) 0);
            } else if (mon->data == &mons[PM_IRON_GOLEM]) {
                if (canseemon(mon))
                    pline("%s rusts.", Monnam(mon));
                mon->mhp -= d(1, 6);
                /* should only be by you */
                if (DEADMONSTER(mon))
                    killed(mon);
            }
            break;
        case POT_OIL:
            if (obj->lamplit)
                explode_oil(obj, tx, ty);
            /* no Glib for monsters */
            break;
        case POT_ACID:
            if (!resists_acid(mon) && !resist(mon, POTION_CLASS, 0, NOTELL)) {
                pline("%s %s in pain!", Monnam(mon),
                      is_silent(mon->data) ? "writhes" : "shrieks");
                if (!is_silent(mon->data))
                    wake_nearto(tx, ty, mon->data->mlevel * 10);
                mon->mhp -= d(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                if (DEADMONSTER(mon)) {
                    if (your_fault)
                        killed(mon);
                    else
                        monkilled(mon, "", AD_ACID);
                }
            }
            break;
        case POT_POLYMORPH:
            (void) bhitm(mon, obj);
            break;
        /*
        case POT_GAIN_LEVEL:
        case POT_LEVITATION:
        case POT_FRUIT_JUICE:
        case POT_MONSTER_DETECTION:
        case POT_OBJECT_DETECTION:
            break;
        */
        }
        wakeup(mon, angermon, TRUE);
    }

    /* Note: potionbreathe() does its own docall() */
    if ((distance == 0 || (distance < 3 && !rn2((1+ACURR(A_DEX))/2)))
        && (!breathless(gy.youmonst.data) || haseyes(gy.youmonst.data)))
        potionbreathe(obj);
    else if (obj->dknown && cansee(tx, ty))
        trycall(obj);

    if (*u.ushops && obj->unpaid) {
        struct monst *shkp = shop_keeper(*in_rooms(u.ux, u.uy, SHOPBASE));

        /* neither of the first two cases should be able to happen;
           only the hero should ever have an unpaid item, and only
           when inside a tended shop */
        if (!shkp) /* if shkp was killed, unpaid ought to cleared already */
            obj->unpaid = 0;
        else if (svc.context.mon_moving) /* obj thrown by monster */
            subfrombill(obj, shkp);
        else /* obj thrown by hero */
            (void) stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful,
                                FALSE);
    }
    obfree(obj, (struct obj *) 0);
}

/* vapors are inhaled or get in your eyes */
void
potionbreathe(struct obj *obj)
{
    boolean unambiguous = FALSE; /* if effect is unambiguous, call makeknown */
    boolean breathe = !breathless(gy.youmonst.data);
    boolean cansmell = breathe && olfaction(gy.youmonst.data);
    boolean eyes = eyecount(gy.youmonst.data);
    const char * eyestr =
        (eyes > 1 ? makeplural(body_part(EYE)) : body_part(EYE));
    unsigned already_in_use = obj->in_use;
    boolean skip_trycall = FALSE;
    /* in upstream NetHack, potionbreathe() is safe because no vapor effects
     * can result in strange_feeling, but in xNetHack since we call
     * peffects, it can. This is a problem if a potion hits the floor and
     * breaks, resulting in hitting the code here, on its way to useup() in
     * strange_feeling(), after the object has already been extracted from
     * lists and prior to being outright deleted. Potions that have been
     * thrown or kicked skip that useup() call, so fake that the potion we're
     * breathing has been thrown, unless there actually was an object in flight.
     */
    boolean clear_thrownobj = FALSE;
    if (!gt.thrownobj && !gk.kickedobj) {
        gt.thrownobj = obj;
        clear_thrownobj = TRUE;
    }

    if (!breathe) {
        /* currently only acid affects eyes */
        if (eyes && obj->otyp == POT_ACID && !Acid_resistance) {
            pline("The fumes sting your %s.", eyestr);
        }
        else {
            pline("The vapors don't seem to affect you.");
        }
        return;
    }

    /* after this, can assume the player is breathing the vapors so should be
     * affected by any potion effects; however, they still might not be able to
     * smell them */

    /* potion of unholy water might be wielded; prevent
       you_were() -> drop_weapon() from dropping it so that it
       remains in inventory where our caller expects it to be */
    obj->in_use = 1;

    /* wearing a wet towel protects both eyes and breathing, even when
       the breath effect might be beneficial; we still pass down to the
       naming opportunity in case potion was thrown at hero by a monster */
    switch (Half_gas_damage ? TOWEL : obj->otyp) {
    case TOWEL:
        pline("Some vapor passes harmlessly around you.");
        break;
    case POT_RESTORE_ABILITY:
    case POT_GAIN_ABILITY:
        if (obj->cursed) {
            if (cansmell) {
                pline("Ulch!  That potion smells terrible!");
            }
            break;
        } else {
            /* restore 1 point in each lost attribute */
            int i, ii, isdone;
            i = rn2(A_MAX); /* start at a random point */
            for (isdone = ii = 0; !isdone && ii < A_MAX; ii++) {
                if (ABASE(i) < AMAX(i)) {
                    ABASE(i)++;
                    /* only first found if not blessed */
                    isdone = !(obj->blessed);
                    disp.botl = TRUE;
                }
                if (++i >= A_MAX)
                    i = 0;
            }
            if (cansmell) {
                pline("Wow!  That potion smells good!");
            }
        }
        break;
    case POT_GAIN_ENERGY:
        u.uen += rnd(2);
        if (u.uen > u.uenmax) {
            u.uenmax++;
            u.uen = u.uenmax;
        }
        You_feel("a brief rush of magical energy.");
        unambiguous = TRUE;
        break;
    case POT_POLYMORPH:
        You_feel("a change coming over you, but it peters out.");
        unambiguous = TRUE;
        break;
    case POT_FULL_HEALING:
        healup(10, 0, FALSE, TRUE);
        exercise(A_CON, TRUE);
        You_feel("a little better.");
        break;
    case POT_EXTRA_HEALING:
        healup(2, 0, FALSE, !obj->cursed);
        exercise(A_CON, TRUE);
        You_feel("a little better.");
        break;
    case POT_HEALING:
        healup(1, 0, FALSE, obj->blessed);
        exercise(A_CON, TRUE);
        You_feel("a little better.");
        break;
    case POT_SICKNESS:
        if (!Role_if(PM_HEALER)) {
            if (Upolyd) {
                if (u.mh <= 5)
                    u.mh = 1;
                else
                    u.mh -= 5;
            } else {
                if (u.uhp <= 5)
                    u.uhp = 1;
                else
                    u.uhp -= 5;
            }
            disp.botl = TRUE;
            exercise(A_CON, FALSE);
        }
        if (cansmell) {
            You("smell something diseased.");
            unambiguous = TRUE;
        }
        break;
    case POT_HALLUCINATION:
        if (!(dmgtype(gy.youmonst.data, AD_HALU)
              || dmgtype(gy.youmonst.data, AD_STUN)
              || gy.youmonst.data == &mons[PM_VIOLET_FUNGUS])) {
            /* This has a longer effect than other potions because the
             * effect if quaffed is considerably longer than those other
             * potions. Also the case for blindness vapors. */
            if (!make_hallucinated(itimeout_incr(HHallucination, rn1(20, 20)),
                                   TRUE, 0L)) {
                /* either hallu is blocked, or we were already hallucinating */
                if (Hallucination) {
                    pline("The cosmicness around you exacerbates.");
                }
                else {
                    pline("You have a momentary vision.");
                }
            }
            unambiguous = TRUE;
        }
        else {
            pline("Nothing seems to happen.");
        }
        break;
    case POT_BOOZE:
        /* a whiff of alcohol isn't going to instantly confuse anyone */
        if (cansmell) {
            /* "peculiar odor" or "puff of vapor" message printed before this */
            pline("It smells like alcohol.");
            unambiguous = TRUE;
        }
        break;
    case POT_CONFUSION:
        if (!Confusion)
            You_feel("somewhat dizzy.");
        make_confused(itimeout_incr(HConfusion, rn1(10, 5)), FALSE);
        unambiguous = TRUE;
        break;
    case POT_INVISIBILITY:
        if (!Blind && !Invis) {
            pline("For an instant you %s!",
                  See_invisible ? "could see right through yourself"
                                : "couldn't see yourself");
            unambiguous = TRUE;
        }
        break;
    case POT_PARALYSIS:
        if (!Free_action) {
            pline("%s seems to be holding you.", Something);
        }
        make_paralyzed(rnd(5), FALSE, "frozen by a potion");
        unambiguous = TRUE;
        break;
    case POT_SLEEPING:
        if (!Free_action && !Sleep_resistance) {
            You_feel("rather tired.");
            nomul(-rn1(5, 5));
            gm.multi_reason = "sleeping off a magical draught";
            gn.nomovemsg = You_can_move_again;
            exercise(A_DEX, FALSE);
        }
        else {
            You("yawn.");
            monstseesu(M_SEEN_SLEEP);
        }
        unambiguous = TRUE;
        break;
    case POT_SPEED:
        if (Fast) {
            Your("legs get a bit more energy.");
        }
        else {
            Your("knees seem more flexible now.");
        }
        unambiguous = TRUE;
        incr_itimeout(&HFast, rnd(10));
        exercise(A_DEX, TRUE);
        break;
    case POT_BLINDNESS:
        if (!Blind && !Unaware) {
            pline("It suddenly gets dark.");
            unambiguous = TRUE;
        }
        make_blinded(itimeout_incr(BlindedTimeout, rn1(20, 20)), FALSE);
        if (!Blind && !Unaware)
            Your1(vision_clears);
        break;
    case POT_WATER:
        if (u.umonnum == PM_GREMLIN) {
            (void) split_mon(&gy.youmonst, (struct monst *) 0);
            unambiguous = TRUE;
        } else if (ismnum(u.ulycn)) {
            /* vapor from [un]holy water will trigger
               transformation but won't cure lycanthropy */
            if (obj->blessed && gy.youmonst.data == &mons[u.ulycn]) {
                you_unwere(FALSE);
                unambiguous = TRUE;
            }
            else if (obj->cursed && !Upolyd) {
                you_were();
                unambiguous = TRUE;
            }
        }
        break;
    case POT_ACID:
        if (Acid_resistance) {
            if (cansmell) {
                pline("It smells %s.", Hallucination ? "tangy" : "sour");
                unambiguous = TRUE;
            }
        }
        else {
            pline("The %s fumes burn your %s and %s!",
                    (Hallucination ? "amaroidal" : "acrid"),
                    (eyes ? eyestr : ""),
                    makeplural(body_part(LUNG)));
            losehp(rnd(2), "acid fumes", KILLED_BY);
            exercise(A_CON, FALSE);
            unambiguous = TRUE;
        }
        break;
    case POT_GAIN_LEVEL:
        more_experienced(5, 0);
        FALLTHROUGH;
        /* FALLTHRU */
    case POT_LEVITATION:
        You_feel("slightly elevated.");
        /* Strictly speaking, this is only unambiguous if the player has showexp
         * turned on or has it off but gains a level in the process, but it's
         * probably better not to make the identification here not based on the
         * current options. */
        unambiguous = TRUE;
        break;
    case POT_SEE_INVISIBLE:
        if (!obj->cursed) {
            make_blinded(0L, TRUE);
        }
        if (!See_invisible) {
            You("think you saw something invisible, but it vanished.");
            unambiguous = TRUE;
        }
        break;
    case POT_FRUIT_JUICE:
        if (cansmell) {
            pline("It smells %s.",
                  (obj->cursed ? (Hallucination ? "overripe" : "rotten")
                               : "sweet"));
            unambiguous = TRUE;
        }
        break;
    case POT_MONSTER_DETECTION:
        /* force uncursed monster detection */
        obj->blessed = obj->cursed = 0;
        /* this used to be unconditional unambiguous = TRUE but that's not
         * actually the case. if you fail to detect something and are a
         * beginner, you get the ambiguous strange feeling message, and will
         * just have been prompted with trycall. If the player opted out of the
         * first trycall (so it remains un-named), don't prompt again. */
        if (!peffect_monster_detection(obj))
            unambiguous = TRUE;
        else
            skip_trycall = TRUE;
        break;
    case POT_OBJECT_DETECTION:
        /* force uncursed object detection */
        obj->blessed = obj->cursed = 0;
        if (!peffect_object_detection(obj))
            unambiguous = TRUE;
        else
            skip_trycall = TRUE;
        break;
    case POT_ENLIGHTENMENT:
        You("have a brief moment of introspection.");
        unambiguous = TRUE;
        break;
    }

    if (clear_thrownobj) {
        gt.thrownobj = (struct obj *) 0;
    }
    if (!already_in_use)
        obj->in_use = 0;
    /* note: no obfree() -- that's our caller's responsibility */
    if (obj->dknown) {
        if (unambiguous)
            makeknown(obj->otyp);
        else if (!skip_trycall)
            trycall(obj);
    }
    return;
}

/* returns the potion type when o1 is dipped in o2 */
staticfn short
mixtype(struct obj *o1, struct obj *o2)
{
    int o1typ = o1->otyp, o2typ = o2->otyp;

    /* cut down on the number of cases below */
    if (o1->oclass == POTION_CLASS
        && (o2typ == POT_GAIN_LEVEL || o2typ == POT_GAIN_ENERGY
            || o2typ == POT_HEALING || o2typ == POT_EXTRA_HEALING
            || o2typ == POT_FULL_HEALING || o2typ == POT_ENLIGHTENMENT
            || o2typ == POT_FRUIT_JUICE)) {
        /* swap o1 and o2 */
        o1typ = o2->otyp;
        o2typ = o1->otyp;
    }

    switch (o1typ) {
    case POT_HEALING:
        if (o2typ == POT_SPEED)
            return POT_EXTRA_HEALING;
        FALLTHROUGH;
        /*FALLTHRU*/
    case POT_EXTRA_HEALING:
    case POT_FULL_HEALING:
        if (o2typ == POT_GAIN_LEVEL || o2typ == POT_GAIN_ENERGY) {
            return (o1typ == POT_HEALING) ? POT_EXTRA_HEALING
                   : (o1typ == POT_EXTRA_HEALING) ? POT_FULL_HEALING
                     : POT_GAIN_ABILITY;
        } else if (o1typ == POT_FULL_HEALING && o2typ == POT_GAIN_ABILITY) {
            return POT_GAIN_ENERGY;
        }
        FALLTHROUGH;
        /*FALLTHRU*/
    case UNICORN_HORN:
        switch (o2typ) {
        case POT_SICKNESS:
            return POT_FRUIT_JUICE;
        case POT_HALLUCINATION:
        case POT_BLINDNESS:
        case POT_CONFUSION:
            return POT_WATER;
        }
        break;
    case AMETHYST: /* "a-methyst" == "not intoxicated" */
        if (o2typ == POT_BOOZE)
            return POT_FRUIT_JUICE;
        break;
    case POT_GAIN_LEVEL:
    case POT_GAIN_ENERGY:
        switch (o2typ) {
        case POT_CONFUSION:
            return (rn2(3) ? POT_BOOZE : POT_ENLIGHTENMENT);
        case POT_HEALING:
            return POT_EXTRA_HEALING;
        case POT_EXTRA_HEALING:
            return POT_FULL_HEALING;
        case POT_FULL_HEALING:
            return POT_GAIN_ABILITY;
        case POT_FRUIT_JUICE:
            return POT_SEE_INVISIBLE;
        case POT_BOOZE:
            return POT_HALLUCINATION;
        }
        break;
    case POT_FRUIT_JUICE:
        switch (o2typ) {
        case POT_SICKNESS:
            return POT_SICKNESS;
        case POT_ENLIGHTENMENT:
        case POT_SPEED:
            return POT_BOOZE;
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_SEE_INVISIBLE;
        }
        break;
    case POT_ENLIGHTENMENT:
        switch (o2typ) {
        case POT_LEVITATION:
            if (rn2(3))
                return POT_GAIN_LEVEL;
            break;
        case POT_FRUIT_JUICE:
            return POT_BOOZE;
        case POT_BOOZE:
            return POT_CONFUSION;
        }
        break;
    }

    return STRANGE_OBJECT;
}

/* getobj callback for object to be dipped (not the thing being dipped into,
 * that uses drink_ok) */
staticfn int
dip_ok(struct obj *obj)
{
    if (!obj)
        return GETOBJ_DOWNPLAY;

    /* dipping gold isn't currently implemented */
    if (obj->oclass == COIN_CLASS)
        return GETOBJ_EXCLUDE;

    if (inaccessible_equipment(obj, (const char *) 0, FALSE))
        return GETOBJ_EXCLUDE_INACCESS;

    return GETOBJ_SUGGEST;
}

/* getobj callback for object to be dipped when hero has slippery hands */
staticfn int
dip_hands_ok(struct obj *obj)
{
    if (!obj && (Glib && can_reach_floor(FALSE)))
        return GETOBJ_SUGGEST;

    return dip_ok(obj);
}

/* call hold_another_object() to deal with a transformed potion; its weight
   won't have changed but it might require an extra slot that isn't available
   or it might merge into some other carried stack */
staticfn void
hold_potion(
    struct obj *potobj,
    const char *drop_fmt, const char *drop_arg,
    const char *hold_msg)
{
    int cap = near_capacity(),
        save_pickup_burden = flags.pickup_burden;

    /* prevent a drop due to current setting of the 'pickup_burden' option */
    if (flags.pickup_burden < cap)
        flags.pickup_burden = cap;
    /* remove from inventory after calculating near_capacity() */
    obj_extract_self(potobj);
    /* re-insert into inventory, possibly merging with compatible stack */
    potobj = hold_another_object(potobj, drop_fmt, drop_arg, hold_msg);
    nhUse(potobj);
    flags.pickup_burden = save_pickup_burden;
    update_inventory();
    return;
}

/* #dip command - get item to dip, then get potion to dip it into;
   precede with 'm' to bypass fountain, pool, or sink at hero's spot */
int
dodip(void)
{
    static const char Dip_[] = "Dip ";
    struct obj *potion, *obj;
    char qbuf[QBUFSZ], obuf[QBUFSZ];
    const char *shortestname; /* last resort obj name for prompt */
    uchar here = levl[u.ux][u.uy].typ;
    boolean is_hands, at_pool = is_pool(u.ux, u.uy),
            at_fountain = IS_FOUNTAIN(here), at_sink = IS_SINK(here),
            at_here = (!iflags.menu_requested
                       && (at_pool || at_fountain || at_sink));

    obj = getobj("dip", at_here ? dip_hands_ok : dip_ok, GETOBJ_PROMPT);
    if (!obj)
        return ECMD_CANCEL;
    if (inaccessible_equipment(obj, "dip", FALSE))
        return ECMD_OK;

    is_hands = (obj == &hands_obj);
    shortestname = (is_hands || is_plural(obj) || pair_of(obj)) ? "them"
                                                                : "it";
    drink_ok_extra = 0;
    /*
     * Bypass safe_qbuf() since it doesn't handle varying suffix without
     * an awful lot of support work.  Format the object once, even though
     * the fountain and pool prompts offer a lot more room for it.
     * 3.6.0 used thesimpleoname() unconditionally, which posed no risk
     * of buffer overflow but drew bug reports because it omits user-
     * supplied type name.
     * getobj: "What do you want to dip <the object> into? [xyz or ?*] "
     */
    if (is_hands) {
        Snprintf(obuf, sizeof obuf, "your %s", makeplural(body_part(HAND)));
    } else {
        Strcpy(obuf, short_oname(obj, doname, thesimpleoname,
                                 /* 128 - (24 + 54 + 1) leaves 49 for
                                    <object> */
                                 QBUFSZ - sizeof "What do you want to dip\
 into? [abdeghjkmnpqstvwyzBCEFHIKLNOQRTUWXZ#-# or ?*] "));
    }

    /* preceding #dip with 'm' skips the possibility of dipping into pools,
       fountains, and sinks plus the extra prompting which those entail */
    if (!iflags.menu_requested) {
        /* Is there a fountain to dip into here? */
        if (!can_reach_floor(FALSE)) {
            ; /* can't dip something into fountain or pool if can't reach */
        } else if (at_fountain) {
            Snprintf(qbuf, sizeof(qbuf), "%s%s into the fountain?", Dip_,
                     flags.verbose ? obuf : shortestname);
            /* "Dip <the object> into the fountain?" */
            if (y_n(qbuf) == 'y') {
                if (!is_hands)
                    obj->pickup_prev = 0;
                dipfountain(obj);
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        } else if (at_sink) {
            Snprintf(qbuf, sizeof(qbuf), "%s%s into the sink?", Dip_,
                     flags.verbose ? obuf : shortestname);
            if (y_n(qbuf) == 'y') {
                if (!is_hands)
                    obj->pickup_prev = 0;
                dipsink(obj);
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        } else if (at_pool) {
            const char *pooltype = waterbody_name(u.ux, u.uy);

            Snprintf(qbuf, sizeof(qbuf), "%s%s into the %s?", Dip_,
                     flags.verbose ? obuf : shortestname, pooltype);
            /* "Dip <the object> into the {pool, moat, &c}?" */
            if (y_n(qbuf) == 'y') {
                if (Levitation) {
                    floating_above(pooltype);
                } else if (u.usteed && !is_swimmer(u.usteed->data)
                           && P_SKILL(P_RIDING) < P_BASIC) {
                    rider_cant_reach(); /* not skilled enough to reach */
                } else if (is_hands || obj == uarmg) {
                    if (!is_hands)
                        obj->pickup_prev = 0;
                    (void) wash_hands();
                } else {
                    obj->pickup_prev = 0;
                    if (obj->otyp == POT_ACID)
                        obj->in_use = 1;
                    if (water_damage(obj, 0, TRUE) != ER_DESTROYED
                        && obj->in_use)
                        useup(obj);
                }
                return ECMD_TIME;
            }
            ++drink_ok_extra;
        }
    }

    /* "What do you want to dip <the object> into? [xyz or ?*] " */
    Snprintf(qbuf, sizeof qbuf, "dip %s into",
             flags.verbose ? obuf : shortestname);
    potion = getobj(qbuf, drink_ok, GETOBJ_NOFLAGS);
    if (!potion)
        return ECMD_CANCEL;
    return potion_dip(obj, potion);
}

/* #altdip - #dip with "what to dip?" and "what to dip it into?" asked
   in the opposite order; ignores floor water; used for context-sensitive
   inventory item-action: the potion has already been selected and is in
   cmdq ready to answer the first getobj() prompt */
int
dip_into(void)
{
    struct obj *obj, *potion;
    char qbuf[QBUFSZ];

    if (!cmdq_peek(CQ_CANNED)) {
        impossible("dip_into: where is potion?");
        return ECMD_FAIL;
    }
    /* note: drink_ok() callback for quaffing is also used to validate
       a potion to dip into */
    drink_ok_extra = 0; /* affects drink_ok(): haven't been asked about and
                         * declined to use a floor feature like a fountain */
    potion = getobj("dip", drink_ok, GETOBJ_NOFLAGS);
    if (!potion || potion->oclass != POTION_CLASS)
        return ECMD_CANCEL;

    /* "What do you want to dip into <the potion>? [abc or ?*] " */
    Snprintf(qbuf, sizeof qbuf, "dip into %s%s",
             is_plural(potion) ? "one of " : "", thesimpleoname(potion));
    obj = getobj(qbuf, dip_ok, GETOBJ_PROMPT);
    if (!obj)
        return ECMD_CANCEL;
    if (inaccessible_equipment(obj, "dip", FALSE))
        return ECMD_OK;
    return potion_dip(obj, potion);
}

staticfn void
poof(struct obj *potion)
{
    if (potion->dknown)
        trycall(potion);
    useup(potion);
}

/* called by dodip() or dip_into() after obj and potion have been chosen */
staticfn int
potion_dip(struct obj *obj, struct obj *potion)
{
    struct obj *singlepotion;
    char qbuf[QBUFSZ];
    short mixture;

    if (potion == obj && potion->quan == 1L) {
        pline("That is a potion bottle, not a Klein bottle!");
        return ECMD_OK;
    }
    if (obj == &hands_obj) {
        You("can't fit your %s into the mouth of the bottle!",
            body_part(HAND));
        return ECMD_OK;
    }

    obj->pickup_prev = 0; /* no longer 'recently picked up' */
    potion->in_use = TRUE; /* assume it will be used up */
    if (potion->otyp == POT_WATER) {
        boolean useeit = !Blind || (obj == ublindf && Blindfolded_only);
        const char *obj_glows = Yobjnam2(obj, "glow");

        if (H2Opotion_dip(potion, obj, useeit, obj_glows)) {
            poof(potion);
            return ECMD_TIME;
        }
    } else if (obj->otyp == POT_POLYMORPH || potion->otyp == POT_POLYMORPH) {
        /* some objects can't be polymorphed */
        if (obj_unpolyable(obj->otyp == POT_POLYMORPH ? potion : obj)) {
            pline1(nothing_happens);
        } else {
            short save_otyp = obj->otyp;

            /* KMH, conduct */
            if (!u.uconduct.polypiles++)
                livelog_printf(LL_CONDUCT, "polymorphed %s first item",
                               uhis());

            obj = poly_obj(obj, STRANGE_OBJECT);

            /*
             * obj might be gone:
             *  poly_obj() -> set_wear() -> Amulet_on() -> useup()
             * if obj->otyp is worn amulet and becomes AMULET_OF_CHANGE.
             */
            if (!obj) {
                makeknown(POT_POLYMORPH);
                return ECMD_TIME;
            } else if (obj->otyp != save_otyp) {
                makeknown(POT_POLYMORPH);
                useup(potion);
                prinv((char *) 0, obj, 0L);
                return ECMD_TIME;
            } else {
                pline1(nothing_seems_to_happen);
                poof(potion);
                return ECMD_TIME;
            }
        }
        potion->in_use = FALSE; /* didn't go poof */
        return ECMD_TIME;
    } else if (obj->oclass == POTION_CLASS && obj->otyp != potion->otyp) {
        int amt = (int) obj->quan;
        boolean magic;

        mixture = mixtype(obj, potion);

        magic = (mixture != STRANGE_OBJECT) ? objects[mixture].oc_magic
            : (objects[obj->otyp].oc_magic || objects[potion->otyp].oc_magic);
        Strcpy(qbuf, "The"); /* assume full stack */
        if (amt > (obj->odiluted ? 2 : magic ? 3 : 7)) {
            /* Trying to dip multiple potions will usually affect only a
               subset; pick an amount between 3 and 8, inclusive, for magic
               potion result, between 7 and N for non-magic. If diluted
               potions are being dipped, only two are affected; this is a
               balance fix to prevent cheap mass alchemy of the (very
               common) potion of healing into the (very valuable) potion of
               full healing, whilst permitting both healing->extra healing
               and extra healing->full healing. */
            if (obj->odiluted)
                amt = 2;
            else if (magic)
                amt = rnd(min(amt, 8) - (3 - 1)) + (3 - 1); /* 1..6 + 2 */
            else
                amt = rnd(amt - (7 - 1)) + (7 - 1); /* 1..(N-6) + 6 */

            if ((long) amt < obj->quan) {
                obj = splitobj(obj, (long) amt);
                Sprintf(qbuf, "%ld of the", obj->quan);
            }
        }
        /* [N of] the {obj(s)} mix(es) with [one of] {the potion}... */
        pline("%s %s %s with %s%s...", qbuf, simpleonames(obj),
              otense(obj, "mix"), (potion->quan > 1L) ? "one of " : "",
              thesimpleoname(potion));
        /* get rid of 'dippee' before potential perm_invent updates */
        useup(potion); /* now gone */
        /* Mixing potions is dangerous...
           KMH, balance patch -- acid is particularly unstable */
        if (obj->cursed || obj->otyp == POT_ACID
            || (obj->otyp == POT_OIL && obj->lamplit) || !rn2(10)) {
            /* it would be better to use up the whole stack in advance
               of the message, but we can't because we need to keep it
               around for potionbreathe() [and we can't set obj->in_use
               to 'amt' because that's not implemented] */
            int dmg = (amt + rnd(9)) * (Acid_resistance ? 1 : 2);
            obj->in_use = 1;
            pline("%sThey explode!", !Deaf ? "BOOM!  " : "");
            wake_nearto(u.ux, u.uy, (BOLT_LIM + 1) * (BOLT_LIM + 1));
            exercise(A_STR, FALSE);
            if (!breathless(gy.youmonst.data) || haseyes(gy.youmonst.data))
                potionbreathe(obj);
            useupall(obj);
            losehp(dmg, /* not physical damage */
                   "alchemic blast", KILLED_BY_AN);
            return ECMD_TIME;
        }

        obj->blessed = obj->cursed = obj->bknown = 0;
        if (Blind || Hallucination)
            obj->dknown = 0;

        if (mixture != STRANGE_OBJECT) {
            obj->otyp = mixture;
        } else {
            struct obj *otmp;

            switch (obj->odiluted ? 1 : rnd(8)) {
            case 1:
                obj->otyp = POT_WATER;
                break;
            case 2:
            case 3:
                obj->otyp = POT_SICKNESS;
                break;
            case 4:
                otmp = mkobj(POTION_CLASS, FALSE);
                obj->otyp = otmp->otyp;
                /* oil uses obj->age field differently from other potions */
                if (obj->otyp == POT_OIL || otmp->otyp == POT_OIL)
                    fixup_oil(obj, otmp);
                obfree(otmp, (struct obj *) 0);
                break;
            default:
                useupall(obj);
                pline_The("mixture %sevaporates.",
                          !Blind ? "glows brightly and " : "");
                return ECMD_TIME;
            }
        }
        obj->odiluted = (obj->otyp != POT_WATER && obj->otyp != POT_OIL);

        if (obj->otyp == POT_WATER && !Hallucination) {
            pline_The("mixture bubbles%s.", Blind ? "" : ", then clears");
        } else if (!Blind) {
            pline_The("mixture looks %s.",
                      hcolor(OBJ_DESCR(objects[obj->otyp])));
        }

        /* this is required when 'obj' was split off from a bigger stack,
           so that 'obj' will now be assigned its own inventory slot;
           it has a side-effect of merging 'obj' into another compatible
           stack if there is one, so we do it even when no split has
           been made in order to get the merge result for both cases;
           as a consequence, mixing while Fumbling drops the mixture */
        freeinv(obj);
        hold_potion(obj, "You drop %s!", doname(obj), (const char *) 0);
        return ECMD_TIME;
    }

    if (potion->otyp == POT_ACID && obj->otyp == CORPSE
        && obj->corpsenm == PM_LICHEN) {
        pline("%s %s %s around the edges.", The(cxname(obj)),
              otense(obj, "turn"), Blind ? "wrinkled"
                                   : potion->odiluted ? hcolor(NH_ORANGE)
                                     : hcolor(NH_RED));
        potion->in_use = FALSE; /* didn't go poof */
        if (potion->dknown)
            trycall(potion);
        return ECMD_TIME;
    }

    if (potion->otyp == POT_WATER && obj->otyp == TOWEL) {
        pline_The("towel soaks it up!");
        /* wetting towel already done via water_damage() in H2Opotion_dip */
        poof(potion);
        return ECMD_TIME;
    }

    if (is_poisonable(obj)) {
        if (potion->otyp == POT_SICKNESS && !obj->opoisoned) {
            char buf[BUFSZ];

            if (potion->quan > 1L)
                Sprintf(buf, "One of %s", the(xname(potion)));
            else
                Strcpy(buf, The(xname(potion)));
            pline("%s forms a coating on %s.", buf, the(xname(obj)));
            obj->opoisoned = TRUE;
            poof(potion);
            return ECMD_TIME;
        } else if (obj->opoisoned && !permapoisoned(obj)
                   && (potion->otyp == POT_HEALING
                       || potion->otyp == POT_EXTRA_HEALING
                       || potion->otyp == POT_FULL_HEALING)) {
            pline("A coating wears off %s.", the(xname(obj)));
            obj->opoisoned = 0;
            poof(potion);
            return ECMD_TIME;
        }
    }

    if (potion->otyp == POT_ACID) {
        if (erode_obj(obj, 0, ERODE_CORRODE, EF_GREASE) != ER_NOTHING) {
            poof(potion);
            return ECMD_TIME;
        }
    }

    if (potion->otyp == POT_OIL) {
        boolean wisx = FALSE;

        if (potion->lamplit) { /* burning */
            fire_damage(obj, TRUE, u.ux, u.uy);
        } else if (potion->cursed) {
            pline_The("potion spills and covers your %s with oil.",
                      fingers_or_gloves(TRUE));
            make_glib((int) (Glib & TIMEOUT) + d(2, 10));
        } else if (obj->oclass != WEAPON_CLASS && !is_weptool(obj)) {
            /* the following cases apply only to weapons */
            goto more_dips;
            /* Oil removes rust and corrosion, but doesn't unburn or repair
             * cracks.  Arrows, etc are classed as metallic due to arrowhead
             * material, but dipping in oil shouldn't repair them.
             */
        } else if ((!is_rustprone(obj) && !is_corrodeable(obj))
                   || is_ammo(obj) || (!obj->oeroded && !obj->oeroded2)) {
            /* uses up potion, doesn't set obj->greased */
            if (!Blind)
                pline("%s %s with an oily sheen.", Yname2(obj),
                      otense(obj, "gleam"));
            else /*if (!uarmg)*/
                pline("%s %s oily.", Yname2(obj), otense(obj, "feel"));
        } else {
            pline("%s %s less %s.", Yname2(obj),
                  otense(obj, !Blind ? "are" : "feel"),
                  (obj->oeroded && obj->oeroded2)
                      ? "corroded and rusty"
                      : obj->oeroded ? "rusty" : "corroded");
            if (obj->oeroded > 0)
                obj->oeroded--;
            if (obj->oeroded2 > 0)
                obj->oeroded2--;
            wisx = TRUE;
        }
        exercise(A_WIS, wisx);
        if (potion->dknown)
            makeknown(potion->otyp);
        useup(potion);
        return ECMD_TIME;
    }

    /* Restore ability has many uses.
     * Like with quaffing it, it generally doesn't have any negative effects
     * if cursed; it just won't do anything.
     * If it could do multiple things to an item (rusty -1 weapon for instance),
     * it will only fix the first case. */
    if (potion->otyp == POT_RESTORE_ABILITY && !potion->cursed) {
        boolean did_something = FALSE;
        boolean learn_it = FALSE;

        /* removing erosion from items */
        if (erosion_matters(obj) && (obj->oeroded || obj->oeroded2)) {
            obj->oeroded = obj->oeroded2 = 0;
            pline("%s as good as new!", Yobjnam2(obj, Blind ? "feel" : "look"));
            learn_it = TRUE;
            did_something = TRUE;
        }
        /* undoing a negative enchantment */
        else if (spe_means_plus(obj) && obj->spe < 0) {
            obj->spe = (potion->blessed ? 0 : obj->spe + 1);
            pline("%s %smore effective.",
                  Yobjnam2(obj, Blind ? "feel" : "look"),
                  obj->spe < 0 ? "a little " : "");
            if (obj->known)
                learn_it = TRUE;
            else
                trycall(potion);
            did_something = TRUE;
        }
        /* refreshing a faded spellbook */
        else if (obj->oclass == SPBOOK_CLASS
                 && obj->otyp != SPE_BOOK_OF_THE_DEAD
                 && obj->otyp != SPE_BLANK_PAPER && obj->otyp != SPE_NOVEL
                 && obj->spestudied > 0) {
            obj->spestudied = 0;
            if (Blind)
                pline("The pages of %s feel crisper.", yname(obj));
            else
                pline("The ink in %s becomes sharp and fresh again!",
                      yname(obj));
            learn_it = TRUE;
            did_something = TRUE;
        }
        /* resetting a cancelled thiefstone */
        else if (obj->otyp == THIEFSTONE
                && !thiefstone_ledger_valid(obj)
                && !In_endgame(&u.uz)) {
            obj->keyed_ledger = ledger_no(&u.uz);
            set_keyed_loc(obj, u.ux, u.uy);
            pline("%s for an instant.", Tobjnam(obj, "quiver"));
            learn_it = TRUE;
            did_something = TRUE;
        }
        if (learn_it && potion->dknown)
            makeknown(POT_RESTORE_ABILITY);
        if (did_something)
            useup(potion);
        return ECMD_TIME;
    }

 more_dips:

    /* Allow filling of MAGIC_LAMPs to prevent identification by player */
    if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP)
        && (potion->otyp == POT_OIL)) {
        /* Turn off engine before fueling, turn off fuel too :-)  */
        if (obj->lamplit || potion->lamplit) {
            useup(potion);
            explode(u.ux, u.uy, 11, d(6, 6), 0, EXPL_FIERY);
            exercise(A_WIS, FALSE);
            return ECMD_TIME;
        }
        /* Adding oil to an empty magic lamp renders it into an oil lamp */
        if ((obj->otyp == MAGIC_LAMP) && obj->spe == 0) {
            obj->otyp = OIL_LAMP;
            obj->age = 0;
        }
        if (obj->age > 1000L) {
            pline("%s %s full.", Yname2(obj), otense(obj, "are"));
            potion->in_use = FALSE; /* didn't go poof */
        } else {
            You("fill %s with oil.", yname(obj));
            check_unpaid(potion);        /* Yendorian Fuel Tax */
            /* burns more efficiently in a lamp than in a bottle;
               diluted potion provides less benefit but we don't attempt
               to track that the lamp now also has some non-oil in it */
            obj->age += (!potion->odiluted ? 4L : 3L) * potion->age / 2L;
            if (obj->age > 1500L)
                obj->age = 1500L;
            useup(potion);
            exercise(A_WIS, TRUE);
        }
        if (potion->dknown)
            makeknown(POT_OIL);
        obj->spe = 1;
        update_inventory();
        return ECMD_TIME;
    }

    potion->in_use = FALSE; /* didn't go poof */
    if ((obj->otyp == UNICORN_HORN || obj->otyp == AMETHYST)
        && (mixture = mixtype(obj, potion)) != STRANGE_OBJECT) {
        char oldbuf[BUFSZ], newbuf[BUFSZ];
        short old_otyp = potion->otyp;
        boolean old_dknown = FALSE;
        boolean more_than_one = potion->quan > 1L;

        oldbuf[0] = '\0';
        if (potion->dknown) {
            old_dknown = TRUE;
            Sprintf(oldbuf, "%s ", hcolor(OBJ_DESCR(objects[potion->otyp])));
        }
        /* with multiple merged potions, split off one and
           just clear it */
        if (potion->quan > 1L) {
            singlepotion = splitobj(potion, 1L);
        } else
            singlepotion = potion;

        costly_alteration(singlepotion, COST_NUTRLZ);
        singlepotion->otyp = mixture;
        singlepotion->blessed = 0;
        if (mixture == POT_WATER)
            singlepotion->cursed = singlepotion->odiluted = 0;
        else
            singlepotion->cursed = obj->cursed; /* odiluted left as-is */
        singlepotion->bknown = FALSE;
        if (Blind) {
            singlepotion->dknown = FALSE;
        } else {
            singlepotion->dknown = !Hallucination;
            *newbuf = '\0';
            if (mixture == POT_WATER && singlepotion->dknown)
                Sprintf(newbuf, "clears");
            else if (!Blind)
                Sprintf(newbuf, "turns %s",
                        hcolor(OBJ_DESCR(objects[mixture])));
            if (*newbuf)
                pline_The("%spotion%s %s.", oldbuf,
                          more_than_one ? " that you dipped into" : "",
                          newbuf);
            else
                pline("Something happens.");

            if (old_dknown
                && !objects[old_otyp].oc_name_known
                && !objects[old_otyp].oc_uname) {
                struct obj fakeobj;

                fakeobj = cg.zeroobj;
                fakeobj.dknown = 1;
                fakeobj.otyp = old_otyp;
                fakeobj.oclass = POTION_CLASS;
                docall(&fakeobj);
            }
        }
        /* remove potion from inventory and re-insert it, possibly stacking
           with compatible ones; override 'pickup_burden' while doing so */
        hold_potion(singlepotion, "You juggle and drop %s!",
                    doname(singlepotion), (const char *) 0);
        return ECMD_TIME;
    }

    if (obj->otyp == CORPSE) {
        if (potion->otyp == POT_FRUIT_JUICE) {
            switch(obj->corpsenm) {
            case PM_BROWN_MOLD:
            case PM_GREEN_MOLD:
            case PM_YELLOW_MOLD:
            case PM_RED_MOLD:
            case PM_BLACK_MOLD:
            case PM_VIOLET_FUNGUS:
                /* MRKR: Molds and fungi have various medicinal properties */
                /* AOS: This code could possibly be merged into the stuff above,
                 * but ultimately I felt that there would be too much of the
                 * above code that we have to make an exception for in this
                 * case. The dipped potion isn't actually changing right now,
                 * and its dknown, beatitude etc aren't affected. */
                pline("%s dissolves and the liquid begins fizzing %s.",
                      The(cxname_singular(obj)),
                      (objdescr_is(potion, "fizzy")
                       || objdescr_is(potion, "effervescent")) ? "even more"
                                                               : "slowly");

                /* order matters: split first, then start the timer, then try to
                 * hold_another_object.  Previously this segfaulted when
                 * hold_another_object was called first, the hero dropped the
                 * potion, and then the timer tried to start on its NULL return
                 * value. */
                if (potion->quan > 1)
                    singlepotion = splitobj(potion, 1);
                else
                    singlepotion = potion;

                singlepotion->corpsenm = obj->corpsenm;

                if (obj->cursed)
                    /* make it turn into sickness instead */
                    singlepotion->corpsenm = PM_BLACK_MOLD;

                useup(obj);
                obj_extract_self(singlepotion);
                start_timer(50 + rn2(50), TIMER_OBJECT, FERMENT,
                            obj_to_any(singlepotion));
                costly_alteration(singlepotion, COST_FERMENT);
                singlepotion = hold_another_object(singlepotion,
                                                   "You juggle and drop %s!",
                                                   doname(singlepotion),
                                                   NULL);
                update_inventory();
                return 1;

            /* no default case, other corpses won't do anything special and will
             * just fall through to "Interesting..." below. */
            }
        }
    }


    pline("Interesting...");
    return ECMD_TIME;
}

void
ferment(anything* arg, long timeout UNUSED)
{
    struct obj* potion = arg->a_obj;
    short corpsenm = potion->corpsenm;
    boolean need_newsym = FALSE;
    coordxy x, y;
    short new_otyp;

    if (!potion) {
        impossible("null potion fermenting?");
        return;
    }

    /* if it has been transformed in the meantime, silently do nothing */
    if (potion->otyp != POT_FRUIT_JUICE)
        return;

    if (corpsenm == PM_BROWN_MOLD)
        new_otyp = POT_SLEEPING;
    else if (corpsenm == PM_GREEN_MOLD)
        new_otyp = POT_ACID;
    else if (corpsenm == PM_YELLOW_MOLD)
        new_otyp = POT_CONFUSION;
    else if (corpsenm == PM_RED_MOLD)
        new_otyp = POT_BOOZE; /* it "tastes like liquid fire" */
    else if (corpsenm == PM_VIOLET_FUNGUS)
        new_otyp = POT_HALLUCINATION;
    else if (corpsenm == PM_BLACK_MOLD)
        /* not necessarily created from black mold; also can denote that
         * something went wrong in the mixing and the potion should turn into
         * sickness instead of whatever it was going to */
        new_otyp = POT_SICKNESS;
    else {
        impossible("Strange fermentation agent %d!", corpsenm);
        return;
    }

    /* corpsenm = 0 actually means giant ant, but the default value of corpsenm
     * on potions appears to be 0. For now this doesn't have any effect because
     * giant ants can't ferment, but this seems wrong... */
    potion->corpsenm = 0;
    if (get_obj_location(potion, &x, &y, 0) && !Blind && cansee(x, y)) {
        /* OBJ_MINVENT used to be shown here, but on the principle that we
         * generally can't see what's in a monster's inventory at a glance,
         * don't show anything that happens to be fermenting there. */
        if (potion->where == OBJ_INVENT) {
            pline("%s %s.", Yobjnam2(potion, "turn"),
                  hcolor(OBJ_DESCR(objects[new_otyp])));
        }
        else if (potion->where == OBJ_FLOOR) {
            You_see("%s turn %s.", an(xname(potion)),
                    hcolor(OBJ_DESCR(objects[new_otyp])));
            /* can't newsym here because otyp has to change after the message is
             * printed */
            need_newsym = TRUE;
        }
    }

    potion->otyp = new_otyp;
    if (need_newsym)
        newsym(x, y);
}

/* *monp grants a wish and then leaves the game */
void
mongrantswish(struct monst **monp)
{
    struct monst *mon = *monp;
    int mx = mon->mx, my = mon->my, glyph = glyph_at(mx, my);

    /* remove the monster first in case wish proves to be fatal
       (blasted by artifact), to keep it out of resulting bones file */
    mongone(mon);
    *monp = 0; /* inform caller that monster is gone */
    /* hide that removal from player--map is visible during wish prompt */
    tmp_at(DISP_ALWAYS, glyph);
    tmp_at(mx, my);
    /* grant the wish */
    makewish();
    /* clean up */
    tmp_at(DISP_END, 0);
}

void
djinni_from_bottle(struct obj *obj)
{
    struct monst *mtmp;
    int chance;

    if (!(mtmp = makemon(&mons[PM_DJINNI], u.ux, u.uy, MM_NOMSG))) {
        pline("It turns out to be empty.");
        return;
    }

    if (!Blind) {
        pline("In a cloud of smoke, %s emerges!", a_monnam(mtmp));
        pline("%s speaks.", Monnam(mtmp));
    } else {
        You("smell acrid fumes.");
        pline("%s speaks.", Something);
    }

    chance = rn2(5);
    if (obj->blessed)
        chance = (chance == 4) ? rnd(4) : 0;
    else if (obj->cursed)
        chance = (chance == 0) ? rn2(4) : 4;
    /* 0,1,2,3,4:  b=80%,5,5,5,5; nc=20%,20,20,20,20; c=5%,5,5,5,80 */

    SetVoice(mtmp, 0, 80, 0);
    switch (chance) {
    case 0:
        verbalize("I am in your debt.  I will grant one wish!");
        /* give a wish and discard the monster (mtmp set to null) */
        mongrantswish(&mtmp);
        break;
    case 1:
        /* if the player is trying to play petless, make it safe for them to rub
         * lamps */
        if (u.uconduct.pets) {
            verbalize("Thank you for freeing me!");
            (void) tamedog(mtmp, (struct obj *) 0, FALSE, FALSE);
            break;
        }
        FALLTHROUGH;
        /* else FALLTHRU */
    case 2:
        verbalize("You freed me!");
        mtmp->mpeaceful = TRUE;
        set_malign(mtmp);
        break;
    case 3:
        verbalize("It is about time!");
        if (canspotmon(mtmp))
            pline("%s vanishes.", Monnam(mtmp));
        mongone(mtmp);
        break;
    default:
        verbalize("You disturbed me, fool!");
        mtmp->mpeaceful = FALSE;
        set_malign(mtmp);
        break;
    }
}

/* clone a gremlin or mold (2nd arg non-null implies heat as the trigger);
   hit points are cut in half (odd HP stays with original) */
struct monst *
split_mon(
    struct monst *mon,  /* monster being split */
    struct monst *mtmp) /* optional attacker whose heat triggered it */
{
    struct monst *mtmp2;
    char reason[BUFSZ];

    reason[0] = '\0';
    if (mtmp)
        Sprintf(reason, " from %s heat",
                (mtmp == &gy.youmonst) ? the_your[1]
                                    : (const char *) s_suffix(mon_nam(mtmp)));

    if (mon == &gy.youmonst) {
        if (u.mh > u.mhmax) /* sanity precaution */
            u.mh = u.mhmax;
        mtmp2 = (u.mh > 1) ? cloneu() : (struct monst *) 0;
        if (mtmp2) {
            /* mtmp2 has been created with mhpmax = u.mhmax, mhp = u.mh / 2,
               and u.mh -= mtmp2->mhp; these reductions for both max hp
               can't make either of them exceed corresponding current hp */
            mtmp2->mhpmax = u.mhmax / 2;
            u.mhmax -= mtmp2->mhpmax;
            disp.botl = TRUE;
            You("multiply%s!", reason);
        }
    } else {
        if (mon->mhp > mon->mhpmax) /* sanity precaution */
            mon->mhp = mon->mhpmax;
        mtmp2 = (mon->mhp > 1) ? clone_mon(mon, 0, 0) : (struct monst *) 0;
        if (mtmp2) {
            assert(mon->mhpmax >= mon->mhp); /* mon->mhpmax > 1 */
            /* mtmp2 has been created with mhpmax = mon->mhpmax,
               mhp = mon->mhp / 2, and mon->mh -= mtmp2->mhp;
               dividing max by 2 can't result in it exceeding current */
            mtmp2->mhpmax = mon->mhpmax / 2;
            mon->mhpmax -= mtmp2->mhpmax;
            if (canspotmon(mon))
                pline("%s multiplies%s!", Monnam(mon), reason);
        }
    }
    return mtmp2;
}

/* Character becomes very fast temporarily. */
void
speed_up(long duration)
{
   if (!Very_fast)
       You("are suddenly moving %sfaster.", Fast ? "" : "much ");
   else
       Your("%s get new energy.", makeplural(body_part(LEG)));

   exercise(A_DEX, TRUE);
   incr_itimeout(&HFast, duration);
}

/*potion.c*/
