package com.theotherpancreas.pump;

import com.theotherpancreas.pump.basal.BasalModel;
import com.theotherpancreas.util.Millis;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Calendar;

import timber.log.Timber;

/**
 * Created by Mike on 5/30/2017.
 */

public class BasalFiveMinuteRxMessage {

    private final ByteBuffer byteBuffer;
    private final int expectedHash;

    public BasalFiveMinuteRxMessage(BasalModel model){

        int byteCount = 288 * 2;
        byteBuffer = ByteBuffer.allocate(byteCount).order(ByteOrder.LITTLE_ENDIAN);

        Calendar cal = Calendar.getInstance();
        applyMidnight(cal);

        float[] decodedDoses = new float[288];
        for (int i = 0; i < 288; i++) {
            float dose = model.getUnitsPerFiveMinutes(cal.getTimeInMillis() + i*Millis.FIVE_MINUTES);
            char encodedDose = BasalEncoder.encode(dose);
            byteBuffer.putChar(encodedDose);
            decodedDoses[i] = BasalEncoder.decode(encodedDose);
        }
        expectedHash = Arrays.hashCode(decodedDoses);
        byteBuffer.position(0);
    }



    public void applyMidnight(Calendar cal) {
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 0);
    }

    public byte[] getInitialMessage() {
        byte motorIndex = 0;
        return new byte[] {(byte) 0xBA, motorIndex};
    }

    public byte[] next() {
        if (!byteBuffer.hasRemaining())
            return null;

        Timber.d("this many bytes remaining: %s",  byteBuffer.remaining());

        int length = 18;
        byte[] result = new byte[length];
        byteBuffer.get(result);
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
        applyMidnight(cal);

        short minutesSinceMidnight = (short) ((currentTime - cal.getTimeInMillis()) / Millis.ONE_MINUTE);
        short howManySecondsFromNowShouldPumpDoBasal = calculateHowManySecondsFromNowPumpShouldDoBasal(currentTime, dexcomWakeTime);

        bb.putShort(minutesSinceMidnight);
        bb.putShort(howManySecondsFromNowShouldPumpDoBasal);
        bb.putInt(expectedHash);
        return bb.array();
    }

    public short calculateHowManySecondsFromNowPumpShouldDoBasal(long currentTime, long dexcomWakeTime) {
        dexcomWakeTime += Millis.THIRTY_SECONDS;
        long nowOverFive = currentTime % Millis.FIVE_MINUTES;
        long dexOverFive = dexcomWakeTime % Millis.FIVE_MINUTES;
        if (dexOverFive < nowOverFive)
            dexOverFive += Millis.FIVE_MINUTES;
        long dexDelta = dexOverFive - nowOverFive;
        return (short) (dexDelta / Millis.ONE_SECOND);
    }
}
