DefinitionBlock ("", "DSDT", 2, "STYL", "HOST817", 0x00000001)
{
    Scope (\_SB)
    {
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A08"))
            Name (_SEG, Zero)
            Method (_BBN, 0, NotSerialized) { Return (Zero) }
            Method (_CRS, 0, NotSerialized)
            {
                Return (ResourceTemplate()
                {
                    WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                        0x0000, 0x0000, 0x00FF, 0x0000, 0x0100)
                })
            }
            Method (X817, 0, NotSerialized) { Return (0x0817) }
        }
    }
}
