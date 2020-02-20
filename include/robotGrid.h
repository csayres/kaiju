#pragma once
#include "robot.h"
#include "target.h"
#include "fiducial.h"
// #include <pybind11/stl_bind.h>

// move constants to cpp file?
extern const double pitch;

extern const double radius_buffer;

// extern const double ang_step;
// extern const int maxPathStepsGlob;
// line smoothing factor
// extern const double epsilon;
extern const double min_targ_sep;



class RobotGrid {
public:
    int nRobots;
    double epsilon;
    double collisionBuffer;
    double angStep;
    double greed;
    double phobia;
    bool didFail;
    int nSteps;
    int seed;
    int maxPathSteps;
    int smoothCollisions;
    bool initialized = false;
    double maxDisplacement;
    std::map<int, std::shared_ptr<Robot>> robotDict;
    std::map<int, std::shared_ptr<Fiducial>> fiducialDict;
    // std::vector<std::array<double, 2>> fiducialList;
    std::map<int, std::shared_ptr<Target>> targetDict;
    std::vector<std::array<double, 2>> perturbArray; // alpha/beta perturbations
    RobotGrid (double angStep = 1, double collisionBuffer = 2, double epsilon = 2, int seed = 0);
    void addRobot(int robotID, double xPos, double yPos, bool hasApogee = true);
    void addTarget(int targetID, double xPos, double yPos, FiberType fiberType, double priority = 0);
    void addFiducial(int fiducialID, double xPos, double yPos, double collisionBuffer = 1.5);
    void initGrid();
    void decollideGrid();
    int getNCollisions();
    std::vector<int> deadlockedRobots(); // robots not on target
    void clearPaths();
    void pathGen(); // step towards fold, initial solution
    void pathGenGreedy(); // stepRotational with encroachment
    void pathGenMDP(double greed, double phobia); // Markov Decision Process
    void simplifyPaths();
    void smoothPaths(int points);
    void verifySmoothed();
    // void optimizeTargets();
    void setCollisionBuffer(double newBuffer);
    // void setTargetList(Eigen::MatrixXd myTargetList); //std::vector<std::array<double, 5>> myTargetList);
    // void addTargetList(Eigen::MatrixXd myTargetList);
    std::shared_ptr<Robot> getRobot(int robotID);
    std::vector<int> targetlessRobots(); // returns robotIDs
    std::vector<int> unreachableTargets(); // returns targetIDs
    std::vector<int> assignedTargets(); // returns targetIDs
    // void pairwiseSwap();
    // void swapTargets(int r1ind, int r2ind);
    // void greedyAssign();
    void clearTargetDict();
    void assignRobot2Target(int robotID, int targID);
    void unassignTarget(int targID);
    void unassignRobot(int robotID);
    bool isValidAssignment(int robotID, int targID);
    std::vector<int> unassignedRobots();
    // bool canSwapTarget(std::shared_ptr<Robot> r1, std::shared_ptr<Robot> r2);
    bool isCollided(int robotID);
    std::vector<int> robotColliders(int robotID);
    std::vector<int> fiducialColliders(int robotID);
    bool neighborEncroachment(std::shared_ptr<Robot> r1);
    // bool isFiducialCollided(std::shared_ptr<Robot> r1);
    // bool isCollidedInd(int robotInd);
    void decollideRobot(int robotID);
    void stepTowardFold(std::shared_ptr<Robot> r1, int stepNum);
    // void stepEuclidean(std::shared_ptr<Robot> r1, int stepNum);
    void stepGreedy(std::shared_ptr<Robot> r1, int stepNum);
    void stepMDP(std::shared_ptr<Robot> r1, int stepNum);
    void stepBeta(std::shared_ptr<Robot> r1, int stepNum);
    // double closestApproach2(int robotID); // squared distance to closest neighbor
    // void smoothPath(std::shared_ptr<Robot> robot, double epsilon);
};
