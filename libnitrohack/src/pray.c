/* Copyright (c) Benson I. Margulies, Mike Stephenson, Steve Linhart, 1989. */
/* NitroHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "epri.h"

static int prayer_done(void);
static struct obj *worst_cursed_item(void);
static int in_trouble(void);
static void fix_worst_trouble(int);
static void angrygods(aligntyp);
static void at_your_feet(const char *);
static void gcrownu(void);
static void pleased(aligntyp);
static void godvoice(aligntyp,const char*);
static void god_zaps_you(aligntyp);
static void fry_by_god(aligntyp);
static void gods_angry(aligntyp);
static void gods_upset(aligntyp);
static void consume_offering(struct obj *);
static boolean water_prayer(boolean);
static boolean blocked_boulder(int,int);

/* simplify a few tests */
#define Cursed_obj(obj,typ) ((obj) && (obj)->otyp == (typ) && (obj)->cursed)

/*
 * Logic behind deities and altars and such:
 * + prayers are made to your god if not on an altar, and to the altar's god
 *   if you are on an altar
 * + If possible, your god answers all prayers, which is why bad things happen
 *   if you try to pray on another god's altar
 * + sacrifices work basically the same way, but the other god may decide to
 *   accept your allegiance, after which they are your god.  If rejected,
 *   your god takes over with your punishment.
 * + if you're in Gehennom, all messages come from Moloch
 */

/*
 *	Moloch, who dwells in Gehennom, is the "renegade" cruel god
 *	responsible for the theft of the Amulet from Marduk, the Creator.
 *	Moloch is unaligned.
 */
static const char *const Moloch = "Moloch";

static const char *const godvoices[] = {
    "booms out",
    "thunders",
    "rings out",
    "booms",
};

/* values calculated when prayer starts, and used when completed */
static aligntyp p_aligntyp;
static int p_trouble;
static int p_type; /* (-1)-3: (-1)=really naughty, 3=really good */

#define PIOUS 20
#define DEVOUT 14
#define FERVENT 9
#define STRIDENT 4

/*
 * The actual trouble priority is determined by the order of the
 * checks performed in in_trouble() rather than by these numeric
 * values, so keep that code and these values synchronized in
 * order to have the values be meaningful.
 */

#define TROUBLE_STONED			13
#define TROUBLE_SLIMED			12
#define TROUBLE_STRANGLED		11
#define TROUBLE_LAVA			10
#define TROUBLE_SICK			 9
#define TROUBLE_STARVING		 8
#define TROUBLE_HIT			 7
#define TROUBLE_LYCANTHROPE		 6
#define TROUBLE_COLLAPSING		 5
#define TROUBLE_STUCK_IN_WALL		 4
#define TROUBLE_CURSED_LEVITATION	 3
#define TROUBLE_UNUSEABLE_HANDS		 2
#define TROUBLE_CURSED_BLINDFOLD	 1

#define TROUBLE_PUNISHED	       (-1)
#define TROUBLE_FUMBLING	       (-2)
#define TROUBLE_CURSED_ITEMS	       (-3)
#define TROUBLE_SADDLE		       (-4)
#define TROUBLE_BLIND		       (-5)
#define TROUBLE_POISONED	       (-6)
#define TROUBLE_WOUNDED_LEGS	       (-7)
#define TROUBLE_HUNGRY		       (-8)
#define TROUBLE_STUNNED		       (-9)
#define TROUBLE_CONFUSED	      (-10)
#define TROUBLE_HALLUCINATION	      (-11)

/* We could force rehumanize of polyselfed people, but we can't tell
   unintentional shape changes from the other kind. Oh well.
   3.4.2: make an exception if polymorphed into a form which lacks
   hands; that's a case where the ramifications override this doubt.
 */

/* Return 0 if nothing particular seems wrong, positive numbers for
   serious trouble, and negative numbers for comparative annoyances. This
   returns the worst problem. There may be others, and the gods may fix
   more than one.

This could get as bizarre as noting surrounding opponents, (or hostile dogs),
but that's really hard.
 */

#define ugod_is_angry() (u.ualign.record < 0)
#define on_altar()	IS_ALTAR(level->locations[u.ux][u.uy].typ)
#define on_shrine()	((level->locations[u.ux][u.uy].altarmask & AM_SHRINE) != 0)
#define a_align(x,y)	((aligntyp)Amask2align(level->locations[x][y].altarmask & AM_MASK))

/* Borrowed from eat.c */

#define SATIATED	0
#define NOT_HUNGRY	1
#define HUNGRY		2
#define WEAK		3
#define FAINTING	4
#define FAINTED		5
#define STARVED		6


static int in_trouble(void)
{
	struct obj *otmp;
	int i, j, count=0;

	/*
	 * major troubles
	 */
	if (Stoned) return TROUBLE_STONED;
	if (Slimed) return TROUBLE_SLIMED;
	if (Strangled) return TROUBLE_STRANGLED;
	if (u.utrap && u.utraptype == TT_LAVA) return TROUBLE_LAVA;
	if (Sick) return TROUBLE_SICK;
	if (u.uhs >= WEAK) return TROUBLE_STARVING;
	if (Upolyd ? (u.mh <= 5 || u.mh*7 <= u.mhmax) :
		(u.uhp <= 5 || u.uhp*7 <= u.uhpmax)) return TROUBLE_HIT;
	if (u.ulycn >= LOW_PM) return TROUBLE_LYCANTHROPE;
	if (near_capacity() >= EXT_ENCUMBER && AMAX(A_STR)-ABASE(A_STR) > 3)
		return TROUBLE_COLLAPSING;

	for (i= -1; i<=1; i++) for (j= -1; j<=1; j++) {
		if (!i && !j) continue;
		if (!isok(u.ux+i, u.uy+j) || IS_ROCK(level->locations[u.ux+i][u.uy+j].typ)
		    || (blocked_boulder(i,j) && !throws_rocks(youmonst.data)))
			count++;
	}
	if (count == 8 && !Passes_walls)
		return TROUBLE_STUCK_IN_WALL;

	if (Cursed_obj(uarmf, LEVITATION_BOOTS) ||
		stuck_ring(uleft, RIN_LEVITATION) ||
		stuck_ring(uright, RIN_LEVITATION))
		return TROUBLE_CURSED_LEVITATION;
	if (nohands(youmonst.data) || !freehand()) {
	    /* for bag/box access [cf use_container()]...
	       make sure it's a case that we know how to handle;
	       otherwise "fix all troubles" would get stuck in a loop */
	    if (welded(uwep)) return TROUBLE_UNUSEABLE_HANDS;
	    if (Upolyd && nohands(youmonst.data) && (!Unchanging ||
		    ((otmp = unchanger()) != 0 && otmp->cursed)))
		return TROUBLE_UNUSEABLE_HANDS;
	}
	if (Blindfolded && ublindf->cursed) return TROUBLE_CURSED_BLINDFOLD;

	/*
	 * minor troubles
	 */
	if (Punished) return TROUBLE_PUNISHED;
	if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING) ||
		Cursed_obj(uarmf, FUMBLE_BOOTS))
	    return TROUBLE_FUMBLING;
	if (worst_cursed_item()) return TROUBLE_CURSED_ITEMS;
	if (u.usteed) {	/* can't voluntarily dismount from a cursed saddle */
	    otmp = which_armor(u.usteed, W_SADDLE);
	    if (Cursed_obj(otmp, SADDLE)) return TROUBLE_SADDLE;
	}

	if (Blinded > 1 && haseyes(youmonst.data)) return TROUBLE_BLIND;
	for (i=0; i<A_MAX; i++)
	    if (ABASE(i) < AMAX(i)) return TROUBLE_POISONED;
	if (Wounded_legs && !u.usteed)
		return TROUBLE_WOUNDED_LEGS;
	if (u.uhs >= HUNGRY) return TROUBLE_HUNGRY;
	if (HStun) return TROUBLE_STUNNED;
	if (HConfusion) return TROUBLE_CONFUSED;
	if (Hallucination) return TROUBLE_HALLUCINATION;
	return 0;
}

/* select an item for TROUBLE_CURSED_ITEMS */
static struct obj *worst_cursed_item(void)
{
    struct obj *otmp;

