package com.theotherpancreas.pump;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/*
 * Opcode 17
 * +------------------+---+----------------------+---+----------------------+---------------------------+---+---+---+----+----+
 * | 0                | 1 | 2                    | 3 | 4                    | 5 | 6                     | 7 | 8 | 9 | 10 | 11 |
 * +------------------+---+----------------------+---+----------------------+---------------------------+---+---+---+----+----+
 * | verification_id  | unreported_basal_insulin | unreported_basal_symlin  | unreported_basal_glucagon |
 * +------------------+--------------------------+--------------------------+---------------------------+
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * |                  |                          |                          |                           |
 * +------------------+--------------------------+--------------------------+---------------------------+
 */

public class PumpStateRxMessage {
    private final int opcode = 17;
    private int verificationId;
    private float unreportedBasalInsulin;
    private float unreportedBasalSymlin;
    private float unreportedBasalGlucagon;


    public PumpStateRxMessage(byte[] bytes){

        ByteBuffer byteBuffer = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);

        verificationId = byteBuffer.get();
        unreportedBasalInsulin = DoseEncoder.decode(byteBuffer.getChar());
        unreportedBasalSymlin = DoseEncoder.decode(byteBuffer.getChar());
        unreportedBasalGlucagon = DoseEncoder.decode(byteBuffer.getChar());
    }

    public int getVerificationId() {
        return verificationId;
    }

    public float getUnreportedBasalGlucagon() {
        return unreportedBasalGlucagon;
    }

    public float getUnreportedBasalInsulin() {
        return unreportedBasalInsulin;
    }

    public float getUnreportedBasalSymlin() {
        return unreportedBasalSymlin;
    }

    public boolean hasUnreporetedBasal() {
        return unreportedBasalInsulin > 0 || unreportedBasalSymlin > 0 || unreportedBasalGlucagon > 0;
    }

    @Override
    public String toString() {
        return "PumpStateRxMessage{" +
                "opcode=" + opcode +
                ", verificationId=" + verificationId +
                ", unreportedBasalInsulin=" + unreportedBasalInsulin +
                ", unreportedBasalSymlin=" + unreportedBasalSymlin +
                ", unreportedBasalGlucagon=" + unreportedBasalGlucagon +
                '}';
    }
}
