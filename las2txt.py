# -*- coding: utf-8 -*-
import subprocess
import os


def process():
    L = file_name("data")
    exefile = "lastools/bin/las2txt.exe"
    for file in L:
        # input = file+"\n"+file[:-3]+".txt "
        input = file + "\n" + "result_data" + file[4:-4] + ".txt "
        print(input)
        parameter = bytes(input, encoding="utf8")
        program = subprocess.Popen(exefile, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        program.communicate(input=parameter)
        print("finished!")
    return

def file_name(file_dir):
    L=[]
    for root, dirs, files in os.walk(file_dir):
        for file in files:
            if os.path.splitext(file)[1] == '.las':
                L.append(os.path.join(root, file))
    return L


if __name__ == '__main__':
    process()