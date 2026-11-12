public final class MatryoshkaEngine {
    static { System.loadLibrary("matryoshka_jni"); }
    private static native long create(int width, int height);
    private static native void scene(long engine, int frame);
    private static native int checksum(long engine);
    private static native boolean save(long engine, String path);
    private static native void destroy(long engine);
    public static void main(String[] args) {
        long handle = create(480, 270);
        if (handle == 0) throw new IllegalStateException("Cannot create framebuffer");
        try {
            scene(handle, 47);
            String output = args.length > 0 ? args[0] : "build/frame_java.ppm";
            if (!save(handle, output)) throw new IllegalStateException("Cannot save " + output);
            System.out.printf("Java host: saved %s checksum=%08x%n", output, checksum(handle));
        } finally { destroy(handle); }
    }
}
