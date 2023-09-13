package com.theotherpancreas.pump;

import com.theotherpancreas.util.Millis;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Created by Mike on 5/30/2017.
 */

public class ManualDoseTxMessage {

    private final ByteBuffer byteBuffer;

    /**
     * +---+-----+---+-----+---+------+----------------+-----------------------------+---+-------+----+----------------+
     * | 0 | 1   | 2 | 3   | 4 | 5    | 6              | 7                           | 8 | 9     | 10 | 11             |
     * +---+-----+---+-----+---+------+----------------+-----------------------------+---+-------+----+----------------+
     * | dose[0] | dose[1] | dose[2]  | verificationId |      bitwiseFlags           | tempBasal | tempDurationMinutes |
     * +---------+---------+----------+----------------+-----------------------------+-----------+---------------------+
     * |(insulin)|(symlin) |(glucogon)|                | 0b00000001-updateTempBasal  |           |                     |
     * |         |         |          |                | 0b00000010-basalIndex bit1  |           |                     |
     * |         |         |          |                | 0b00000100-basalIndex bit2  |           |                     |
     * |         |         |          |                | 0b00001000-tempBasalActive  |           |                     |
     * |         |         |          |                | 0b00010000-tempBasalPercent |           |                     |
     * |         |         |          |                | 0b00100000                  |           |                     |
     * |         |         |          |                | 0b01000000                  |           |                     |
     * |         |         |          |                | 0b10000000-unsafe           |           |                     |
     * +---------+---------+----------+----------------+-----------------------------+-----------+---------------------+
     */
    public ManualDoseTxMessage(float insulinDose, float symlinDose, float glucagonDose, byte verificationId, boolean unsafe, int basalIndex, boolean tempBasalActive, boolean tempBasalIsPercent, float tempBasal, long tempDurationMillis) {
        byteBuffer = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN);

        byteBuffer.putChar(DoseEncoder.encode(insulinDose));
        byteBuffer.putChar(DoseEncoder.encode(symlinDose));
        byteBuffer.putChar(DoseEncoder.encode(glucagonDose));
        byteBuffer.put(verificationId);

        int flags = basalIndex & 0b00000011;
        if (tempBasalActive)
            flags |= 0b00000100;
        if (tempBasalIsPercent)
            flags |= 0b00001000;
        if (unsafe)
            flags |= 0b10000000;
        byteBuffer.put((byte) flags);

        byteBuffer.putChar(DoseEncoder.encode(tempBasal));
        byteBuffer.putChar((char) (tempDurationMillis / Millis.ONE_MINUTE));
    }

    public byte[] getPayload()
    {
        return byteBuffer.array();
    }
}
