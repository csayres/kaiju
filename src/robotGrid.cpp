#include <iostream>
#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <Eigen/Dense>
#include <algorithm>    // std::random_shuffle
#include <chrono>       // std::chrono::system_clock
#include "utils.h"
#include "robotGrid.h"

// define constants
// const double betaCollisionRadius = 1.5; // mm (3mm wide)

const double pitch = 22.4; // distance to next nearest neighbor

// const double angStep = 1; // degrees
// const int maxPathStepsGlob = (int)(ceil(700.0/angStep));
// line smoothing factor
// const double epsilon =  5 * angStep; // was 7*angStep for 0.1 step size

// const double min_targ_sep = 8; // mm
// const double min_targ_sep = 0; // mm

// bool sortTargList(std::array<double, 5> targ1, std::array<double, 5> targ2){
//     return targ1[3] > targ2[3];
// }

// bool sortTargList(std::shared_ptr<Target> t1, std::shared_ptr<Target> t2){
//     return t1->priority > t2->priority;
// }

// bool reverseSortTargList(std::shared_ptr<Target> t1, std::shared_ptr<Target> t2){
//     return t1->priority < t2->priority;
// }

// bool sortrobotID(std::shared_ptr<Robot> robot1, std::shared_ptr<Robot> robot2){
//     return robot1->id < robot2->id;
// }

RobotGrid::RobotGrid(double angStep, double collisionBuffer, double epsilon, int seed)
    : angStep(angStep), collisionBuffer(collisionBuffer), epsilon(epsilon)
{
    // nDia is number of robots along equator of grid
    srand(seed);
    // epsilon = myEpsilon;
    // collisionBuffer = myCollisionBuffer;
    // angStep = myAngStep;
    smoothCollisions = 0;
    maxPathSteps = (int)(ceil(1500.0/angStep));
}

void RobotGrid::addRobot(int robotID, double xPos, double yPos, bool hasApogee){
    // ensure robot id doesn't already exist
    if (initialized){
        throw std::runtime_error("RobotGrid is already initialized, no more robots allowed");
    }
    if (robotDict.count(robotID) > 0){
        throw std::runtime_error("Robot ID already exists");
    }
    robotDict[robotID] = std::make_shared<Robot>(robotID, xPos, yPos, angStep, hasApogee);
    robotDict[robotID]->setCollisionBuffer(collisionBuffer);
}

void RobotGrid::addTarget(int targetID, double xPos, double yPos, FiberType fiberType, double priority){
    if (!initialized){
        throw std::runtime_error("Initialize RobotGrid before adding targets");
    }
    if (targetDict.count(targetID) > 0){
        throw std::runtime_error("Target ID already exists");
    }

    targetDict[targetID] = std::make_shared<Target>(targetID, xPos, yPos, fiberType, priority);
    // add target to robots and robots to target
    for (auto rPair : robotDict){
        auto r = rPair.second;
        if (isValidAssignment(r->id, targetID)){
            r->validTargetIDs.push_back(targetID);
            targetDict[targetID]->validRobotIDs.push_back(r->id);
        }
    }
}

void RobotGrid::addFiducial(int fiducialID, double xPos, double yPos, double collisionBuffer){
    if (initialized){
        throw std::runtime_error("RobotGrid is already initialized, no more fiducials allowed");
    }
    if (fiducialDict.count(fiducialID) > 0){
        throw std::runtime_error("Fiducial ID already exists");
    }
    fiducialDict[fiducialID] = std::make_shared<Fiducial>(fiducialID, xPos, yPos, collisionBuffer);
}

void RobotGrid::initGrid(){
    // associate neighbors with fiducials and robots in grid
    double dx, dy, dist;
    if (initialized){
        throw std::runtime_error("RobotGrid is already initialized, don't do it twice!");
    }
    initialized = true;
    nRobots = robotDict.size();

    for (auto rPair1 : robotDict){
        // add fiducials (potential to collide with)
        auto r1 = rPair1.second;
        for (auto fPair : fiducialDict){
            auto fiducial = fPair.second;
            dx = r1->xPos - fiducial->x;
            dy = r1->yPos - fiducial->y;
            dist = hypot(dx, dy);
            if (dist < pitch+1) { // +1 for numerical buffer
                // std::cout << "add fiducial " << dist << std::endl;
                r1->addFiducialNeighbor(fiducial->id);
            }
        }
        // initialize all to alpha beta 0,0
        r1->setAlphaBeta(0,0);
        // int r2Ind = -1;
        for (auto rPair2 : robotDict){
            auto r2 = rPair2.second;
            // r2Ind++;
            // add neighbors (potential to collide with)
            if (r1->id==r2->id){
                continue;
            }
            dx = r1->xPos - r2->xPos;
            dy = r1->yPos - r2->yPos;
            dist = hypot(dx, dy);
            if (dist < (2*pitch+1)){ //+1 for numerical buffer
                // these robots are neighbors
                r1->addRobotNeighbor(r2->id);
            }
        }
    }
}

std::shared_ptr<Robot> RobotGrid::getRobot(int robotID){
    return robotDict[robotID];
}

void RobotGrid::setCollisionBuffer(double newBuffer){
    collisionBuffer = newBuffer;
    for (auto rPair : robotDict){
        auto r = rPair.second;
        r->setCollisionBuffer(newBuffer);
    }
}

void RobotGrid::decollideGrid(){
    if (!initialized){
        throw std::runtime_error("Initialize RobotGrid before decollideGrid");
    }
    for (int ii=0; ii<1000; ii++){
        // std::cout << "n collisions " << getNCollisions() << std::endl;
        if (!getNCollisions()){
            break;
        }

        for (auto rPair : robotDict){
            auto robotID = rPair.first;
            if (isCollided(robotID)){
                decollideRobot(robotID);
            }
        }
    }

    if (getNCollisions()){
        std::cout << "cannot decolide grid" << std::endl;
        throw std::runtime_error("Unable do decollide robot!!!");
    }

}


void RobotGrid::smoothPaths(int points){
    for (auto rPair : robotDict){
        auto r = rPair.second;
        r->smoothVelocity(points);
    }
}


void RobotGrid::simplifyPaths(){
    for (auto rPair : robotDict){
        auto r = rPair.second;
        r->simplifyPath(epsilon);
    }
}

