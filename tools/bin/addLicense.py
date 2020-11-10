#!/usr/bin/python3
# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

import re
import sys
import os
import mimetypes

def getLicText(fileExt): 
    comment = '#'
    if re.match(r'\.h\>|\.hpp\>|\.c\>|\.cpp\>', fileExt, re.I):
        comment = '//'

    LICENSE_TEXT = f'{comment} Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.\n\n'
    return LICENSE_TEXT

def hasLicense(fileName):
    licPatt = re.compile(r'license|copyright', re.I)
    with open(fileName, 'r') as fh:
        for l in fh:
            if licPatt.search(l):
                return True
    return False

def addLicense(fileName):
    bname = os.path.basename(fileName)
    ext = os.path.splitext(bname)[1]

    # print(f'{bname} {ext}')
    licText = getLicText(ext)

    fh = open(fileName, 'r')
    lines = fh.readlines()
    fh.close()

    lines.insert(0, licText)

    fh = open(fileName, 'w')
    fh.writelines(lines)
    fh.close()
    return

def guessByExt(ext):
    if re.search(r'cpp|cxx|c|h|hpp|py|cmake|txt|sh', ext, re.I):
        return 'text/code'
#end

def checkAndAddLicense(fileName):
    typeStr = mimetypes.guess_type(fileName)[0]

    if not typeStr:
        typeStr = guessByExt(os.path.splitext(fileName)[1])

    if typeStr and re.match(r'text', typeStr): 
        if hasLicense(fileName):
            print(f'Found license header in {fileName}')
        else:
            print(f'Adding license header to {fileName}')
            addLicense(fileName)

    else:
        print(f'Not a text file. Ignoring {fileName}, file mime-type {typeStr}')
    return

def traverseDir(dirName):
    for root, dirs, files in os.walk(dirName):
        for f in files:
            checkAndAddLicense(os.path.join(root, f))
    return

def main():
    if len(sys.argv) != 2:
        sys.exit(f'{argv[0]} <filename>')

    mimetypes.init()

    fileOrDir = sys.argv[1]

    if os.path.isfile(fileOrDir):
        checkAndAddLicense(fileOrDir)
    elif os.path.isdir(fileOrDir):
        traverseDir(fileOrDir)
    else:
        sys.exit(f'{fileOrDir} not a valid file or directory')


    return


if __name__ == '__main__':
    main()


