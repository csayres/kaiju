import pytest
import matplotlib.pyplot as plt
import numpy
from kaiju import RobotGrid
from kaiju import utils
import time
import json
import pickle

nDia = 15
angStep = 3
collisionBuffer = 3
epsilon = angStep * 2
hasApogee = True


def test_forwardGreedy(plot=False):
    xPos, yPos = utils.hexFromDia(35, pitch=22.4)
    seed = 1
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=seed
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10

    rg.decollideGrid()

    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(robot.alpha, robot.beta)
        robot.setAlphaBeta(0, 180)
    assert rg.getNCollisions() == 0
    rg.pathGenGreedy()
    if plot:
        utils.plotPaths(rg, filename="forwardGreedy.mp4")

def test_reverseGreedy(plot=False):
    xPos, yPos = utils.hexFromDia(35, pitch=22.4)
    seed = 1
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=seed
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10

    rg.decollideGrid()

    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(0, 180)
    assert rg.getNCollisions() == 0
    rg.pathGenGreedy()
    if plot:
        utils.plotPaths(rg, filename="reverseGreedy.mp4")

def test_forwardMDP(plot=False):
    xPos, yPos = utils.hexFromDia(35, pitch=22.4)
    seed = 1
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=seed
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10

    rg.decollideGrid()

    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(robot.alpha, robot.beta)
        robot.setAlphaBeta(0, 180)
    assert rg.getNCollisions() == 0
    rg.pathGenMDP()
    if plot:
        utils.plotPaths(rg, filename="forwardMDP.mp4")

def test_reverseMDP(plot=False):
    greed = 1
    phobia = 0
    angStep = 1
    downsample = int(numpy.floor(3 / angStep))
    xPos, yPos = utils.hexFromDia(35, pitch=22.4)
    seed = 1
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=seed
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10

    rg.decollideGrid()

    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(0, 180)
    assert rg.getNCollisions() == 0
    rg.pathGenMDP(greed, phobia)
    if plot:
        utils.plotPaths(rg, downsample=downsample, filename="reverseMDP.mp4")

def test_setMDP(plot=False):

    greed = 0.8
    phobia = 0.2
    xPos, yPos = utils.hexFromDia(45, pitch=22.4)
    print("using ", len(xPos), "robots")
    # collisionBuffer = 3
    angStep = 0.5
    collisionBuffer = 3
    downsample = int(numpy.floor(3 / angStep))
    for seed in range(100):
        rg = RobotGrid(
            stepSize=angStep, collisionBuffer=collisionBuffer,
            epsilon=epsilon, seed=seed
        )

        for robotID, (x, y) in enumerate(zip(xPos, yPos)):
            rg.addRobot(robotID, x, y, hasApogee)
        rg.initGrid()
        for rID in rg.robotDict:
            robot = rg.getRobot(rID)
            robot.setXYUniform()
        assert rg.getNCollisions() > 10

        rg.decollideGrid()

        for robot in rg.robotDict.values():
            robot.setDestinationAlphaBeta(90, 180)
        assert rg.getNCollisions() == 0
        rg.pathGenMDP(greed, phobia)

        deadlockedRobots = []
        for r in rg.robotDict.values():
            if not r.onTargetVec[-1]:
                deadlockedRobots.append(r.id)
        if len(deadlockedRobots):
            print("seed", seed, "failed with these", deadlockedRobots, "in ", rg.nSteps)
            break
        else:
            print("seed", seed, "didn't fail", rg.nSteps, " taken to solve")

    if plot:
        utils.plotPaths(rg, downsample=downsample, filename="reverseSetMDP.mp4")

def test_initialConfigs(plot=True):

    xPos, yPos = utils.hexFromDia(21, pitch=22.4)
    angStep = 1
    greed = 1
    phobia = 0
    downsample = int(numpy.floor(3 / angStep))
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=1
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10
    if plot:
        utils.plotOne(-1, rg, figname="angStepO.png", isSequence=False)

    rg.decollideGrid()
    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(0, 180)
    if plot:
        utils.plotOne(-1, rg, figname="angStepD.png", isSequence=False)
    rg.pathGenMDP(greed, phobia)
    if plot:
        utils.plotOne(-1, rg, figname="angStepE.png", isSequence=False)
    if plot:
        utils.plotPaths(rg, downsample=downsample, filename="init.mp4")

def test_tofile(plot=False):
    xPos, yPos = utils.hexFromDia(37, pitch=22.4, rotAngle=90)
    print("n robots", len(xPos))
    angStep = 1
    greed = 0.8
    phobia = 0.2
    downsample = int(numpy.floor(3 / angStep))
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=1
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
    assert rg.getNCollisions() > 10

    rg.decollideGrid()
    for robot in rg.robotDict.values():
        robot.setDestinationAlphaBeta(10, 170)
    rg.pathGenMDP(greed, phobia)
    # rg.smoothPaths(3)
    # rg.simplifyPaths()
    # rg.verifySmoothed()
    # t1 = time.time()
    # rg.summaryJSON("json.txt")
    # print("json took", (time.time()-t1))

    # t1 = time.time()
    # rg.summaryPickle("rg.pkl")
    # print("pickle took", (time.time()-t1))

    # t1 = time.time()
    # g = json.load(open("json.txt", "r"))
    # print("json load took", (time.time()-t1))

    # t1 = time.time()
    # g = pickle.load(open("rg.pkl", "rb"))
    # print("pickle load took", (time.time()-t1))
    if plot:
        utils.plotOne(-1, rg, figname="tofile.png", isSequence=False)
    # if plot:
    #     utils.plotPaths(rg, downsample=downsample, filename="tofile.mp4")


def test_fatty(plot=False):
    xPos, yPos = utils.hexFromDia(11, pitch=22.4)
    print("n robots", len(xPos))
    angStep = 0.1
    greed = 0.8
    phobia = 0.2
    collisionBuffer = 4
    downsample = int(numpy.floor(3 / angStep))
    rg = RobotGrid(
        stepSize=angStep, collisionBuffer=collisionBuffer,
        epsilon=epsilon, seed=1
    )

    for robotID, (x, y) in enumerate(zip(xPos, yPos)):
        rg.addRobot(robotID, x, y, hasApogee)
    rg.initGrid()
    for rID in rg.robotDict:
        robot = rg.getRobot(rID)
        robot.setXYUniform()
        robot.setDestinationAlphaBeta(90,180)
    # assert rg.getNCollisions() > 10

    rg.decollideGrid()

    rg.pathGenMDP(greed, phobia)

    # rg.smoothPaths(3)
    # rg.simplifyPaths()
    # rg.verifySmoothed()
    if plot:
        utils.plotPaths(rg, downsample=downsample, filename="fatty.mp4")

if __name__ == "__main__":
    # test_forwardGreedy(plot=True)
    # test_reverseGreedy(plot=True)
    # test_forwardMDP(plot=True)
    # test_reverseMDP(plot=True)
    # test_initialConfigs(plot=True)

    # test_setMDP(plot=True)
    test_tofile(plot=True)
    # test_forwardPathGen(plot=True)
    # test_reversePathGen(plot=True)
    # test_reversePathGen2(plot=True)
    # test_reversePathGen3(plot=True)
    # test_forwardPathGen2(plot=True)

