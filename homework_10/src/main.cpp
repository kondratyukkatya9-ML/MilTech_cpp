#include <iostream>
#include <cmath>
#include "json.hpp" 
#include <fstream>
#include <cstring>
#include <memory>
#include <string>
using json = nlohmann::json;


#define ENABLE_LOG   1
#define ENABLE_DEBUG 0

#if ENABLE_LOG
  #define LOG(msg) std::cout << "[LOG] " << msg << std::endl
#else
  #define LOG(msg)
#endif

#if ENABLE_DEBUG
  #define DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
  #define DEBUG(msg)
#endif

struct Coord {
    float x;
    float y;

    //Додавання координат
    Coord operator+(const Coord& other) const {
        Coord result;
        result.x = x + other.x;
        result.y = y + other.y;
        return result;
    }
    //Віднімання координат
    Coord operator-(const Coord& other) const {
        Coord result;
        result.x = x - other.x;
        result.y = y - other.y;
        return result;
    }
    //Множення координат на скаляр
    Coord operator*(float s) const {
        Coord result;
        result.x = x * s;
        result.y = y * s;
        return result;
    }
    //Ділення координат на скаляр
    Coord operator/(float s ) const {
        Coord result;
        result.x = x / s;
        result.y = y / s;
        return result;
    }
    //Перевірка на рівність координат
    bool operator==(const Coord& other) const {
        return (x == other.x && y == other.y);
    }
};
//Обчислення довжини вектора
 float length(Coord c) {
    return hypotf(c.x, c.y);
}
//Нормалізація вектора
Coord normalize(Coord c) {
    return c / length(c);
}

struct AmmoParams {
    char name[32];
    float mass; // маса (кг)
    float drag; // коефіцієнт опору
    float lift; // коефіцієнт підйому
};
struct DroneConfig {
    Coord startPos;
    float altitude;
    float initialDir;
    float attackSpeed;
    float accelPath;
    char  ammoName[32];
    float arrayTimeStep;
    float simTimeStep;
    float hitRadius;
    float angularSpeed;
    float turnThreshold;
};
struct SimStep {
    Coord pos;
    float direction;
    int   state;
    std::string stateName;
    int   targetIdx;
    Coord dropPoint;
    Coord aimPoint;
    Coord predictedTarget;
};
enum DroneState {
    STOPPED,
    ACCELERATING,
    DECELERATING,
    TURNING,
    MOVING
};
struct DroneContext {
    Coord dronePos;
    float droneDir;
    float droneSpeed;
    DroneState droneState;

    float newDir;
    float deltaAngle;

    float attackSpeed;
    float accel;
    float angularSpeed;
    float turnThreshold;
    float simTimeStep;
};

struct BallisticTable {
    std::vector<float> axisZ0;
    std::vector<float> axisV0;
    std::vector<float> axisM;
    std::vector<float> axisD;
    std::vector<float> axisL;

    struct Result {
        float t;
        float hDist;
    };

    std::vector<Result> data;

    size_t index(int iz, int iv, int im, int id, int il) const {
        return ((((size_t)iz * axisV0.size() + iv)
                              * axisM.size()  + im)
                              * axisD.size()  + id)
                              * axisL.size()  + il;
    }

    const Result& at(int iz, int iv, int im, int id, int il) const {
        return data[index(iz, iv, im, id, il)];
    }