void RobotGrid::verifySmoothed(){
    smoothCollisions = 0;
    for (int ii = 0; ii < nSteps; ii++){
        for (auto rPair : robotDict){
            auto r = rPair.second;
            r->setAlphaBeta(r->interpSimplifiedAlphaPath[ii](1), r->interpSimplifiedBetaPath[ii](1));
            // std::cout << " robot id " << r.id << std::endl;
        }
        smoothCollisions += getNCollisions();
    }
    // std::cout << "interp collisions: " << nCollisions << std::endl;
}



int RobotGrid::getNCollisions(){
    // return number of collisions found
    int nCollide = 0;
    for (auto rPair : robotDict){
        auto rId = rPair.first;
        if (isCollided(rId)) {
            nCollide++;
        }
    }
    return nCollide;
}

void RobotGrid::clearPaths(){
    if (!initialized){
        throw std::runtime_error("Initialize RobotGrid before pathGen");
    }
    for (auto rPair : robotDict){
        auto r = rPair.second;
        // verify that a target alpha beta has been set
        if (!r->hasTargetAlphaBeta){
            throw std::runtime_error("One or more robots have not received target alpha/beta");
        }
        // clear any existing path
        r->alphaPath.clear();
        r->betaPath.clear();
        r->simplifiedAlphaPath.clear();
        r->simplifiedBetaPath.clear(); // sparse
        r->interpSimplifiedAlphaPath.clear();
        r->interpSimplifiedBetaPath.clear(); // dense
        r->smoothedAlphaPath.clear();
        r->smoothedBetaPath.clear();
        r->smoothAlphaVel.clear();
        r->smoothBetaVel.clear();
        r->interpAlphaX.clear();
        r->interpAlphaY.clear();
        r->interpBetaX.clear();
        r->interpBetaY.clear(); // smoothed
        r->roughAlphaX.clear();
        r->roughAlphaY.clear();
        r->roughBetaX.clear();
        r->roughBetaY.clear();
        r->onTargetVec.clear();
    }

}

void RobotGrid::pathGenMDP(){
    // path gen 2 steps towards alpha beta target
    clearPaths();
    didFail = true;
    int ii;
    std::vector<int> robotIDs;
    for (auto rPair : robotDict){
        robotIDs.push_back(rPair.first);
    }

    for (ii=0; ii<maxPathSteps; ii++){
        // unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::random_shuffle(robotIDs.begin(), robotIDs.end());
        bool allAtTarget = true;
        for (auto robotID : robotIDs){
            auto r = robotDict[robotID];
            // std::cout << "path gen " << r.betaOrientation.size() << " " << r.betaModel.size() << std::endl;
            // std::cout << "alpha beta " << r.alpha << " " << r.beta << std::endl;
            stepMDP(r, ii);

            if (r->score()!=0) {
                // could just check the last elemet in onTargetVec? same thing.
                // or use robot->score
                allAtTarget = false;
            }
        }

        if (allAtTarget){
            std::cout << "all at target" << std::endl;
            didFail = false;
            break;
        }
    }

    nSteps = ii+1;
}

void RobotGrid::pathGenGreedy(){
    // path gen 2 steps towards alpha beta target
    clearPaths();
    didFail = true;
    int ii;
    for (ii=0; ii<maxPathSteps; ii++){

        bool allAtTarget = true;

        for (auto rPair : robotDict){
            auto r = rPair.second;
            // std::cout << "path gen " << r.betaOrientation.size() << " " << r.betaModel.size() << std::endl;
            // std::cout << "alpha beta " << r.alpha << " " << r.beta << std::endl;
            stepGreedy(r, ii);

            if (r->score()!=0) {
                // could just check the last elemet in onTargetVec? same thing.
                // or use robot->score
                allAtTarget = false;
            }
        }

        if (allAtTarget){
            std::cout << "all at target" << std::endl;
            didFail = false;
            break;
        }
    }

    nSteps = ii+1;
}


void RobotGrid::pathGen(){
    // first prioritize robots based on their alpha positions
    // robots closest to alpha = 0 are at highest risk with extended
    // betas for getting locked, so try to move those first
    // int pathPad = 20 / (float)angStep;
    clearPaths();
    didFail = true;
    int ii;
    for (ii=0; ii<maxPathSteps; ii++){
        bool allFolded = true;

        for (auto rPair : robotDict){
            auto r = rPair.second;
            // std::cout << "path gen " << r.betaOrientation.size() << " " << r.betaModel.size() << std::endl;
            // std::cout << "alpha beta " << r.alpha << " " << r.beta << std::endl;
            stepTowardFold(r, ii);
            if (r->beta!=180 or r->alpha!=0) { // stop when beta = 180} or r.alpha!=0)){
                allFolded = false;
            }
        }

        if (allFolded){
            didFail = false;
            break;
        }
    }

    nSteps = ii+1;
}

void RobotGrid::clearTargetDict(){
    targetDict.clear(); // does clear destroy the shared_ptrs?
    // clear all robot target lists
    for (auto rPair : robotDict){
        auto r = rPair.second;
        r->validTargetIDs.clear();
        r->clearAssignment();
    }
}

// void RobotGrid::addTargetList(Eigen::MatrixXd myTargetList){
//     int nRows = myTargetList.rows();
//     // sort by ascending priority 0 first
//     // std::sort(myTargetList.begin(), myTargetList.end(), sortTargList);

//     // add targets to robots and robots to targets
//     for (int ii = 0; ii < nRows; ii++){
//         auto targPtr = std::make_shared<Target>((int)myTargetList(ii, 0), myTargetList(ii, 1), myTargetList(ii, 2), (int)myTargetList(ii, 3), (int)myTargetList(ii, 4) );
//         targetList.push_back(std::move(targPtr));
//     }
//     // sort target list in order of priority
//     std::sort(targetList.begin(), targetList.end(), sortTargList);

//     // associate available targets to robots
//     int targInd = -1;
//     for (auto t : targetList){
//         targInd++;
//         int robotInd = -1;
//         for (auto r : allRobots){
//             robotInd++;
//             if (r->isValidTarget(t->x, t->y, t->fiberID)){
//                 r->targetInds.push_back(targInd);  // should be sorted by priority
//                 t->robotInds.push_back(robotInd);
//             }
//         }
//     }
// }

// void RobotGrid::setTargetList(Eigen::MatrixXd myTargetList){ //std::vector<std::array<double, 5>> myTargetList){
//     // targetID, x, y, priority, fiberID (1=apogee 2=boss)
//     // std::array<double, 2> ab;

// 	clearTargetList();
// 	addTargetList(myTargetList);
// }

