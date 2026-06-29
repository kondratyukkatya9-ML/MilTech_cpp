#include <iostream>
#include <cmath>
#include "json.hpp" 
#include <fstream>
#include <cstring>
#include <memory>
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

    std::ifstream ft("targets.json");
    json jt;
    ft >> jt;

    int tgtCount  = jt["targetCount"];
    int timeSteps = jt["timeSteps"];

    Coord** targets = new Coord*[tgtCount];
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
    delete[] targets;
    return 1;
}
LOG("Ammo found: " << ammo[bombIdx].name);

// Ініціалізація дрона
Coord dronePos = config.startPos;
float droneDir = config.initialDir;
float droneSpeed = 0.0f;
DroneState droneState = STOPPED;
float currentTime = 0.0f;
int currentTarget = -1;
float accel = (config.attackSpeed * config.attackSpeed) / (2.0f * config.accelPath);

// Динамічний масив кроків симуляції
const int MAX_STEPS = 10000;
SimStep* steps = new SimStep[MAX_STEPS];
int stepCount = 0;

// Основний цикл симуляції
while (stepCount < MAX_STEPS) {
    float bestTime = -1.0f;
    int bestTarget = -1;

    for (int i = 0; i < tgtCount; i++) {
        Coord tPos = interpolateTarget(targets, i, currentTime, config.arrayTimeStep, timeSteps);
        float ft = calcFlightTime(config.altitude, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
        if (ft < 0) continue;

        float dt = config.simTimeStep;
        Coord tNext = interpolateTarget(targets, i, currentTime + dt, config.arrayTimeStep, timeSteps);
        Coord tVel = (tNext - tPos) * (1.0f / dt);
        Coord predicted = tPos + tVel * ft;

        float h = calcHorizDist(ft, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
        Coord firePoint = calcFirePoint(dronePos, predicted, h, config.accelPath);
        float dist = length(firePoint - dronePos);

        float timeToStop = (i != currentTarget) ? calcTimeToStop(droneState, droneSpeed, config.attackSpeed, accel) : 0.0f;
        float totalTime = ft + dist / config.attackSpeed + timeToStop;

        if (bestTime < 0 || totalTime < bestTime) {
            bestTime = totalTime;
            bestTarget = i;
        }
    }
    currentTarget = bestTarget;

    Coord tPos = interpolateTarget(targets, currentTarget, currentTime, config.arrayTimeStep, timeSteps);
    float ft = calcFlightTime(config.altitude, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
    float h = calcHorizDist(ft, config.attackSpeed, ammo[bombIdx].mass, ammo[bombIdx].drag, ammo[bombIdx].lift);
    Coord tNext = interpolateTarget(targets, currentTarget, currentTime + config.simTimeStep, config.arrayTimeStep, timeSteps);
    Coord tVel = (tNext - tPos) * (1.0f / config.simTimeStep);
    Coord predicted = tPos + tVel * ft;
    Coord firePoint = calcFirePoint(dronePos, predicted, h, config.accelPath);

    float newDir = atan2f(firePoint.y - dronePos.y, firePoint.x - dronePos.x);
    float deltaAngle = newDir - droneDir;
    while (deltaAngle >  3.14159f) deltaAngle -= 2*3.14159f;
    while (deltaAngle < -3.14159f) deltaAngle += 2*3.14159f;

    // Заповнюємо крок симуляції
    steps[stepCount].pos             = dronePos;
    steps[stepCount].direction       = droneDir;
    steps[stepCount].state           = (int)droneState;
    steps[stepCount].targetIdx       = currentTarget;
    steps[stepCount].dropPoint       = firePoint;
    steps[stepCount].aimPoint        = dronePos + Coord{cosf(droneDir), sinf(droneDir)} * h;
    steps[stepCount].predictedTarget = predicted;

    updateDrone(dronePos, droneDir, droneSpeed, droneState,
                newDir, deltaAngle, config.attackSpeed, accel,
                config.angularSpeed, config.turnThreshold, config.simTimeStep);

    float distToFire = length(firePoint - dronePos);
    if (distToFire <= config.hitRadius && droneState == MOVING) break;

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
    step["state"]           = steps[i].state;
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

delete[] steps;
steps = nullptr;

ammo = nullptr;

for (int i = 0; i < tgtCount; i++)
    delete[] targets[i];
delete[] targets;
targets = nullptr;
    return 0;
}
