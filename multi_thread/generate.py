import os
import re

fileList = os.listdir("./")

for file in fileList:
    splitFile = os.path.splitext(file)
    fileName,fileType = splitFile
    result = re.match(r'record(.*)', fileName)
    if result:
        fp = open(file, "r")
        recordFile = open("perform.txt", "a+")
        context = fp.read()
        recordFile.write(context)
        fp.close()
        recordFile.close()
