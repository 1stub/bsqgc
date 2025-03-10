#include "../src/runtime/memory/gc.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>

//
// Doesn't work with new cpp conversion, after some of the gc TODOs are
// taken care of come back and run some tests here
//

GCAllocator alloc1(8, REAL_ENTRY_SIZE(8), collect);
GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);

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
    double x;
    double y;
    double z;
} Position;

typedef struct {
    double vx;
    double vy;
    double vz;
} Velocity;

typedef struct {
    double fx;
    double fy;
    double fz;
} Forces;

typedef struct {
    std::string name;
    double mass;
    Position pos;
    Velocity vel;
} Body;

#define CHECK_ARRAY_BOUNDS(A) \
do { \
/*    if(static_bodies_index == mut_max_static_bodies - 1) {\
        mut_max_static_bodies += STATIC_BODIES_STEP;\
        A = realloc(A, mut_max_static_bodies * sizeof(Body));\
    } \
  */  \
} while(0)

#define MAX_BODIES 2048
Body static_bodies[MAX_BODIES];
int static_bodies_index = 0;

#define N 5
#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24

#define GET_MASS(D) (D * SOLAR_MASS)

Body jupiter = {
    .name = "jupiter", 
    .mass = GET_MASS(0.000954791938424326609),
    .pos = {
        4.84143144246472090,
        -1.16032004402742839,
        -0.103622044471123109
    },
    .vel = {
        0.00166007664274403694 * DAYS_PER_YEAR,
        0.00769901118419740425 * DAYS_PER_YEAR,
        -0.0000690460016972063023 * DAYS_PER_YEAR
    }
};

Body saturn = {
    .name = "saturn", 
    .mass = GET_MASS(0.000285885980666130812),
    .pos = {
        8.34336671824457987,
        4.12479856412430479,
        -0.403523417114321381
    },
    .vel = {
        -0.00276742510726862411 * DAYS_PER_YEAR,
        0.00499852801234917238 * DAYS_PER_YEAR,
        0.0000230417297573763929 * DAYS_PER_YEAR
    }
};

Body uranus = {
    .name = "uranus", 
    .mass = GET_MASS(0.0000436624404335156298),
    .pos = {
        12.8943695621391310,
        -15.1111514016986312,
        -0.223307578892655734
    },
    .vel = {
        0.00296460137564761618 * DAYS_PER_YEAR,
        0.00237847173959480950 * DAYS_PER_YEAR,
        -0.0000296589568540237556 * DAYS_PER_YEAR
    }
};

Body neptune = {
    .name = "neptune", 
    .mass = GET_MASS(0.0000515138902046611451),
    .pos = {
        15.3796971148509165,
        -25.9193146099879641,
        0.179258772950371181
    },
    .vel = {
        0.00268067772490389322 * DAYS_PER_YEAR,
        0.00162824170038242295 * DAYS_PER_YEAR,
        -0.0000951592254519715870 * DAYS_PER_YEAR
    }
};

Body sun = {
    .name = "sun", 
    .mass = SOLAR_MASS,
    .pos = {
        0.0,
        0.0,
        0.0
    },
    .vel = {
        0.0,
        0.0,
        0.0
    }
};

Body* offsetMomemtum(Body b, double px, double py, double pz) {
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

    CHECK_ARRAY_BOUNDS(static_bodies);

    static_bodies[static_bodies_index] = body;
    Body* ret = &static_bodies[static_bodies_index++];

    return ret;
}

double kineticEnergy(Body b) 
{
    return 0.5 * b.mass * ((b.vel.vx * b.vel.vx) + (b.vel.vy * b.vel.vy) + (b.vel.vz * b.vel.vz));
}

