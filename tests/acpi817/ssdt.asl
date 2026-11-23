DefinitionBlock ("", "SSDT", 2, "STYL", "EXTR817", 0x00000001)
{
    Scope (\_SB.PCI0)
    {
        Method (S817, 0, NotSerialized) { Return (0x05A5) }
    }
}