    bool load(const char* path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        int nZ, nV, nM, nD, nL;
        f >> nZ >> nV >> nM >> nD >> nL;

        axisZ0.resize(nZ); for (auto& v : axisZ0) f >> v;
        axisV0.resize(nV); for (auto& v : axisV0) f >> v;
        axisM.resize(nM);  for (auto& v : axisM)  f >> v;
        axisD.resize(nD);  for (auto& v : axisD)  f >> v;
        axisL.resize(nL);  for (auto& v : axisL)  f >> v;

        size_t total = (size_t)nZ*nV*nM*nD*nL;
        data.resize(total);

        for (size_t i = 0; i < total; i++)
            f >> data[i].t >> data[i].hDist;

        return f.good();
    }
    struct Interp {
    int lo;
    float frac;
};

Interp findInterp(float val, const std::vector<float>& axis) const {
    if (val <= axis.front()) return {0, 0.0f};
    if (val >= axis.back())  return {(int)axis.size()-2, 1.0f};
    int i = 0;
    for (int j = 0; j < (int)axis.size()-1; j++)
        if (axis[j] <= val && val <= axis[j+1]) { i = j; break; }
    float frac = (val - axis[i]) / (axis[i+1] - axis[i]);
    return {i, frac};
}

Result lerp(const Result& a, const Result& b, float t) const {
    return {a.t + (b.t - a.t) * t, a.hDist + (b.hDist - a.hDist) * t};
}

Result lookup(float Z0, float V0, float m, float d, float l) const {
    Interp iz = findInterp(Z0, axisZ0);
    Interp iv = findInterp(V0, axisV0);
    Interp im = findInterp(m,  axisM);
    Interp id = findInterp(d,  axisD);
    Interp il = findInterp(l,  axisL);

    Result v[16];
    for (int a = 0; a < 2; a++)
    for (int b = 0; b < 2; b++)
    for (int c = 0; c < 2; c++)
    for (int e = 0; e < 2; e++) {
        auto& lo = at(iz.lo+a, iv.lo+b, im.lo+c, id.lo+e, il.lo);
        auto& hi = at(iz.lo+a, iv.lo+b, im.lo+c, id.lo+e, il.lo+1);
        v[a*8+b*4+c*2+e] = lerp(lo, hi, il.frac);
    }
    Result w[8];
    for (int a = 0; a < 2; a++)
    for (int b = 0; b < 2; b++)
    for (int c = 0; c < 2; c++)
        w[a*4+b*2+c] = lerp(v[a*8+b*4+c*2], v[a*8+b*4+c*2+1], id.frac);

    Result u[4];
    for (int a = 0; a < 2; a++)
    for (int b = 0; b < 2; b++)
        u[a*2+b] = lerp(w[a*4+b*2], w[a*4+b*2+1], im.frac);

    Result s[2];
    for (int a = 0; a < 2; a++)
        s[a] = lerp(u[a*2], u[a*2+1], iv.frac);

    return lerp(s[0], s[1], iz.frac);
}
};

class IDroneState {
public:
    virtual ~IDroneState() = default;
    virtual std::unique_ptr<IDroneState> execute(DroneContext& ctx) = 0;
    virtual const char* name() const = 0;
};




class StateStopped;
class StateAccelerating;
class StateDecelerating;
class StateTurning;
class StateMoving;

class StateStopped : public IDroneState {
public:
    std::unique_ptr<IDroneState> execute(DroneContext& ctx) override;
    const char* name() const override { return "Stopped"; }
};

class StateAccelerating : public IDroneState {
public:
    std::unique_ptr<IDroneState> execute(DroneContext& ctx) override;
    const char* name() const override { return "Accelerating"; }
};

class StateDecelerating : public IDroneState {
public:
    std::unique_ptr<IDroneState> execute(DroneContext& ctx) override;
    const char* name() const override { return "Decelerating"; }
};

class StateTurning : public IDroneState {
public:
    std::unique_ptr<IDroneState> execute(DroneContext& ctx) override;
    const char* name() const override { return "Turning"; }
};

class StateMoving : public IDroneState {
public:
    std::unique_ptr<IDroneState> execute(DroneContext& ctx) override;
    const char* name() const override { return "Moving"; }
};

std::unique_ptr<IDroneState> StateStopped::execute(DroneContext& ctx) {
    ctx.droneDir = ctx.newDir;
    return std::make_unique<StateAccelerating>();
}

std::unique_ptr<IDroneState> StateAccelerating::execute(DroneContext& ctx) {
    ctx.droneSpeed += ctx.accel * ctx.simTimeStep;
    if (ctx.droneSpeed >= ctx.attackSpeed) {
        ctx.droneSpeed = ctx.attackSpeed;
        ctx.dronePos.x += ctx.droneSpeed * cosf(ctx.droneDir) * ctx.simTimeStep;
        ctx.dronePos.y += ctx.droneSpeed * sinf(ctx.droneDir) * ctx.simTimeStep;
        return std::make_unique<StateMoving>();
    }
    ctx.dronePos.x += ctx.droneSpeed * cosf(ctx.droneDir) * ctx.simTimeStep;
    ctx.dronePos.y += ctx.droneSpeed * sinf(ctx.droneDir) * ctx.simTimeStep;
    return nullptr;
}

