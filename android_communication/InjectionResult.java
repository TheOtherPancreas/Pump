package com.theotherpancreas.pump;

/**
 * Created by Mike on 8/22/2016.
 */
public class InjectionResult {
    public final float insulinInjected;
    public final float symlinInjected;
    public final float glucagonInjected;
    public final boolean insulinOcclusion;
    public final boolean symlinOcclusion;
    public final boolean glucagonOcclusion;
    public boolean insulinEncoderError;
    public boolean symlinEncoderError;
    public boolean glucagonEncoderError;
    private final boolean unreportedBasal;
    private final float batteryLevel;


    public InjectionResult(float insulinInjected, float symlinInjected, float glucagonInjected, boolean insulinOcclusion, boolean symlinOcclusion, boolean glucagonOcclusion, boolean insulinEncoderError, boolean symlinEncoderError, boolean glucagonEncoderError, boolean unreportedBasal, float batteryLevel) {
        this.insulinInjected = insulinInjected;
        this.symlinInjected = symlinInjected;
        this.glucagonInjected = glucagonInjected;
        this.insulinOcclusion = insulinOcclusion;
        this.symlinOcclusion = symlinOcclusion;
        this.glucagonOcclusion = glucagonOcclusion;
        this.insulinEncoderError = insulinEncoderError;
        this.symlinEncoderError = symlinEncoderError;
        this.glucagonEncoderError = glucagonEncoderError;
        this.unreportedBasal = unreportedBasal;
        this.batteryLevel = batteryLevel;
    }

    public boolean hasOcclusion() {
        return insulinOcclusion || symlinOcclusion || glucagonOcclusion;
    }

    public boolean hasEncoderError() {
        return insulinEncoderError || symlinEncoderError;
    }

    @Override
    public String toString() {
        return "InjectionResult{" +
                "insulinInjected=" + insulinInjected +
                ", symlinInjected=" + symlinInjected +
                ", glucagonInjected=" + glucagonInjected +
                ", insulinOcclusion=" + insulinOcclusion +
                ", symlinOcclusion=" + symlinOcclusion +
                ", glucagonOcclusion=" + glucagonOcclusion +
                ", unreportedBasal=" + unreportedBasal +
                ", batteryLevel=" + batteryLevel +
                '}';
    }
}