    /* if strained or worse, check for loadstone first */
    if (near_capacity() >= HVY_ENCUMBER) {
	for (otmp = invent; otmp; otmp = otmp->nobj)
	    if (Cursed_obj(otmp, LOADSTONE)) return otmp;
    }
    /* weapon takes precedence if it is interfering
       with taking off a ring or putting on a shield */
    if (welded(uwep) && (uright || bimanual(uwep))) {	/* weapon */
	otmp = uwep;
    /* gloves come next, due to rings */
    } else if (uarmg && uarmg->cursed) {		/* gloves */
	otmp = uarmg;
    /* then shield due to two handed weapons and spells */
    } else if (uarms && uarms->cursed) {		/* shield */
	otmp = uarms;
    /* then cloak due to body armor */
    } else if (uarmc && uarmc->cursed) {		/* cloak */
	otmp = uarmc;
    } else if (uarm && uarm->cursed) {			/* suit */
	otmp = uarm;
    } else if (uarmh && uarmh->cursed) {		/* helmet */
	otmp = uarmh;
    } else if (uarmf && uarmf->cursed) {		/* boots */
	otmp = uarmf;
    } else if (uarmu && uarmu->cursed) {		/* shirt */
	otmp = uarmu;
    } else if (uamul && uamul->cursed) {		/* amulet */
	otmp = uamul;
    } else if (uleft && uleft->cursed) {		/* left ring */
	otmp = uleft;
    } else if (uright && uright->cursed) {		/* right ring */
	otmp = uright;
    } else if (ublindf && ublindf->cursed) {		/* eyewear */
	otmp = ublindf;	/* must be non-blinding lenses */
    /* if weapon wasn't handled above, do it now */
    } else if (welded(uwep)) {				/* weapon */
	otmp = uwep;
    /* active secondary weapon even though it isn't welded */
    } else if (uswapwep && uswapwep->cursed && u.twoweap) {
	otmp = uswapwep;
    /* all worn items ought to be handled by now */
    } else {
	for (otmp = invent; otmp; otmp = otmp->nobj) {
	    if (!otmp->cursed) continue;
	    if (otmp->otyp == LOADSTONE || confers_luck(otmp))
		break;
	}
    }
    return otmp;
}

static void fix_worst_trouble(int trouble)
{
	int i;
	struct obj *otmp = 0;
	const char *what = NULL;
	static const char leftglow[] = "left ring softly glows",
				   rightglow[] = "right ring softly glows";

	switch (trouble) {
	    case TROUBLE_STONED:
		    pline("You feel more limber.");
		    Stoned = 0;
		    iflags.botl = 1;
		    delayed_killer = 0;
		    break;
	    case TROUBLE_SLIMED:
		    pline("The slime disappears.");
		    Slimed = 0;
		    iflags.botl = 1;
		    delayed_killer = 0;
		    break;
	    case TROUBLE_STRANGLED:
		    if (uamul && uamul->otyp == AMULET_OF_STRANGULATION) {
			pline("Your amulet vanishes!");
			useup(uamul);
		    }
		    pline("You can breathe again.");
		    Strangled = 0;
		    iflags.botl = 1;
		    break;
	    case TROUBLE_LAVA:
		    pline("You are back on solid ground.");
		    /* teleport should always succeed, but if not,
		     * just untrap them.
		     */
		    if (!safe_teleds(FALSE))
			u.utrap = 0;
		    break;
	    case TROUBLE_STARVING:
		    losestr(-1);
		    /* fall into... */
	    case TROUBLE_HUNGRY:
		    pline("Your %s feels content.", body_part(STOMACH));
		    init_uhunger();
		    iflags.botl = 1;
		    break;
	    case TROUBLE_SICK:
		    pline("You feel better.");
		    make_sick(0L, NULL, FALSE, SICK_ALL);
		    break;
	    case TROUBLE_HIT:
		    /* "fix all troubles" will keep trying if hero has
		       5 or less hit points, so make sure they're always
		       boosted to be more than that */
		    pline("You feel much better.");
		    if (Upolyd) {
			u.mhmax += rnd(5);
			if (u.mhmax <= 5) u.mhmax = 5+1;
			u.mh = u.mhmax;
		    }
		    if (u.uhpmax < u.ulevel * 5 + 11) u.uhpmax += rnd(5);
		    if (u.uhpmax <= 5) u.uhpmax = 5+1;
		    u.uhp = u.uhpmax;
		    iflags.botl = 1;
		    break;
	    case TROUBLE_COLLAPSING:
		    ABASE(A_STR) = AMAX(A_STR);
		    iflags.botl = 1;
		    break;
	    case TROUBLE_STUCK_IN_WALL:
		    pline("Your surroundings change.");
		    /* no control, but works on no-teleport levels */
		    safe_teleds(FALSE);
		    break;
	    case TROUBLE_CURSED_LEVITATION:
		    if (Cursed_obj(uarmf, LEVITATION_BOOTS)) {
			otmp = uarmf;
		    } else if ((otmp = stuck_ring(uleft,RIN_LEVITATION)) !=0) {
			if (otmp == uleft) what = leftglow;
		    } else if ((otmp = stuck_ring(uright,RIN_LEVITATION))!=0) {
			if (otmp == uright) what = rightglow;
		    }
		    goto decurse;
	    case TROUBLE_UNUSEABLE_HANDS:
		    if (welded(uwep)) {
			otmp = uwep;
			goto decurse;
		    }
		    if (Upolyd && nohands(youmonst.data)) {
			if (!Unchanging) {
			    pline("Your shape becomes uncertain.");
			    rehumanize();  /* "You return to {normal} form." */
			} else if ((otmp = unchanger()) != 0 && otmp->cursed) {
			    /* otmp is an amulet of unchanging */
			    goto decurse;
			}
		    }
		    if (nohands(youmonst.data) || !freehand())
			impossible("fix_worst_trouble: couldn't cure hands.");
		    break;
	    case TROUBLE_CURSED_BLINDFOLD:
		    otmp = ublindf;
		    goto decurse;
	    case TROUBLE_LYCANTHROPE:
		    you_unwere(TRUE);
		    break;
	/*
	 */
	    case TROUBLE_PUNISHED:
		    pline("Your chain disappears.");
		    unpunish();
		    break;
	    case TROUBLE_FUMBLING:
		    if (Cursed_obj(uarmg, GAUNTLETS_OF_FUMBLING))
			otmp = uarmg;
		    else if (Cursed_obj(uarmf, FUMBLE_BOOTS))
			otmp = uarmf;
		    goto decurse;
		    /*NOTREACHED*/
		    break;
	    case TROUBLE_CURSED_ITEMS:
		    otmp = worst_cursed_item();
		    if (otmp == uright) what = rightglow;
		    else if (otmp == uleft) what = leftglow;
decurse:
		    if (!otmp) {
			impossible("fix_worst_trouble: nothing to uncurse.");
			return;
		    }
		    uncurse(otmp);
		    if (!Blind) {
			pline("Your %s %s.", what ? what :
				(const char *)aobjnam(otmp, "softly glow"),
			     hcolor("amber"));
			otmp->bknown = TRUE;
		    }
		    update_inventory();
		    break;
	    case TROUBLE_POISONED:
		    if (Hallucination)
			pline("There's a tiger in your tank.");
		    else
			pline("You feel in good health again.");
		    for (i=0; i<A_MAX; i++) {
			if (ABASE(i) < AMAX(i)) {
				ABASE(i) = AMAX(i);
				iflags.botl = 1;
			}
		    }
		    encumber_msg();
		    break;
	    case TROUBLE_BLIND:
		{
		    int num_eyes = eyecount(youmonst.data);
		    const char *eye = body_part(EYE);

		    pline("Your %s feel%s better.",
			 (num_eyes == 1) ? eye : makeplural(eye),
			 (num_eyes == 1) ? "s" : "");
		    u.ucreamed = 0;
		    make_blinded(0L,FALSE);
		    break;
		}
	    case TROUBLE_WOUNDED_LEGS:
		    heal_legs();
		    break;
	    case TROUBLE_STUNNED:
		    make_stunned(0L,TRUE);
		    break;
	    case TROUBLE_CONFUSED:
		    make_confused(0L,TRUE);
		    break;
	    case TROUBLE_HALLUCINATION:
		    pline ("Looks like you are back in Kansas.");
		    make_hallucinated(0L,FALSE,0L);
		    break;
	    case TROUBLE_SADDLE:
		    otmp = which_armor(u.usteed, W_SADDLE);
		    uncurse(otmp);
		    if (!Blind) {
			pline("%s %s %s.",
			      s_suffix(upstart(y_monnam(u.usteed))),
			      aobjnam(otmp, "softly glow"),
			      hcolor("amber"));
			otmp->bknown = TRUE;
		    }
		    break;
	}
}

/* "I am sometimes shocked by...  the nuns who never take a bath without
 * wearing a bathrobe all the time.  When asked why, since no man can see them,
 * they reply 'Oh, but you forget the good God'.  Apparently they conceive of
 * the Deity as a Peeping Tom, whose omnipotence enables Him to see through
 * bathroom walls, but who is foiled by bathrobes." --Bertrand Russell, 1943
 * Divine wrath, dungeon walls, and armor follow the same principle.
 */