std::unique_ptr<IDroneState> StateMoving::execute(DroneContext& ctx) {
    if (fabsf(ctx.deltaAngle) > ctx.turnThreshold) {
        ctx.dronePos.x += ctx.droneSpeed * cosf(ctx.droneDir) * ctx.simTimeStep;
        ctx.dronePos.y += ctx.droneSpeed * sinf(ctx.droneDir) * ctx.simTimeStep;
        return std::make_unique<StateDecelerating>();
    }
    ctx.droneDir = ctx.newDir;
    ctx.dronePos.x += ctx.droneSpeed * cosf(ctx.droneDir) * ctx.simTimeStep;
    ctx.dronePos.y += ctx.droneSpeed * sinf(ctx.droneDir) * ctx.simTimeStep;
    return nullptr;
}

std::unique_ptr<IDroneState> StateDecelerating::execute(DroneContext& ctx) {
    ctx.droneSpeed -= ctx.accel * ctx.simTimeStep;
    if (ctx.droneSpeed <= 0) {
        ctx.droneSpeed = 0;
        return std::make_unique<StateTurning>();
    }
    ctx.dronePos.x += ctx.droneSpeed * cosf(ctx.droneDir) * ctx.simTimeStep;
    ctx.dronePos.y += ctx.droneSpeed * sinf(ctx.droneDir) * ctx.simTimeStep;
    return nullptr;
}

std::unique_ptr<IDroneState> StateTurning::execute(DroneContext& ctx) {
    float turnAmount = ctx.angularSpeed * ctx.simTimeStep;
    if (fabsf(ctx.deltaAngle) <= turnAmount) {
        ctx.droneDir = ctx.newDir;
        return std::make_unique<StateAccelerating>();
    }
    ctx.droneDir += (ctx.deltaAngle > 0) ? turnAmount : -turnAmount;
    return nullptr;
}



Coord interpolateTarget(Coord** targets, int targetIdx, 
                         float t, float arrayTimeStep, int timeSteps) {
    int idx  = (int)(t / arrayTimeStep) % timeSteps;
    int next = (idx + 1) % timeSteps;
    float frac = (t - (int)(t / arrayTimeStep) * arrayTimeStep) / arrayTimeStep;
    Coord result;
    result.x = targets[targetIdx][idx].x + 
               (targets[targetIdx][next].x - targets[targetIdx][idx].x) * frac;
    result.y = targets[targetIdx][idx].y + 
               (targets[targetIdx][next].y - targets[targetIdx][idx].y) * frac;
    return result;
}
float calcFlightTime(float zd, float V0, float m, float d, float l) {
    float g = 9.81f;
    float a = d*g*m - 2*d*d*l*V0;
    float b = -3*g*m*m + 3*d*l*m*V0;
    float c = 6*m*m*zd;
    float p = -(b*b) / (3*a*a);
    float q = 2*b*b*b / (27*a*a*a) + c/a;
    float arg = 3*q / (2*p) * sqrtf(-3/p);
    if (arg < -1.0f || arg > 1.0f) return -1.0f;
    float phi = acosf(arg);
    return 2*sqrtf(-p/3) * cosf((phi + 4*3.14159f) / 3) - b/(3*a);
}

float calcHorizDist(float t, float V0, float m, float d, float l) {
    float g = 9.81f;
    float l2 = l*l;
    float l4 = l2*l2;
    return V0*t
        - t*t*d*V0/(2*m)
        + t*t*t*(6*d*g*l*m - 6*d*d*(l2-1)*V0)/(36*m*m)
        + t*t*t*t*(-6*d*d*g*l*(1+l2+l4)*m + 3*d*d*d*l2*(1+l2)*V0 + 6*d*d*d*l4*(1+l2)*V0)/(36*(1+l2)*(1+l2)*m*m*m)
        + t*t*t*t*t*(3*d*d*d*g*l*l*l*m - 3*d*d*d*d*l2*(1+l2)*V0)/(36*(1+l2)*m*m*m*m);
}
Coord calcFirePoint(Coord dronePos, Coord targetPos,
                    float h, float accelPath) {
    Coord delta = targetPos - dronePos;
    float D = length(delta);
    Coord dir = normalize(delta);

    if (h + accelPath > D) {
        Coord startPos = targetPos - dir * (h + accelPath);
        Coord delta2 = startPos - dronePos;
        float D2 = length(delta2);
        return dronePos + normalize(delta2) * (D2 - h);
    }
    return dronePos + dir * (D - h);
}
float calcTimeToStop(DroneState state, float speed, 
                     float attackSpeed, float accel) {
    if (state == ACCELERATING) return speed / accel;
    if (state == MOVING)       return attackSpeed / accel;
    return 0.0f;
}