// void RobotGrid::greedyAssign(){
//     // assign the highest priority targets to robots
//     // initialize each robot to its highest priority target
//     // only allow one target per robot
//     // int robotInd = -1;
//     for (auto rPair : robotDict){
//         // robotInd++;
//         auto r = rPair.second;
//         for (auto targetInd : r->targetInds){
//             auto targ = targetList[targetInd];
//             if (targ->isAssigned()){
//                 // target has been assigned to other robot
//                 continue;
//             }
//             if (r->isValidTarget(targ->x, targ->y, targ->fiberID)){
//                 targ->assignRobot(r.id);
//                 r->assignTarget(targetInd, targ->x, targ->y, targ->fiberID);
//                 break; // break from target loop
//             }
//         }
//     }
// }

// void RobotGrid::pairwiseSwap(){
//     // look for pairwise swaps that reduce collisions
//     int r1ind = -1;
//     for (auto r1 : allRobots){
//         r1ind++;
//         if (isCollided(r1)){
//             // use getNCollisions because the swap may
//             // decolide the robot, but may introduce a new
//             // collision
//             double initialCollisions = getNCollisions();
//             for (auto r2ind : r1->neighborInds){
//                 auto r2 = allRobots[r2ind];
//                 if (canSwapTarget(r1, r2)){
//                     swapTargets(r1ind, r2ind);
//                     if (initialCollisions <= getNCollisions()){
//                         // swap targets back
//                         // collision still exists
//                         // or is worse!
//                         swapTargets(r1ind, r2ind);
//                     }
//                     else {
//                         // collision resolved
//                         break;
//                     }
//                 }
//             }
//         }
//     }
// }

// bool RobotGrid::canSwapTarget(std::shared_ptr<Robot> r1, std::shared_ptr<Robot> r2){
//     // std::cout << "in canSwapTarget" << std::endl;
//     if (!r1->isAssigned() or !r2->isAssigned()){
//         // one positioner is without a target
//         return false;
//     }
//     auto r1targ = targetList[r1->assignedTargetInd];
//     auto r2targ = targetList[r2->assignedTargetInd];
//     bool r1canReach = r1->isValidTarget(r2targ->x, r2targ->y, r2targ->fiberID);
//     bool r2canReach = r2->isValidTarget(r1targ->x, r1targ->y, r1targ->fiberID);
//     return r1canReach and r2canReach;
// }

// void RobotGrid::swapTargets(int r1Ind, int r2Ind){
//     // std::shared_ptr<Target> savedTarget = r1->assignedTarget;
//     // r1->assignTarget(r2->assignedTarget);
//     // r2->assignTarget(savedTarget);
//     // // update target->robot assignments
//     // r1->assignedTarget->assignRobot(r1);
//     // r2->assignedTarget->assignRobot(r2);
//     auto r1 = allRobots[r1Ind];
//     auto r2 = allRobots[r2Ind];

//     auto r1targ = targetList[r1->assignedTargetInd];
//     auto r2targ = targetList[r2->assignedTargetInd];
//     auto r1targInd = r1->assignedTargetInd;
//     auto r2targInd = r2->assignedTargetInd;

//     r1->assignTarget(r2targInd, r2targ->x, r2targ->y, r2targ->fiberID);
//     r2->assignTarget(r1targInd, r1targ->x, r1targ->y, r1targ->fiberID);

//     r1targ->assignRobot(r2Ind);
//     r2targ->assignRobot(r1Ind);

// }

std::vector<int> RobotGrid::unassignedRobots(){
    // return the ids of unassigned robots
    std::vector<int> idleRobotIDs;
    for (auto rPair : robotDict){
        auto robot = rPair.second;
        if (!robot->isAssigned()){
            idleRobotIDs.push_back(robot->id);
        }
    }
    return idleRobotIDs;
}

std::vector<int> RobotGrid::targetlessRobots(){
    // return the ids of robots that can reach no targets in targetDict
    std::vector<int> idleRobotIDs;
    for (auto rPair : robotDict){
        auto robot = rPair.second;
        if (robot->validTargetIDs.size()==0){
            idleRobotIDs.push_back(robot->id);
        }
    }
    return idleRobotIDs;
}

std::vector<int> RobotGrid::unreachableTargets(){
    // return list of targetIDs that are not reachable
    // by any robot
    if (targetDict.size()==0){
        throw std::runtime_error("unreachableTargets() failure, target list not yet set");
    }
    std::vector<int> badTargs;
    for (auto tPair : targetDict){
        auto targ = tPair.second;
        if (targ->validRobotIDs.size()==0){
            badTargs.push_back(targ->id);
        }
    }
    return badTargs;
}

std::vector<int> RobotGrid::assignedTargets(){
    // return list of targetIDs that have been assigned
    // to robots
    std::vector<int> assignedTargIDs;

    for (auto tPair : targetDict){
        auto t = tPair.second;
        if (t->isAssigned()){
            assignedTargIDs.push_back(t->id);
        }
    }

    return assignedTargIDs;
}

// bool RobotGrid::isValidRobotTarget(int robotID, int targetInd1){
//     auto robot = robotDict[robotID];
//     for (auto targetInd2 : robot->targetInds){
//         auto target = targetList[targetInd2];
//         if (targetInd1 == targetInd2 and !target->isAssigned()){
// 					return true;
//         }
//     }
//     return false;
// }

void RobotGrid::unassignTarget(int targID){
    // clear the the target assignment, and the
    // robot to which it's assigned
    auto target = targetDict[targID];
    if (!target->isAssigned()){
        // do nothing, target isnt assigned
        return;
    }
    auto robot = robotDict[target->assignedRobotID];
    target->clearAssignment();
    robot->clearAssignment();
}

void RobotGrid::unassignRobot(int robotID){
    // clear the the target assignment, and the
    // robot to which it's assigned
    auto robot = robotDict[robotID];
    if (!robot->isAssigned()){
        // do nothing, target isnt assigned
        return;
    }
    auto target = targetDict[robot->assignedTargetID];
    target->clearAssignment();
    robot->clearAssignment();
}

void RobotGrid::assignRobot2Target(int robotID, int targetID){
    // releases robot's previous target if present
    // releases target's previous robot if present
    auto robot = robotDict[robotID];
    auto target = targetDict[targetID];

    int ii = std::count(robot->validTargetIDs.begin(), robot->validTargetIDs.end(), targetID);
    if (ii == 0){
        throw std::runtime_error("target not valid for robot");
    }
    unassignRobot(robotID);
    unassignTarget(targetID);
    target->assignRobot(robotID);
    robot->assignTarget(targetID);
    auto ab = robot->alphaBetaFromFiberXY(target->x, target->y, target->fiberType);
    robot->setAlphaBeta(ab[0], ab[1]);
}

