import unittest
from uart_receipts import complete_lines
class CompleteReceipts(unittest.TestCase):
    def test_every_prefix(self):
        before=b'STYLEOS DESKTOP DOCUMENT SAVE\r\n'
        event=b'STYLEOS S863 EVENT root=0x1 n=0x4 active=0x4 text=0x1 text-dirty=0x0 writes=0x1\r\n'
        for n in range(len(event)):
            self.assertEqual(complete_lines(before+event[:n]),[before.rstrip(b'\r\n')])
        self.assertEqual(complete_lines(before+event),[before.rstrip(b'\r\n'),event.rstrip(b'\r\n')])
    def test_complete_malformed_not_hidden(self):
        self.assertEqual(complete_lines(b'STYLEOS S863 EVENT broken\n'),[b'STYLEOS S863 EVENT broken'])
        self.assertEqual(complete_lines(b'partial'),[])
        self.assertEqual(complete_lines(b''),[])
if __name__=='__main__':
    unittest.main()
