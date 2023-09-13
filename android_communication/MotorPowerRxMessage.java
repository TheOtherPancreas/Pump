package com.theotherpancreas.pump;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by Mike on 5/30/2017.
 */

public class MotorPowerRxMessage {

    private final int motorIndex;
    private final float powerLevel;

    /**
     * @param motorIndex 0 for insulin, 1 for symlin, 2 for glucagon
     * @param powerLevel value between 0 and 100
     */
    public MotorPowerRxMessage(int motorIndex, float powerLevel) {

        this.motorIndex = motorIndex;
        this.powerLevel = powerLevel;
    }

    /*
     * +------------+------------+
     * | 0          | 1          |
     * +------------+------------+
     * | motorIndex | powerLevel |
     * +------------+------------+
     */
    public byte[] getPayload() {
        ByteBuffer bb = ByteBuffer.allocate(3).order(ByteOrder.LITTLE_ENDIAN);

        bb.put((byte) motorIndex);
        bb.put((byte) powerLevel);

        return bb.array();
    }

}