bool RobotGrid::isValidAssignment(int robotID, int targetID){
    auto robot = robotDict[robotID];
    auto target = targetDict[targetID];
    // first a quick position cut
    // quick position cut fails near edges, get rid of it
    // double targDist = hypot(target->x - robot->xPos, target->y - robot->yPos);
    // if (targDist > maxReach or targDist < minReach) {
    //     return false;
    // }
    if (target->fiberType == ApogeeFiber and !robot->hasApogee){
        return false;
    }
    auto ab = robot->alphaBetaFromFiberXY(target->x, target->y, target->fiberType);
    // check alpha beta valid
    if (std::isnan(ab[0]) or std::isnan(ab[1])){
        return false;
    }
    // check alpha beta in range
    if (ab[0]<0 or ab[0]>=360){
        return false;
    }
    if (ab[1]<0 or ab[1]>180){
        return false;
    }
    // save current alpha beta
    double savedAlpha = robot->alpha;
    double savedBeta = robot->beta;
    robot->setAlphaBeta(ab[0], ab[1]);
    auto collidedFiducials = fiducialColliders(robotID);
    if (collidedFiducials.size() != 0){
        // this target interferes with a fiducial which is immobile
        return false;
    }
    // reset alpha beta
    robot->setAlphaBeta(savedAlpha, savedBeta);
    return true;
}

// void RobotGrid::optimizeTargets(){
//     // rewrite!!!
// }

double RobotGrid::closestApproach2(int robotID){
    double minDist2 = 1e15; // to be minimized
    auto robot1 = robotDict[robotID];
    for (auto otherRobotID : robot1->robotNeighbors){
        auto robot2 = robotDict[otherRobotID];
        auto dist2 = dist3D_Segment_to_Segment(
                robot2->betaCollisionSegment[0], robot2->betaCollisionSegment[1],
                robot1->betaCollisionSegment[0], robot1->betaCollisionSegment[1]
            );
        if (dist2 < minDist2){
            minDist2 = dist2;
        }
    }
    return minDist2;
}

bool RobotGrid::isCollided(int robotID){
    auto robotsColliding = robotColliders(robotID);
    if (robotsColliding.size() != 0){
        return true;
    }
    auto fiducialsColliding = fiducialColliders(robotID);
    if (fiducialsColliding.size() != 0){
        return true;
    }
    return false;
}


double RobotGrid::encroachmentScore(int robotID, double distance){
    // score, separation2
    // look ahead and see robots getting close
    double dist2, dist, score;
    double encroachment = 0;
    // for (auto nsd : neighborScoreDist){
    //     if (score < nsd[0] and nsd[1] < 2.5*collisionBuffer){
    //         // 4*collisions buffer is factor of two bigger
    //         // than collision
    //         encroachment += 1/nsd[1];
    //     }
    // }


    // check collisions with neighboring robots
    auto robot1 = robotDict[robotID];
    score = robot1->score();
    for (auto otherRobotID : robot1->robotNeighbors){
        auto robot2 = robotDict[otherRobotID];
        // squared distance returned
        dist2 = dist3D_Segment_to_Segment(
                robot2->betaCollisionSegment[0], robot2->betaCollisionSegment[1],
                robot1->betaCollisionSegment[0], robot1->betaCollisionSegment[1]
            );

        dist = sqrt(dist2);

        if ((score < robot2->score()) and (dist < distance)){
            encroachment += 1/dist;
        }
    }
    return encroachment;
}


std::vector<int> RobotGrid::robotColliders(int robotID){

    std::vector<int> collidingNeighbors;
    double dist2, collideDist2;
    // check collisions with neighboring robots
    auto robot1 = robotDict[robotID];
    for (auto otherRobotID : robot1->robotNeighbors){
        auto robot2 = robotDict[otherRobotID];
        // squared distance returned
        dist2 = dist3D_Segment_to_Segment(
                robot2->betaCollisionSegment[0], robot2->betaCollisionSegment[1],
                robot1->betaCollisionSegment[0], robot1->betaCollisionSegment[1]
            );

        collideDist2 = (2*collisionBuffer)*(2*collisionBuffer);
        if (dist2 < collideDist2){
            // std::cout << "dist " << dist2 - collide_dist_squared << std::endl;
            collidingNeighbors.push_back(robot2->id);
        }

    }
    return collidingNeighbors;
}

std::vector<int> RobotGrid::fiducialColliders(int robotID){

    std::vector<int> collidingNeighbors;
    auto robot = robotDict[robotID];
    // std::cout << "isFiducialCollided" << std::endl;
    double dist2, collideDist2;
    // std::cout << "n fiducials " << fiducials.size() << std::endl;
    for (auto fiducialID : robot->fiducialNeighbors){
        auto fiducial = fiducialDict[fiducialID];
        // squared distance
        // std::array<double, 2> xyCoord = {fiducial->x, fiducial->y};
        Eigen::Vector3d xyzCoord = {fiducial->x, fiducial->y, focalZ};
        dist2 = dist3D_Point_to_Segment(
                xyzCoord, robot->betaCollisionSegment[0],
                robot->betaCollisionSegment[1]
                );
        collideDist2 =  (robot->collisionBuffer+fiducial->collisionBuffer) *
                        (robot->collisionBuffer+fiducial->collisionBuffer);

        if (dist2 < collideDist2){
            collidingNeighbors.push_back(fiducial->id);
        }
    }
    return collidingNeighbors;

}


void RobotGrid::decollideRobot(int robotID){
    // remove assigned target if present
    // std::cout << "decolliding robot " << robot->id << std::endl;
    unassignRobot(robotID);
    auto robot = robotDict[robotID];
    for (int ii=0; ii<1000; ii++){
        robot->setXYUniform();
        // nDecollide ++;
        if (!isCollided(robotID)){
            // std::cout << "decollide successful " << std::endl;
            break;
        }
    }
    // are we still collided?
    // if (isCollided(robot)){
    //     throw std::runtime_error("Unable do decollide robot!!!");
    // }
}

