# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/usr/bin/python3

import xlsxwriter as xlsx
from BitVector import BitVector


READ_SIZE = 8 # assume that we read 8 bytes out of a bank at a time
MEM_COLS = 4 # columns per bank
MEM_CHANNELS = 4
MEM_BANKS = 8
MEM_ROWS = 8 # rows per bank

COL_BITS_LOW = (0, 1) 
CH_BITS = (1, 3)
BANK_BITS = (3, 6)
COL_BITS_HIGH = (6, 7)
ROW_BITS = (7, 10)

ADDR_SIZE = 10 # 10 bits of address, based on ROW_BITS[1]

arraySize = 512;

class Bank:
    def __init__(self):
        self.data = [ [ 'x' for x in range(0, MEM_COLS) ] for x in range(0, MEM_ROWS)]
        self.rows = MEM_ROWS
        self.cols = MEM_COLS

    def print(self):
        for i in self.data:
            for j in i:
                print(f'{j}, ')
            print('\n')


class Channel:
    def __init__(self):
        self.banks = [ Bank() for x in range(0, MEM_BANKS) ]


class MemModule:
    def __init__(self):
        self.channels = [ Channel() for x in range(0, MEM_CHANNELS) ]
        self.size = READ_SIZE * MEM_CHANNELS * MEM_BANKS * MEM_ROWS * MEM_COLS



b = Bank()
# b.print()

c = Channel()
# print(c.banks)

mem = MemModule()

def getId(addr, bitRange):
    # bit vector indexes MSB as bit 0 and LSB as bit N-1 where N is the lenght.
    # What a shame
    return int(addr[ADDR_SIZE - bitRange[1]: ADDR_SIZE - bitRange[0]])

def getIdSplitRange(addr, lowBits, highBits):
    segLow = addr[ADDR_SIZE-lowBits[1]: ADDR_SIZE-lowBits[0]]
    segHigh = addr[ADDR_SIZE-highBits[1]: ADDR_SIZE-highBits[0]]
    return int(segHigh + segLow)

# fill in the memory with the array size
# arraySize = 64 # TODO: remove
for index in range(0, arraySize):
    addr = BitVector(intVal=index, size=ADDR_SIZE)
    chId = getId(addr, CH_BITS)
    bankId = getId(addr, BANK_BITS)
    rowId = getId(addr, ROW_BITS)
    colId = getIdSplitRange(addr, COL_BITS_LOW, COL_BITS_HIGH)

    print(f'addr={addr} index={index} chId={chId} bankId={bankId} rowId={rowId} colId={colId}')

    wordBeg = index * READ_SIZE;
    wordEnd = (index+1) * READ_SIZE;

    dataWord = f'{wordBeg}:{wordEnd}'

    mem.channels[chId].banks[bankId].data[rowId][colId] = dataWord

# layout the memory in excel sheet

workbook = xlsx.Workbook('exampleLayout.xlsx')
worksheet = workbook.add_worksheet('layout')

ws = worksheet

def printBitsHelper(prefix, bitRange, begX, begY, offset=0):
    x = begX
    y = begY
    for i in range(bitRange[1]- bitRange[0]-1, 0-1, -1): 
        ws.write(x, y, f'{prefix}{i+offset}')
        y += 1
    return y


def printAddrBits(begX, begY):
    x = begX
    y = begY
    y = printBitsHelper('ROW', ROW_BITS, x, y)
    colBitsLowLen = COL_BITS_LOW[1] - COL_BITS_LOW[0]
    y = printBitsHelper('COL', COL_BITS_HIGH, x, y, colBitsLowLen)
    y = printBitsHelper('BANK', BANK_BITS, x, y)
    y = printBitsHelper('CH', CH_BITS, x, y)
    y = printBitsHelper('COL', COL_BITS_LOW, x, y)

def printBank(bank, begX, begY):
    for i in range(0,len(bank.data)):
        for j in range(0, len(bank.data[i])):
                ws.write(i + begX, j + begY, bank.data[i][j])

def printChannel(ch, begX, begY):
    x = begX
    y = begY
    bId = 0
    for bank in ch.banks:
        ws.write(x, y, f'Bank: {bId}')
        bId += 1
        x += 1
        printBank(bank, x, y)
        x += bank.rows + 1 # +1 for a blank row

def printMemModule(mem, begX, begY):
    x = begX
    y = begY
    ws.write(x, y, f'Memory size={mem.size}')
    x += 1
    printAddrBits(x, y)
    x += 1
    chId = 0
    for ch in mem.channels:
        ws.write(x, y, f'Channel: {chId}')
        chId += 1
        printChannel(ch, x+1, y)
        y += ch.banks[0].cols + 1 # +1 for a blank col



printMemModule(mem, 0, 0)


workbook.close()

