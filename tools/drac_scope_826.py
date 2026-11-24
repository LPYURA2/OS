#!/usr/bin/env python3
"""Narrow static scope check of the exact QEMU AML fixture; NOT a namespace evaluator."""
import argparse, hashlib, json, struct
from pathlib import Path

def need(ok, message):
    if not ok: raise ValueError(message)

def pkg(raw, at, limit):
    need(at < limit <= len(raw), 'missing PkgLength')
    lead=raw[at]; n=lead>>6
    need(at+1+n<=limit and (not n or not lead&0x30), 'malformed PkgLength')
    length=lead & (15 if n else 63)
    for i in range(n): length |= raw[at+1+i] << (4+8*i)
    body=at+1+n;end=at+length
    need(body<=end<=limit, 'package out of parent bounds')
    return body,end

def named(raw, at, limit, opcode, name):
    need(raw[at:at+len(opcode)]==opcode, 'unexpected opcode')
    start,end=pkg(raw,at+len(opcode),limit)
    need(start+len(name)<=end and raw[start:start+len(name)]==name,'unexpected name')
    return start+len(name),end

def scope(raw):
    need(len(raw)>=36 and raw[:4]==b'DSDT' and int.from_bytes(raw[4:8],'little')==len(raw) and sum(raw)%256==0, 'DSDT header/checksum')
    _,root_end=named(raw,36,len(raw),b'\x10',b'\\\x00')
    sb_body,sb_end=named(raw,root_end,len(raw),b'\x10',b'_SB_')
    _,pci_end=named(raw,sb_body,sb_end,b'\x5b\x82',b'PCI0')
    drac_body,drac_end=named(raw,pci_end,sb_end,b'\x5b\x82',b'DRAC')
    need(drac_end==sb_end,'unexpected trailing sibling data')
    prefix=b'\x08_HID\x0dPNP0C01\x00\x08_CRS\x11'
    need(raw.startswith(prefix,drac_body),'DRAC unexpected objects')
    buf_start,buf_end=pkg(raw,drac_body+len(prefix),drac_end)
    need(buf_end==drac_end and raw[buf_start:buf_start+2]==b'\x0a\x1c','buffer shape')
    data=raw[buf_start+2:buf_end]
    need(len(data)==28 and data[:6]==bytes.fromhex('871700000c01') and data[-2:]==b'\x79\x00','resource descriptor')
    gran,lo,hi,trans,length=struct.unpack('<IIIII',data[6:26]);need(gran==trans==0 and length>0 and lo+length==hi+1,'resource extent')
    return {'candidate_path':r'\_SB.DRAC','pci0_end':pci_end,'drac_start':pci_end,'sb_end':sb_end,'drac_end':drac_end,'hid':'PNP0C01','interval':[lo,hi+1],'resource_hex':data.hex(),'dsdt_sha256':hashlib.sha256(raw).hexdigest(),'static_only':True}

def reseal(b):
    b=bytearray(b);b[9]=0;b[9]=(-sum(b))%256;return bytes(b)

def negatives(raw):
    s=scope(raw); at=s['drac_start']; n=0
    for idx,v in [(36,0x11),(37,0x3f),(110,0x11),(111,0x10),(113,ord('X')),(117,0x10),(119,0x48),(at,0x5c),(at+2,0x3f),(at+3,ord('X')),(at+10,ord('X'))]:
        b=bytearray(raw);b[idx]=v
        try:scope(reseal(b))
        except ValueError:n+=1
        else:raise AssertionError('accepted mutation '+str(idx))
    return n

def main():
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    modes=('disabled','intel','bridge-intel','bridge-two-hop-intel','amd'); results={};n=0
    for mode in modes:
        raw=(a.input/mode/'dsdt.aml').read_bytes();r=scope(raw);need(r['interval']==[0xe0000000,0xf0000000],'fixture range mismatch');results[mode]=r
        if mode=='disabled':n=negatives(raw)
        print(mode,r['candidate_path'],'PCI0 end',r['pci0_end'],'DRAC end',r['drac_end'],'PASS')
    a.output.write_text(json.dumps({'modes':results,'negative_pass':n,'namespace_evaluated':False,'ecam_reservation_verified':False},indent=2)+'\n');print(f'{n}/{n} synthetic scope refusals PASS; static candidate only')
if __name__=='__main__':main()