std::vector<int> RobotGrid::deadlockedRobots(){
    std::vector<int> deadlockedRobotIDs;
    for (auto rPair : robotDict){
        auto robot = rPair.second;
        if (robot->targetAlpha != robot->alpha or robot->targetBeta != robot->beta){
            deadlockedRobotIDs.push_back(robot->id);
        }
    }
    return deadlockedRobotIDs;
}

void RobotGrid::stepGreedy(std::shared_ptr<Robot> robot, int stepNum){

    double encroachment, score;
    double currAlpha = robot->alpha;
    double currBeta = robot->beta;
    double bestAlpha, bestBeta, bestScore;
    bestAlpha = currAlpha;
    bestBeta = currBeta;
    bestScore = 1e16; // to be minimized
    // bestScore = robot->score() + 1/closestApproach2(robot->id);
    // bestScore = 1e16;

    Eigen::Vector2d alphaPathPoint;
    Eigen::Vector2d betaPathPoint;
    Eigen::Vector2d temp;

    alphaPathPoint(0) = stepNum;
    betaPathPoint(0) = stepNum;


    if (robot->score()==0){
        // at target don't move
        alphaPathPoint(1) = currAlpha;
        betaPathPoint(1) = currBeta;
        robot->alphaPath.push_back(alphaPathPoint);
        robot->betaPath.push_back(betaPathPoint);
        robot->onTargetVec.push_back(true);

        // note make collision segment just two points

        temp(0) = stepNum;
        temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
        robot->roughAlphaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
        robot->roughAlphaY.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
        robot->roughBetaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
        robot->roughBetaY.push_back(temp);

        return;
    }

    robot->lastStepNum = stepNum;

    // check all move combinations for each axis
    for (int betaDir=-1; betaDir<2; betaDir++){
        for (int alphaDir=-1; alphaDir<2; alphaDir++){

            double nextAlpha = currAlpha + alphaDir * angStep;
            double nextBeta = currBeta + betaDir * angStep;
            // careful not to overshoot
            if (currAlpha > robot->targetAlpha and nextAlpha <= robot->targetAlpha){
                nextAlpha = robot->targetAlpha;
            }
            if (currAlpha < robot->targetAlpha and nextAlpha >= robot->targetAlpha){
                nextAlpha = robot->targetAlpha;
            }
            if (currBeta > robot->targetBeta and nextBeta <= robot->targetBeta){
                nextBeta = robot->targetBeta;
            }
            if (currBeta < robot->targetBeta and nextBeta >= robot->targetBeta){
                nextBeta = robot->targetBeta;
            }
            // handle limits of travel
            // can probably ditch this as target
            // must be in range anyways
            if (nextAlpha > 360){
                nextAlpha = 360;
            }
            if (nextAlpha < 0){
                nextAlpha = 0;
            }
            if (nextBeta > 360){
                nextBeta = 360;
            }
            if (nextBeta < 0){
                nextBeta = 0;
            }

            robot->setAlphaBeta(nextAlpha, nextBeta);
            score = robot->score();
            // double encroachment = 0;

            if (!isCollided(robot->id)){
                if (score < bestScore){
                    bestScore = score;
                    bestAlpha = nextAlpha;
                    bestBeta = nextBeta;

                }

                else if (score == bestScore and randomSample() >= 0.5){
                    // flip a coin to see whether to accept
                    bestScore = score;
                    bestAlpha = nextAlpha;
                    bestBeta = nextBeta;
                }
            }
        }
    }  // end loop over perturbations

    // set alpha beta to best found option
    robot->setAlphaBeta(bestAlpha, bestBeta);
    alphaPathPoint(1) = bestAlpha;
    betaPathPoint(1) = bestBeta;
    robot->alphaPath.push_back(alphaPathPoint);
    robot->betaPath.push_back(betaPathPoint);
    if (robot->score()==0){
        robot->onTargetVec.push_back(true);
    }
    else {
        robot->onTargetVec.push_back(false);
    }

    // add alpha/beta xy points
    // Eigen::Vector2d temp;
    temp(0) = stepNum;
    temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
    robot->roughAlphaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
    robot->roughAlphaY.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
    robot->roughBetaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
    robot->roughBetaY.push_back(temp);


}

