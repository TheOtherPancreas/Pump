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

public class BasalConfirmationTxMessage {
    private final int opcode = 19;
    private final byte[] bytes;


    public BasalConfirmationTxMessage(float confirmedBasalInsulin, float confirmedBasalSymlin, float confirmedBasalGlucagon){

        ByteBuffer byteBuffer = ByteBuffer.allocate(6).order(ByteOrder.LITTLE_ENDIAN);

        byteBuffer.putChar(DoseEncoder.encode(confirmedBasalInsulin));
        byteBuffer.putChar(DoseEncoder.encode(confirmedBasalSymlin));
        byteBuffer.putChar(DoseEncoder.encode(confirmedBasalGlucagon));
        bytes = byteBuffer.array();
    }

    public byte[] getBytes() {
        return bytes;
    }
}
