#include "../src/runtime/memory/gc.h"

struct TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = NULL,
    .typekey = "Empty"
};

/* Need to make bin24? */
struct TypeInfoBase Vec3 = {
    .type_id = 1,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "000",
    .typekey = "Vec3"
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
    char* name;
    float mass;
    Position pos;
    Position vel;
} Body;

#define GET_MASS(F) (F * SOLAR_MASS)

#define N 5
#define PI 3.141592653589793
#define SOLAR_MASS (4 * PI * PI)
#define DAYS_PER_YEAR 365.24
#define PAIRS (N*(N-1)/2)


const Body jupiter = {
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

const Body saturn = {
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

const Body uranus = {
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

const Body neptune = {
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

const Body sun = {
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

/* Body* ? */
Body offsetMomemtum(Body b, float px, float py, float pz) 
{
    Body body = {
        b.name,
        b.mass,
        b.pos,
        .vel = {
            -px,
            -py,
            -pz
        }
    }; 
    return body;   
}

int main() {
    return 0;
}