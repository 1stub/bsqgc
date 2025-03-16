#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct TypeInfoBase CelestialBodyType = {
    .type_id = 1,
    .type_size = 32,
    .slot_size = 4,
    .ptr_mask = "2011",  
    .typekey = "CelestialBodyType"
}; //name, mass, pos, vel

struct TypeInfoBase VelocityType = {
    .type_id = 2,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "000",  
    .typekey = "VelocityType"
};

struct TypeInfoBase PositionType = {
    .type_id = 3,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "000",  
    .typekey = "PositionType"
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
    const char* name;
    double mass;
    Position* pos;
    Velocity* vel;
} Body;

GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);
GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);

#define N 5
#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24

#define GET_MASS(D) (D * SOLAR_MASS)

// Encode a string pointer with its length in the lower 3 bits
#define ENCODE_STRING_PTR(ptr, len) ((const char*)((uintptr_t)(ptr) | ((len) & 0x7)))

//In the global array we can put actual bodies references in here 
//in order to keep them alive properly
Body* garray[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
//jupiter, saturn, uranus, neptune, sun

const double jupiter_mass = GET_MASS(0.000954791938424326609);
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

const double saturn_mass = GET_MASS(0.000285885980666130812);
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

const double uranus_mass = GET_MASS(0.0000436624404335156298);
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

const double neptune_mass = GET_MASS(0.0000515138902046611451);
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

//mass is SOLAR_MASS
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

void offsetMomemtum(Body* b, double px, double py, double pz) {
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

void createBody(int i, Position p, Velocity v, const char* n, double m) 
{
    garray[i] = AllocType(Body, alloc4, &CelestialBodyType);
    (garray[i])->pos = AllocType(Position, alloc3, &PositionType);
    (garray[i])->vel = AllocType(Velocity, alloc3, &VelocityType);
    
    (garray[i])->name = n;
    (garray[i])->mass = m;
    (garray[i])->pos->x = p.x;
    (garray[i])->pos->y = p.y;
    (garray[i])->pos->z = p.z;
    (garray[i])->vel->vx = v.vx;
    (garray[i])->vel->vy = v.vy;
    (garray[i])->vel->vz = v.vz;
}

//Populates garray with body references
void createNBodySystem() 
{
    createBody(0, jupiter_pos, jupiter_velocity, ENCODE_STRING_PTR("jupiter", 7), jupiter_mass);
    createBody(1, saturn_position, saturn_velocity, ENCODE_STRING_PTR("saturn", 6), saturn_mass);
    createBody(2, uranus_position, uranus_velocity, ENCODE_STRING_PTR("uranus", 6), uranus_mass);
    createBody(3, neptune_position, neptune_velocity, ENCODE_STRING_PTR("neptune", 7), neptune_mass);

    //sun
    double px = 0.0, py = 0.0, pz = 0.0;
    for (int i = 0; i < (N - 1); i++) {
        px += garray[i]->vel->vx * garray[i]->mass;
        py += garray[i]->vel->vy * garray[i]->mass;
        pz += garray[i]->vel->vz * garray[i]->mass;
    }

    createBody(4, sun_position, sun_velocity, ENCODE_STRING_PTR("sun", 6), SOLAR_MASS);
}

double potentialEnergyCompute() 
{
    double potential = 0.0;
    for (int i = 0; i < N; i++) {
        Body* b0 = garray[i];

        for (int j = i + 1; j < N; j++) {
            Body* b1 = garray[j];
            
            potential += (b0->mass * b1->mass) / distance(b0, b1);
        }
    }

    return potential;
}

double energy() 
{
    double kinetic = 0.0;
    for(int i = 0; i < N; i++) {
        Body* b = garray[0]; 
        kinetic += kineticEnergy(b);
    }

    double potential = potentialEnergyCompute();
    return (kinetic - potential);
}


static inline Forces getForces(Body* b0, double dt) 
{
    Forces forces_b1 = {.fx = 0.0, .fy = 0.0, .fz = 0.0};

    for (int j = 0; j < N; j++) {
        Body* b1 = garray[j];

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
void advance(double dt) 
{
    for(int i = 0; i < N; i++) {
        Body* b0 = garray[i];
        Forces forces = getForces(b0, dt);

        double fx = forces.fx;
        double fy = forces.fy;
        double fz = forces.fz;

        Velocity nvel = {.vx = fx, .vy = fy, .vz = fz};
        Position npos = {
            .x = b0->pos->x + (fx * dt),
            .y = b0->pos->y + (fy * dt),
            .z = b0->pos->z + (fz * dt)  
        };

        Body* new_body = AllocType(Body, alloc4, &CelestialBodyType);
        new_body->pos = AllocType(Position, alloc3, &PositionType);
        new_body->vel = AllocType(Velocity, alloc3, &VelocityType);
        new_body->name = b0->name;
        new_body->mass = b0->mass;
        new_body->pos->x = npos.x;
        new_body->pos->y = npos.y;
        new_body->pos->z = npos.z;
        new_body->vel->vx = nvel.vx;
        new_body->vel->vy = nvel.vy;
        new_body->vel->vz = nvel.vz;
    }
}

int main(int argc, char** argv) 
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[2] = { &alloc3, &alloc4 };
    gtl_info.initializeGC<2>(allocs);

    int n = 50000000;
    createNBodySystem();
    double step = 0.01;
    
    printf("energy: %g\n", energy());

    for(int i = 0; i < n; i++) {
        advance(step);
    }

    gtl_info.disable_stack_refs_for_tests = true;

    printf("energy: %g\n", energy());

    return 0;
}