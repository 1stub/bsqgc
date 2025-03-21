#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

TypeInfoBase CelestialBodyType = {
    .type_id = 1,
    .type_size = 32,
    .slot_size = 4,
    .ptr_mask = "1100",  
    .typekey = "CelestialBodyType"
}; 

TypeInfoBase VelocityType = {
    .type_id = 2,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "000",  
    .typekey = "VelocityType"
};

TypeInfoBase PositionType = {
    .type_id = 3,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "000",  
    .typekey = "PositionType"
};

TypeInfoBase ListNode5Type = {
    .type_id = 4,
    .type_size = 40,
    .slot_size = 5,
    .ptr_mask = "11111",  
    .typekey = "ListNode5Type"
}; //next, ptr to cur object

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
    Position* pos;
    Velocity* vel;
    uint8_t name;
    double mass;
} Body;

GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);
GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);
GCAllocator alloc5(40, REAL_ENTRY_SIZE(40), collect);

#define N 5
#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24

#define GET_MASS(D) (D * SOLAR_MASS)

//Perhaps could be nice to just store each bodies original values here
Body* garray[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

double jupiter_mass = GET_MASS(0.000954791938424326609);
const Position jupiter_pos = {
    4.84143144246472090,
    -1.16032004402742839,
    -0.103622044471123109
};
const Velocity jupiter_velocity = {
    0.00166007664274403694 * DAYS_PER_YEAR,
    0.00769901118419740425 * DAYS_PER_YEAR,
    -0.0000690460016972063023 * DAYS_PER_YEAR
};

double saturn_mass = GET_MASS(0.000285885980666130812);
const Position saturn_position = {
    8.34336671824457987,
    4.12479856412430479,
    -0.403523417114321381
};
const Velocity saturn_velocity = {
    -0.00276742510726862411 * DAYS_PER_YEAR,
    0.00499852801234917238 * DAYS_PER_YEAR,
    0.0000230417297573763929 * DAYS_PER_YEAR
};

double uranus_mass = GET_MASS(0.0000436624404335156298);
const Position uranus_position = {
    12.8943695621391310,
    -15.1111514016986312,
    -0.223307578892655734
};
const Velocity uranus_velocity = {
    0.00296460137564761618 * DAYS_PER_YEAR,
    0.00237847173959480950 * DAYS_PER_YEAR,
    -0.0000296589568540237556 * DAYS_PER_YEAR
};

double neptune_mass = GET_MASS(0.0000515138902046611451);
const Position neptune_position = {
    15.3796971148509165,
    -25.9193146099879641,
    0.179258772950371181
};
const Velocity neptune_velocity = {
    0.00268067772490389322 * DAYS_PER_YEAR,
    0.00162824170038242295 * DAYS_PER_YEAR,
    -0.0000951592254519715870 * DAYS_PER_YEAR
};

double sun_mass = SOLAR_MASS;
const Position sun_position = {
    0.0,
    0.0,
    0.0
};
const Velocity sun_velocity = {
    0.0,
    0.0,
    0.0
};

void offsetMomemtum(Body* b, double px, double py, double pz) 
{
    b->vel->vx = -px / SOLAR_MASS;
    b->vel->vy = -py / SOLAR_MASS;
    b->vel->vz = -pz / SOLAR_MASS;
}

double kineticEnergy(Body* b) 
{
    return 0.5 * b->mass * ((b->vel->vx * b->vel->vx) + (b->vel->vy * b->vel->vy) + (b->vel->vz * b->vel->vz));
}

double distance(Body* b0, Body* b1) 
{
    double dx = b0->pos->x - b1->pos->x;
    double dy = b0->pos->y - b1->pos->y;
    double dz = b0->pos->z - b1->pos->z;
    return sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

Body* createBody(Position p, Velocity v, uint64_t n, double m) 
{
    Body* b = AllocType(Body, alloc4, &CelestialBodyType);

    //handle potential collection trigger preventing invalid metadata access
    b->pos = nullptr;
    b->vel = nullptr;

    b->pos = AllocType(Position, alloc3, &PositionType);
    b->vel = AllocType(Velocity, alloc3, &VelocityType);
    
    b->name = n;
    b->mass = m;
    b->pos->x = p.x;
    b->pos->y = p.y;
    b->pos->z = p.z;
    b->vel->vx = v.vx;
    b->vel->vy = v.vy;
    b->vel->vz = v.vz;

    return b;
}

Body** createNBodySystem() {
    Body** planets = AllocType(Body*, alloc5, &ListNode5Type);

    planets[0] = createBody(jupiter_pos, jupiter_velocity, 0, jupiter_mass);
    planets[1] = createBody(saturn_position, saturn_velocity, 1, saturn_mass);
    planets[2] = createBody(uranus_position, uranus_velocity, 2, uranus_mass);
    planets[3] = createBody(neptune_position, neptune_velocity, 3, neptune_mass);

    double px = 0.0, py = 0.0, pz = 0.0;
    for (int i = 0; i < (N - 1); i++) {
        px += planets[i]->vel->vx * planets[i]->mass;
        py += planets[i]->vel->vy * planets[i]->mass;
        pz += planets[i]->vel->vz * planets[i]->mass;
    }

    planets[4] = createBody(sun_position, sun_velocity, 4, sun_mass);
    offsetMomemtum(planets[4], px, py, pz);

    return planets;
}

double potentialEnergyCompute(Body** planets) 
{
    double potential = 0.0;
    for(int i = 0; i < N; i++) {
        Body* b0 = planets[i];

        for (int j = i+1; j < N; j++) {
            Body* b1 = planets[j];
            
            potential += (b0->mass * b1->mass) / distance(b0, b1);
        }
    }

    return potential;
}

double energy(Body** planets) 
{
    double kinetic = 0.0;
    for(int i = 0; i < N; i++) {
        Body* b = planets[i]; 
        kinetic += kineticEnergy(b);
    }

    double potential = potentialEnergyCompute(planets);
    return (kinetic - potential);
}


static inline Forces getForces(Body** planets, Body* b0, double dt) 
{
    Forces forces_b1 = {.fx = 0.0, .fy = 0.0, .fz = 0.0};
    for (int i = 0; i < N; i++) {
        Body* b1 = planets[i];

        if (b0->name == b1->name) {
            continue;
        }

        double dx = b1->pos->x - b0->pos->x;
        double dy = b1->pos->y - b0->pos->y;
        double dz = b1->pos->z - b0->pos->z;

        double dist = distance(b0, b1);
        double mag = dt / (dist * dist * dist);

        forces_b1.fx += dx * b1->mass * mag;
        forces_b1.fy += dy * b1->mass * mag;
        forces_b1.fz += dz * b1->mass * mag;
    }

    Forces forces = {.fx = 0.0, .fy = 0.0, .fz = 0.0};
    forces.fx = b0->vel->vx + forces_b1.fx;
    forces.fy = b0->vel->vy + forces_b1.fy;
    forces.fz = b0->vel->vz + forces_b1.fz;

    return forces;
}

/* advance to next system */
Body** advance(Body** planets, double dt) 
{
    if (planets == nullptr) {
        assert(false);
    }
    Body** new_planets = AllocType(Body*, alloc5, &ListNode5Type);
    new_planets[0] = nullptr;
    new_planets[1] = nullptr;
    new_planets[2] = nullptr;
    new_planets[3] = nullptr;
    new_planets[4] = nullptr;

    for(int i = 0; i < N; i++) {
        Body* b0 = planets[i];
        Forces forces = getForces(planets, b0, dt);

        double fx = forces.fx;
        double fy = forces.fy;
        double fz = forces.fz;

        Velocity nvel = {.vx = fx, .vy = fy, .vz = fz};
        Position npos = {
            .x = b0->pos->x + (fx * dt),
            .y = b0->pos->y + (fy * dt),
            .z = b0->pos->z + (fz * dt)  
        };

        new_planets[i] = createBody(npos, nvel, b0->name, b0->mass);
    }

    return new_planets;
}

int main(int argc, char** argv) 
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[3] = { &alloc3, &alloc4, &alloc5 };
    gtl_info.initializeGC<3>(allocs);

    int n = 50000000;
    Body** sys = createNBodySystem();
    double step = 0.01;
    
    printf("energy: %g\n", energy(sys));

    for(int i = 0; i < n; i++) {
        sys = advance(sys, step);
    }

    gtl_info.disable_stack_refs_for_tests = true;

    printf("energy: %g\n", energy(sys));

    return 0;
}