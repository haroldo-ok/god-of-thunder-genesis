// God of Thunder - Sega Genesis Port
// move.c - Actor movement, collision, combat
//
// Ported from 1_move.c. This file contains pure gameplay logic with no
// hardware dependencies — the only changes are:
//   - int → s16 (explicit sizes for 68K)
//   - far pointers removed
//   - play_sound / display_* calls unchanged (routed to Genesis sound.c / panel.c)
//   - key_flag[] → JOY_readJoypad() wrapper (see input.c)
//   - shot collision now calls free_actor_sprite() when destroying shots

#include <genesis.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── External globals ─────────────────────────────────────────────────────────
extern s16 thor_real_y1;

// ─── Frame animation ──────────────────────────────────────────────────────────
void next_frame(ACTOR *actr) {
    actr->frame_count--;
    if (actr->frame_count <= 0) {
        actr->next++;
        if (actr->next > 3) actr->next = 0;
        actr->frame_count = actr->frame_speed;
    }
}

// ─── Geometric helpers ────────────────────────────────────────────────────────
s16 point_within(s16 x, s16 y, s16 x1, s16 y1, s16 x2, s16 y2) {
    return (x >= x1 && x <= x2 && y >= y1 && y <= y2) ? 1 : 0;
}

s16 overlap(s16 x1, s16 y1, s16 x2, s16 y2,
            s16 x3, s16 y3, s16 x4, s16 y4) {
    if (x1>=x3 && x1<=x4 && y1>=y3 && y1<=y4) return 1;
    if (x2>=x3 && x2<=x4 && y2>=y3 && y2<=y4) return 1;
    if (x1>=x3 && x1<=x4 && y2>=y3 && y2<=y4) return 1;
    if (x2>=x3 && x2<=x4 && y1>=y3 && y1<=y4) return 1;
    if (x3>=x1 && x3<=x2 && y3>=y1 && y3<=y2) return 1;
    if (x4>=x1 && x4<=x2 && y4>=y1 && y4<=y2) return 1;
    if (x3>=x1 && x3<=x2 && y4>=y1 && y4<=y2) return 1;
    if (x4>=x1 && x4<=x2 && y3>=y1 && y3<=y2) return 1;
    return 0;
}

s16 reverse_direction(ACTOR *actr) {
    if (actr->dir == 1) return 0;
    if (actr->dir == 2) return 3;
    if (actr->dir == 3) return 2;
    return 1;
}

// ─── Thor throws hammer ───────────────────────────────────────────────────────
void thor_shoots(void) {
    if ((hammer->used != 1) && (!hammer->dead) && (!thor->shot_cnt)) {
        play_sound(SWISH, 0);
        thor->shot_cnt    = 20;
        hammer->used      = 1;
        hammer->dir       = thor->dir;
        hammer->last_dir  = thor->dir;
        hammer->x         = thor->x;
        hammer->y         = thor->y + 2;
        hammer->move      = 2;
        hammer->next      = 0;
        hammer->last_x[0] = hammer->last_x[1] = hammer->x;
        hammer->last_y[0] = hammer->last_y[1] = hammer->y;
        load_actor_sprite(hammer);
    }
}

// ─── Score / penalty helpers ──────────────────────────────────────────────────
s16 kill_good_guy(void) {
    if (!killgg_inform && !thunder_flag) {
        odin_speaks(2010, 0);
        killgg_inform = 1;
    }
    add_score(-1000);
    return 0;
}

// ─── Actor takes damage ───────────────────────────────────────────────────────
void actor_damaged(ACTOR *actr, s16 damage) {
    if (!setup.skill)      damage *= 2;
    else if (setup.skill == 2) damage /= 2;

    if (!actr->vunerable && actr->type != ATYPE_SHOT &&
        (actr->solid & 0x7F) != 2) {
        actr->vunerable = STAMINA;
        if (damage >= (s16)actr->health) {
            if (actr->type != ATYPE_NPC) {
                add_score((s32)actr->init_health * 10);
                display_score();
            } else {
                kill_good_guy();
            }
            actor_destroyed(actr);
        } else {
            actr->show         = 10;
            actr->health      -= (u8)damage;
            actr->speed_count += 8;
        }
    } else if (!actr->vunerable) {
        actr->vunerable = STAMINA;
        if (actr->func_num == 4) switch_icons();
        if (actr->func_num == 7) rotate_arrows();
    }
}

// ─── Thor takes damage from actr ─────────────────────────────────────────────
void thor_damaged(ACTOR *actr) {
    s16 damage;
    actr->hit_thor = 1;
    damage = (s16)(s8)actr->strength;
    if (damage != (s16)(s8)255) {
        if (!setup.skill)      damage /= 2;
        else if (setup.skill == 2) damage *= 2;
    }
    if ((!thor->vunerable && !shield_on) || damage == (s16)(s8)255) {
        if (damage >= (s16)thor->health) {
            thor->vunerable = 40;
            thor->show      = 0;
            thor->health    = 0;
            display_health();
            exit_flag = 2;
        } else if (damage > 0) {
            thor->vunerable  = 40;
            play_sound(OW, 0);
            thor->show       = 10;
            thor->health    -= (u8)damage;
            display_health();
        }
    }
}

