package com.theotherpancreas.pump;

import com.theotherpancreas.pump.basal.BasalEntry;
import com.theotherpancreas.pump.basal.BasalModel;
import com.theotherpancreas.util.Millis;
import com.theotherpancreas.util.Tools;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Calendar;
import java.util.LinkedList;

import timber.log.Timber;

import static com.theotherpancreas.util.Millis.FIVE_MINUTES;

/**
 * Created by Mike on 5/30/2017.
 */

public class BasalInterpolationRxMessage {

    private final ByteBuffer byteBuffer;
    private final int expectedHash;

    public BasalInterpolationRxMessage(BasalModel model){

        LinkedList<BasalEntry> entries = model.getEntries();
        int byteCount = entries.size() * 4;

        byteBuffer = ByteBuffer.allocate(byteCount).order(ByteOrder.LITTLE_ENDIAN);
        char[] encodedDoses = new char[288];

        for (int i = 0; i < 288; i++) {
            encodedDoses[i] = 0;
        }

        for (BasalEntry entry : entries) {
            int index = (int) (entry.getMillisAfterMidnight() / FIVE_MINUTES);
            encodedDoses[index] = BasalEncoder.encode(entry.getUnitsPerFiveMinutes());
            byteBuffer.putShort((short)index);
            byteBuffer.putChar(BasalEncoder.encode(entry.getUnitsPerFiveMinutes()));
        }

        expectedHash = Arrays.hashCode(encodedDoses);
        byteBuffer.position(0);
    }

    public byte[] getInitialMessage() {
        byte motorIndex = 0;
        return new byte[] {(byte) 0xBA, motorIndex};
    }

    /*
     * +-----------------------++---+-------------+-----------++---+-------------+-----------++---+-------------+----+------++----+------------+----+------+
     * | 0                     || 1 | 2           | 3 | 4     || 5 | 6           | 7 | 8     || 9 | 10          | 11 | 12   || 13 | 14         | 15 | 16   |
     * +-----------------------++---+-------------+-----------++---+-------------+-----------++---+-------------+----+------++----+------------+----+------+
     * | entriesInTransmission || fiveMinuteIndex | basalDose || fiveMinuteIndex | basalDose || fiveMinuteIndex | basalDose || fiveMinuteIndex | basalDose |
     * +-----------------------++-----------------+-----------++-----------------+-----------++-----------------+-----------++-----------------+-----------+
     */
    public byte[] next() {
        if (!byteBuffer.hasRemaining())
            return null;
        int remaining = byteBuffer.remaining();
        Timber.d("this many bytes remaining: %s",  remaining);

        int length = Math.min(16, remaining);
        byte[] result = new byte[length + 1];
        result[0] = (byte) (length/4);
        byteBuffer.get(result, 1, length);
        return result;
    }


    /*
     * +---+------------------+---+------------------------------------+---+---+---+---+
     * | 0 | 1                | 2 | 3                                  | 4 | 5 | 6 | 7 |
     * +---+------------------+---+------------------------------------+---+---+---+---+
     * | minutesSinceMidnight | howManySecondsFromNowShouldPumpDoBasal | expectedHash  |
     * +----------------------+----------------------------------------+---------------+
     */
    public byte[] getFinalMessage(long currentTime, long dexcomWakeTime) {
        ByteBuffer bb = ByteBuffer.allocate(9).order(ByteOrder.LITTLE_ENDIAN);
        byte opcode = (byte) 0xBE;
        bb.put(opcode);

        Calendar cal = Calendar.getInstance();
        cal.setTimeInMillis(currentTime);
        Tools.applyMidnight(cal);

        short minutesSinceMidnight = (short) ((currentTime - cal.getTimeInMillis()) / Millis.ONE_MINUTE);
        short howManySecondsFromNowShouldPumpDoBasal = calculateHowManySecondsFromNowPumpShouldDoBasal(currentTime, dexcomWakeTime);

        bb.putShort(minutesSinceMidnight);
        bb.putShort(howManySecondsFromNowShouldPumpDoBasal);
        bb.putInt(expectedHash);
        return bb.array();
    }

    public short calculateHowManySecondsFromNowPumpShouldDoBasal(long currentTime, long dexcomWakeTime) {
        dexcomWakeTime += Millis.THIRTY_SECONDS;
        long nowOverFive = currentTime % FIVE_MINUTES;
        long dexOverFive = dexcomWakeTime % FIVE_MINUTES;
        if (dexOverFive < nowOverFive)
            dexOverFive += FIVE_MINUTES;
        long dexDelta = dexOverFive - nowOverFive;
        return (short) (dexDelta / Millis.ONE_SECOND);
    }
}