void RobotGrid::stepMDP(std::shared_ptr<Robot> robot, int stepNum){

    double encroachment, score, dist, dist2, localEnergy, closestNeighbor, cost;
    double currAlpha = robot->alpha;
    double currBeta = robot->beta;
    double bestAlpha, bestBeta, bestScore, bestEncroachment;
    bestAlpha = currAlpha;
    bestBeta = currBeta;
    bestScore = 1e16; // to be minimized
    bestEncroachment = 1e16; // to be minimized
    double bestCost = 1e16; // to be minimized
    // bestScore = robot->score() + 1/closestApproach2(robot->id);
    // bestScore = 1e16;

    Eigen::Vector2d alphaPathPoint;
    Eigen::Vector2d betaPathPoint;
    Eigen::Vector2d temp;

    alphaPathPoint(0) = stepNum;
    betaPathPoint(0) = stepNum;

    // nextAlpha, nextBeta, local energy, score
    std::vector<std::array<double, 4>> stateOptions;


    if (robot->score()==0 and encroachmentScore(robot->id, 2.4*collisionBuffer)==0){ // replace this with score?
    // if (robot->score()==0 and betaEncroachmentScore2(robot->id)==0){ // replace this with score?
    // if (robot->betaScore()==0 and encroachmentScore(robot->id, 2*collisionBuffer+robot->maxDisplacement)==0){ // replace this with score?
        // done folding no one coming don't move
        alphaPathPoint(1) = currAlpha;
        betaPathPoint(1) = currBeta;
        robot->alphaPath.push_back(alphaPathPoint);
        robot->betaPath.push_back(betaPathPoint);
        robot->onTargetVec.push_back(true);

        // note make collision segment just two points

        temp(0) = stepNum;
        temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
        robot->roughAlphaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
        robot->roughAlphaY.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
        robot->roughBetaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
        robot->roughBetaY.push_back(temp);

        return;
    }

    robot->lastStepNum = stepNum;

    // check all move combinations for each axis
    for (int betaDir=-1; betaDir<2; betaDir++){
        for (int alphaDir=-1; alphaDir<2; alphaDir++){

            double nextAlpha = currAlpha + alphaDir * angStep;
            double nextBeta = currBeta + betaDir * angStep;
            // careful not to overshoot
            if (currAlpha > robot->targetAlpha and nextAlpha <= robot->targetAlpha){
                nextAlpha = robot->targetAlpha;
            }
            if (currAlpha < robot->targetAlpha and nextAlpha >= robot->targetAlpha){
                nextAlpha = robot->targetAlpha;
            }
            if (currBeta > robot->targetBeta and nextBeta <= robot->targetBeta){
                nextBeta = robot->targetBeta;
            }
            if (currBeta < robot->targetBeta and nextBeta >= robot->targetBeta){
                nextBeta = robot->targetBeta;
            }
            // handle limits of travel
            // can probably ditch this as target
            // must be in range anyways
            if (nextAlpha > 360){
                nextAlpha = 360;
            }
            if (nextAlpha < 0){
                nextAlpha = 0;
            }
            if (nextBeta > 360){
                nextBeta = 360;
            }
            if (nextBeta < 0){
                nextBeta = 0;
            }

            robot->setAlphaBeta(nextAlpha, nextBeta);
            // score is min possible steps till goal
            score = robot->score(); // std::max(robot->alphaScore(), robot->betaScore()); //robot->score(); //+ nextClosestApproach2;
            localEnergy = 0;
            closestNeighbor = 1e16; // to be minimized

            // compute robot's local energy, and nearest neighbor
            for (auto otherRobotID : robot->robotNeighbors){
                auto otherRobot = robotDict[otherRobotID];
                dist2 = dist3D_Segment_to_Segment(
                    otherRobot->betaCollisionSegment[0], otherRobot->betaCollisionSegment[1],
                    robot->betaCollisionSegment[0], robot->betaCollisionSegment[1]
                );

                localEnergy += 1/dist2;
                dist = sqrt(dist2);
                if (dist < closestNeighbor){
                    closestNeighbor = dist;
                }
            }

            if (closestNeighbor < 2*collisionBuffer){
                // this is not a viable move option
                // go on to next try
                continue;
            }
            stateOptions.push_back({nextAlpha, nextBeta, localEnergy, score});

            // double encroachment = 0;
        }
    } // end loop over perturbations


    // pick the best move, first shuffle the possiblity array
    std::random_shuffle(stateOptions.begin(), stateOptions.end());
    // figure out probabilities of transistioning based on score
    // score is distance left to move

    // determine whether to rank on score, or energy
    int ind;
    if (randomSample() > 0.8) { //} (0.75 + stepNum / (0.25*maxPathSteps))){
        ind = 2; // increase distance to neighbors
    }
    else{
        ind = 3; // reduce score
    }

    for (auto stateOption : stateOptions){
        score = stateOption[ind];
        if (score < bestScore and randomSample() > 0.2){
            // almost always pick a better score
            bestScore = score;
            bestAlpha = stateOption[0];
            bestBeta = stateOption[1];
        }
        else if (score == bestScore and randomSample() > 0.5){
            bestScore = score;
            bestAlpha = stateOption[0];
            bestBeta = stateOption[1];
        }

    }


    // set alpha beta to best found option
    robot->setAlphaBeta(bestAlpha, bestBeta);
    alphaPathPoint(1) = bestAlpha;
    betaPathPoint(1) = bestBeta;
    robot->alphaPath.push_back(alphaPathPoint);
    robot->betaPath.push_back(betaPathPoint);
    if (robot->targetAlpha == robot->alpha and robot->targetBeta == robot->beta){
        robot->onTargetVec.push_back(true);
    }
    else {
        robot->onTargetVec.push_back(false);
    }

    // add alpha/beta xy points
    // Eigen::Vector2d temp;
    temp(0) = stepNum;
    temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
    robot->roughAlphaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
    robot->roughAlphaY.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
    robot->roughBetaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
    robot->roughBetaY.push_back(temp);


}

void RobotGrid::stepTowardFold(std::shared_ptr<Robot> robot, int stepNum){
    double currAlpha = robot->alpha;
    double currBeta = robot->beta;
    Eigen::Vector2d alphaPathPoint;
    Eigen::Vector2d betaPathPoint;
    Eigen::Vector2d temp;
    alphaPathPoint(0) = stepNum;
    betaPathPoint(0) = stepNum;
    if (currBeta==180 and currAlpha==0){
        // done folding don't move
        alphaPathPoint(1) = currAlpha;
        betaPathPoint(1) = currBeta;
        robot->alphaPath.push_back(alphaPathPoint);
        robot->betaPath.push_back(betaPathPoint);

        temp(0) = stepNum;
        temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
        robot->roughAlphaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
        robot->roughAlphaY.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
        robot->roughBetaX.push_back(temp);
        temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
        robot->roughBetaY.push_back(temp);

        return;
    }
    // this is for keeping track of last step
    // only updates if robot hasn't reached fold
    robot->lastStepNum = stepNum;
    // begin trying options pick first that works
    for (int ii=0; ii<robot->alphaBetaArr.rows(); ii++){
        double nextAlpha = currAlpha + robot->alphaBetaArr(ii, 0);
        double nextBeta = currBeta + robot->alphaBetaArr(ii, 1);
        if (nextAlpha > 360){
            nextAlpha = 360;
        }
        if (nextAlpha < 0){
            nextAlpha = 0;
        }
        if (nextBeta > 180){
            nextBeta = 180;
        }
        if (nextBeta < 0){
            nextBeta = 0;
        }
        // if next choice results in no move skip it
        // always favor a move
        if (nextBeta==currBeta and nextAlpha==currAlpha){
            continue;
        }
        robot->setAlphaBeta(nextAlpha, nextBeta);
        if (!isCollided(robot->id)){
            alphaPathPoint(1) = nextAlpha;
            betaPathPoint(1) = nextBeta;
            robot->alphaPath.push_back(alphaPathPoint);
            robot->betaPath.push_back(betaPathPoint);

            // add alpha/beta xy points

            temp(0) = stepNum;
            // std::cout << "beta orientation size: " << betaOrientation.size() << std::endl;
            // std::cout << "beta model size: " << betaModel.size() << std::endl;
            temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
            // std::cout << "step toward fold " << ii << std::endl;

            robot->roughAlphaX.push_back(temp);

            temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
            robot->roughAlphaY.push_back(temp);
            temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
            robot->roughBetaX.push_back(temp);
            temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
            robot->roughBetaY.push_back(temp);

            return;
        }
    }

    // no move options worked,
    // settle for a non-move
    robot->setAlphaBeta(currAlpha, currBeta);
    alphaPathPoint(1) = currAlpha;
    betaPathPoint(1) = currBeta;
    robot->alphaPath.push_back(alphaPathPoint);
    robot->betaPath.push_back(betaPathPoint);

    // add alpha/beta xy points
    // Eigen::Vector2d temp;
    temp(0) = stepNum;
    temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
    robot->roughAlphaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
    robot->roughAlphaY.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
    robot->roughBetaX.push_back(temp);
    temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
    robot->roughBetaY.push_back(temp);
}