// ─── Actor destroyed (replace with explosion/sparkle or mark dead) ────────────
void actor_destroyed(ACTOR *actr) {
    s16 x, y, x1, y1;
    u8  r, n, t;

    if (actr->actor_num > 2) {
        x  = actr->x;
        y  = actr->y;
        x1 = actr->x;
        y1 = actr->y;
        r  = actr->rating;
        n  = actr->actor_num;
        t  = actr->type;

        // Free old sprite before overwriting the struct
        free_actor_sprite(actr);

        if (actr->func_num == 255) memcpy(actr, &explosion, sizeof(ACTOR));
        else                        memcpy(actr, &sparkle,   sizeof(ACTOR));

        actr->type       = t;
        actr->actor_num  = n;
        actr->rating     = r;
        actr->x          = x;   actr->y          = y;
        actr->last_x[0]  = x1;  actr->last_x[1]  = x;
        actr->last_y[0]  = y1;  actr->last_y[1]  = y;
        actr->speed_count = actr->speed;
        actr->used       = 1;
        actr->num_shots  = 3;   // used to flag explosion reverse
        actr->vunerable  = 255;

        // Spawn new sprite for explosion/sparkle
        load_actor_sprite(actr);
    } else {
        // Thor or Hammer: just mark dead
        free_actor_sprite(actr);
        actr->dead = 2;
        actr->used = 0;
    }
}

// ─── Actor fires a shot (internal helper) ────────────────────────────────────
static s16 _actor_shoots(ACTOR *actr, s16 dir) {
    s16 i, cx, cy;
    ACTOR *act;
    s16 t = actr->shot_type - 1;

    for (i = MAX_ENEMIES + 3; i < MAX_ACTORS; i++) {
        if (!actor[i].used && !actor[i].dead) {
            act = &actor[i];
            memcpy(act, &shot[t], sizeof(ACTOR));

            cy = (actr->size_y < act->size_y)
                 ? actr->y - (act->size_y - actr->size_y) / 2
                 : actr->y + (actr->size_y - act->size_y) / 2;
            cx = (actr->size_x < act->size_x)
                 ? actr->x - (act->size_x - actr->size_x) / 2
                 : actr->x + (actr->size_x - act->size_x) / 2;

            if (cy > 174) cy = 174;
            if (cx > 304) cx = 304;

            act->x = (s16)cx; act->y = (s16)cy;
            act->last_dir   = (u8)dir;
            act->next       = 0;
            act->dir        = (u8)dir;
            if (act->directions == 1) act->dir = 0;
            else if (act->directions == 4 && act->frames == 1) {
                act->next = (u8)dir;
                act->dir  = 0;
            }
            act->frame_count = act->frame_speed;
            act->speed_count = act->speed;
            act->last_x[0]   = act->last_x[1]  = actr->x;
            act->last_y[0]   = act->last_y[1]  = (s16)cy;
            act->used        = 1;
            act->creator     = actr->actor_num;
            act->move_count  = act->num_moves;
            act->dead        = 0;
            act->actor_num   = (u8)i;

            actr->shot_actor = (u8)i;
            actr->num_shots++;
            actr->shot_cnt = 20;
            shot_ok = 0;

            load_actor_sprite(act);
            return 1;
        }
    }
    return 0;
}

// ─── Actor shoots with line-of-sight check ────────────────────────────────────
s16 actor_shoots(ACTOR *actr, s16 dir) {
    s16 i, cx, cy, tx, ty;
    s16 icn = 140;

    cx = (actr->x + (actr->size_x / 2)) >> 4;
    cy = ((actr->y + actr->size_y) - 2) >> 4;
    tx = thor->center_x;
    ty = thor->center_y;

    if (shot[actr->shot_type - 1].flying == 1) icn = 80;

    switch (dir) {
        case 0:
            for (i = ty + 1; i <= cy; i++)
                if (scrn.icon[i][cx] < icn) return 0;
            break;
        case 1:
            for (i = cy; i <= ty; i++)
                if (scrn.icon[i][cx] < icn) return 0;
            break;
        case 2:
            for (i = tx; i < cx; i++)
                if (scrn.icon[cy][i] < icn) return 0;
            break;
        case 3:
            for (i = cx; i < tx; i++)
                if (scrn.icon[cy][i] < icn) return 0;
            break;
    }
    return _actor_shoots(actr, dir);
}

// ─── Unconditional shot (no line-of-sight check) ──────────────────────────────
void actor_always_shoots(ACTOR *actr, s16 dir) {
    _actor_shoots(actr, dir);
}

// ─── Per-tick actor update ────────────────────────────────────────────────────
void move_actor(ACTOR *actr) {
    s16 i;

    if (actr->vunerable)  actr->vunerable--;
    if (actr->shot_cnt)   actr->shot_cnt--;
    if (actr->show)       actr->show--;

    // Shooting decision
    if (!actr->shot_cnt && shot_ok) {
        if (actr->shots_allowed) {
            if (actr->num_shots < actr->shots_allowed) {
                shot_pattern_func[actr->shot_pattern](actr);
            }
        }
    }

    // Movement
    actr->speed_count--;
    if (actr->speed_count <= 0) {
        actr->speed_count = actr->move_counter
                            ? (s8)(actr->speed << 1)
                            : (s8)actr->speed;

        if (actr->type == ATYPE_SHOT)
            i = shot_movement_func[actr->move](actr);
        else
            i = movement_func[actr->move](actr);

        if (actr->directions == 2) i &= 1;
        if (i != actr->dir) actr->dir = (u8)i;
    } else {
        i = actr->dir;
    }

    // Keep x coordinate even (original game aligns to 2-pixel grid)
    actr->x &= ~1;

    // Update sprite visibility and animation
    actor_set_anim(actr);
}
