// .NET 8+ example of a C# layer using the same C ABI. Requires .NET SDK.
using System;
using System.Runtime.InteropServices;
static class NativeEngine {
    [DllImport("matryoshka_engine", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr mw_create(int w, int h);
    [DllImport("matryoshka_engine", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void mw_destroy(IntPtr engine);
    [DllImport("matryoshka_engine", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void mw_scene(IntPtr engine, uint frame);
    [DllImport("matryoshka_engine", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint mw_checksum(IntPtr engine);
    [DllImport("matryoshka_engine", CallingConvention = CallingConvention.Cdecl, CharSet=CharSet.Ansi)]
    [return: MarshalAs(UnmanagedType.I4)]
    internal static extern int mw_save_ppm(IntPtr engine, string path);
}
class Program {
    static int Main(string[] args) {
        IntPtr e = NativeEngine.mw_create(480, 270);
        if (e == IntPtr.Zero) return 2;
        try {
            NativeEngine.mw_scene(e, 47);
            string dest = args.Length > 0 ? args[0] : "frame_csharp.ppm";
            if (NativeEngine.mw_save_ppm(e, dest) == 0) return 3;
            Console.WriteLine($"C# host: saved {dest} checksum={NativeEngine.mw_checksum(e):x8}");
            return 0;
        } finally { NativeEngine.mw_destroy(e); }
    }
}
