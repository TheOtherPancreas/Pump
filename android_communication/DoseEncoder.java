package com.theotherpancreas.pump;

/**
 * Due to the limited number of bytes we can transmit over bluetooth, as well as the limited number
 * of bytes we can store in flash on the RFDuino, we encode doses and injections in 2 bytes.
 * This encoding allows for 0.005u accuracy, and up to 327.67 units
 */
public class DoseEncoder {

    public static char encode(float amount) {
        if (amount * 200 > Character.MAX_VALUE)
            return Character.MAX_VALUE;
        if (amount < 0)
            return 0;
        return (char)(amount * 200);
    }

    public static float decode(char val) {
        return val / 200f;
    }
}