void updateDrone(Coord& dronePos, float& droneDir,
                 float& droneSpeed, DroneState& droneState,
                 float newDir, float deltaAngle,
                 float attackSpeed, float accel,
                 float angularSpeed, float turnThreshold, 
                 float simTimeStep) {
    if (droneState == STOPPED) {
        droneDir   = newDir;
        droneState = ACCELERATING;
    } else if (droneState == ACCELERATING) {
        droneSpeed += accel * simTimeStep;
        if (droneSpeed >= attackSpeed) {
            droneSpeed = attackSpeed;
            droneState = MOVING;
        }
        dronePos.x += droneSpeed * cosf(droneDir) * simTimeStep;
        dronePos.y += droneSpeed * sinf(droneDir) * simTimeStep;
    } else if (droneState == MOVING) {
        if (fabsf(deltaAngle) > turnThreshold)
            droneState = DECELERATING;
        else
            droneDir = newDir;
        dronePos.x += droneSpeed * cosf(droneDir) * simTimeStep;
        dronePos.y += droneSpeed * sinf(droneDir) * simTimeStep;
    } else if (droneState == DECELERATING) {
        droneSpeed -= accel * simTimeStep;
        if (droneSpeed <= 0) {
            droneSpeed = 0;
            droneState = TURNING;
        }
        dronePos.x += droneSpeed * cosf(droneDir) * simTimeStep;
        dronePos.y += droneSpeed * sinf(droneDir) * simTimeStep;
    } else if (droneState == TURNING) {
        float turnAmount = angularSpeed * simTimeStep;
        if (fabsf(deltaAngle) <= turnAmount) {
            droneDir   = newDir;
            droneState = ACCELERATING;
        } else {
            droneDir += (deltaAngle > 0) ? turnAmount : -turnAmount;
        }
    }
}
// ==================== ДЗ10: DronePhysics ====================

struct DroneCommand {
    float desiredDir;  // бажаний напрямок польоту
};

struct DroneTelemetry {
    Coord pos;
    Coord speed;
    float timeSecSinceStart;
};

class DronePhysics {
private:
    Coord dronePos_;
    float droneDir_;
    float droneSpeed_;
    std::unique_ptr<IDroneState> droneState_;
    float desiredDir_ = 0.0f;

    float accel_;
    float angularSpeed_;
    float turnThreshold_;
    float attackSpeed_;

public:
    DronePhysics(Coord startPos, float initialDir, float accel,
                 float angularSpeed, float turnThreshold, float attackSpeed)
        : dronePos_(startPos)
        , droneDir_(initialDir)
        , droneSpeed_(0.0f)
        , droneState_(std::make_unique<StateStopped>())
        , desiredDir_(initialDir)
        , accel_(accel)
        , angularSpeed_(angularSpeed)
        , turnThreshold_(turnThreshold)
        , attackSpeed_(attackSpeed)

    {}

    
    void setCommand(const DroneCommand& cmd) {
        desiredDir_ = cmd.desiredDir;
    }

    // Один крок фізики: dt секунд часу
    void step(float dt) {
        float deltaAngle = desiredDir_ - droneDir_;
        while (deltaAngle >  3.14159f) deltaAngle -= 2*3.14159f;
        while (deltaAngle < -3.14159f) deltaAngle += 2*3.14159f;

        DroneContext ctx;
        ctx.dronePos      = dronePos_;
        ctx.droneDir      = droneDir_;
        ctx.droneSpeed    = droneSpeed_;
        ctx.newDir        = desiredDir_;
        ctx.deltaAngle    = deltaAngle;
        ctx.attackSpeed   = attackSpeed_;   
        ctx.accel         = accel_;
        ctx.angularSpeed  = angularSpeed_;
        ctx.turnThreshold = turnThreshold_;
        ctx.simTimeStep   = dt;

        auto next = droneState_->execute(ctx);
        if (next) droneState_ = std::move(next);

        dronePos_   = ctx.dronePos;
        droneDir_   = ctx.droneDir;
        droneSpeed_ = ctx.droneSpeed;
    }