// bool RobotGrid::isCollidedInd(int robotInd){
//     auto robot = allRobots[robotInd];
//     return isCollided(robot);
// }

// void RobotGrid::smoothPath(std::shared_ptr<Robot> robot1, double epsilon){
//     // smooth a previously generated path
//     double interpSmoothAlpha, interpSmoothBeta;
//     int npts;
//     Eigen::Vector2d atemp, btemp;
//     RamerDouglasPeucker(robot1->alphaPath, epsilon, robot1->simplifiedAlphaPath);
//     // bias alpha positive direction because we are approaching zero
//     npts = robot1->simplifiedAlphaPath.size();
//     for (int ii=1; ii<npts-1; ii++){
//         // only shift internal (not end) points
//         robot1->simplifiedAlphaPath[ii](1) = robot1->simplifiedAlphaPath[ii](1);// + epsilon;
//     }

//     RamerDouglasPeucker(robot1->betaPath, epsilon, robot1->simplifiedBetaPath);
//     // bias beta negative direction because we are approaching 180
//     // linearly interpolate smooth paths to same step values
//     // as computed
//     // bias alpha positive direction because we are approaching zero
//     npts = robot1->simplifiedBetaPath.size();
//     for (int ii=1; ii<npts-1; ii++){
//         // only shift internal (not end) points
//         robot1->simplifiedBetaPath[ii](1) = robot1->simplifiedBetaPath[ii](1);// - epsilon;
//     }

//     // calculate smoothed alpha betas at every step
//     int nDensePoints = robot1->alphaPath.size();
//     for (int ii=0; ii<nDensePoints; ii++){
//         double xVal = alphaPath[ii](0);
//         atemp(0) = xVal; // interpolation step
//         btemp(0) = xVal;
//         interpSmoothAlpha = linearInterpolate(robot1->simplifiedAlphaPath, xVal);
//         // bias alpha in positive direction because we're approaching zero
//         atemp(1) = interpSmoothAlpha;
//         interpSimplifiedAlphaPath.push_back(atemp);
//         interpSmoothBeta = linearInterpolate(robot1->simplifiedBetaPath, xVal);
//         btemp(1) = interpSmoothBeta;
//         interpSimplifiedBetaPath.push_back(btemp);

//         // populate interpXY points for alpha/beta ends
//         robot1->setAlphaBeta(interpSmoothAlpha, interpSmoothBeta);
//         atemp(1) = betaCollisionSegment[0](0); // xAlphaEnd
//         interpAlphaX.push_back(atemp);
//         atemp(1) = betaCollisionSegment[0](1); // yAlphaEnd
//         interpAlphaY.push_back(atemp);
//         atemp(1) = betaCollisionSegment.back()(0); // xBetaEnd
//         interpBetaX.push_back(atemp);
//         atemp(1) = betaCollisionSegment.back()(1); // yBetaEnd
//         interpBetaY.push_back(atemp);

//         atemp(1) = 0; // not collided
//         if (isCollided()){
//             atemp(1) = 1;
//         }
//         interpCollisions.push_back(atemp);

//     }

// }

// void RobotGrid::optimizeTargets(){
//     // warning...only considering boss fibers
//     // at the moment
//     double myX,myY,nX,nY;
//     bool swapWorked;
//     // Robot * rn;
//     // int randIndex;
//     // int randNeighborIndex;

//     // could compile all this stuff into lookup
//     // tables but this is working ok

//     for (auto r : allRobots){
//         if (!r->isCollided()){
//             continue;  // this robot isn't collided skip it
//         }
//         swapWorked = false;
//         // save current assignment
//         myX = r->bossFiberPos(0);
//         myY = r->bossFiberPos(1);
//         std::vector<int> swappableNeighbors;
//         for (int ii=0; ii < r->neighbors.size(); ii++){
//             auto rn = r->neighbors[ii];
//             nX = rn->bossFiberPos(0);
//             nY = rn->bossFiberPos(1);
//             if (!rn->checkFiberXYGlobal(myX, myY, 2)){
//                 // neighbor can't reach mine
//                 // move to next robot
//                 continue;
//             }
//             if (!r->checkFiberXYGlobal(nX, nY, 2)){
//                 // i cant reach neighbor
//                 // move to next robot
//                 continue;
//             }
//             // we can reach eachother, test it
//             swappableNeighbors.push_back(ii);
//             r->setFiberXY(nX, nY, 2);
//             rn->setFiberXY(myX, myY, 2);
//             if (!r->isCollided()){
//                 // no longer collided
//                 // keep this swap!
//                 // first one that's good
//                 swapWorked = true;
//                 break;
//             }
//             // swap them back
//             r->setFiberXY(myX, myY, 2);
//             rn->setFiberXY(nX, nY, 2);
//         }

//         // if (!swapWorked and swappableNeighbors.size()>0){
//         //     // we didn't find any swap that got rid
//         //     // of collision, pick a random valid
//         //     // swap for this robot
//         //     // set them back,
//         //     // swap didn't work
//         //     randIndex = rand() % swappableNeighbors.size();
//         //     rn = r.neighbors[swappableNeighbors[randIndex]];
//         //     nX = rn->fiber_XYZ(0);
//         //     nY = rn->fiber_XYZ(1);

//         //     r.setFiberXY(nX, nY);
//         //     rn->setFiberXY(myX, myY);
//         // }


//     }


// }







////// euclidean /////////
// void RobotGrid::stepEuclidean(std::shared_ptr<Robot> robot, int stepNum){
//     bool alphaOnTarg, betaOnTarg;
//     double currAlpha = robot->alpha;
//     double currBeta = robot->beta;
//     double bestAlpha, bestBeta;
//     bestAlpha = currAlpha;
//     bestBeta = currBeta;
//     int bestInd = -1;

//     // Eigen::Vector3d currMetFiberPos;
//     // currMetFiberPos = robot->metFiberPos;

//     Eigen::Vector2d alphaPathPoint;
//     Eigen::Vector2d betaPathPoint;
//     Eigen::Vector2d temp;