static void god_zaps_you(aligntyp resp_god)
{
	if (u.uswallow) {
	    pline("Suddenly a bolt of lightning comes down at you from the heavens!");
	    pline("It strikes %s!", mon_nam(u.ustuck));
	    if (!resists_elec(u.ustuck)) {
		pline("%s fries to a crisp!", Monnam(u.ustuck));
		/* Yup, you get experience.  It takes guts to successfully
		 * pull off this trick on your god, anyway.
		 */
		xkilled(u.ustuck, 0);
	    } else
		pline("%s seems unaffected.", Monnam(u.ustuck));
	} else {
	    pline("Suddenly, a bolt of lightning strikes you!");
	    if (Reflecting) {
		shieldeff(u.ux, u.uy);
		if (Blind)
		    pline("For some reason you're unaffected.");
		else
		    ureflects("%s reflects from your %s.", "It");
	    } else if (Shock_resistance) {
		shieldeff(u.ux, u.uy);
		pline("It seems not to affect you.");
	    } else
		fry_by_god(resp_god);
	}

	pline("%s is not deterred...", align_gname(resp_god));
	if (u.uswallow) {
	    pline("A wide-angle disintegration beam aimed at you hits %s!",
			mon_nam(u.ustuck));
	    if (!resists_disint(u.ustuck)) {
		pline("%s fries to a crisp!", Monnam(u.ustuck));
		xkilled(u.ustuck, 2); /* no corpse */
	    } else
		pline("%s seems unaffected.", Monnam(u.ustuck));
	} else {
	    pline("A wide-angle disintegration beam hits you!");

	    /* disintegrate shield and body armor before disintegrating
	     * the impudent mortal, like black dragon breath -3.
	     */
	    if (uarms && !(EReflecting & W_ARMS) &&
	    		!(EDisint_resistance & W_ARMS))
		destroy_arm(uarms);
	    if (uarmc && !(EReflecting & W_ARMC) &&
	    		!(EDisint_resistance & W_ARMC))
		destroy_arm(uarmc);
	    if (uarm && !(EReflecting & W_ARM) &&
	    		!(EDisint_resistance & W_ARM) && !uarmc)
		destroy_arm(uarm);
	    if (uarmu && !uarm && !uarmc) destroy_arm(uarmu);
	    if (!Disint_resistance)
		fry_by_god(resp_god);
	    else {
		pline("You bask in its black glow for a minute...");
		godvoice(resp_god, "I believe it not!");
	    }
	    if (Is_astralevel(&u.uz) || Is_sanctum(&u.uz)) {
		/* one more try for high altars */
		verbalize("Thou cannot escape my wrath, mortal!");
		summon_minion(resp_god, FALSE);
		summon_minion(resp_god, FALSE);
		summon_minion(resp_god, FALSE);
		verbalize("Destroy %s, my servants!", uhim());
	    }
	}
}

static void fry_by_god(aligntyp resp_god)
{
	char killerbuf[64];

	pline("You fry to a crisp.");
	killer_format = KILLED_BY;
	sprintf(killerbuf, "the wrath of %s", align_gname(resp_god));
	killer = killerbuf;
	done(DIED);
}

static void angrygods(aligntyp resp_god)
{
	int	maxanger;

	if (Inhell) resp_god = A_NONE;
	u.ublessed = 0;

	/* changed from tmp = u.ugangr + abs (u.uluck) -- rph */
	/* added test for alignment diff -dlc */
	if (resp_god != u.ualign.type)
	    maxanger =  u.ualign.record/2 + (Luck > 0 ? -Luck/3 : -Luck);
	else
	    maxanger =  3*u.ugangr +
		((Luck > 0 || u.ualign.record >= STRIDENT) ? -Luck/3 : -Luck);
	if (maxanger < 1) maxanger = 1; /* possible if bad align & good luck */
	else if (maxanger > 15) maxanger = 15;	/* be reasonable */

	switch (rn2(maxanger)) {
	    case 0:
	    case 1:	pline("You feel that %s is %s.", align_gname(resp_god),
			    Hallucination ? "bummed" : "displeased");
			break;
	    case 2:
	    case 3:
			godvoice(resp_god,NULL);
			pline("\"Thou %s, %s.\"",
			    (ugod_is_angry() && resp_god == u.ualign.type)
				? "hast strayed from the path" :
						"art arrogant",
			      youmonst.data->mlet == S_HUMAN ? "mortal" : "creature");
			verbalize("Thou must relearn thy lessons!");
			adjattrib(A_WIS, -1, FALSE);
			losexp(NULL);
			break;
	    case 6:	if (!Punished) {
			    gods_angry(resp_god);
			    punish(NULL);
			    break;
			} /* else fall thru */
	    case 4:
	    case 5:	gods_angry(resp_god);
			if (!Blind && !Antimagic)
			    pline("%s glow surrounds you.",
				  An(hcolor("black")));
			rndcurse();
			break;
	    case 7:
	    case 8:	godvoice(resp_god,NULL);
			verbalize("Thou durst %s me?",
				  (on_altar() &&
				   (a_align(u.ux,u.uy) != resp_god)) ?
				  "scorn":"call upon");
			pline("\"Then die, %s!\"",
			      youmonst.data->mlet == S_HUMAN ? "mortal" : "creature");
			summon_minion(resp_god, FALSE);
			break;

	    default:	gods_angry(resp_god);
			god_zaps_you(resp_god);
			break;
	}
	u.ublesscnt = rnz(300);
	return;
}

/* helper to print "str appears at your feet", or appropriate */
static void at_your_feet(const char *str)
{
	if (Blind) str = "Something";
	if (u.uswallow) {
	    /* barrier between you and the floor */
	    pline("%s %s into %s %s.", str, vtense(str, "drop"),
		  s_suffix(mon_nam(u.ustuck)), mbodypart(u.ustuck, STOMACH));
	} else {
	    pline("%s %s %s your %s!", str,
		  Blind ? "lands" : vtense(str, "appear"),
		  Levitation ? "beneath" : "at",
		  makeplural(body_part(FOOT)));
	}
}