double distance(Body b0, Body b1) 
{
    double dx = b0.pos.x - b1.pos.x;
    double dy = b0.pos.y - b1.pos.y;
    double dz = b0.pos.z - b1.pos.z;
    return sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

void** createNBodySystem() 
{
    //static_bodies = malloc(STATIC_BODIES_STEP * sizeof(Body));
    void** all_bodies = AllocType(void*, alloc2, &ListNode);

    /* Dynamically create constant bodies in our gc pages */
    Body* gc_jupiter = AllocType(Body, alloc1, &Empty);
    gc_jupiter = &jupiter;

    Body* gc_saturn = AllocType(Body, alloc1, &Empty);
    gc_saturn = &saturn;

    Body* gc_uranus = AllocType(Body, alloc1, &Empty);
    gc_uranus = &uranus;

    Body* gc_neptune = AllocType(Body, alloc1, &Empty);
    gc_neptune = &neptune;

    Body* planets[4] = {gc_jupiter, gc_saturn, gc_uranus, gc_neptune};

    /* This does not preserve functional nature, but should be okay. */
    double px = 0.0, py = 0.0, pz = 0.0;

    for (int i = 0; i < (N - 1); i++) {
        px += planets[i]->vel.vx * planets[i]->mass;
        py += planets[i]->vel.vy * planets[i]->mass;
        pz += planets[i]->vel.vz * planets[i]->mass;
    }

    Body* gc_sun = AllocType(Body, alloc1, &Empty);;
    gc_sun = offsetMomemtum(sun, px, py, pz);

    all_bodies[0] = gc_sun;
    all_bodies[1] = AllocType(void*, alloc2, &ListNode);

    void** it = (void**)all_bodies[1];
    it[0] = planets[0];
    it[1] = AllocType(void*, alloc2, &ListNode);
    it = (void**)it[1];

    it[0] = planets[1];
    it[1] = AllocType(void*, alloc2, &ListNode);
    it = (void**)it[1];

    it[0] = (void*)planets[2];
    it[1] = AllocType(Body, alloc1, &Empty);;
    it = (void**)it[1];

    it[0] = planets[3];

    return all_bodies;
}

double potentialEnergyCompute(void** bodies) 
{
    double potential = 0.0;
    void** it_0 = bodies;

    for (int i = 0; i < N; i++) {
        Body* b0 = (Body*)(it_0[0]);
        void** it_1 = (void**)it_0[1];

        for (int j = i + 1; j < N; j++) {
            Body* b1 = (Body*)(it_1[0]);
            
            potential += (b0->mass * b1->mass) / distance(*b0, *b1);
            it_1 = (void**)it_1[1]; 
        }
        it_0 = (void**)it_0[1]; 
    }

    return potential;
}

double energy(void** bodies) 
{
    void** it = bodies;
    double kinetic = 0.0;
    for(int i = 0; i < N; i++) {
        Body* b = (Body*)(it[0]); 
        kinetic += kineticEnergy(*b);
        it = (void**)it[1];
    }

    double potential = potentialEnergyCompute(bodies);
    return (kinetic - potential);
}


static inline Forces getForces(void** bodies, Body* b0, double dt) 
{
    void** it_1 = bodies;
    Forces forces_b1 = {.fx = 0.0, .fy = 0.0, .fz = 0.0};

    for (int j = 0; j < N; j++) {
        Body* b1 = (Body*)(it_1[0]); 

        if (b0->name == b1->name) {
            it_1 = (void**)it_1[1]; 
            continue;
        }

        double dx = b1->pos.x - b0->pos.x;
        double dy = b1->pos.y - b0->pos.y;
        double dz = b1->pos.z - b0->pos.z;

        double dist = distance(*b0, *b1);
        double mag = dt / (dist * dist * dist);

        forces_b1.fx += dx * b1->mass * mag;
        forces_b1.fy += dy * b1->mass * mag;
        forces_b1.fz += dz * b1->mass * mag;

        it_1 = (void**)it_1[1]; 

        //debug_print("%s, %s\n", b0->name, b1->name);
    }

    Forces forces = {.fx = 0.0, .fy = 0.0, .fz = 0.0};
    forces.fx = b0->vel.vx + forces_b1.fx;
    forces.fy = b0->vel.vy + forces_b1.fy;
    forces.fz = b0->vel.vz + forces_b1.fz;

    return forces;
}

/* advance to next system */
void** advance(void** bodies, double dt) 
{
    /* We are going to need to create a new all_bodies list from bodies arg */
    void** new_bodies = AllocType(void*, alloc2, &ListNode);
    void** new_bodies_it = new_bodies;

    void** it_0 = bodies;
    for(int i = 0; i < N; i++) {
        Body* b0 = (Body*)(it_0[0]);
        Forces forces = getForces(bodies, b0, dt);

        double fx = forces.fx;
        double fy = forces.fy;
        double fz = forces.fz;

        Velocity nvel = {.vx = fx, .vy = fy, .vz = fz};
        Position npos = {
            .x = b0->pos.x + (fx * dt),
            .y = b0->pos.y + (fy * dt),
            .z = b0->pos.z + (fz * dt)  
        };

        Body new_body = {
            .name = b0->name,
            .mass = b0->mass,
            .pos = npos,
            .vel = nvel
        };

        CHECK_ARRAY_BOUNDS(static_bodies);

        /* Insert into static storage and new gc page list */
        static_bodies[static_bodies_index] = new_body;

        Body* gc_new_body = AllocType(Body, alloc1, &Empty);;
        gc_new_body = &static_bodies[static_bodies_index++];

        new_bodies_it[0] = gc_new_body;

        /* Not end of list */
        if(i < (N-1)) {
            new_bodies_it[1] = AllocType(void*, alloc2, &ListNode);;
        } else {
            new_bodies_it[1] = AllocType(Body, alloc1, &Empty);;
        }

        new_bodies_it = (void**)new_bodies_it[1];
        it_0 = (void**)it_0[1];
    }

    return new_bodies;
}

void* garray[3] = {nullptr, nullptr, nullptr};

int main(int argc, char** argv) 
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[2] = { &alloc1, &alloc2 };
    gtl_info.initializeGC<2>(allocs);

    int n = 5;
    void** sys = createNBodySystem();
    double step = 0.01;
    
    printf("energy: %g\n", energy(sys));

    /* Currently does not work for large n, fails after about 20 advances */
    for(int i = 0; i < n; i++) {
        if(i % 10 == 0) {
            collect();
            static_bodies_index = 0;
        }
        sys = advance(sys, step);
    }

    gtl_info.disable_stack_refs_for_tests = true;

    collect();

    printf("energy: %g\n", energy(sys));

    return 0;
}