    // Поточна телеметрія — позиція, швидкість, час оновлення
    DroneTelemetry getTelemetry() const {
        DroneTelemetry t;
        t.pos = dronePos_;
        t.speed = Coord{ droneSpeed_ * cosf(droneDir_), droneSpeed_ * sinf(droneDir_) };
        t.timeSecSinceStart = 0.0f;  
        return t;
    }

    // Поточний напрямок і назва стану 
    float getDirection() const { return droneDir_; }
    const char* getStateName() const { return droneState_->name(); }
};

int main() {
    // Читаємо config.json
    std::ifstream fc("config.json");
    if (!fc.is_open()) {
        std::cerr << "Cannot open config.json" << std::endl;
        return 1;
    }
    json jc;
    fc >> jc;



    DroneConfig config;
    config.startPos.x   = jc["drone"]["position"]["x"];
    config.startPos.y   = jc["drone"]["position"]["y"];
    config.altitude     = jc["drone"]["altitude"];
    config.initialDir   = jc["drone"]["initialDirection"];
    config.attackSpeed  = jc["drone"]["attackSpeed"];
    config.accelPath    = jc["drone"]["accelerationPath"];
    config.angularSpeed = jc["drone"]["angularSpeed"];
    config.turnThreshold= jc["drone"]["turnThreshold"];
    config.simTimeStep  = jc["simulation"]["timeStep"];
    config.hitRadius    = jc["simulation"]["hitRadius"];
    config.arrayTimeStep= jc["targetArrayTimeStep"];
    std::strncpy(config.ammoName, jc["ammo"].get<std::string>().c_str(), 31);

    LOG("Config loaded: speed=" << config.attackSpeed);

    std::ifstream fa("ammo.json"); 
    json ja;                        
    fa >> ja;                       

    int ammoCount = ja.size();      

    std::unique_ptr<AmmoParams[]> ammo(new AmmoParams[ammoCount]);  
    for (int i = 0; i < ammoCount; ++i) {
        ammo[i].mass = ja[i]["mass"];
        ammo[i].drag = ja[i]["drag"];
        ammo[i].lift = ja[i]["lift"];
        std::strncpy(ammo[i].name, ja[i]["name"].get<std::string>().c_str(), 31);
    }
    LOG("Ammo loaded: " << ammoCount << " types");

    BallisticTable table;
if (!table.load("ballistic_table.txt")) {
    std::cerr << "Cannot open ballistic_table.txt" << std::endl;
    return 1;
}
LOG("Ballistic table loaded");

    std::ifstream ft("targets.json");
    json jt;
    ft >> jt;

    int tgtCount  = jt["targetCount"];
    int timeSteps = jt["timeSteps"];

    std::unique_ptr<Coord*[]> targets(new Coord*[tgtCount]);
for (int i = 0; i < tgtCount; i++) { 
    targets[i] = new Coord[timeSteps];
    for (int j = 0; j < timeSteps; j++) {
        targets[i][j].x = jt["targets"][i]["positions"][j]["x"];
        targets[i][j].y = jt["targets"][i]["positions"][j]["y"];
    }
}
LOG("Targets loaded: " << tgtCount);

// Знаходимо боєприпас
int bombIdx = -1;
for (int i = 0; i < ammoCount; i++) {
    if (strcmp(ammo[i].name, config.ammoName) == 0) {
        bombIdx = i;
        break;
    }
}
if (bombIdx == -1) {
    std::cerr << "Unknown ammo: " << config.ammoName << std::endl;
    
    return 1;
}
LOG("Ammo found: " << ammo[bombIdx].name);

// Ініціалізація дрона
float accel = (config.attackSpeed * config.attackSpeed) / (2.0f * config.accelPath);

DronePhysics physics(config.startPos, config.initialDir, accel,
                      config.angularSpeed, config.turnThreshold, config.attackSpeed);

float currentTime = 0.0f;
int currentTarget = -1;

// Динамічний масив кроків симуляції
const int MAX_STEPS = 10000;
std::unique_ptr<SimStep[]> steps(new SimStep[MAX_STEPS]);
int stepCount = 0;

// Основний цикл симуляції
while (stepCount < MAX_STEPS) {
    Coord dronePos = physics.getTelemetry().pos;

    float bestTime = -1.0f;
    int bestTarget = -1;

    for (int i = 0; i < tgtCount; i++) {
        Coord tPos = interpolateTarget(targets.get(), i, currentTime, config.arrayTimeStep, timeSteps);
        

        float dt = config.simTimeStep;
        Coord tNext = interpolateTarget(targets.get(), i, currentTime + dt, config.arrayTimeStep, timeSteps);
        Coord tVel = (tNext - tPos) * (1.0f / dt);
        auto res = table.lookup(config.altitude, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
        float ft = res.t;
        float h = res.hDist;
        Coord predicted = tPos + tVel * ft;

        Coord firePoint = calcFirePoint(dronePos, predicted, h, config.accelPath);
        float dist = length(firePoint - dronePos);
        float timeToStop = 0.0f; 
        float totalTime = ft + dist / config.attackSpeed + timeToStop;

        if (bestTime < 0 || totalTime < bestTime) {
            bestTime = totalTime;
            bestTarget = i;
        }
    }
    currentTarget = bestTarget;

    Coord tPos = interpolateTarget(targets.get(), currentTarget, currentTime, config.arrayTimeStep, timeSteps);
    Coord tNext = interpolateTarget(targets.get(), currentTarget, currentTime + config.simTimeStep, config.arrayTimeStep, timeSteps);
    auto res = table.lookup(config.altitude, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
    float ft = res.t;
    float h = res.hDist;
    Coord tVel = (tNext - tPos) * (1.0f / config.simTimeStep);
    Coord predicted = tPos + tVel * ft;
    Coord firePoint = calcFirePoint(dronePos, predicted, h, config.accelPath);

    float newDir = atan2f(firePoint.y - dronePos.y, firePoint.x - dronePos.x);
    

// Заповнюємо крок симуляції
    steps[stepCount].pos             = dronePos;
    steps[stepCount].direction       = physics.getDirection();
    steps[stepCount].stateName       = physics.getStateName();
    steps[stepCount].targetIdx       = currentTarget;
    steps[stepCount].dropPoint       = firePoint;
    steps[stepCount].aimPoint = dronePos + Coord{cosf(physics.getDirection()), sinf(physics.getDirection())} * h;
    steps[stepCount].predictedTarget = predicted;

    DroneContext ctx;
    physics.setCommand({newDir});
    physics.step(config.simTimeStep);
    

    Coord dronePosAfter = physics.getTelemetry().pos;
    float distToFire = length(firePoint - dronePosAfter);
    if (distToFire <= config.hitRadius && std::string(physics.getStateName()) == "Moving") break;
    
    stepCount++;
    currentTime += config.simTimeStep;
}
LOG("Simulation complete. Steps: " << stepCount);

// Запис у simulation.json
json out;
out["totalSteps"] = stepCount;
out["steps"] = json::array();
for (int i = 0; i < stepCount; i++) {
    json step;
    step["position"]        = {{"x", steps[i].pos.x}, {"y", steps[i].pos.y}};
    step["direction"]       = steps[i].direction;
    step["state"] = steps[i].stateName;
    step["targetIndex"]     = steps[i].targetIdx;
    step["dropPoint"]       = {{"x", steps[i].dropPoint.x}, {"y", steps[i].dropPoint.y}};
    step["aimPoint"]        = {{"x", steps[i].aimPoint.x}, {"y", steps[i].aimPoint.y}};
    step["predictedTarget"] = {{"x", steps[i].predictedTarget.x}, {"y", steps[i].predictedTarget.y}};
    out["steps"].push_back(step);
}
std::ofstream fout("simulation.json");
fout << out.dump(2);
fout.close();
LOG("simulation.json written");

// Звільнення пам'яті


for (int i = 0; i < tgtCount; i++)
    delete[] targets[i];

    return 0;
}