#define ok_wep(o) ((o) && ((o)->oclass == WEAPON_CLASS || is_weptool(o)))
static void gcrownu(void)
{
    struct obj *obj;
    boolean already_exists, in_hand;
    short class_gift;
    int sp_no;

    HSee_invisible |= FROMOUTSIDE;
    HFire_resistance |= FROMOUTSIDE;
    HCold_resistance |= FROMOUTSIDE;
    HShock_resistance |= FROMOUTSIDE;
    HSleep_resistance |= FROMOUTSIDE;
    HPoison_resistance |= FROMOUTSIDE;
    godvoice(u.ualign.type, NULL);

    obj = ok_wep(uwep) ? uwep : 0;
    already_exists = in_hand = FALSE;	/* lint suppression */
    switch (u.ualign.type) {
    case A_LAWFUL:
	u.uevent.uhand_of_elbereth = 1;
	verbalize("I crown thee...  The Hand of Elbereth!");
	historic_event(FALSE, "were crowned as the Hand of Elbereth!");
	break;
    case A_NEUTRAL:
	u.uevent.uhand_of_elbereth = 2;
	in_hand = (uwep && uwep->oartifact == ART_VORPAL_BLADE);
	already_exists = exist_artifact(LONG_SWORD, artiname(ART_VORPAL_BLADE));
	verbalize("Thou shalt be my Envoy of Balance!");
	historic_event(FALSE, "were named as the Envoy of Balance!");
	break;
    case A_CHAOTIC:
	u.uevent.uhand_of_elbereth = 3;
	in_hand = (uwep && uwep->oartifact == ART_STORMBRINGER);
	already_exists = exist_artifact(RUNESWORD, artiname(ART_STORMBRINGER));
	verbalize("Thou art chosen to %s for My Glory!",
		  already_exists && !in_hand ? "take lives" : "steal souls");
	historic_event(FALSE, "were chosen to %s for your god's glory!",
		       already_exists && !in_hand ? "take lives" : "steal souls");
	break;
    }

    class_gift = STRANGE_OBJECT;
    /* 3.3.[01] had this in the A_NEUTRAL case below,
       preventing chaotic wizards from receiving a spellbook */
    if (Role_if (PM_WIZARD) &&
	    (!uwep || (uwep->oartifact != ART_VORPAL_BLADE &&
		       uwep->oartifact != ART_STORMBRINGER)) &&
	    !carrying(SPE_FINGER_OF_DEATH)) {
	class_gift = SPE_FINGER_OF_DEATH;
 make_splbk:
	obj = mksobj(level, class_gift, TRUE, FALSE);
	bless(obj);
	obj->bknown = TRUE;
	at_your_feet("A spellbook");
	dropy(obj);
	u.ugifts++;
	/* when getting a new book for known spell, enhance
	   currently wielded weapon rather than the book */
	for (sp_no = 0; sp_no < MAXSPELL; sp_no++)
	    if (spl_book[sp_no].sp_id == class_gift) {
		if (ok_wep(uwep)) obj = uwep;	/* to be blessed,&c */
		break;
	    }
    } else if (Role_if (PM_MONK) &&
	    (!uwep || !uwep->oartifact) &&
	    !carrying(SPE_RESTORE_ABILITY)) {
	/* monks rarely wield a weapon */
	class_gift = SPE_RESTORE_ABILITY;
	goto make_splbk;
    }

    switch (u.ualign.type) {
    case A_LAWFUL:
	if (class_gift != STRANGE_OBJECT) {
	    ;		/* already got bonus above */
	} else if (obj && obj->otyp == LONG_SWORD && !obj->oartifact) {
	    if (!Blind) pline("Your sword shines brightly for a moment.");
	    obj = oname(obj, artiname(ART_EXCALIBUR));
	    if (obj && obj->oartifact == ART_EXCALIBUR) u.ugifts++;
	}
	/* acquire Excalibur's skill regardless of weapon or gift */
	unrestrict_weapon_skill(P_LONG_SWORD);
	if (obj && obj->oartifact == ART_EXCALIBUR)
	    discover_artifact(ART_EXCALIBUR);
	break;
    case A_NEUTRAL:
	if (class_gift != STRANGE_OBJECT) {
	    ;		/* already got bonus above */
	} else if (in_hand) {
	    pline("Your %s goes snicker-snack!", xname(obj));
	    obj->dknown = TRUE;
	} else if (!already_exists) {
	    obj = mksobj(level, LONG_SWORD, FALSE, FALSE);
	    obj = oname(obj, artiname(ART_VORPAL_BLADE));
	    obj->spe = 1;
	    at_your_feet("A sword");
	    dropy(obj);
	    u.ugifts++;
	}
	/* acquire Vorpal Blade's skill regardless of weapon or gift */
	unrestrict_weapon_skill(P_LONG_SWORD);
	if (obj && obj->oartifact == ART_VORPAL_BLADE)
	    discover_artifact(ART_VORPAL_BLADE);
	break;
    case A_CHAOTIC:
      {
	char swordbuf[BUFSZ];

	sprintf(swordbuf, "%s sword", hcolor("black"));
	if (class_gift != STRANGE_OBJECT) {
	    ;		/* already got bonus above */
	} else if (in_hand) {
	    pline("Your %s hums ominously!", swordbuf);
	    obj->dknown = TRUE;
	} else if (!already_exists) {
	    obj = mksobj(level, RUNESWORD, FALSE, FALSE);
	    obj = oname(obj, artiname(ART_STORMBRINGER));
	    at_your_feet(An(swordbuf));
	    obj->spe = 1;
	    dropy(obj);
	    u.ugifts++;
	}
	/* acquire Stormbringer's skill regardless of weapon or gift */
	unrestrict_weapon_skill(P_BROAD_SWORD);
	if (obj && obj->oartifact == ART_STORMBRINGER)
	    discover_artifact(ART_STORMBRINGER);
	break;
      }
    default:
	obj = 0;	/* lint */
	break;
    }

    /* enhance weapon regardless of alignment or artifact status */
    if (ok_wep(obj)) {
	bless(obj);
	obj->oeroded = obj->oeroded2 = 0;
	obj->oerodeproof = TRUE;
	obj->bknown = obj->rknown = TRUE;
	if (obj->spe < 1) obj->spe = 1;
	/* acquire skill in this weapon */
	unrestrict_weapon_skill(weapon_type(obj));
    } else if (class_gift == STRANGE_OBJECT) {
	/* opportunity knocked, but there was nobody home... */
	pline("You feel unworthy.");
    }
    update_inventory();
    return;
}


static void pleased(aligntyp g_align)
{
	/* don't use p_trouble, worst trouble may get fixed while praying */
	int trouble = in_trouble();	/* what's your worst difficulty? */
	int pat_on_head = 0, kick_on_butt;

	pline("You feel that %s is %s.", align_gname(g_align),
	    u.ualign.record >= DEVOUT ?
	    Hallucination ? "pleased as punch" : "well-pleased" :
	    u.ualign.record >= STRIDENT ?
	    Hallucination ? "ticklish" : "pleased" :
	    Hallucination ? "full" : "satisfied");

	/* not your deity */
	if (on_altar() && p_aligntyp != u.ualign.type) {
		adjalign(-1);
		return;
	} else if (u.ualign.record < 2 && trouble <= 0) adjalign(1);

	/* depending on your luck & align level, the god you prayed to will:
	   - fix your worst problem if it's major.
	   - fix all your major problems.
	   - fix your worst problem if it's minor.
	   - fix all of your problems.
	   - do you a gratuitous favor.

	   if you make it to the the last category, you roll randomly again
	   to see what they do for you.

	   If your luck is at least 0, then you are guaranteed rescued
	   from your worst major problem. */

	if (!trouble && u.ualign.record >= DEVOUT) {
	    /* if hero was in trouble, but got better, no special favor */
	    if (p_trouble == 0) pat_on_head = 1;
	} else {
	    int action = rn1(Luck + (on_altar() ? 3 + on_shrine() : 2), 1);

	    if (!on_altar()) action = min(action, 3);
	    if (u.ualign.record < STRIDENT)
		action = (u.ualign.record > 0 || !rnl(2)) ? 1 : 0;

	    switch(min(action,5)) {
	    case 5: pat_on_head = 1;
	    case 4: do fix_worst_trouble(trouble);
		    while ((trouble = in_trouble()) != 0);
		    break;

	    case 3: fix_worst_trouble(trouble);
	    case 2: while ((trouble = in_trouble()) > 0)
		    fix_worst_trouble(trouble);
		    break;

	    case 1: if (trouble > 0) fix_worst_trouble(trouble);
	    case 0: break; /* your god blows you off, too bad */
	    }
	}

    /* note: can't get pat_on_head unless all troubles have just been
       fixed or there were no troubles to begin with; hallucination
       won't be in effect so special handling for it is superfluous */
    if (pat_on_head)
	switch(rn2((Luck + 6)>>1)) {
	case 0:	break;
	case 1:
	    if (uwep && (welded(uwep) || uwep->oclass == WEAPON_CLASS ||
			 is_weptool(uwep))) {
		char repair_buf[BUFSZ];

		*repair_buf = '\0';
		if (uwep->oeroded || uwep->oeroded2)
		    sprintf(repair_buf, " and %s now as good as new",
			    otense(uwep, "are"));

		if (uwep->cursed) {
		    uncurse(uwep);
		    uwep->bknown = TRUE;
		    if (!Blind)
			pline("Your %s %s%s.", aobjnam(uwep, "softly glow"),
			     hcolor("amber"), repair_buf);
		    else pline("You feel the power of %s over your %s.",
			u_gname(), xname(uwep));
		    *repair_buf = '\0';
		} else if (!uwep->blessed) {
		    bless(uwep);
		    uwep->bknown = TRUE;
		    if (!Blind)
			pline("Your %s with %s aura%s.",
			     aobjnam(uwep, "softly glow"),
			     an(hcolor("light blue")), repair_buf);
		    else pline("You feel the blessing of %s over your %s.",
			u_gname(), xname(uwep));
		    *repair_buf = '\0';
		}

		/* fix any rust/burn/rot damage, but don't protect
		   against future damage */
		if (uwep->oeroded || uwep->oeroded2) {
		    uwep->oeroded = uwep->oeroded2 = 0;
		    /* only give this message if we didn't just bless
		       or uncurse (which has already given a message) */
		    if (*repair_buf)
			pline("Your %s as good as new!",
			     aobjnam(uwep, Blind ? "feel" : "look"));
		}
		update_inventory();
	    }
	    break;
	case 3:
	    /* takes 2 hints to get the music to enter the stronghold */
	    if (!u.uevent.uopened_dbridge) {
		if (u.uevent.uheard_tune < 1) {
		    godvoice(g_align,NULL);
		    verbalize("Hark, %s!",
			  youmonst.data->mlet == S_HUMAN ? "mortal" : "creature");
		    verbalize(
			"To enter the castle, thou must play the right tune!");
		    u.uevent.uheard_tune++;
		    break;
		} else if (u.uevent.uheard_tune < 2) {
		    You_hear("a divine music...");
		    pline("It sounds like:  \"%s\".", tune);
		    u.uevent.uheard_tune++;
		    break;
		}
	    }
	    /* Otherwise, falls into next case */
	case 2:
	    if (!Blind)
		pline("You are surrounded by %s glow.", an(hcolor("golden")));
	    /* if any levels have been lost (and not yet regained),
	       treat this effect like blessed full healing */
	    if (u.ulevel < u.ulevelmax) {
		u.ulevelmax -= 1;	/* see potion.c */
		pluslvl(FALSE);
	    } else {
		u.uhpmax += 5;
		if (Upolyd) u.mhmax += 5;
	    }
	    u.uhp = u.uhpmax;
	    if (Upolyd) u.mh = u.mhmax;
	    ABASE(A_STR) = AMAX(A_STR);
	    if (u.uhunger < 900) init_uhunger();
	    if (u.uluck < 0) u.uluck = 0;
	    make_blinded(0L,TRUE);
	    iflags.botl = 1;
	    break;
	case 4: {
	    struct obj *otmp;
	    int any = 0;

	    if (Blind)
		pline("You feel the power of %s.", u_gname());
	    else pline("You are surrounded by %s aura.",
		     an(hcolor("light blue")));
	    for (otmp=invent; otmp; otmp=otmp->nobj) {
		if (otmp->cursed) {
		    uncurse(otmp);
		    if (!Blind) {
			pline("Your %s %s.", aobjnam(otmp, "softly glow"),
			     hcolor("amber"));
			otmp->bknown = TRUE;
			++any;
		    }
		}
	    }
	    if (any) update_inventory();
	    break;
	}
	case 5: {
	    const char *msg="\"and thus I grant thee the gift of %s!\"";
	    godvoice(u.ualign.type, "Thou hast pleased me with thy progress,");
	    if (!(HTelepat & INTRINSIC))  {
		HTelepat |= FROMOUTSIDE;
		pline(msg, "Telepathy");
		if (Blind) see_monsters();
	    } else if (!(HFast & INTRINSIC))  {
		HFast |= FROMOUTSIDE;
		pline(msg, "Speed");
	    } else if (!(HStealth & INTRINSIC))  {
		HStealth |= FROMOUTSIDE;
		pline(msg, "Stealth");
	    } else {
		if (!(HProtection & INTRINSIC))  {
		    HProtection |= FROMOUTSIDE;
		    if (!u.ublessed)  u.ublessed = rn1(3, 2);
		} else u.ublessed++;
		pline(msg, "my protection");
	    }
	    verbalize("Use it wisely in my name!");
	    break;
	}
	case 7:
	case 8:
	case 9:		/* KMH -- can occur during full moons */
	    if (flags.elbereth_enabled)
		if (u.ualign.record >= PIOUS && !u.uevent.uhand_of_elbereth) {
		    gcrownu();
		    break;
		} /* else FALLTHRU */
	case 6:	{
	    struct obj *otmp;
	    int sp_no, trycnt = u.ulevel + 1;

	    at_your_feet("An object");
	    /* not yet known spells given preference over already known ones */
	    /* Also, try to grant a spell for which there is a skill slot */
	    otmp = mkobj(level, SPBOOK_CLASS, TRUE);
	    while (--trycnt > 0) {
		if (otmp->otyp != SPE_BLANK_PAPER) {
		    for (sp_no = 0; sp_no < MAXSPELL; sp_no++)
			if (spl_book[sp_no].sp_id == otmp->otyp) break;
		    if (sp_no == MAXSPELL &&
			!P_RESTRICTED(spell_skilltype(otmp->otyp)))
			break;	/* usable, but not yet known */
		} else {
		    if (!objects[SPE_BLANK_PAPER].oc_name_known ||
			    carrying(MAGIC_MARKER)) break;
		}
		otmp->otyp = rnd_class(bases[SPBOOK_CLASS], SPE_BLANK_PAPER);
	    }
	    bless(otmp);
	    place_object(otmp, level, u.ux, u.uy);
	    break;
	}
	default:	impossible("Confused deity!");
	    break;
	}

	u.ublesscnt = rnz(350);
	kick_on_butt = u.uevent.udemigod ? 1 : 0;

	if (flags.elbereth_enabled && u.uevent.uhand_of_elbereth)
	    kick_on_butt++;

	if (kick_on_butt)
	    u.ublesscnt += kick_on_butt * rnz(1000);

	return;
}

