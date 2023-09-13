package com.theotherpancreas.pump;

public class SelfTestResult {
    boolean success;
    float dose;
    float injection;
    boolean occlusion;
    boolean encoderError;
    boolean variation;

    public SelfTestResult(boolean success, float dose, float injection, boolean occlusion, boolean encoderError, boolean variation) {
        this.success = success;
        this.dose = dose;
        this.injection = injection;
        this.occlusion = occlusion;
        this.encoderError = encoderError;
        this.variation = variation;
    }

    public boolean isSuccess() {
        return success;
    }

    public void setSuccess(boolean success) {
        this.success = success;
    }

    public float getDose() {
        return dose;
    }

    public void setDose(float dose) {
        this.dose = dose;
    }

    public float getInjection() {
        return injection;
    }

    public void setInjection(float injection) {
        this.injection = injection;
    }

    public boolean isOcclusion() {
        return occlusion;
    }

    public void setOcclusion(boolean occlusion) {
        this.occlusion = occlusion;
    }

    public boolean isEncoderError() {
        return encoderError;
    }

    public void setEncoderError(boolean encoderError) {
        this.encoderError = encoderError;
    }

    public boolean isVariation() {
        return variation;
    }

    public void setVariation(boolean variation) {
        this.variation = variation;
    }

    @Override
    public String toString() {
        return "SelfTestResult{" +
                "success=" + success +
                ", dose=" + dose +
                ", injection=" + injection +
                ", occlusion=" + occlusion +
                ", encoderError=" + encoderError +
                ", variation=" + variation +
                '}';
    }
}
