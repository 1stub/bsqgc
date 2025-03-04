#include "../src/runtime/memory/gc.h"
#include <math.h>

struct TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = NULL,
    .typekey = "Empty"
};

struct TypeInfoBase ListNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "01",
    .typekey = "ListNode"
};

typedef struct {
    float x;
    float y;
    float z;
} Position;

typedef struct {
    float vx;
    float vy;
    float vz;
} Velocity;

typedef struct {
    float fx;
    float fy;
    float fz;
} Forces;

typedef struct {
    char* name;
    float mass;
    Position pos;
    Velocity vel;
} Body;

#define GET_MASS(F) (F * SOLAR_MASS)

#define N 5
#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24
#define PAIRS (N*(N-1)/2)

Body jupiter = {
    .name = "jupiter", 
    .mass = GET_MASS(0.000954791938424326609f),
    .pos = {
        4.84143144246472090f,
        -1.16032004402742839f,
        -0.103622044471123109f
    },
    .vel = {
        0.00166007664274403694f * DAYS_PER_YEAR,
        0.00769901118419740425f * DAYS_PER_YEAR,
        -0.0000690460016972063023f * DAYS_PER_YEAR
    }
};

Body saturn = {
    .name = "saturn", 
    .mass = GET_MASS(0.000285885980666130812f),
    .pos = {
        8.34336671824457987f,
        4.12479856412430479f,
        -0.403523417114321381f
    },
    .vel = {
        -0.00276742510726862411f * DAYS_PER_YEAR,
        0.00499852801234917238f * DAYS_PER_YEAR,
        0.0000230417297573763929f * DAYS_PER_YEAR
    }
};

Body uranus = {
    .name = "uranus", 
    .mass = GET_MASS(0.0000436624404335156298f),
    .pos = {
        12.8943695621391310f,
        -15.1111514016986312f,
        -0.223307578892655734f
    },
    .vel = {
        0.00296460137564761618f * DAYS_PER_YEAR,
        0.00237847173959480950f * DAYS_PER_YEAR,
        -0.0000296589568540237556f * DAYS_PER_YEAR
    }
};

Body neptune = {
    .name = "neptune", 
    .mass = GET_MASS(0.0000515138902046611451f),
    .pos = {
        15.3796971148509165f,
        -25.9193146099879641f,
        0.179258772950371181f
    },
    .vel = {
        0.00268067772490389322f * DAYS_PER_YEAR,
        0.00162824170038242295f * DAYS_PER_YEAR,
        -0.0000951592254519715870f * DAYS_PER_YEAR
    }
};

Body sun = {
    .name = "sun", 
    .mass = SOLAR_MASS,
    .pos = {
        0.0f,
        0.0f,
        0.0f
    },
    .vel = {
        0.0f,
        0.0f,
        0.0f
    }
};

Body offsetMomemtum(Body b, float px, float py, float pz) 
{
    Body body = {
        .name = b.name,
        .mass = b.mass,
        .pos = b.pos,
        .vel = {
            -px / SOLAR_MASS,
            -py / SOLAR_MASS,
            -pz / SOLAR_MASS
        }
    }; 
    return body;   
}

float kineticEnergy(Body b) 
{
    return 0.5f * b.mass * (b.vel.vx * b.vel.vx + b.vel.vy * b.vel.vy + b.vel.vz * b.vel.vz);
}