/* either blesses or curses water on the altar,
 * returns true if it found any water here.
 */
static boolean water_prayer(boolean bless_water)
{
    struct obj* otmp;
    long changed = 0;
    boolean other = FALSE, bc_known = !(Blind || Hallucination);

    for (otmp = level->objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
	/* turn water into (un)holy water */
	if (otmp->otyp == POT_WATER &&
		(bless_water ? !otmp->blessed : !otmp->cursed)) {
	    otmp->blessed = bless_water;
	    otmp->cursed = !bless_water;
	    otmp->bknown = bc_known;
	    changed += otmp->quan;
	} else if (otmp->oclass == POTION_CLASS)
	    other = TRUE;
    }
    if (!Blind && changed) {
	pline("%s potion%s on the altar glow%s %s for a moment.",
	      ((other && changed > 1L) ? "Some of the" :
					(other ? "One of the" : "The")),
	      ((other || changed > 1L) ? "s" : ""), (changed > 1L ? "" : "s"),
	      (bless_water ? hcolor("light blue") : hcolor("black")));
    }
    return (boolean)(changed > 0L);
}

static void godvoice(aligntyp g_align, const char *words)
{
    const char *quot = "";
    if (words)
	quot = "\"";
    else
	words = "";

    pline("The voice of %s %s: %s%s%s", align_gname(g_align),
	  godvoices[rn2(SIZE(godvoices))], quot, words, quot);
}

static void gods_angry(aligntyp g_align)
{
    godvoice(g_align, "Thou hast angered me.");
}

/* The g_align god is upset with you. */
static void gods_upset(aligntyp g_align)
{
	if (g_align == u.ualign.type) u.ugangr++;
	else if (u.ugangr) u.ugangr--;
	angrygods(g_align);
}

static const char sacrifice_types[] = { FOOD_CLASS, AMULET_CLASS, 0 };

static void consume_offering(struct obj *otmp)
{
    if (Hallucination)
	switch (rn2(3)) {
	    case 0:
		pline("Your sacrifice sprouts wings and a propeller and roars away!");
		break;
	    case 1:
		pline("Your sacrifice puffs up, swelling bigger and bigger, and pops!");
		break;
	    case 2:
		pline("Your sacrifice collapses into a cloud of dancing particles and fades away!");
		break;
	}
    else if (Blind && u.ualign.type == A_LAWFUL)
	pline("Your sacrifice disappears!");
    else pline("Your sacrifice is consumed in a %s!",
	      u.ualign.type == A_LAWFUL ? "flash of light" : "burst of flame");
    if (carried(otmp)) useup(otmp);
    else useupf(otmp, 1L);
    exercise(A_WIS, TRUE);
}


