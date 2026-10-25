package com.gamemod;

public class NativeLib {
    static {
        System.loadLibrary("gamemodifier");
    }

    public native int findGameProcess();
    public native int[] readValue();
    public native boolean writeValue(boolean enableAntiBan);
    public native long getModuleBase();
    public native String getTargetPackage();
}