float distance(Body b0, Body b1) 
{
    float dx = b0.pos.x - b1.pos.x;
    float dy = b0.pos.y - b1.pos.y;
    float dz = b0.pos.z - b1.pos.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

void** createNBodySystem() 
{
    void** all_bodies = (void**)allocate(&a_bin16, &ListNode);

    /* Dynamically create constant bodies in our gc pages */
    Body* gc_jupiter = (Body*)allocate(&a_bin8, &Empty);
    gc_jupiter = &jupiter;

    Body* gc_saturn = (Body*)allocate(&a_bin8, &Empty);
    gc_saturn = &saturn;

    Body* gc_uranus = (Body*)allocate(&a_bin8, &Empty);
    gc_uranus = &uranus;

    Body* gc_neptune = (Body*)allocate(&a_bin8, &Empty);
    gc_neptune = &neptune;

    Body* planets[4] = {gc_jupiter, gc_saturn, gc_uranus, gc_neptune};

    /* This does not preserve functional nature, but should be okay. */
    float px = 0.0f, py = 0.0f, pz = 0.0f;

    for (int i = 0; i < (N - 1); i++) {
        px += planets[i]->vel.vx * planets[i]->mass;
        py += planets[i]->vel.vy * planets[i]->mass;
        pz += planets[i]->vel.vz * planets[i]->mass;
    }

    Body new_sun = offsetMomemtum(sun, px, py, pz);
    Body* gc_sun = (Body*)allocate(&a_bin8, &Empty);
    gc_sun = &new_sun;

    all_bodies[0] = gc_sun;
    all_bodies[1] = (void**)allocate(&a_bin16, &ListNode);

    void** it = all_bodies[1];
    it[0] = (void*)planets[0];
    it[1] = (void**)allocate(&a_bin16, &ListNode);
    it = it[1];

    it[0] = (void*)planets[1];
    it[1] = (void**)allocate(&a_bin16, &ListNode);
    it = it[1];

    it[0] = (void*)planets[2];
    it[1] = allocate(&a_bin8, &Empty);
    it = it[1];

    it[0] = (void*)planets[3];

    return all_bodies;
}

static inline Forces getForces(void** bodies, Body* b0, float dt) {
    void** it_1 = bodies;
    Forces forces = {.fx = 0.0f, .fy = 0.0f, .fz = 0.0f};

    for (int j = 0; j < N; j++) {
        Body* b1 = (Body*)(it_1[0]); 

        if (b0->name == b1->name) {
            it_1 = it_1[1]; 
            continue;
        }

        float dx = b1->pos.x - b0->pos.x;
        float dy = b1->pos.y - b0->pos.y;
        float dz = b1->pos.z - b0->pos.z;

        float dist = distance(*b0, *b1);
        float mag = dt / (dist * dist * dist);

        forces.fx += dx * b1->mass * mag;
        forces.fy += dy * b1->mass * mag;
        forces.fz += dz * b1->mass * mag;

        it_1 = it_1[1]; 

        debug_print("%s, %s\n", b0->name, b1->name);
    }

    return forces;
}
/* advance to next system */
void** advance(void** bodies, float dt) 
{
    /* We are going to need to create a new all_bodies list from bodies arg */
    void** new_bodies = (void**)allocate(&a_bin16, &ListNode);
    void** new_bodies_it = new_bodies;

    void** it_0 = bodies;
    for(int i = 0; i < N; i++) {
        Body* b0 = (Body*)(it_0[0]);
        Forces forces = getForces(bodies, b0, dt);

        float fx = b0->vel.vx + forces.fx;
        float fy = b0->vel.vy + forces.fy;
        float fz = b0->vel.vz + forces.fz;


        Velocity nvel = {.vx = fx, .vy = fy, .vz = fz};
        Position npos = {
            .x = b0->pos.x + (fx * dt),
            .y = b0->pos.y + (fy * dt),
            .z = b0->pos.z + (fz * dt)  
        };

        Body new_body = {
            .mass = b0->mass,
            .name = b0->name,
            .pos = npos,
            .vel = nvel
        };

        Body* gc_new_body = (Body*)allocate(&a_bin8, &Empty);
        gc_new_body = &new_body;

        new_bodies_it[0] = gc_new_body;

        /* Not end of list */
        if(i < (N-1)) {
            new_bodies_it[1] = (void**)allocate(&a_bin16, &ListNode);
        } else {
            new_bodies_it[1] = (void**)allocate(&a_bin8, &Empty);
        }

        new_bodies_it = new_bodies_it[1];
        it_0 = it_0[1];
    }

    return new_bodies;
}

int main() 
{
    initializeStartup();

    register void* rbp asm("rbp");
    initializeThreadLocalInfo(rbp);

    void** sys = createNBodySystem();
    float step = 0.01;
    
    sys = advance(sys, step);
    sys = advance(sys, step);
    sys = advance(sys, step);

    loadNativeRootSet();
    collect();
    return 0;
}