int dosacrifice(struct obj *otmp)
{
    int value = 0;
    int pm;
    aligntyp altaralign = a_align(u.ux,u.uy);
    /* The real current alignment, without modification
     * by a helm of opposite alignment. */
    aligntyp real_alignment = u.ualignbase[A_CURRENT];

    if (!on_altar() || u.uswallow) {
	pline("You are not standing on an altar.");
	return 0;
    }

    if (In_endgame(&u.uz)) {
	if (otmp && !validate_object(otmp, sacrifice_types, "sacrifice"))
	    return 0;
	else if (!otmp)
	    otmp = getobj(sacrifice_types, "sacrifice");
	if (!otmp) return 0;
    } else {
	if (otmp && otmp->otyp != CORPSE) {
	    pline("You can't sacrifice that!");
	    return 0;
	}
	else if (!otmp)
	    otmp = floorfood("sacrifice", 1);
	if (!otmp) return 0;
    }
    /*
      Was based on nutritional value and aging behavior (< 50 moves).
      Sacrificing a food ration got you max luck instantly, making the
      gods as easy to please as an angry dog!

      Now only accepts corpses, based on the game's evaluation of their
      toughness.  Human and pet sacrifice, as well as sacrificing unicorns
      of your alignment, is strongly discouraged.
     */

#define MAXVALUE 24 /* Highest corpse value (besides Wiz) */

    if (otmp->otyp == CORPSE) {
	const struct permonst *ptr = &mons[otmp->corpsenm];
	struct monst *mtmp;
	extern const int monstr[];

	/* KMH, conduct */
	u.uconduct.gnostic++;

	/* you're handling this corpse, even if it was killed upon the altar */
	feel_cockatrice(otmp, TRUE);

	if (otmp->corpsenm == PM_ACID_BLOB
		|| (moves <= peek_at_iced_corpse_age(otmp) + 50)) {
	    value = monstr[otmp->corpsenm] + 1;
	    if (otmp->oeaten)
		value = eaten_stat(value, otmp);
	}

	if (your_race(ptr)) {
	    if (is_demon(youmonst.data)) {
		pline("You find the idea very satisfying.");
		exercise(A_WIS, TRUE);
	    } else if (u.ualign.type != A_CHAOTIC) {
		    pline("You'll regret this infamous offense!");
		    exercise(A_WIS, FALSE);
	    }

	    if (altaralign != A_CHAOTIC && altaralign != A_NONE) {
		/* curse the lawful/neutral altar */
		pline("The altar is stained with %s blood.", urace.adj);
		if (!Is_astralevel(&u.uz))
		    level->locations[u.ux][u.uy].altarmask = AM_CHAOTIC;
		angry_priest();
	    } else {
		struct monst *dmon;
		const char *demonless_msg;

		/* Human sacrifice on a chaotic or unaligned altar */
		/* is equivalent to demon summoning */
		if (altaralign == A_CHAOTIC && u.ualign.type != A_CHAOTIC) {
		    pline(
		     "The blood floods the altar, which vanishes in %s cloud!",
			  an(hcolor("black")));
		    level->locations[u.ux][u.uy].typ = ROOM;
		    level->locations[u.ux][u.uy].altarmask = 0;
		    newsym(u.ux, u.uy);
		    angry_priest();
		    demonless_msg = "cloud dissipates";
		} else {
		    /* either you're chaotic or altar is Moloch's or both */
		    pline("The blood covers the altar!");
		    change_luck(altaralign == A_NONE ? -2 : 2);
		    demonless_msg = "blood coagulates";
		}
		if ((pm = dlord(altaralign)) != NON_PM &&
		    (dmon = makemon(&mons[pm], level, u.ux, u.uy, NO_MM_FLAGS))) {
		    pline("You have summoned %s!", a_monnam(dmon));
		    if (sgn(u.ualign.type) == sgn(dmon->data->maligntyp))
			dmon->mpeaceful = TRUE;
		    pline("You are terrified, and unable to move.");
		    nomul(-3, "being terrified of a demon");
		} else pline("The %s.", demonless_msg);
	    }

	    if (u.ualign.type != A_CHAOTIC) {
		adjalign(-5);
		u.ugangr += 3;
		adjattrib(A_WIS, -1, TRUE);
		if (!Inhell) angrygods(u.ualign.type);
		change_luck(-5);
	    } else adjalign(5);
	    if (carried(otmp)) useup(otmp);
	    else useupf(otmp, 1L);
	    return 1;
	} else if (otmp->oxlth && otmp->oattached == OATTACHED_MONST
		    && ((mtmp = get_mtraits(otmp, FALSE)) != NULL)
		    && mtmp->mtame) {
	    /* mtmp is a temporary pointer to a tame monster's attributes,
	     * not a real monster */
	    pline("So this is how you repay loyalty?");
	    adjalign(-3);
	    value = -1;
	    HAggravate_monster |= FROMOUTSIDE;
	} else if (is_undead(ptr)) { /* Not demons--no demon corpses */
	    if (u.ualign.type != A_CHAOTIC)
		value += 1;
	} else if (is_unicorn(ptr)) {
	    int unicalign = sgn(ptr->maligntyp);

	    /* If same as altar, always a very bad action. */
	    if (unicalign == altaralign) {
		pline("Such an action is an insult to %s!",
		      (unicalign == A_CHAOTIC)
		      ? "chaos" : unicalign ? "law" : "balance");
		adjattrib(A_WIS, -1, TRUE);
		value = -5;
	    } else if (u.ualign.type == altaralign) {
		/* If different from altar, and altar is same as yours, */
		/* it's a very good action */
		if (u.ualign.record < ALIGNLIM)
		    pline("You feel appropriately %s.", align_str(u.ualign.type));
		else pline("You feel you are thoroughly on the right path.");
		adjalign(5);
		value += 3;
	    } else
		/* If sacrificing unicorn of your alignment to altar not of */
		/* your alignment, your god gets angry and it's a conversion */
		if (unicalign == u.ualign.type) {
		    u.ualign.record = -1;
		    value = 1;
		} else value += 3;
	}
    } /* corpse */

    if (otmp->otyp == AMULET_OF_YENDOR) {
	if (!Is_astralevel(&u.uz)) {
	    if (Hallucination)
		    pline("You feel homesick.");
	    else
		    pline("You feel an urge to return to the surface.");
	    return 1;
	} else {
	    /* The final Test.	Did you win? */
	    if (uamul == otmp) Amulet_off();
	    u.uevent.ascended = 1;
	    if (carried(otmp)) useup(otmp); /* well, it's gone now */
	    else useupf(otmp, 1L);
	    pline("You offer the Amulet of Yendor to %s...", a_gname());
	    /* Only true believers are rewarded. */
	    if (real_alignment != altaralign) {
		/* And the opposing team picks you up and
		   carries you off on their shoulders */
		adjalign(-99);
		pline("%s accepts your gift, and gains dominion over %s...",
		      a_gname(), align_gname(real_alignment));
		pline("%s is enraged...", align_gname(real_alignment));
		pline("Fortunately, %s permits you to live...", a_gname());
		pline("A cloud of %s smoke surrounds you...",
		      hcolor("orange"));
		done(ESCAPED);
	    } else { /* super big win */
		adjalign(10);
pline("An invisible choir sings, and you are bathed in radiance...");
		godvoice(altaralign, "Congratulations, mortal!");
		win_pause_output(P_MESSAGE);
verbalize("In return for thy service, I grant thee the gift of Immortality!");
		pline("You ascend to the status of Demigod%s...",
		    flags.female ? "dess" : "");
		historic_event(FALSE, "offered the Amulet of Yendor to %s and ascended"
		               " to the status of Demigod%s!", u_gname(), flags.female ? "dess" : "");
		done(ASCENDED);
	    }
	}
    } /* real Amulet */

    if (otmp->otyp == FAKE_AMULET_OF_YENDOR) {
	    if (flags.soundok)
		You_hear("a nearby thunderclap.");
	    if (!otmp->known) {
		pline("You realize you have made a %s.",
		    Hallucination ? "boo-boo" : "mistake");
		otmp->known = TRUE;
		change_luck(-1);
		return 1;
	    } else {
		/* don't you dare try to fool the gods */
		change_luck(-3);
		adjalign(-1);
		u.ugangr += 3;
		value = -3;
	    }
    } /* fake Amulet */

    if (value == 0) {
	pline("Nothing happens.");
	return 1;
    }

    if (altaralign != u.ualign.type &&
	(Is_astralevel(&u.uz) || Is_sanctum(&u.uz))) {
	/*
	 * REAL BAD NEWS!!! High altars cannot be converted.  Even an attempt
	 * gets the god who owns it truely pissed off.
	 */
	pline("You feel the air around you grow charged...");
	pline("Suddenly, you realize that %s has noticed you...", a_gname());
	godvoice(altaralign, "So, mortal!  You dare desecrate my High Temple!");
	/* Throw everything we have at the player */
	god_zaps_you(altaralign);
    } else if (value < 0) { /* I don't think the gods are gonna like this... */
	gods_upset(altaralign);
    } else {
	int saved_anger = u.ugangr;
	int saved_cnt = u.ublesscnt;
	int saved_luck = u.uluck;

	/* Sacrificing at an altar of a different alignment */
	if (u.ualign.type != altaralign) {
	    /* Is this a conversion ? */
	    /* An unaligned altar in Gehennom will always elicit rejection. */
	    if (ugod_is_angry() || (altaralign == A_NONE && Inhell)) {
		if (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL] &&
		   altaralign != A_NONE) {
		    pline("You have a strong feeling that %s is angry...", u_gname());
		    consume_offering(otmp);
		    pline("%s accepts your allegiance.", a_gname());

		    /* The player wears a helm of opposite alignment? */
		    if (uarmh && uarmh->otyp == HELM_OF_OPPOSITE_ALIGNMENT)
			u.ualignbase[A_CURRENT] = altaralign;
		    else
			u.ualign.type = u.ualignbase[A_CURRENT] = altaralign;
		    u.ublessed = 0;
		    iflags.botl = 1;

		    pline("You have a sudden sense of a new direction.");
		    /* Beware, Conversion is costly */
		    change_luck(-3);
		    u.ublesscnt += 300;
		    adjalign((int)(u.ualignbase[A_ORIGINAL] * (ALIGNLIM / 2)));
		} else {
		    u.ugangr += 3;
		    adjalign(-5);
		    pline("%s rejects your sacrifice!", a_gname());
		    godvoice(altaralign, "Suffer, infidel!");
		    change_luck(-5);
		    adjattrib(A_WIS, -2, TRUE);
		    if (!Inhell) angrygods(u.ualign.type);
		}
		return 1;
	    } else {
		consume_offering(otmp);
		pline("You sense a conflict between %s and %s.",
		    u_gname(), a_gname());
		if (rn2(8 + u.ulevel) > 5) {
		    struct monst *pri;
		    pline("You feel the power of %s increase.", u_gname());
		    exercise(A_WIS, TRUE);
		    change_luck(1);
		    /* Yes, this is supposed to be &=, not |= */
		    level->locations[u.ux][u.uy].altarmask &= AM_SHRINE;
		    /* the following accommodates stupid compilers */
		    level->locations[u.ux][u.uy].altarmask =
			level->locations[u.ux][u.uy].altarmask | (Align2amask(u.ualign.type));
		    if (!Blind)
			pline("The altar glows %s.",
			      hcolor(
			      u.ualign.type == A_LAWFUL ? "white" :
			      u.ualign.type ? "black" : "gray"));

		    if (rnl(u.ulevel) > 6 && u.ualign.record > 0 &&
		       rnd(u.ualign.record) > (3*ALIGNLIM)/4)
			summon_minion(altaralign, TRUE);
		    /* anger priest; test handles bones files */
		    if ((pri = findpriest(temple_occupied(u.urooms))) &&
		       !p_coaligned(pri))
			angry_priest();
		} else {
		    pline("Unluckily, you feel the power of %s decrease.",
			  u_gname());
		    change_luck(-1);
		    exercise(A_WIS, FALSE);
		    if (rnl(u.ulevel) > 6 && u.ualign.record > 0 &&
		       rnd(u.ualign.record) > (7*ALIGNLIM)/8)
			summon_minion(altaralign, TRUE);
		}
		return 1;
	    }
	}

	consume_offering(otmp);
	/* OK, you get brownie points. */
	if (u.ugangr) {
	    u.ugangr -=
		((value * (u.ualign.type == A_CHAOTIC ? 2 : 3)) / MAXVALUE);
	    if (u.ugangr < 0) u.ugangr = 0;
	    if (u.ugangr != saved_anger) {
		if (u.ugangr) {
		    pline("%s seems %s.", u_gname(),
			  Hallucination ? "groovy" : "slightly mollified");

		    if ((int)u.uluck < 0) change_luck(1);
		} else {
		    pline("%s seems %s.", u_gname(), Hallucination ?
			  "cosmic (not a new fact)" : "mollified");

		    if ((int)u.uluck < 0) u.uluck = 0;
		}
	    } else { /* not satisfied yet */
		if (Hallucination)
		    pline("The gods seem tall.");
		else pline("You have a feeling of inadequacy.");
	    }
	} else if (ugod_is_angry()) {
	    if (value > MAXVALUE) value = MAXVALUE;
	    if (value > -u.ualign.record) value = -u.ualign.record;
	    adjalign(value);
	    pline("You feel partially absolved.");
	} else if (u.ublesscnt > 0) {
	    u.ublesscnt -=
		((value * (u.ualign.type == A_CHAOTIC ? 500 : 300)) / MAXVALUE);
	    if (u.ublesscnt < 0) u.ublesscnt = 0;
	    if (u.ublesscnt != saved_cnt) {
		if (u.ublesscnt) {
		    if (Hallucination)
			pline("You realize that the gods are not like you and I.");
		    else
			pline("You have a hopeful feeling.");
		    if ((int)u.uluck < 0) change_luck(1);
		} else {
		    if (Hallucination)
			pline("Overall, there is a smell of fried onions.");
		    else
			pline("You have a feeling of reconciliation.");
		    if ((int)u.uluck < 0) u.uluck = 0;
		}
	    }
	} else {
	    int nartifacts = nartifact_exist();

	    /* you were already in pretty good standing */
	    /* The player can gain an artifact */
	    /* The chance goes down as the number of artifacts goes up */
	    if (u.ulevel > 2 && u.uluck >= 0 &&
		!rn2(10 + (2 * u.ugifts * nartifacts))) {
		otmp = mk_artifact(NULL, a_align(u.ux,u.uy));
		if (otmp) {
		    if (otmp->spe < 0) otmp->spe = 0;
		    if (otmp->cursed) uncurse(otmp);
		    otmp->oerodeproof = TRUE;
		    dropy(otmp);
		    at_your_feet("An object");
		    godvoice(u.ualign.type, "Use my gift wisely!");
		    historic_event(FALSE, "received %s from %s.",
				   artiname(otmp->oartifact), u_gname());
		    u.ugifts++;
		    u.ublesscnt = rnz(300 + (50 * nartifacts));
		    exercise(A_WIS, TRUE);
		    /* make sure we can use this weapon */
		    unrestrict_weapon_skill(weapon_type(otmp));
		    discover_artifact(otmp->oartifact);
		    return 1;
		}
	    }
	    change_luck((value * LUCKMAX) / (MAXVALUE * 2));
	    if ((int)u.uluck < 0) u.uluck = 0;
	    if (u.uluck != saved_luck) {
		if (Blind)
		    pline("You think something brushed your %s.", body_part(FOOT));
		else pline(Hallucination ?
		    "You see crabgrass at your %s.  A funny thing in a dungeon." :
		    "You glimpse a four-leaf clover at your %s.",
		    makeplural(body_part(FOOT)));
	    }
	}
    }
    return 1;
}


