#!/usr/bin/env python3
"""Bitwise hardware design checks for M8 arithmetic logic unit.
The adders are constructed from 1-bit Boolean gate primitives.
No multi-bit Python arithmetic is used to compute the ALU result.
"""

def gate_and(a,b):return (a&b)&1
def gate_or(a,b):return (a|b)&1
def gate_xor(a,b):return (a^b)&1
def gate_not(a):return 1-(a&1)

def add8(a,b,carry_in=0):
    result=0
    carry=carry_in
    for i in range(8):
        x=(a>>i)&1
        y=(b>>i)&1
        p=gate_xor(x,y)
        s=gate_xor(p,carry)
        carry=gate_or(gate_and(x,y),gate_and(p,carry))
        result|=s<<i
    return result,carry

def subtract8(a,b):
    result,carry=add8(a,b^255,1)
    return result,1-carry # borrow flag

def eval_alu(op,a,b):
    if op=='ADD':
        result,carry=add8(a,b)
    elif op=='SUB':
        result,carry=subtract8(a,b)
    elif op=='AND':
        result=0
        for i in range(8):result|=gate_and((a>>i)&1,(b>>i)&1)<<i
        carry=0
    elif op=='OR':
        result=0
        for i in range(8):result|=gate_or((a>>i)&1,(b>>i)&1)<<i
        carry=0
    elif op=='XOR':
        result=0
        for i in range(8):result|=gate_xor((a>>i)&1,(b>>i)&1)<<i
        carry=0
    else:raise ValueError(op)
    return result,carry,int(result==0)

def test():
    count=0
    for a in range(256):
        for b in range(256):
            expected={'ADD':((a+b)&255,int(a+b>255)),
                      'SUB':((a-b)&255,int(a<b)),
                      'AND':(a&b,0),'OR':(a|b,0),'XOR':(a^b,0)}
            for name,(res,carry) in expected.items():
                got=eval_alu(name,a,b)
                assert got==(res,carry,int(res==0)),(name,a,b,got)
                count+=1
    print('GATE ALU PASS:',count,'input/opcode cases, 8-bit ripple-carry + flags')

if __name__=='__main__':test()
