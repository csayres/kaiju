import pytest

from kaiju.robotGrid import RobotGrid
from kaiju import utils

import coordio


def test_uniqueFiducial():
    angStep = 1
    collisionBuffer = 2
    epsilon = angStep * 2
    hasApogee = True
    fiducialID = 1
    seed = 0
    rg = RobotGrid(angStep, collisionBuffer, epsilon, seed)
    rg.addFiducial(fiducialID, [0, 0, 0])
    with pytest.raises(RuntimeError) as excinfo:
        rg.addFiducial(fiducialID, [30, 0, 0])
    assert "Fiducial ID already exists" in str(excinfo.value)

def test_fiducial(plot=False):
    angStep = 1
    collisionBuffer = 2
    fiducialCollisionBuffer = 1.5
    epsilon = angStep * 2
    hasApogee = True
    robotID = 1
    fiducialID = 10
    seed = 0
    rg = RobotGrid(angStep, collisionBuffer, epsilon, seed)
    rg.addRobot(robotID, str(robotID), [0, 0, 0], hasApogee)
    rg.addFiducial(fiducialID, [22.4, 0, coordio.defaults.POSITIONER_HEIGHT], fiducialCollisionBuffer)
    rg.initGrid()
    robot = rg.getRobot(robotID)
    for betaAng in range(20):
        robot.setAlphaBeta(90,betaAng)
        rColliders = rg.robotColliders(robotID)
        fColliders = rg.fiducialColliders(robotID)
        if plot:
            utils.plotOne(0, rg, figname="fiducial_%i.png"%betaAng, isSequence=False, xlim=[-30, 30], ylim=[-30, 30])

        # assert len(rColliders) == 0
        # if betaAng < 14:
        #     assert fColliders == [fiducialID]
        # else:
        #     assert len(fColliders) == 0


if __name__ == "__main__":
    test_fiducial(plot=True)