/* determine prayer results in advance; also used for enlightenment */
/* praying: false means no messages should be given */
boolean can_pray(boolean praying)
{
    int alignment;

    p_aligntyp = on_altar() ? a_align(u.ux,u.uy) : u.ualign.type;
    p_trouble = in_trouble();

    if (is_demon(youmonst.data) && (p_aligntyp != A_CHAOTIC)) {
	if (praying)
	    pline("The very idea of praying to a %s god is repugnant to you.",
		  p_aligntyp ? "lawful" : "neutral");
	return FALSE;
    }

    if (praying)
	pline("You begin praying to %s.", align_gname(p_aligntyp));

    if (u.ualign.type && u.ualign.type == -p_aligntyp)
	alignment = -u.ualign.record;		/* Opposite alignment altar */
    else if (u.ualign.type != p_aligntyp)
	alignment = u.ualign.record / 2;	/* Different alignment altar */
    else alignment = u.ualign.record;

    if ((p_trouble > 0) ? (u.ublesscnt > 200) : /* big trouble */
	(p_trouble < 0) ? (u.ublesscnt > 100) : /* minor difficulties */
	(u.ublesscnt > 0))			/* not in trouble */
	p_type = 0;		/* too soon... */
    else if ((int)Luck < 0 || u.ugangr || alignment < 0)
	p_type = 1;		/* too naughty... */
    else /* alignment >= 0 */ {
	if (on_altar() && u.ualign.type != p_aligntyp)
	    p_type = 2;
	else
	    p_type = 3;
    }

    if (is_undead(youmonst.data) && !Inhell &&
	(p_aligntyp == A_LAWFUL || (p_aligntyp == A_NEUTRAL && !rn2(10))))
	p_type = -1;
    /* Note:  when !praying, the random factor for neutrals makes the
       return value a non-deterministic approximation for enlightenment.
       This case should be uncommon enough to live with... */

    return !praying ? (boolean)(p_type == 3 && !Inhell) : TRUE;
}

int dopray(void)
{
    /* Confirm accidental slips of Alt-P */
    if (flags.prayconfirm)
	if (yn("Are you sure you want to pray?") == 'n')
	    return 0;

    u.uconduct.gnostic++;
    /* Praying implies that the hero is conscious and since we have
       no deafness attribute this implies that all verbalized messages
       can be heard.  So, in case the player has used the 'O' command
       to toggle this accessible flag off, force it to be on. */
    flags.soundok = 1;

    /* set up p_type and p_alignment */
    if (!can_pray(TRUE)) return 0;

    if (wizard && p_type >= 0) {
	if (yn("Force the gods to be pleased?") == 'y') {
	    u.ublesscnt = 0;
	    if (u.uluck < 0) u.uluck = 0;
	    if (u.ualign.record <= 0) u.ualign.record = 1;
	    u.ugangr = 0;
	    if (p_type < 2) p_type = 3;
	}
    }

    nomul(-3, "praying");
    nomovemsg = "You finish your prayer.";
    afternmv = prayer_done;

    if (p_type == 3 && !Inhell) {
	/* if you've been true to your god you can't die while you pray */
	if (!Blind)
	    pline("You are surrounded by a shimmering light.");
	u.uinvulnerable = TRUE;
    }

    return 1;
}

