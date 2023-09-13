package com.theotherpancreas.pump;

/**
 * Due to the limited number of bytes we can transmit over bluetooth, as well as the limited number
 * of bytes we can store in flash on the RFDuino, we encode doses and injections in a single byte.
 * We designed this encoding to be simple, yet super precise and accurate at low doses, but less
 * accurate at higher doses where differences are less significant.
 *
 * To illustrate:
 * The first 13 values all represent a dose less than 0.1units
 * The first 40 values all represent a dose less than 1unit
 * The next 17 values represent doses from 1.0000 - 1.9600
 * The next 13 values represent doses from 2.0306 - 2.9756
 * The next 10 values represent doses from 3.0625 - 3.9006
 * ~~~
 * The final 15 values represent doses from 36.0000 - 40.6406
 * The final 3 values represent doses from 40.0056 - 40.6406
 *
 * This encoding is a lossy encoding which introduces "error" the following are max errors at each level
 *
 * < 0.1 max error is 0.007u
 * < 1.0 max error is 0.025u
 * < 2.0 max error is 0.035u
 * < 3.0 max error is 0.043u
 * < 4.0 max error is 0.049u
 * < 5.0 max error is 0.056u
 * < 10 max error is 0.079u
 * < 20 max error is 0.111u
 * < 30 max error is 0.137u
 * < Max Dose (40.64) max error is 0.159u
 *
 * Most inaccurate cases:
 * Someone dosing 40.48u would actually get 40.32u which is 0.16u less than dosed
 * Someone dosing 40.49u would actually get 40.64u which is 0.15u more than dosed
 *
 */
public class DoseCompressionEncoder {

    private static int encode(float amount) {
        double result = Math.sqrt(amount * 1600);
        if (result > 255)
            return 255;

        int floor = (int)Math.floor(result);
        int ceil = (int)Math.ceil(result);

        float floorDiff = Math.abs(decode(floor) - amount);
        float ceilDiff = Math.abs(decode(ceil) - amount);
        return floorDiff < ceilDiff ? floor : ceil;
    }

    private static float decode(int val) {
        return (val * val)/1600f;
    }
}
