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
    .ptr_mask = "1100",  
    .typekey = "CelestialBodyType"
}; 

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

TypeInfoBase ListNodeType = {
    .type_id = 4,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "11",  
    .typekey = "ListNodeType"
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
    uint64_t name;
    double mass;
} Body;

struct ListNode {
    ListNode* next;
    Body* body;
} ;

GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);
GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);
GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);

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

ListNode* createNBodySystem() {
    ListNode* planets = AllocType(ListNode, alloc2, &ListNodeType);
    ListNode* current = planets;

    current->body = createBody(jupiter_pos, jupiter_velocity, 0, jupiter_mass);
    current->next = AllocType(ListNode, alloc2, &ListNodeType);
    current = current->next;

    current->body = createBody(saturn_position, saturn_velocity, 1, saturn_mass);
    current->next = AllocType(ListNode, alloc2, &ListNodeType);
    current = current->next;

    current->body = createBody(uranus_position, uranus_velocity, 2, uranus_mass);
    current->next = AllocType(ListNode, alloc2, &ListNodeType);
    current = current->next;

    current->body = createBody(neptune_position, neptune_velocity, 3, neptune_mass);
    current->next = nullptr;

    double px = 0.0, py = 0.0, pz = 0.0;
    ListNode* temp = planets;
    while (temp != nullptr) {
        px += temp->body->vel->vx * temp->body->mass;
        py += temp->body->vel->vy * temp->body->mass;
        pz += temp->body->vel->vz * temp->body->mass;
        temp = temp->next;
    }

    current->next = AllocType(ListNode, alloc2, &ListNodeType);
    current = current->next;

    current->body = createBody(sun_position, sun_velocity, 4, sun_mass);
    current->next = nullptr; 

    return planets;
}

double potentialEnergyCompute(ListNode* planets) 
{
    double potential = 0.0;
    ListNode* it1 = planets;
    while(it1 != nullptr) {
        Body* b0 = it1->body;
        it1 = it1->next;

        ListNode* it2 = it1;
        while (it2 != nullptr) {
            Body* b1 = it2->body;
            
            potential += (b0->mass * b1->mass) / distance(b0, b1);

            it2 = it2->next;
        }
    }

    return potential;
}

double energy(ListNode* planets) 
{
    ListNode* it = planets;

    double kinetic = 0.0;
    while(it != nullptr) {
        Body* b = it->body; 
        kinetic += kineticEnergy(b);

        it = it->next;
    }

    double potential = potentialEnergyCompute(planets);
    return (kinetic - potential);
}


static inline Forces getForces(ListNode* planets, Body* b0, double dt) 
{
    Forces forces_b1 = {.fx = 0.0, .fy = 0.0, .fz = 0.0};
    ListNode* it = planets;

    while (it != nullptr) {
        Body* b1 = it->body;

        if (b0->name == b1->name) {
            it = it->next;
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

        it = it->next;
    }

    Forces forces = {.fx = 0.0, .fy = 0.0, .fz = 0.0};
    forces.fx = b0->vel->vx + forces_b1.fx;
    forces.fy = b0->vel->vy + forces_b1.fy;
    forces.fz = b0->vel->vz + forces_b1.fz;

    return forces;
}

/* advance to next system */
ListNode* advance(ListNode* planets, double dt) 
{
    ListNode* new_planets = AllocType(ListNode, alloc2, &ListNodeType);
    ListNode* current = new_planets;    

    ListNode* planets_it = planets;

    while(planets_it != nullptr) {
        Body* b0 = planets_it->body;
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

        planets_it = planets_it->next;
        current->body = createBody(npos, nvel, b0->name, b0->mass);
        if(planets_it == nullptr) {
            current->next = nullptr;
        }
        else {
            current->next = AllocType(ListNode, alloc2, &ListNodeType);
        }
    }

    return new_planets;
}

int main(int argc, char** argv) 
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[3] = { &alloc2, &alloc3, &alloc4 };
    gtl_info.initializeGC<3>(allocs);

    int n = 50000000;
    ListNode* sys = createNBodySystem();
    double step = 0.01;
    
    printf("energy: %g\n", energy(sys));

    for(int i = 0; i < n; i++) {
        if(i % 500000 == 0) {
            printf("%i\n", i);
        }
        sys = advance(sys, step);
        //collect();
    }

    gtl_info.disable_stack_refs_for_tests = true;

    printf("energy: %g\n", energy(sys));

    return 0;
}