//     alphaPathPoint(0) = stepNum;
//     betaPathPoint(0) = stepNum;

//     // hangle alpha wrapping
//     if (currAlpha == 360 and robot->targetAlpha==0){
//         alphaOnTarg = true;
//     }
//     else if (currAlpha == 0 and robot->targetAlpha==360){
//         alphaOnTarg = true;
//     }
//     else if (currAlpha == robot->targetAlpha){
//         alphaOnTarg = true;
//     }
//     else {
//         alphaOnTarg = false;
//     }

//     // hangle alpha wrapping
//     if (currBeta == 360 and robot->targetBeta==0){
//         betaOnTarg = true;
//     }
//     else if (currBeta == 0 and robot->targetBeta==360){
//         betaOnTarg = true;
//     }
//     else if (currBeta == robot->targetBeta){
//         betaOnTarg = true;
//     }
//     else {
//         betaOnTarg = false;
//     }

//     if (betaOnTarg and alphaOnTarg){
//         // done folding don't move
//         alphaPathPoint(1) = currAlpha;
//         betaPathPoint(1) = currBeta;
//         robot->alphaPath.push_back(alphaPathPoint);
//         robot->betaPath.push_back(betaPathPoint);
//         robot->onTargetVec.push_back(true);

//         // note make collision segment just two points

//         temp(0) = stepNum;
//         temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
//         robot->roughAlphaX.push_back(temp);
//         temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
//         robot->roughAlphaY.push_back(temp);
//         temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
//         robot->roughBetaX.push_back(temp);
//         temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
//         robot->roughBetaY.push_back(temp);

//         return;
//     }

//     robot->lastStepNum = stepNum;

//     // look at all move options, rank them in terms of distance
//     // to target met fiber pos
//     Eigen::Array<double, 8, 2> alphaBetaStepChoices;
//     double minSquaredDist = 1e15; // something large to be minimized
//     alphaBetaStepChoices <<  -1,  1,
//                              0,  1,
//                              1,  1,
//                             -1,  0,
//                             1, 0,
//                             -1, -1,
//                             0, -1,
//                             1, -1;

//     for (int ii=0; ii<alphaBetaStepChoices.rows(); ii++){
//         double nextAlpha = currAlpha + alphaBetaStepChoices(ii,0)*angStep;
//         double nextBeta = currBeta + alphaBetaStepChoices(ii,1)*angStep;
//         // careful not to overshoot
//         if (currAlpha > robot->targetAlpha and nextAlpha < robot->targetAlpha){
//             nextAlpha = robot->targetAlpha;
//         }
//         if (currAlpha < robot->targetAlpha and nextAlpha > robot->targetAlpha){
//             nextAlpha = robot->targetAlpha;
//         }
//         if (currBeta > robot->targetBeta and nextBeta < robot->targetBeta){
//             nextBeta = robot->targetBeta;
//         }
//         if (currBeta < robot->targetBeta and nextBeta > robot->targetBeta){
//             nextBeta = robot->targetBeta;
//         }

//         // handle limits of travel
//         // can probably ditch this as target
//         // must be in range anyways
//         if (nextAlpha > 360){
//             nextAlpha = 360;
//         }
//         if (nextAlpha < 0){
//             nextAlpha = 0;
//         }
//         if (nextBeta > 360){
//             nextBeta = 360;
//         }
//         if (nextBeta < 0){
//             nextBeta = 0;
//         }
//         // if next choice results in no move skip it
//         // always favor a move
//         if (nextBeta==currBeta and nextAlpha==currAlpha){
//             continue;
//         }
//         robot->setAlphaBeta(nextAlpha, nextBeta);

//         if (!isCollided(robot->id)){
//             // this is a valid move see if its the best option so far
//             // calculate euclidean distance to target
//             auto dv = robot->targMetFiberPos - robot->metFiberPos;
//             auto norm = dv.squaredNorm();
//             if (norm < minSquaredDist){
//                 // this is so far the best option, record it
//                 minSquaredDist = norm;
//                 bestAlpha = nextAlpha;
//                 bestBeta = nextBeta;
//                 bestInd = ii;
//             }
//         }

//     // pick the best move (it may be a non move)
//         // best Alpha /beta were never updated
//         // if (robot->id == 1){
//         //     std::cout << "robot 1 step " << stepNum << " best alpha beta " << bestAlpha << "," << bestBeta << " bestInd " << bestInd << " norm " << minSquaredDist << std::endl;
//         // }


//     }

//     robot->setAlphaBeta(bestAlpha, bestBeta);
//     alphaPathPoint(1) = bestAlpha;
//     betaPathPoint(1) = bestBeta;
//     robot->alphaPath.push_back(alphaPathPoint);
//     robot->betaPath.push_back(betaPathPoint);
//     robot->onTargetVec.push_back(false);

//     // add alpha/beta xy points
//     // Eigen::Vector2d temp;
//     temp(0) = stepNum;
//     temp(1) = robot->betaCollisionSegment[0](0); // xAlphaEnd
//     robot->roughAlphaX.push_back(temp);
//     temp(1) = robot->betaCollisionSegment[0](1); // yAlphaEnd
//     robot->roughAlphaY.push_back(temp);
//     temp(1) = robot->betaCollisionSegment.back()(0); // xBetaEnd
//     robot->roughBetaX.push_back(temp);
//     temp(1) = robot->betaCollisionSegment.back()(1); // yBetaEnd
//     robot->roughBetaY.push_back(temp);



// void RobotGrid::pathGen2(){
//     // path gen 2 steps towards a target
//     clearPaths();
//     didFail = true;
//     int ii;
//     for (ii=0; ii<maxPathSteps; ii++){

//         bool allAtTarget = true;

//         for (auto rPair : robotDict){
//             auto r = rPair.second;
//             // std::cout << "path gen " << r.betaOrientation.size() << " " << r.betaModel.size() << std::endl;
//             // std::cout << "alpha beta " << r.alpha << " " << r.beta << std::endl;
//             stepEuclidean(r, ii);

//             if (r->beta!=r->targetBeta or r->alpha!=r->targetAlpha) {
//                 // could just check the last elemet in onTargetVec? same thing.
//                 allAtTarget = false;
//             }
//         }

//         if (allAtTarget){
//             std::cout << "all at target" << std::endl;
//             didFail = false;
//             break;
//         }
//     }

//     nSteps = ii+1;
// }

// }