static int prayer_done(void)
{
    aligntyp alignment = p_aligntyp;

    u.uinvulnerable = FALSE;
    if (p_type == -1) {
	godvoice(alignment,
		 alignment == A_LAWFUL ?
		 "Vile creature, thou durst call upon me?" :
		 "Walk no more, perversion of nature!");
	pline("You feel like you are falling apart.");
	/* KMH -- Gods have mastery over unchanging */
	rehumanize();
	losehp(rnd(20), "residual undead turning effect", KILLED_BY_AN);
	exercise(A_CON, FALSE);
	return 1;
    }
    if (Inhell) {
	pline("Since you are in Gehennom, %s won't help you.",
	      align_gname(alignment));
	/* haltingly aligned is least likely to anger */
	if (u.ualign.record <= 0 || rnl(u.ualign.record))
	    angrygods(u.ualign.type);
	return 0;
    }

    if (p_type == 0) {
	if (on_altar() && u.ualign.type != alignment)
	    water_prayer(FALSE);
	u.ublesscnt += rnz(250);
	change_luck(-3);
	gods_upset(u.ualign.type);
    } else if (p_type == 1) {
	if (on_altar() && u.ualign.type != alignment)
	    water_prayer(FALSE);
	angrygods(u.ualign.type);	/* naughty */
    } else if (p_type == 2) {
	if (water_prayer(FALSE)) {
	    /* attempted water prayer on a non-coaligned altar */
	    u.ublesscnt += rnz(250);
	    change_luck(-3);
	    gods_upset(u.ualign.type);
	} else pleased(alignment);
    } else {
	/* coaligned */
	if (on_altar())
	    water_prayer(TRUE);
	pleased(alignment); /* nice */
    }
    return 1;
}

int doturn(void)
{	/* Knights & Priest(esse)s only please */

	struct monst *mtmp, *mtmp2;
	int once, range, xlev;

	if (!Role_if (PM_PRIEST) && !Role_if(PM_KNIGHT)) {
		/* Try to use turn undead spell. */
		if (objects[SPE_TURN_UNDEAD].oc_name_known) {
		    int sp_no;
		    for (sp_no = 0; sp_no < MAXSPELL &&
			 spl_book[sp_no].sp_id != NO_SPELL &&
			 spl_book[sp_no].sp_id != SPE_TURN_UNDEAD; sp_no++);

		    if (sp_no < MAXSPELL &&
			spl_book[sp_no].sp_id == SPE_TURN_UNDEAD)
			    return spelleffects(sp_no, TRUE);
		}

		pline("You don't know how to turn undead!");
		return 0;
	}
	u.uconduct.gnostic++;

	if ((u.ualign.type != A_CHAOTIC &&
		    (is_demon(youmonst.data) || is_undead(youmonst.data))) ||
				u.ugangr > 6 /* "Die, mortal!" */) {

		pline("For some reason, %s seems to ignore you.", u_gname());
		aggravate();
		exercise(A_WIS, FALSE);
		return 0;
	}

	if (Inhell) {
	    pline("Since you are in Gehennom, %s won't help you.", u_gname());
	    aggravate();
	    return 0;
	}
	pline("Calling upon %s, you chant an arcane formula.", u_gname());
	exercise(A_WIS, TRUE);

	/* note: does not perform unturn_dead() on victims' inventories */
	range = BOLT_LIM + (u.ulevel / 5);	/* 5 to 11 */
	range *= range;
	once = 0;
	for (mtmp = level->monlist; mtmp; mtmp = mtmp2) {
	    mtmp2 = mtmp->nmon;

	    if (DEADMONSTER(mtmp)) continue;
	    if (!cansee(mtmp->mx,mtmp->my) ||
		distu(mtmp->mx,mtmp->my) > range) continue;

	    if (!mtmp->mpeaceful && (is_undead(mtmp->data) ||
		   (is_demon(mtmp->data) && (u.ulevel > (MAXULEV/2))))) {

		    mtmp->msleeping = 0;
		    if (Confusion) {
			if (!once++)
			    pline("Unfortunately, your voice falters.");
			mtmp->mflee = 0;
			mtmp->mfrozen = 0;
			mtmp->mcanmove = 1;
		    } else if (!resist(mtmp, '\0', 0, TELL)) {
			xlev = 6;
			switch (mtmp->data->mlet) {
			    /* this is intentional, lichs are tougher
			       than zombies. */
			case S_LICH:    xlev += 2;  /*FALLTHRU*/
			case S_GHOST:   xlev += 2;  /*FALLTHRU*/
			case S_VAMPIRE: xlev += 2;  /*FALLTHRU*/
			case S_WRAITH:  xlev += 2;  /*FALLTHRU*/
			case S_MUMMY:   xlev += 2;  /*FALLTHRU*/
			case S_ZOMBIE:
			    if (u.ulevel >= xlev &&
				    !resist(mtmp, '\0', 0, NOTELL)) {
				if (u.ualign.type == A_CHAOTIC) {
				    mtmp->mpeaceful = 1;
				    set_malign(mtmp);
				} else { /* damn them */
				    killed(mtmp);
				}
				break;
			    } /* else flee */
			    /*FALLTHRU*/
			default:
			    monflee(mtmp, 0, FALSE, TRUE);
			    break;
			}
		    }
	    }
	}
	nomul(-5, "trying to turn the monsters");
	return 1;
}

const char *a_gname(void)
{
    return a_gname_at(u.ux, u.uy);
}

/* returns the name of an altar's deity */
const char *a_gname_at(xchar x, xchar y)
{
    if (!IS_ALTAR(level->locations[x][y].typ)) return NULL;

    return align_gname(a_align(x,y));
}

/* returns the name of the player's deity */
const char *u_gname(void)
{
    return align_gname(u.ualign.type);
}

const char *align_gname(aligntyp alignment)
{
    const char *gnam;

    switch (alignment) {
     case A_NONE:	gnam = Moloch; break;
     case A_LAWFUL:	gnam = urole.lgod; break;
     case A_NEUTRAL:	gnam = urole.ngod; break;
     case A_CHAOTIC:	gnam = urole.cgod; break;
     default:		impossible("unknown alignment.");
			gnam = "someone"; break;
    }
    if (*gnam == '_') ++gnam;
    return gnam;
}

/* hallucination handling for priest/minion names: select a random god
   iff character is hallucinating */
const char *halu_gname(aligntyp alignment)
{
    if (!Hallucination) return align_gname(alignment);

    return rnd_gname(ROLE_RANDOM);
}

/* select a random god based on role if provided */
const char *rnd_gname(int role)
{
    const char *gnam;
    int which;

    /* select random role if valid role supplied */
    which = (validrole(role)) ? role : randrole();
    switch (rn2(3)) {
     case 0:	gnam = roles[which].lgod; break;
     case 1:	gnam = roles[which].ngod; break;
     case 2:	gnam = roles[which].cgod; break;
     default:	gnam = 0; break;		/* lint suppression */
    }
    if (!gnam) gnam = Moloch;
    if (*gnam == '_') ++gnam;
    return gnam;
}

/* deity's title */
const char *align_gtitle(aligntyp alignment)
{
    const char *gnam, *result = "god";

    switch (alignment) {
     case A_LAWFUL:	gnam = urole.lgod; break;
     case A_NEUTRAL:	gnam = urole.ngod; break;
     case A_CHAOTIC:	gnam = urole.cgod; break;
     default:		gnam = 0; break;
    }
    if (gnam && *gnam == '_') result = "goddess";
    return result;
}

void altar_wrath(int x, int y)
{
    aligntyp altaralign = a_align(x,y);

    if (!strcmp(align_gname(altaralign), u_gname())) {
	godvoice(altaralign, "How darest thou desecrate my altar!");
	adjattrib(A_WIS, -1, FALSE);
    } else {
	pline("A voice (could it be %s?) whispers:",
	      align_gname(altaralign));
	verbalize("Thou shalt pay, infidel!");
	change_luck(-1);
    }
}

/* assumes isok() at one space away, but not necessarily at two */
static boolean blocked_boulder(int dx, int dy)
{
    struct obj *otmp;
    long count = 0L;

    for (otmp = level->objects[u.ux+dx][u.uy+dy]; otmp; otmp = otmp->nexthere) {
	if (otmp->otyp == BOULDER)
	    count += otmp->quan;
    }

    switch(count) {
	case 0: return FALSE; /* no boulders--not blocked */
	case 1: break; /* possibly blocked depending on if it's pushable */
	default: return TRUE; /* >1 boulder--blocked after they push the top
	    one; don't force them to push it first to find out */
    }

    if (!isok(u.ux+2*dx, u.uy+2*dy))
	return TRUE;
    if (IS_ROCK(level->locations[u.ux+2*dx][u.uy+2*dy].typ))
	return TRUE;
    if (sobj_at(BOULDER, level, u.ux+2*dx, u.uy+2*dy))
	return TRUE;

    return FALSE;
}

/*pray.c*/